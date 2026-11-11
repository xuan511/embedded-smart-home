// HOST ONLY. Real local MQTT packets + phone client + production portable C.
// The HTTP adapter, sensor samples, clock and UART transport are test fixtures;
// ESP-IDF Wi-Fi/httpd/MQTT, USB enumeration, NVS and physical loads are NOT run.
const assert = require('node:assert/strict')
const http = require('node:http')
const net = require('node:net')
const { spawn } = require('node:child_process')
const readline = require('node:readline')
const path = require('node:path')
const mqtt = require('mqtt')
const aedes = require('aedes')()
const { createClient, lanAddress, displayState } = require('../../miniprogram/utils/client')
const root = path.resolve(__dirname, '../..')
const executable = path.join(root, '.test-deps', process.platform === 'win32' ? 'bridge_fixture.exe' : 'bridge_fixture')
const fixture = spawn(executable, [], { stdio: ['pipe', 'pipe', 'inherit'] })
let waiter, lastState
const lines = readline.createInterface({ input: fixture.stdout })
lines.on('line', line => { const resolve = waiter; waiter = null; if (resolve) resolve(JSON.parse(line)) })
fixture.on('error', e => { console.error(e); process.exitCode = 1 })
let serial = Promise.resolve()
function command(text) {
  const task = serial.then(() => new Promise(resolve => { waiter = resolve; fixture.stdin.write(text + '\n') }))
  serial = task.then(() => {}); return task
}
const token = 'host-test-token-only-1234'
const broker = net.createServer(aedes.handle)
const server = http.createServer(async (req, res) => {
  const reply = (status, body) => { res.writeHead(status, { 'content-type': 'application/json', 'cache-control': 'no-store' }); res.end(JSON.stringify(body)) }
  if (req.headers.authorization !== 'Bearer ' + token) return reply(401, { error: 'unauthorized' })
  if (req.url === '/api/state' && req.method === 'GET') return reply(200, await command('@state'))
  if (req.url !== '/api/control' || req.method !== 'POST') return reply(404, { error: 'not_found' })
  let body = ''
  for await (const chunk of req) { body += chunk; if (body.length > 512) return reply(413, { error: 'body_size' }) }
  const result = await command(body)
  if (result.error) return reply(400, result)
  lastState = result
  reply(202, { id: result.request_id, queued: true, duplicate: false })
})
const listen = server => new Promise(resolve => server.listen(0, '127.0.0.1', resolve))
function transport(options) {
  fetch(options.url, {
    method: options.method, headers: options.header,
    body: options.method === 'POST' ? JSON.stringify(options.data) : undefined,
    signal: AbortSignal.timeout(options.timeout)
  }).then(async res => options.success({ statusCode: res.status, data: await res.json() })).catch(options.fail)
}
let pub, sub
async function main() {
  await listen(broker); await listen(server)
  const brokerUrl = 'mqtt://127.0.0.1:' + broker.address().port
  sub = await mqtt.connectAsync(brokerUrl, { reconnectPeriod: 0 })
  await sub.subscribeAsync('home/host-fixture/#')
  const messages = []
  sub.on('message', (topic, payload) => messages.push({ topic, body: payload.toString() }))
  pub = await mqtt.connectAsync(brokerUrl, { reconnectPeriod: 0, will: { topic: 'home/host-fixture/availability', payload: 'offline', qos: 1, retain: true } })
  await pub.publishAsync('home/host-fixture/availability', 'online', { qos: 1, retain: true })
  const client = createClient(transport, 'http://127.0.0.1:' + server.address().port, token)
  await command('@sample 27 55 200')
  let s = await client.state()
  assert.equal(s.source, 'test'); assert.equal(s.temperature_c, 27); assert.equal(s.physical_feedback, false)
  const id = await client.send({ action: 'fan', on: true })
  s = await client.state(); assert.equal(s.request_id, id); assert.equal(s.request_result, 'ack'); assert.equal(s.fan_on, true)
  assert.equal(s.mode, 'manual')
  await assert.rejects(createClient(transport, 'http://127.0.0.1:' + server.address().port, 'wrong').state(), /unauthorized/)
  await assert.rejects(client.send({ action: 'fan', on: 1 }), /invalid_command/)
  await client.send({ action: 'thresholds', fan_on_c: 35, fan_off_c: 30, light_on_lux: 80, light_off_lux: 120 })
  s = await client.state(); assert.equal(s.synced, true); assert.equal(s.fan_on_c, 35)
  await client.send({ action: 'mode', mode: 'auto' })
  s = await command('@sample 36 55 50'); assert.equal(s.fan_on, true); assert.equal(s.light_on, true)
  s = await command('@sample 34 55 100'); assert.equal(s.fan_on, true); assert.equal(s.light_on, true)
  s = await command('@sample 29 55 140'); assert.equal(s.fan_on, false); assert.equal(s.light_on, false)
  s = await command('@fault'); assert.equal(s.temperature_c, null); assert.equal(s.fan_on, false); assert.equal(s.light_on, false)
  await command('@sample 36 55 50')
  s = await command('@age'); assert.equal(s.stale, true); assert.equal(s.temperature_c, null)
  await client.send({ action: 'fan', on: true })
  s = await command('@detach'); assert.equal(s.online, false); assert.equal(s.stale, true); assert.equal(s.fan_on, null)
  assert.equal(s.source, 'none'); assert.equal(s.simulated, false)
  await command('@attach'); s = await command('@sample 29 55 200')
  assert.equal(s.synced, true); assert.equal(s.fan_on_c, 35); assert.equal(s.fan_on, false)
  await pub.publishAsync('home/host-fixture/state', JSON.stringify(s), { qos: 0, retain: false })
  const until = Date.now() + 2000
  while (!messages.some(m => m.topic.endsWith('/state')) && Date.now() < until) await new Promise(r => setTimeout(r, 20))
  const received = messages.find(m => m.topic.endsWith('/state')); assert.ok(received)
  assert.deepEqual(JSON.parse(received.body), s)
  pub.stream.destroy() // Unexpected transport loss must exercise the actual MQTT Last Will.
  const deadline = Date.now() + 2000
  while (!messages.some(m => m.body === 'offline') && Date.now() < deadline) await new Promise(r => setTimeout(r, 20))
  assert.ok(messages.some(m => m.topic.endsWith('/availability') && m.body === 'offline'))
  assert.equal(lanAddress('192.168.1.22'), true); assert.equal(lanAddress('172.20.1.1'), true)
  assert.equal(lanAddress('8.8.8.8'), false); assert.equal(lanAddress('192.168.1.300'), false)
  assert.equal(displayState({ stale: true, temperature_c: 30 }).temperature, '-- / 故障或过期')
  console.log('PASS: actual phone client -> HTTP fixture -> C codec/Korvo/STM32 state machines -> ACK')
  console.log('PASS: sensor TEST fixtures, fan/light manual/auto/hysteresis, fault/stale/disconnect/reconnect/config sync')
  console.log('PASS: real local MQTT publish/subscribe and unexpected-disconnect Last Will; invalid token/input')
  console.log('NOT RUN: USB host, Wi-Fi, ESP httpd/ESP-MQTT runtime, NVS power cycle, WeChat runtime and physical sensors/fan')
}
const timeout = setTimeout(() => { console.error('FAIL: host integration timeout'); process.exit(1) }, 20000)
main().catch(e => { console.error(e); process.exitCode = 1 }).finally(async () => {
  clearTimeout(timeout); fixture.stdin.end(); fixture.kill(); lines.close()
  if (pub) await pub.endAsync(true).catch(() => {})
  if (sub) await sub.endAsync(true).catch(() => {})
  server.closeAllConnections(); server.close(); broker.close(); aedes.close()
})
