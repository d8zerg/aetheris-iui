/** Reactive wrapper around a raw Session snapshot. Emits 'change' events on updates. */
export class AetherisSession extends EventTarget {
    _raw;
    constructor(session) {
        super();
        this._raw = session;
    }
    get id() { return this._raw.id; }
    get action_id() { return this._raw.action_id; }
    get operator_id() { return this._raw.operator_id; }
    get tenant_id() { return this._raw.tenant_id; }
    get state() { return this._raw.state; }
    get confirmation_mode() { return this._raw.confirmation_mode; }
    get slots() { return this._raw.slots; }
    get clarification_question() { return this._raw.clarification_question; }
    get preview_result_json() { return this._raw.preview_result_json; }
    get archive_reason() { return this._raw.archive_reason; }
    get created_at_us() { return this._raw.created_at_us; }
    get updated_at_us() { return this._raw.updated_at_us; }
    get unfilled_required() {
        return this._raw.slots.filter((s) => s.required && s.value_json === null);
    }
    get all_required_filled() {
        return this.unfilled_required.length === 0;
    }
    get is_archived() {
        return this._raw.state === 'archive';
    }
    get raw() {
        return this._raw;
    }
    /**
     * Called by AetherisClient when the server sends a session_updated message.
     * @internal
     */
    _update(session) {
        this._raw = session;
        this.dispatchEvent(new CustomEvent('change', {
            detail: { session: this._raw },
            bubbles: false,
        }));
    }
}
//# sourceMappingURL=session.js.map