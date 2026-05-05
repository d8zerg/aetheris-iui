export type SessionState = 'fill' | 'clarification' | 'preview' | 'commit' | 'archive';
export type ConfirmationMode = 'automatic' | 'single' | 'typed' | 'multi_party' | 'cooling_off';
export type ArchiveReason = 'completed' | 'cancelled' | 'expired';
export interface Slot {
    readonly name: string;
    readonly required: boolean;
    /** Compact JSON-encoded value, or null when not yet filled. */
    readonly value_json: string | null;
}
/** Full session snapshot as sent by the C++ server over the wire. */
export interface Session {
    readonly id: string;
    readonly action_id: string;
    readonly operator_id: string;
    readonly tenant_id: string;
    readonly state: SessionState;
    readonly confirmation_mode: ConfirmationMode;
    readonly slots: readonly Slot[];
    readonly clarification_question: string;
    readonly preview_result_json: string;
    readonly archive_reason: ArchiveReason | null;
    /** Unix microseconds */
    readonly created_at_us: number;
    /** Unix microseconds */
    readonly updated_at_us: number;
}
export type ServerMessage = {
    readonly type: 'session_created';
    readonly session: Session;
} | {
    readonly type: 'session_updated';
    readonly session: Session;
} | {
    readonly type: 'error';
    readonly code: string;
    readonly message: string;
    readonly details?: Readonly<Record<string, string>>;
};
export type ClientMessage = {
    readonly type: 'process_intent';
    readonly intent_text: string;
    readonly locale: string;
    readonly granted_scopes: readonly string[];
    readonly operator_id: string;
    readonly tenant_id: string;
} | {
    readonly type: 'fill_slot';
    readonly session_id: string;
    readonly slot_name: string;
    readonly value_json: string;
} | {
    readonly type: 'preview';
    readonly session_id: string;
} | {
    readonly type: 'confirm';
    readonly session_id: string;
} | {
    readonly type: 'reject_preview';
    readonly session_id: string;
} | {
    readonly type: 'cancel';
    readonly session_id: string;
    readonly note?: string;
};
export interface ProcessIntentRequest {
    readonly intent_text: string;
    readonly locale?: string;
    readonly granted_scopes?: readonly string[];
    readonly operator_id: string;
    readonly tenant_id: string;
}
export interface FillSlotRequest {
    readonly session_id: string;
    readonly slot_name: string;
    readonly value_json: string;
}
//# sourceMappingURL=types.d.ts.map