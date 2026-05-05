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
export declare class WebSocketTransport implements Transport {
    private readonly url;
    private ws;
    private readonly handlers;
    constructor(url: string);
    connect(): Promise<void>;
    send(message: ClientMessage): void;
    onMessage(handler: (message: ServerMessage) => void): () => void;
    disconnect(): void;
    get connected(): boolean;
}
/**
 * In-memory transport for tests and offline demos.
 *
 * The test code calls simulateMessage() to push server -> client events;
 * outgoing client -> server messages are collected in the `sent` array.
 */
export declare class MockTransport implements Transport {
    private readonly handlers;
    private _connected;
    readonly sent: ClientMessage[];
    connect(): Promise<void>;
    send(message: ClientMessage): void;
    onMessage(handler: (message: ServerMessage) => void): () => void;
    disconnect(): void;
    get connected(): boolean;
    /** Simulate the server pushing a message to the client. */
    simulateMessage(message: ServerMessage): void;
    /** Clears the sent message log. */
    clearSent(): void;
}
//# sourceMappingURL=transport.d.ts.map