import type { Session } from '@aetheris-iui/core';

/**
 * <aui-session-preview> - read-only view of a session in the 'preview' state.
 *
 * Attributes:
 *   session-json - JSON-serialized Session object
 *
 * Events:
 *   aui:confirm        - user confirmed the preview
 *   aui:reject-preview - user rejected the preview (wants to change parameters)
 */
export class AuiSessionPreview extends HTMLElement {
  private _shadow!: ShadowRoot;
  private _session: Session | null = null;

  static get observedAttributes(): string[] {
    return ['session-json'];
  }

  connectedCallback(): void {
    this._shadow = this.attachShadow({ mode: 'open' });
    this._tryParse();
    this._render();
  }

  attributeChangedCallback(name: string, _old: string | null, _value: string | null): void {
    if (name === 'session-json') this._tryParse();
    if (this._shadow) this._render();
  }

  private _tryParse(): void {
    const raw = this.getAttribute('session-json');
    if (!raw) { this._session = null; return; }
    try { this._session = JSON.parse(raw) as Session; } catch { this._session = null; }
  }

  private _render(): void {
    if (!this._session) {
      this._shadow.innerHTML = '<p>No session data.</p>';
      return;
    }
    const s = this._session;
    const filledSlots = s.slots
      .filter((sl) => sl.value_json !== null)
      .map((sl) => {
        let display = sl.value_json ?? '';
        try { display = JSON.parse(sl.value_json!) as string; } catch { /* keep raw */ }
        return `<tr><td>${sl.name}</td><td>${display}</td></tr>`;
      })
      .join('');

    this._shadow.innerHTML = `
      <style>
        :host { display: block; padding: 16px; border: 1px solid #ddd; border-radius: 6px; }
        h3 { margin: 0 0 12px; font-size: 1rem; }
        table { width: 100%; border-collapse: collapse; margin-bottom: 16px; }
        th { text-align: left; font-size: 0.8rem; color: #666; padding: 4px; border-bottom: 1px solid #eee; }
        td { padding: 4px; font-size: 0.9rem; }
        .preview-json { background: #f8f8f8; padding: 8px; border-radius: 4px; font-size: 0.8rem; margin-bottom: 16px; }
        .actions { display: flex; gap: 8px; }
        button { padding: 7px 16px; border-radius: 4px; font-size: 0.9rem; cursor: pointer; }
        .btn-confirm { background: #0057b7; color: #fff; border: 1px solid #0057b7; }
        .btn-reject { background: #fff; color: #333; border: 1px solid #bbb; }
        .btn-confirm:hover { background: #003d82; }
        .btn-reject:hover { background: #f0f0f0; }
      </style>
      <h3>Preview - ${s.action_id}</h3>
      ${filledSlots ? `
        <table>
          <thead><tr><th>Parameter</th><th>Value</th></tr></thead>
          <tbody>${filledSlots}</tbody>
        </table>
      ` : ''}
      ${s.preview_result_json ? `
        <div class="preview-json">
          <strong>Dry-run result:</strong>
          <pre>${s.preview_result_json}</pre>
        </div>
      ` : ''}
      <div class="actions">
        <button class="btn-confirm" data-action="confirm">Confirm action</button>
        <button class="btn-reject" data-action="reject">Change parameters</button>
      </div>
    `;

    this._shadow.querySelector('[data-action="confirm"]')?.addEventListener('click', () => {
      this.dispatchEvent(new CustomEvent('aui:confirm', { bubbles: true, composed: true }));
    });
    this._shadow.querySelector('[data-action="reject"]')?.addEventListener('click', () => {
      this.dispatchEvent(new CustomEvent('aui:reject-preview', { bubbles: true, composed: true }));
    });
  }
}

customElements.define('aui-session-preview', AuiSessionPreview);
