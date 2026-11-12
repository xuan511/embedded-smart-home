const { createClient, lanAddress, displayState, ageState, validState } = require('../../utils/client')
Page({
  data: {
    ip: '', token: '', connected: false, canControl: false, busy: false, state: null, display: {},
    message: '连接 Korvo 后查看状态；手机和 Korvo 必须在同一网段',
    fanOn: '30', fanOff: '28', lightOn: '100', lightOff: '150'
  },
  onLoad() {
    const settings = wx.getStorageSync('home_connection') || {}
    this.setData({ ip: settings.ip || '', token: settings.token || '' })
  },
  onShow() {
    this._visible = true
    if (this._client) { this.refreshAge(); this.poll() }
  },
  onHide() {
    this._visible = false
    this._generation = (this._generation || 0) + 1
    clearTimeout(this._timer); clearTimeout(this._ageTimer)
    this._freshState = null; this._operation = null; this._sending = null; this._pollOwner = null
    const update = { connected: false, canControl: false, busy: false, state: null, display: {} }
    if (this.data.busy) update.message = '页面已离开，操作结果需刷新后确认；不会自动重发'
    this.setData(update)
  },
  onUnload() { this.onHide() },
  input(e) { this.setData({ [e.currentTarget.dataset.key]: e.detail.value }) },
  connect() {
    const ip = this.data.ip.trim(), token = this.data.token.trim()
    if (!lanAddress(ip) || !/^[A-Za-z0-9_-]{16,64}$/.test(token)) {
      this.setData({ message: '请输入 Korvo 的内网 IPv4 地址和 16～64 位访问令牌' }); return
    }
    clearTimeout(this._timer)
    clearTimeout(this._ageTimer)
    this._generation = (this._generation || 0) + 1
    this._freshState = null; this._operation = null; this._sending = null; this._pollOwner = null
    this._lastStateTime = 0
    this.setData({ connected: false, canControl: false, busy: false, state: null, display: {}, message: '正在获取此设备的状态…' })
    this._client = createClient(o => wx.request(o), 'http://' + ip, token)
    this._thresholdsLoaded = false
    wx.setStorageSync('home_connection', { ip, token })
    this._visible = true
    this.refreshAge()
    this.poll()
  },
  refreshAge() {
    if (!this._visible || !this._client) return
    if (this._freshState) {
      const elapsed = Math.max(0, Date.now() - this._lastStateTime)
      const s = ageState(this._freshState, elapsed)
      const fresh = elapsed < 3000
      const update = {
        state: s, display: fresh ? displayState(s) : {}, connected: fresh,
        canControl: fresh && (s.simulated || (s.online && s.synced))
      }
      if (!fresh && this.data.connected) update.message = '状态连接已过期，等待新数据；操作结果需重新确认，不自动重发'
      this.setData(update)
    }
    clearTimeout(this._ageTimer)
    const generation = this._generation
    this._ageTimer = setTimeout(() => { if (generation === this._generation) this.refreshAge() }, 250)
  },
  async poll() {
    if (!this._visible || !this._client || this._pollOwner === this._generation) return
    const generation = this._generation
    this._pollOwner = generation
    const requestedAt = Date.now(), client = this._client
    try {
      const s = await client.state()
      if (!this._visible || generation !== this._generation) return
      if (!validState(s))
        throw new Error('状态格式不兼容，请更新 Korvo 固件')
      this._lastStateTime = requestedAt
      this._freshState = s
      const aged = ageState(s, Date.now() - requestedAt)
      const fresh = Date.now() - requestedAt < 3000
      const update = { state: aged, display: fresh ? displayState(aged) : {}, connected: fresh, canControl: fresh && (aged.simulated || (aged.online && aged.synced)) }
      if (fresh && !this.data.connected && !this._operation && !this._sending)
        update.message = '已获取控制中心状态；请核对数据来源与 USB/参数状态'
      if (!this._thresholdsLoaded) {
        Object.assign(update, { fanOn: String(s.fan_on_c), fanOff: String(s.fan_off_c), lightOn: String(s.light_on_lux), lightOff: String(s.light_off_lux) })
        this._thresholdsLoaded = true
      }
      const op = this._operation
      if (op && s.rejected_id === op.id) {
        update.message = '控制服务忙，本次请求被拒绝；请等待后重新操作'
        update.busy = false; this._operation = null
      } else if (op && s.request_id === op.id && s.request_result !== 'pending') {
        const labels = { ack: 'STM32 已应答；请观察实际负载', simulated: '仅模拟执行，没有真实负载', saved: '阈值已保存；检查参数同步状态', rejected: 'STM32/控制服务拒绝了请求', timeout: '请求超时，结果未知；不会自动重发', save_error: '保存失败，阈值未更新' }
        update.message = labels[s.request_result] || s.request_status
        update.busy = false; this._operation = null
      } else if (op && Date.now() > op.deadline) {
        update.message = '未收到本次请求的最终结果，状态可能被其他操作替换；不会自动重发'
        update.busy = false; this._operation = null
      }
      this.setData(update)
    } catch (e) {
      if (this._visible && generation === this._generation) {
        const message = e.message + (this.data.busy ? '；操作结果未知，不会自动重发' : '')
        this._freshState = null; this._operation = null
        this.setData({ connected: false, canControl: false, state: null, display: {}, message, busy: !!this._sending })
      }
    } finally {
      if (generation === this._generation) {
        this._pollOwner = null
        clearTimeout(this._timer)
        if (this._visible) this._timer = setTimeout(() => this.poll(), 1000)
      }
    }
  },
  async send(command) {
    if (!this._visible || !this._client || !this.data.connected || this.data.busy || this._sending || Date.now() - this._lastStateTime >= 3000) return
    if (command.action !== 'thresholds' && !this.data.canControl) return
    const generation = this._generation, client = this._client
    const sending = { generation }
    this._sending = sending
    this.setData({ busy: true, message: '正在提交请求…' })
    try {
      const id = await client.send(command)
      if (!this._visible || generation !== this._generation) return
      this._operation = { id, deadline: Date.now() + 5000 }
      this.setData({ busy: true, message: '请求已入队，等待 STM32/保存结果…' })
    } catch (e) {
      if (!this._visible || generation !== this._generation) return
      this._operation = null
      this.setData({ busy: false, message: e.message + '；结果未知时先刷新观察，不自动重发' })
    } finally {
      if (this._sending === sending) this._sending = null
    }
  },
  control(e) {
    const d = e.currentTarget.dataset
    if (d.action === 'mode') this.send({ action: 'mode', mode: d.mode })
    else this.send({ action: d.action, on: d.on === 'true' })
  },
  thresholds() {
    const c = { action: 'thresholds', fan_on_c: Number(this.data.fanOn), fan_off_c: Number(this.data.fanOff), light_on_lux: Number(this.data.lightOn), light_off_lux: Number(this.data.lightOff) }
    if ([this.data.fanOn, this.data.fanOff, this.data.lightOn, this.data.lightOff].some(s => !String(s).trim()) ||
        ![c.fan_on_c, c.fan_off_c, c.light_on_lux, c.light_off_lux].every(Number.isFinite) || c.fan_on_c <= c.fan_off_c || c.light_on_lux >= c.light_off_lux) {
      this.setData({ message: '检查阈值：风扇开启温度 > 关闭温度；开灯照度 < 关灯照度' }); return
    }
    this.send(c)
  }
})
