// No MQTT library or cloud service is needed on the phone. Korvo uploads MQTT;
// this client talks to the authenticated LAN API and waits for STM32 reports.
function lanAddress(ip) {
  if (!/^(\d{1,3}\.){3}\d{1,3}$/.test(ip)) return false
  const parts = ip.split('.')
  if (parts.some(part => part !== String(Number(part)))) return false
  const p = parts.map(Number)
  if (p.some(n => n > 255) || p[3] === 0 || p[3] === 255) return false
  return p[0] === 10 || (p[0] === 172 && p[1] >= 16 && p[1] <= 31) || (p[0] === 192 && p[1] === 168)
}

function createClient(request, baseUrl, token) {
  let nextId = 1 + Math.floor(Math.random() * 1000000000)
  function call(path, method, data) {
    return new Promise((resolve, reject) => request({
      url: baseUrl + path, method, data, timeout: 2500,
      header: { Authorization: 'Bearer ' + token, 'content-type': 'application/json' },
      success: res => {
        if (res.statusCode >= 200 && res.statusCode < 300) resolve(res.data)
        else reject(new Error((res.data && res.data.error) || ('HTTP ' + res.statusCode)))
      },
      fail: () => reject(new Error('网络不可用：检查 Wi-Fi、同网段 IP 和手机局域网权限'))
    }))
  }
  return {
    state: () => call('/api/state', 'GET'),
    // A network timeout is an UNKNOWN result. Never retry automatically.
    send: command => {
      nextId = nextId >= 2147483647 ? 1 : nextId + 1
      const body = Object.assign({}, command, { id: nextId })
      return call('/api/control', 'POST', body).then(result => {
        if (result.id !== body.id || result.queued !== true) throw new Error('无效的控制应答')
        return body.id
      })
    }
  }
}

function displayState(s) {
  const reading = (n, unit, fault) => Number.isFinite(n) && !s.stale && !fault ? n.toFixed(1) + unit : '-- / 故障或过期'
  const output = n => {
    if (!s.outputs_known && !s.simulated) return '未知'
    const origin = s.simulated ? '模拟' : 'STM32 回报'
    return n === true ? '开（' + origin + '）' : n === false ? '关（' + origin + '）' : '未知'
  }
  return {
    temperature: reading(s.temperature_c, ' ℃', s.temperature_fault), humidity: reading(s.humidity_pct, ' %', s.humidity_fault),
    light: reading(s.illuminance_lux, ' lx', s.light_fault), fan: output(s.fan_on), lamp: output(s.light_on),
    source: s.source === 'sensor' ? '真实传感器' : s.source === 'test' ? '测试数据' : s.source === 'simulation' ? '离线模拟' : '无数据',
    mode: s.mode === 'auto' ? '自动' : s.mode === 'manual' ? '手动' : '未知'
  }
}

// This is our schema-1 payload contract, not a guarantee made by wx.request.
// In particular the string "false" must never be treated as a true mode/link.
function validState(s) {
  if (!s || typeof s !== 'object' || Array.isArray(s) || s.schema !== 1) return false
  const flags = ['simulated', 'online', 'synced', 'stale', 'outputs_known', 'pending',
    'temperature_fault', 'humidity_fault', 'light_fault', 'wifi', 'mqtt', 'sd_ok']
  if (!flags.every(key => typeof s[key] === 'boolean') || s.physical_feedback !== false) return false
  if (!['sensor', 'test', 'simulation', 'none'].includes(s.source) || s.simulated !== (s.source === 'simulation')) return false
  const count = n => Number.isInteger(n) && n >= 0 && n <= 2147483647
  if (![s.sample_age_ms, s.link_age_ms, s.request_id, s.rejected_id].every(count) ||
      !Number.isInteger(s.stale_ms) || s.stale_ms < 1000 || s.stale_ms > 60000 ||
      !Number.isInteger(s.stage) || s.stage < 0 || s.stage > 4) return false
  const reading = (key, fault, low, high) => s.stale || s[fault] ? s[key] === null :
    Number.isFinite(s[key]) && s[key] >= low && s[key] <= high
  if (!reading('temperature_c', 'temperature_fault', -40, 125) ||
      !reading('humidity_pct', 'humidity_fault', 0, 100) ||
      !reading('illuminance_lux', 'light_fault', 0, 100000)) return false
  if (s.outputs_known || s.simulated) {
    if (!['auto', 'manual'].includes(s.mode) || typeof s.fan_on !== 'boolean' || typeof s.light_on !== 'boolean') return false
  } else if (s.mode !== 'unknown' || s.fan_on !== null || s.light_on !== null) return false
  if (!s.simulated && s.outputs_known && !s.online) return false
  return ['none', 'pending', 'ack', 'rejected', 'timeout', 'simulated', 'saved', 'save_error'].includes(s.request_result) &&
    typeof s.request_status === 'string' &&
    [s.fan_on_c, s.fan_off_c, s.light_on_lux, s.light_off_lux].every(Number.isFinite) &&
    s.fan_on_c > s.fan_off_c && s.light_on_lux < s.light_off_lux
}

// Add the whole request/transport interval conservatively. Receiving a delayed
// HTTP response must not turn an old sample or old link report fresh again.
function ageState(snapshot, elapsedMs) {
  const s = Object.assign({}, snapshot)
  const elapsed = Math.max(0, elapsedMs)
  s.sample_age_ms = Math.min(2147483647, snapshot.sample_age_ms + elapsed)
  s.link_age_ms = Math.min(2147483647, snapshot.link_age_ms + elapsed)
  s.stale = snapshot.stale || s.sample_age_ms >= snapshot.stale_ms
  if (!s.simulated && (!snapshot.online || s.link_age_ms >= 3000)) {
    s.online = s.synced = s.outputs_known = false
    s.stale = true
    s.mode = 'unknown'; s.light_on = s.fan_on = null
  }
  if (s.stale) s.temperature_c = s.humidity_pct = s.illuminance_lux = null
  return s
}
module.exports = { createClient, lanAddress, displayState, ageState, validState }
