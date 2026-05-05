import type { ProcessIntentRequest } from './types.js';
import type { Transport } from './transport.js';
import { AetherisSession } from './session.js';
export type ErrorHandler = (code: string, message: string) => void;
export interface ClientOptions {
    /** Called for any server-side error message not tied to an active request. */
    onError?: ErrorHandler;
}
/**
 * High-level client that manages the session lifecycle over a Transport.
 *
 * Usage:
 *   const client = new AetherisClient(new WebSocketTransport('ws://localhost:9090'));
 *   await client.connect();
 *   const session = await client.processIntent({ intent_text: '…', operator_id: '…', tenant_id: '…' });
 *   session.addEventListener('change', () => console.log(session.state));
 */
export declare class AetherisClient extends EventTarget {
    private readonly transport;
    private readonly sessions;
    private removeListener;
    private readonly errorHandler;
    constructor(transport: Transport, options?: ClientOptions);
    connect(): Promise<void>;
    disconnect(): void;
    get connected(): boolean;
    /**
     * Sends a 'process_intent' request and resolves with the resulting AetherisSession
     * once the server emits 'session_created'.
     */
    processIntent(request: ProcessIntentRequest): Promise<AetherisSession>;
    fillSlot(sessionId: string, slotName: string, valueJson: string): void;
    preview(sessionId: string): void;
    confirm(sessionId: string): void;
    rejectPreview(sessionId: string): void;
    cancel(sessionId: string, note?: string): void;
    getSession(sessionId: string): AetherisSession | undefined;
    private handleMessage;
    private getOrCreate;
}
//# sourceMappingURL=client.d.ts.map