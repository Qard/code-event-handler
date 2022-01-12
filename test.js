const assert = require('assert')
const CodeEvents = require('./')

let called = false

const types = [
  'Builtin',
  'Callback',
  'Eval',
  'Function',
  'InterpretedFunction',
  'Handler',
  'BytecodeHandler',
  'LazyCompile',
  'RegExp',
  'Script',
  'Stub',
  'Relocation'
]

const events = new CodeEvents(event => {
  called = true

  assert.ok(typeof event.address === 'number', 'address should be a number')
  assert.ok(typeof event.previousAddress === 'number', 'previousAddress should be a number')
  assert.ok(typeof event.size === 'number', 'size should be a number')
  assert.ok(types.includes(event.type), `type should be one of: ${types.join(', ')}`)
  assert.ok(typeof event.comment === 'string', 'comment should be a string')
  assert.ok(typeof event.functionName === 'string', 'functionName should be a string')
  assert.ok(typeof event.scriptName === 'string', 'scriptName should be a string')
  assert.ok(typeof event.line === 'number', 'line should be a number')
  assert.ok(typeof event.column === 'number', 'column should be a number')
})

events.enable()

setTimeout(() => {
  events.disable()
}, 100)

process.on('beforeExit', () => {
  assert.equal(called, true, 'should have called the code event handler')
  console.log('OK')
})
