const { createClient, lanAddress, displayState } = require('../../utils/client')
Page({
  data: {
    ip: '', token: '', connected: false, busy: false, state: null, display: {},
    message: '连接 Korvo 后查看状态；手机和 Korvo 必须在同一网段',
    fanOn: '30', fanOff: '28', lightOn: '100', lightOff: '150'
  },
  onLoad() {
    const settings = wx.getStorageSync('home_connection') || {}
    this.setData({ ip: settings.ip || '', token: settings.token || '' })
  },
  onShow() { this._visible = true; if (this._client) this.poll() },
  onHide() { this._visible = false; clearTimeout(this._timer); this.setData({ connected: false }) },
  onUnload() { this.onHide() },
  input(e) { this.setData({ [e.currentTarget.dataset.key]: e.detail.value }) },
  connect() {
    const ip = this.data.ip.trim(), token = this.data.token.trim()
    if (!lanAddress(ip) || !/^[A-Za-z0-9_-]{16,64}$/.test(token)) {
      this.setData({ message: '请输入 Korvo 的内网 IPv4 地址和 16～64 位访问令牌' }); return
    }
    clearTimeout(this._timer)
    this._generation = (this._generation || 0) + 1
    this._client = createClient(o => wx.request(o), 'http://' + ip, token)
    this._thresholdsLoaded = false
    wx.setStorageSync('home_connection', { ip, token })
    this._visible = true
    this.poll()
  },
  async poll() {
    if (!this._visible || !this._client || this._polling) return
    this._polling = true
    const generation = this._generation
    try {
      const s = await this._client.state()
      if (!this._visible || generation !== this._generation) return
      if (s.schema !== 1) throw new Error('状态格式不兼容')
      this._lastStateTime = Date.now()
      const update = { state: s, display: displayState(s), connected: true }
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
      if (this._visible && generation === this._generation)
        this.setData({ connected: false, display: {}, message: e.message, busy: false })
      this._operation = null
    } finally {
      this._polling = false
      if (this._visible) this._timer = setTimeout(() => this.poll(), 1000)
    }
  },
  async send(command) {
    if (!this._client || !this.data.connected || this.data.busy || Date.now() - this._lastStateTime > 3000) return
    this.setData({ busy: true, message: '正在提交请求…' })
    try {
      const id = await this._client.send(command)
      this._operation = { id, deadline: Date.now() + 5000 }
      this.setData({ message: '请求已入队，等待 STM32/保存结果…' })
    } catch (e) {
      this._operation = null
      this.setData({ busy: false, message: e.message + '；结果未知时先刷新观察，不自动重发' })
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
