/**
 * @aetheris-iui/node - Node.js binding via stdio IPC.
 *
 * Wraps the aetheris_stdio_server binary in a child_process, communicates
 * via JSON-lines on stdin/stdout.  Suitable for use in Electron apps,
 * Node.js services, and CLI tools that embed Aetheris-IUI as a sidecar.
 *
 * Usage:
 *   import { AetherisClient } from './index.mjs';
 *   const client = new AetherisClient('/path/to/aetheris_stdio_server');
 *   await client.connect();
 *   const { version } = await client.version();
 *   await client.close();
 */

import { spawn } from 'node:child_process';
import { createInterface } from 'node:readline';

export class AetherisError extends Error {
  constructor(message) {
    super(message);
    this.name = 'AetherisError';
  }
}

export class AetherisClient {
  #proc = null;
  #rl = null;
  #pending = [];
  #connected = false;

  constructor(serverPath) {
    this.serverPath = serverPath;
  }

  /** Start the server process and wait until it's ready. */
  async connect() {
    if (this.#connected) return;

    this.#proc = spawn(this.serverPath, [], {
      stdio: ['pipe', 'pipe', 'pipe'],
    });

    this.#proc.on('error', (err) => {
      this.#reject(new AetherisError(`server process error: ${err.message}`));
    });

    this.#rl = createInterface({ input: this.#proc.stdout, crlfDelay: Infinity });
    this.#rl.on('line', (line) => {
      const resolve = this.#pending.shift();
      if (resolve) resolve(JSON.parse(line));
    });

    this.#connected = true;
  }

  /** Send a ping; resolves with { type: 'pong', version: string }. */
  async ping() {
    return this.#request({ type: 'ping' });
  }

  /** Get version info; resolves with { type: 'version', version: string, abi: number }. */
  async version() {
    return this.#request({ type: 'version' });
  }

  /**
   * Project a session through the interface layer.
   * @param {object} session  Plain-object matching the Session interface.
   * @returns {Promise<{type:'snapshot', snapshot: object}>}
   */
  async sessionSnapshot(session) {
    const resp = await this.#request({ type: 'snapshot', session });
    if (resp.type === 'error') {
      throw new AetherisError(resp.message ?? 'unknown server error');
    }
    return resp;
  }

  /** Graceful shutdown. Resolves when the server process exits. */
  async close() {
    if (!this.#connected) return;
    this.#connected = false;
    try {
      await this.#request({ type: 'quit' });
    } catch { /* server exits immediately */ }
    this.#proc.stdin.end();
    await new Promise((resolve) => this.#proc.once('close', resolve));
  }

  async #request(msg) {
    if (!this.#connected) throw new AetherisError('client is not connected');
    return new Promise((resolve) => {
      this.#pending.push(resolve);
      this.#proc.stdin.write(JSON.stringify(msg) + '\n');
    });
  }

  #reject(err) {
    const pending = this.#pending.splice(0);
    for (const resolve of pending) resolve({ type: 'error', message: err.message });
  }
}
