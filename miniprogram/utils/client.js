// No MQTT library or cloud service is needed on the phone. Korvo uploads MQTT;
// this client talks to the authenticated LAN API and waits for STM32 reports.
function lanAddress(ip) {
  if (!/^(\d{1,3}\.){3}\d{1,3}$/.test(ip)) return false
  const p = ip.split('.').map(Number)
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
  const reading = (n, unit) => Number.isFinite(n) && !s.stale ? n.toFixed(1) + unit : '-- / 故障或过期'
  const output = n => n === true ? '开（STM32 回报）' : n === false ? '关（STM32 回报）' : '未知'
  return {
    temperature: reading(s.temperature_c, ' ℃'), humidity: reading(s.humidity_pct, ' %'),
    light: reading(s.illuminance_lux, ' lx'), fan: output(s.fan_on), lamp: output(s.light_on),
    source: s.source === 'sensor' ? '真实传感器' : s.source === 'test' ? '测试数据' : s.source === 'simulation' ? '离线模拟' : '无数据',
    mode: s.mode === 'auto' ? '自动' : s.mode === 'manual' ? '手动' : '未知'
  }
}
module.exports = { createClient, lanAddress, displayState }
