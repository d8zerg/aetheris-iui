import { AetherisSession } from './session.js';
/**
 * High-level client that manages the session lifecycle over a Transport.
 *
 * Usage:
 *   const client = new AetherisClient(new WebSocketTransport('ws://localhost:9090'));
 *   await client.connect();
 *   const session = await client.processIntent({ intent_text: '…', operator_id: '…', tenant_id: '…' });
 *   session.addEventListener('change', () => console.log(session.state));
 */
export class AetherisClient extends EventTarget {
    transport;
    sessions = new Map();
    removeListener = null;
    errorHandler;
    constructor(transport, options = {}) {
        super();
        this.transport = transport;
        this.errorHandler = options.onError;
    }
    async connect() {
        await this.transport.connect();
        this.removeListener = this.transport.onMessage((msg) => this.handleMessage(msg));
    }
    disconnect() {
        this.removeListener?.();
        this.removeListener = null;
        this.transport.disconnect();
    }
    get connected() {
        return this.transport.connected;
    }
    /**
     * Sends a 'process_intent' request and resolves with the resulting AetherisSession
     * once the server emits 'session_created'.
     */
    async processIntent(request) {
        const msg = {
            type: 'process_intent',
            intent_text: request.intent_text,
            locale: request.locale ?? 'en',
            granted_scopes: request.granted_scopes ?? [],
            operator_id: request.operator_id,
            tenant_id: request.tenant_id,
        };
        return new Promise((resolve, reject) => {
            const unsub = this.transport.onMessage((incoming) => {
                if (incoming.type === 'session_created') {
                    unsub();
                    resolve(this.getOrCreate(incoming.session));
                }
                else if (incoming.type === 'error') {
                    unsub();
                    reject(new Error(`[${incoming.code}] ${incoming.message}`));
                }
            });
            this.transport.send(msg);
        });
    }
    fillSlot(sessionId, slotName, valueJson) {
        this.transport.send({
            type: 'fill_slot',
            session_id: sessionId,
            slot_name: slotName,
            value_json: valueJson,
        });
    }
    preview(sessionId) {
        this.transport.send({ type: 'preview', session_id: sessionId });
    }
    confirm(sessionId) {
        this.transport.send({ type: 'confirm', session_id: sessionId });
    }
    rejectPreview(sessionId) {
        this.transport.send({ type: 'reject_preview', session_id: sessionId });
    }
    cancel(sessionId, note) {
        this.transport.send({ type: 'cancel', session_id: sessionId, note });
    }
    getSession(sessionId) {
        return this.sessions.get(sessionId);
    }
    handleMessage(msg) {
        if (msg.type === 'session_created') {
            const session = this.getOrCreate(msg.session);
            this.dispatchEvent(new CustomEvent('session_created', { detail: { session } }));
        }
        else if (msg.type === 'session_updated') {
            const session = this.getOrCreate(msg.session);
            session._update(msg.session);
            this.dispatchEvent(new CustomEvent('session_updated', { detail: { session } }));
        }
        else if (msg.type === 'error') {
            this.errorHandler?.(msg.code, msg.message);
            this.dispatchEvent(new CustomEvent('error', { detail: msg }));
        }
    }
    getOrCreate(raw) {
        let session = this.sessions.get(raw.id);
        if (!session) {
            session = new AetherisSession(raw);
            this.sessions.set(raw.id, session);
        }
        return session;
    }
}
//# sourceMappingURL=client.js.map