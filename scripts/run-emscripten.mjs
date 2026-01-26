#!/usr/bin/env node

import path from 'node:path';
import { parseArgs } from "node:util";

function help() {
  console.log(`Usage: run-emscripten.mjs [--preload FILE] [script.js] -- [arguments]

Options:
  --preload FILE: Load the package created by Emscripten's file_packager.py`);
}

const { values, positionals } = parseArgs({
  allowPositionals: true,
  options: {
    'preload': { type: 'string' },
    'help': { type: 'boolean' },
  },
})
if (values.help) {
  help();
  process.exit(0);
}
const preload = values.preload;
const moduleFile = positionals[0];
const moduleArgs = positionals.slice(1);
if ((!moduleFile) || (!moduleFile.match(/.*\.m?js$/))) {
  console.error("module JS file must be specified as the first argument");
  help();
  process.exit(1);
}

const targetModule = await import(path.resolve(moduleFile));
let preloadModule;
if (preload) preloadModule = await import(path.resolve(preload));

var Module = {
  preRun: [],
  arguments: moduleArgs,
  mainScriptUrlOrBlob: path.resolve(moduleFile),
};
Module["preRun"].push((Module) => {
  const decoder = new TextDecoder('utf-8');
  Module.TTY.default_tty_ops.put_char = (tty, val) => {
    process.stdout.write(decoder.decode(new Uint8Array([val])));
  }
  Module.TTY.default_tty1_ops.put_char = (tty, val) => {
    process.stderr.write(decoder.decode(new Uint8Array([val])));
  }
});
if (preloadModule) preloadModule.default(Module);

if (process.stdin.isTTY) {
  process.stdin.setRawMode(true);
}
function restoreTTY() {
  if (process.stdin.isTTY) {
    process.stdin.setRawMode(false);
  }
  process.exit(0);
}
process.on('exit', restoreTTY);
process.on('SIGINT', restoreTTY);
process.on('SIGTERM', restoreTTY);

await targetModule.default(Module);
