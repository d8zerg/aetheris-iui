import type { ClientMessage, ServerMessage } from './types.js';

/** Abstraction over the communication channel with the C++ backend. */
export interface Transport {
  send(message: ClientMessage): void;
  onMessage(handler: (message: ServerMessage) => void): () => void;
  connect(): Promise<void>;
  disconnect(): void;
  get connected(): boolean;
}

/** Production transport over a native WebSocket connection. */
export class WebSocketTransport implements Transport {
  private ws: WebSocket | null = null;
  private readonly handlers = new Set<(msg: ServerMessage) => void>();

  constructor(private readonly url: string) {}

  async connect(): Promise<void> {
    return new Promise<void>((resolve, reject) => {
      this.ws = new WebSocket(this.url);
      this.ws.onopen = () => resolve();
      this.ws.onerror = () => reject(new Error(`WebSocket connection failed: ${this.url}`));
      this.ws.onmessage = (event: MessageEvent<string>) => {
        try {
          const msg = JSON.parse(event.data) as ServerMessage;
          this.handlers.forEach((h) => h(msg));
        } catch {
          console.error('[AetherisIUI] Failed to parse server message', event.data);
        }
      };
    });
  }

  send(message: ClientMessage): void {
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
      throw new Error('WebSocket is not connected');
    }
    this.ws.send(JSON.stringify(message));
  }

  onMessage(handler: (message: ServerMessage) => void): () => void {
    this.handlers.add(handler);
    return () => this.handlers.delete(handler);
  }

  disconnect(): void {
    this.ws?.close();
    this.ws = null;
  }

  get connected(): boolean {
    return this.ws?.readyState === WebSocket.OPEN;
  }
}

/**
 * In-memory transport for tests and offline demos.
 *
 * The test code calls simulateMessage() to push server -> client events;
 * outgoing client -> server messages are collected in the `sent` array.
 */
export class MockTransport implements Transport {
  private readonly handlers = new Set<(msg: ServerMessage) => void>();
  private _connected = false;
  readonly sent: ClientMessage[] = [];

  async connect(): Promise<void> {
    this._connected = true;
  }

  send(message: ClientMessage): void {
    if (!this._connected) throw new Error('MockTransport is not connected');
    this.sent.push(message);
  }

  onMessage(handler: (message: ServerMessage) => void): () => void {
    this.handlers.add(handler);
    return () => this.handlers.delete(handler);
  }

  disconnect(): void {
    this._connected = false;
  }

  get connected(): boolean {
    return this._connected;
  }

  /** Simulate the server pushing a message to the client. */
  simulateMessage(message: ServerMessage): void {
    this.handlers.forEach((h) => h(message));
  }

  /** Clears the sent message log. */
  clearSent(): void {
    this.sent.length = 0;
  }
}
