import type { ArchiveReason, ConfirmationMode, Session, SessionState, Slot } from './types.js';

export interface SessionChangeDetail {
  readonly session: Readonly<Session>;
}

/** Reactive wrapper around a raw Session snapshot. Emits 'change' events on updates. */
export class AetherisSession extends EventTarget {
  private _raw: Session;

  constructor(session: Session) {
    super();
    this._raw = session;
  }

  get id(): string { return this._raw.id; }
  get action_id(): string { return this._raw.action_id; }
  get operator_id(): string { return this._raw.operator_id; }
  get tenant_id(): string { return this._raw.tenant_id; }
  get state(): SessionState { return this._raw.state; }
  get confirmation_mode(): ConfirmationMode { return this._raw.confirmation_mode; }
  get slots(): readonly Slot[] { return this._raw.slots; }
  get clarification_question(): string { return this._raw.clarification_question; }
  get preview_result_json(): string { return this._raw.preview_result_json; }
  get archive_reason(): ArchiveReason | null { return this._raw.archive_reason; }
  get created_at_us(): number { return this._raw.created_at_us; }
  get updated_at_us(): number { return this._raw.updated_at_us; }

  get unfilled_required(): readonly Slot[] {
    return this._raw.slots.filter((s) => s.required && s.value_json === null);
  }

  get all_required_filled(): boolean {
    return this.unfilled_required.length === 0;
  }

  get is_archived(): boolean {
    return this._raw.state === 'archive';
  }

  get raw(): Readonly<Session> {
    return this._raw;
  }

  /**
   * Called by AetherisClient when the server sends a session_updated message.
   * @internal
   */
  _update(session: Session): void {
    this._raw = session;
    this.dispatchEvent(
      new CustomEvent<SessionChangeDetail>('change', {
        detail: { session: this._raw },
        bubbles: false,
      }),
    );
  }
}
