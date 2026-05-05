/** Production transport over a native WebSocket connection. */
export class WebSocketTransport {
    url;
    ws = null;
    handlers = new Set();
    constructor(url) {
        this.url = url;
    }
    async connect() {
        return new Promise((resolve, reject) => {
            this.ws = new WebSocket(this.url);
            this.ws.onopen = () => resolve();
            this.ws.onerror = () => reject(new Error(`WebSocket connection failed: ${this.url}`));
            this.ws.onmessage = (event) => {
                try {
                    const msg = JSON.parse(event.data);
                    this.handlers.forEach((h) => h(msg));
                }
                catch {
                    console.error('[AetherisIUI] Failed to parse server message', event.data);
                }
            };
        });
    }
    send(message) {
        if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
            throw new Error('WebSocket is not connected');
        }
        this.ws.send(JSON.stringify(message));
    }
    onMessage(handler) {
        this.handlers.add(handler);
        return () => this.handlers.delete(handler);
    }
    disconnect() {
        this.ws?.close();
        this.ws = null;
    }
    get connected() {
        return this.ws?.readyState === WebSocket.OPEN;
    }
}
/**
 * In-memory transport for tests and offline demos.
 *
 * The test code calls simulateMessage() to push server -> client events;
 * outgoing client -> server messages are collected in the `sent` array.
 */
export class MockTransport {
    handlers = new Set();
    _connected = false;
    sent = [];
    async connect() {
        this._connected = true;
    }
    send(message) {
        if (!this._connected)
            throw new Error('MockTransport is not connected');
        this.sent.push(message);
    }
    onMessage(handler) {
        this.handlers.add(handler);
        return () => this.handlers.delete(handler);
    }
    disconnect() {
        this._connected = false;
    }
    get connected() {
        return this._connected;
    }
    /** Simulate the server pushing a message to the client. */
    simulateMessage(message) {
        this.handlers.forEach((h) => h(message));
    }
    /** Clears the sent message log. */
    clearSent() {
        this.sent.length = 0;
    }
}
//# sourceMappingURL=transport.js.map