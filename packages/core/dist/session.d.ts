import type { ArchiveReason, ConfirmationMode, Session, SessionState, Slot } from './types.js';
export interface SessionChangeDetail {
    readonly session: Readonly<Session>;
}
/** Reactive wrapper around a raw Session snapshot. Emits 'change' events on updates. */
export declare class AetherisSession extends EventTarget {
    private _raw;
    constructor(session: Session);
    get id(): string;
    get action_id(): string;
    get operator_id(): string;
    get tenant_id(): string;
    get state(): SessionState;
    get confirmation_mode(): ConfirmationMode;
    get slots(): readonly Slot[];
    get clarification_question(): string;
    get preview_result_json(): string;
    get archive_reason(): ArchiveReason | null;
    get created_at_us(): number;
    get updated_at_us(): number;
    get unfilled_required(): readonly Slot[];
    get all_required_filled(): boolean;
    get is_archived(): boolean;
    get raw(): Readonly<Session>;
    /**
     * Called by AetherisClient when the server sends a session_updated message.
     * @internal
     */
    _update(session: Session): void;
}
//# sourceMappingURL=session.d.ts.map