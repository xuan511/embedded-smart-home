// SPDX-License-Identifier: MIT
// Page lifecycle/HTTP timing tests with official API-shaped callbacks, not a
// WeChat runtime or a physical-device test.
const assert = require('node:assert/strict')
const fs = require('node:fs')
const path = require('node:path')
const vm = require('node:vm')
const { createRequire } = require('node:module')
const client = require('../miniprogram/utils/client')

const pagePath = path.resolve(__dirname, '../miniprogram/pages/home/home.js')
function fixture() {
  let definition, clock = 10000, nextTimer = 0
  const requests = [], timers = new Map()
  const wx = {
    getStorageSync: () => null, setStorageSync: () => {},
    request: options => { requests.push(options); return { abort() {} } }
  }
  vm.runInNewContext(fs.readFileSync(pagePath, 'utf8'), {
    require: createRequire(pagePath), Page: value => { definition = value }, wx,
    Date: { now: () => clock },
    setTimeout: (fn, delay) => { const id = ++nextTimer; timers.set(id, { fn, delay }); return id },
    clearTimeout: id => timers.delete(id)
  }, { filename: pagePath })
  const page = Object.assign({}, definition, { data: structuredClone(definition.data) })
  page.setData = update => Object.assign(page.data, update)
  page.onLoad(); page.onShow()
  return {
    page, requests, timers,
    advance: amount => { clock += amount },
    respond: (index, data) => requests[index].success({ statusCode: 200, data }),
    fail: index => requests[index].fail({ errMsg: 'request:fail timeout' })
  }
}
const flush = () => new Promise(resolve => setImmediate(resolve))
function state(extra = {}) {
  return Object.assign({
    schema: 1, source: 'sensor', stage: 4, simulated: false,
    online: true, synced: true, stale: false, sample_age_ms: 50, stale_ms: 5000, link_age_ms: 10,
    temperature_c: 27, humidity_pct: 55, illuminance_lux: 200,
    temperature_fault: false, humidity_fault: false, light_fault: false,
    outputs_known: true, mode: 'auto', light_on: false, fan_on: false, wifi: true, mqtt: false, sd_ok: false, physical_feedback: false,
    fan_on_c: 28, fan_off_c: 26, light_on_lux: 100, light_off_lux: 200,
    request_id: 0, rejected_id: 0, request_result: 'none', request_status: '', pending: false
  }, extra)
}
async function connect(f, ip = '192.168.1.10') {
  f.page.setData({ ip, token: 'test-token-1234567890' })
  f.page.connect()
  f.respond(f.requests.length - 1, state())
  await flush()
  assert.equal(f.page.data.connected, true)
}

