# code-event-handler

This is a JavaScript interface to [`v8::CodeEventHandler`][CodeEventHandler].

## Install

```sh
npm install code-event-handler
```

## Usage

```js
const CodeEventHandler = require('code-event-handler')

const events = new CodeEventHandler(event => {
  console.log(event)
})

events.enable()

setTimeout(() => {
  events.disable()
}, 1000)
```

### API

I've simply described the API surface area with TypeScript interfaces for now.
More in-depth docs will come later.

```ts
enum CodeEventType {
  Builtin = 'Builtin',
  Callback = 'Callback',
  Eval = 'Eval',
  Function = 'Function',
  InterpretedFunction = 'InterpretedFunction',
  Handler = 'Handler',
  BytecodeHandler = 'BytecodeHandler',
  LazyCompile = 'LazyCompile',
  RegExp = 'RegExp',
  Script = 'Script',
  Stub = 'Stub',
  Relocation = 'Relocation'
}

interface CodeEvent {
  address: string;
  previousAddress: string;
  size: number;
  functionName: string;
  scriptName: string;
  line: number;
  column: number;
  type: CodeEventType;
  comment: string;
}

interface CodeEventHandler {
  new (handler: (event: CodeEvent): void);
  enable(): void;
  disable(): void;
}
```

## Perf map files

An additional helper to use this interface to generate perf map files exists in
[perf-map.js](perf-map.js). You can preload it into your app by doing:

```sh
node -r code-event-handler/perf-map.js app.js
```

It also accepts environment varialbes for `PERF_MAP_PATH` to change the path
from the default `/tmp/perf-$pid.map` and `PERF_MAP_TYPES` which should be a
comma-separated list of type names to record.

[CodeEventHandler]: https://v8docs.nodesource.com/node-16.13/d2/d08/classv8_1_1_code_event_handler.html
