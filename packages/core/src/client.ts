import type { ClientMessage, ProcessIntentRequest, ServerMessage, Session } from './types.js';
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
export class AetherisClient extends EventTarget {
  private readonly sessions = new Map<string, AetherisSession>();
  private removeListener: (() => void) | null = null;
  private readonly errorHandler: ErrorHandler | undefined;

  constructor(
    private readonly transport: Transport,
    options: ClientOptions = {},
  ) {
    super();
    this.errorHandler = options.onError;
  }

  async connect(): Promise<void> {
    await this.transport.connect();
    this.removeListener = this.transport.onMessage((msg) => this.handleMessage(msg));
  }

  disconnect(): void {
    this.removeListener?.();
    this.removeListener = null;
    this.transport.disconnect();
  }

  get connected(): boolean {
    return this.transport.connected;
  }

  /**
   * Sends a 'process_intent' request and resolves with the resulting AetherisSession
   * once the server emits 'session_created'.
   */
  async processIntent(request: ProcessIntentRequest): Promise<AetherisSession> {
    const msg: ClientMessage = {
      type: 'process_intent',
      intent_text: request.intent_text,
      locale: request.locale ?? 'en',
      granted_scopes: request.granted_scopes ?? [],
      operator_id: request.operator_id,
      tenant_id: request.tenant_id,
    };

    return new Promise<AetherisSession>((resolve, reject) => {
      const unsub = this.transport.onMessage((incoming: ServerMessage) => {
        if (incoming.type === 'session_created') {
          unsub();
          resolve(this.getOrCreate(incoming.session));
        } else if (incoming.type === 'error') {
          unsub();
          reject(new Error(`[${incoming.code}] ${incoming.message}`));
        }
      });
      this.transport.send(msg);
    });
  }

  fillSlot(sessionId: string, slotName: string, valueJson: string): void {
    this.transport.send({
      type: 'fill_slot',
      session_id: sessionId,
      slot_name: slotName,
      value_json: valueJson,
    });
  }

  preview(sessionId: string): void {
    this.transport.send({ type: 'preview', session_id: sessionId });
  }

  confirm(sessionId: string): void {
    this.transport.send({ type: 'confirm', session_id: sessionId });
  }

  rejectPreview(sessionId: string): void {
    this.transport.send({ type: 'reject_preview', session_id: sessionId });
  }

  cancel(sessionId: string, note?: string): void {
    this.transport.send({ type: 'cancel', session_id: sessionId, note });
  }

  getSession(sessionId: string): AetherisSession | undefined {
    return this.sessions.get(sessionId);
  }

  private handleMessage(msg: ServerMessage): void {
    if (msg.type === 'session_created') {
      const session = this.getOrCreate(msg.session);
      this.dispatchEvent(new CustomEvent('session_created', { detail: { session } }));
    } else if (msg.type === 'session_updated') {
      const session = this.getOrCreate(msg.session);
      session._update(msg.session);
      this.dispatchEvent(new CustomEvent('session_updated', { detail: { session } }));
    } else if (msg.type === 'error') {
      this.errorHandler?.(msg.code, msg.message);
      this.dispatchEvent(new CustomEvent('error', { detail: msg }));
    }
  }

  private getOrCreate(raw: Session): AetherisSession {
    let session = this.sessions.get(raw.id);
    if (!session) {
      session = new AetherisSession(raw);
      this.sessions.set(raw.id, session);
    }
    return session;
  }
}