async function run() {
  // Connecting to B must invalidate A before B's first response is received.
  {
    const f = fixture(); await connect(f)
    f.page.setData({ ip: '192.168.1.20' }); f.page.connect()
    assert.equal(f.page.data.connected, false, 'must not use A state while connecting to B')
    const count = f.requests.length
    await f.page.send({ action: 'fan', on: true })
    assert.equal(f.requests.length, count, 'unverified B must not receive a control request')
  }
  // A late failed poll from A must not erase the active operation for B.
  {
    const f = fixture(); await connect(f)
    const oldPoll = f.page.poll(); const oldIndex = f.requests.length - 1
    f.page.setData({ ip: '192.168.1.20' }); f.page.connect()
    const newIndex = f.requests.length - 1
    assert.notEqual(newIndex, oldIndex, 'new device poll must not wait on an old device')
    f.respond(newIndex, state()); await flush()
    const sending = f.page.send({ action: 'fan', on: true })
    const control = f.requests[f.requests.length - 1]
    control.success({ statusCode: 202, data: { id: control.data.id, queued: true } })
    await sending
    const operation = f.page._operation
    f.fail(oldIndex); await oldPoll
    assert.equal(f.page._operation, operation, 'old failure must not cancel new operation')
    assert.equal(f.page.data.connected, true)
  }
  // Leaving the page invalidates pending callback results without replaying.
  {
    const f = fixture(); await connect(f)
    const sending = f.page.send({ action: 'fan', on: true })
    const control = f.requests[f.requests.length - 1]
    f.page.onHide()
    control.success({ statusCode: 202, data: { id: control.data.id, queued: true } })
    await sending
    assert.equal(f.page.data.connected, false)
    assert.equal(f.page.data.busy, false)
    assert.equal(f.page._operation, null)
    f.page.onShow()
    assert.equal(f.requests.filter(r => r.method === 'POST').length, 1, 'returning to page must not replay')
  }
  // Request transmission time counts against sample/link freshness.
  {
    const f = fixture()
    f.page.setData({ ip: '192.168.1.10', token: 'test-token-1234567890' })
    f.page.connect(); f.advance(400)
    f.respond(0, state({ sample_age_ms: 4800, link_age_ms: 2800 })); await flush()
    assert.equal(f.page.data.state.stale, true)
    assert.equal(f.page.data.state.online, false)
    assert.equal(f.page.data.state.temperature_c, null)
    assert.equal(f.page.data.state.fan_on, null)
    assert.equal(f.page.data.canControl, false)
    f.advance(3000); f.page.refreshAge()
    assert.equal(f.page.data.connected, false, 'old snapshot must expire even if next poll has not finished')
  }
  // Queue acceptance must remain pending until a matching terminal result.
  {
    const f = fixture(); await connect(f)
    const sending = f.page.send({ action: 'fan', on: true })
    const control = f.requests[f.requests.length - 1]
    control.success({ statusCode: 202, data: { id: control.data.id, queued: true } }); await sending
    assert.equal(f.page.data.busy, true)
    assert.equal(f.page.data.state.fan_on, false)
    const poll = f.page.poll()
    f.respond(f.requests.length - 1, state({ request_id: control.data.id, request_result: 'ack', fan_on: true }))
    await poll
    assert.equal(f.page.data.busy, false)
    assert.equal(f.page.data.state.fan_on, true)
    assert.match(f.page.data.message, /STM32/)
  }
  // A GET failure while POST is still in flight must not allow a second POST.
  {
    const f = fixture(); await connect(f)
    const sending = f.page.send({ action: 'fan', on: true })
    const control = f.requests[f.requests.length - 1]
    const failedPoll = f.page.poll()
    f.fail(f.requests.length - 1); await failedPoll
    assert.equal(f.page.data.busy, true)
    assert.match(f.page.data.message, /结果未知/)
    const freshPoll = f.page.poll()
    f.respond(f.requests.length - 1, state()); await freshPoll
    await f.page.send({ action: 'light', on: true })
    assert.equal(f.requests.filter(r => r.method === 'POST').length, 1)
    control.success({ statusCode: 202, data: { id: control.data.id, queued: true } }); await sending
    assert.equal(f.page.data.busy, true)
  }
  assert.equal(client.lanAddress('192.168.1.010'), false, 'reject ambiguous leading-zero octets')
  // HTTP success is insufficient: reject truthy strings and incompatible payloads.
  for (const extra of [
    { simulated: 'false' }, { online: 'false' }, { synced: 'false' }, { outputs_known: 'false' },
    { source: 'unidentified' }, { simulated: true, source: 'sensor' }, { temperature_c: '27' },
    { stale_ms: 0 }, { link_age_ms: -1 }, { fan_on: 'false' }
  ]) {
    const f = fixture()
    f.page.setData({ ip: '192.168.1.10', token: 'test-token-1234567890' }); f.page.connect()
    f.respond(0, state(extra)); await flush()
    assert.equal(f.page.data.connected, false, JSON.stringify(extra))
    assert.equal(f.page.data.canControl, false)
    await f.page.send({ action: 'fan', on: true })
    assert.equal(f.requests.filter(r => r.method === 'POST').length, 0)
  }
  assert.equal(client.validState(state()), true)
  assert.equal(client.validState(state({ simulated: true, source: 'simulation', online: false })), true)
  assert.match(client.displayState(state({ simulated: true, source: 'simulation', fan_on: true })).fan, /模拟/)
  assert.equal(client.displayState(state({ temperature_fault: true })).temperature, '-- / 故障或过期')
  console.log('PASS: mini program device switching, late callbacks, page lifecycle, sample/link ageing and ACK ownership')
  console.log('NOT RUN: official WeChat compiler/runtime, phone LAN permissions and physical sensors/loads')
}
run().catch(error => { console.error(error); process.exitCode = 1 })
