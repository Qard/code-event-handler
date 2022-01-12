const CodeEvents = require('bindings')('addon.node')
const { createWriteStream } = require('fs')

function hex (s) {
  return s.toString(16)
}

function location ({ scriptName, line }) {
  return scriptName ? ` ${scriptName}:${line}` : ''
}

function symbolize ({ comment, functionName, ...event }) {
  return comment || `${functionName}${location(event)}`
}

function filterFunc (accept) {
  if (typeof accept === 'string') {
    accept = accept.split(',')
  }

  return Array.isArray(accept)
    ? event => accept.includes(event.type)
    : accept
}

CodeEvents.perfMap = function perfMap ({
  path = `/tmp/perf-${process.pid}.map`,
  accept
} = {}) {
  const filter = filterFunc(accept)
  const out = createWriteStream(path)

  function onEvent (event) {
    if (filter && !filter(event)) return
    const { address, size, type } = event
    out.write(`${hex(address)} ${hex(size)} ${type}:${symbolize(event)}\n`)
  }

  const codeEvents = new CodeEvents(onEvent)
  codeEvents.enable()

  process.on('beforeExit', () => {
    codeEvents.disable()
    out.end()
  })
}

module.exports = CodeEvents
