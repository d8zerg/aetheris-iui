import type { ConfirmationMode } from '@aetheris-iui/core';

/**
 * <aui-confirm-button> - implements all five confirmation modes.
 *
 * Attributes:
 *   mode          - 'automatic' | 'single' | 'typed' | 'multi_party' | 'cooling_off'
 *   typed-phrase  - phrase the user must type for 'typed' mode (default: "CONFIRM")
 *   cooling-seconds - countdown duration for 'cooling_off' mode (default: 30)
 *   pending       - boolean; when set in 'multi_party' mode, shows waiting state
 *   disabled      - boolean; prevents all interaction
 *
 * Events:
 *   aui:confirm  - bubbles + composed; fired when the user completes confirmation
 *   aui:send-for-approval - fired in 'multi_party' mode when the approval request is sent
 */
export class AuiConfirmButton extends HTMLElement {
  private _shadow!: ShadowRoot;
  private _mode: ConfirmationMode = 'single';
  private _typedPhrase = 'CONFIRM';
  private _coolingSecs = 30;
  private _remaining = 0;
  private _timer: ReturnType<typeof setInterval> | null = null;
  private _pending = false;

  static get observedAttributes(): string[] {
    return ['mode', 'typed-phrase', 'cooling-seconds', 'pending', 'disabled'];
  }

  connectedCallback(): void {
    this._shadow = this.attachShadow({ mode: 'open' });
    this._render();
    if (this._mode === 'cooling_off') {
      this._startCooldown();
    } else if (this._mode === 'automatic') {
      queueMicrotask(() => this._fireConfirm());
    }
  }

  disconnectedCallback(): void {
    this._stopCooldown();
  }

  attributeChangedCallback(name: string, _old: string | null, value: string | null): void {
    switch (name) {
      case 'mode': this._mode = (value as ConfirmationMode) ?? 'single'; break;
      case 'typed-phrase': this._typedPhrase = value ?? 'CONFIRM'; break;
      case 'cooling-seconds': this._coolingSecs = parseInt(value ?? '30', 10); break;
      case 'pending': this._pending = value !== null; break;
    }
    if (this._shadow) this._render();
  }

  private _render(): void {
    const disabled = this.hasAttribute('disabled');
    this._shadow.innerHTML = `
      <style>
        :host { display: inline-block; font-family: inherit; }
        button {
          padding: 8px 20px; border: 1px solid #0057b7; border-radius: 4px;
          background: #0057b7; color: #fff; font-size: 0.9rem;
          cursor: pointer; transition: background 0.15s;
        }
        button:disabled { opacity: 0.45; cursor: not-allowed; }
        button:hover:not(:disabled) { background: #003d82; }
        input {
          display: block; width: 100%; box-sizing: border-box;
          padding: 6px 8px; margin-bottom: 6px;
          border: 1px solid #bbb; border-radius: 4px; font-size: 0.9rem;
        }
        .hint { font-size: 0.75rem; color: #666; margin-bottom: 6px; }
        .pending { color: #888; font-style: italic; font-size: 0.9rem; }
        .countdown { font-size: 0.75rem; color: #888; margin-top: 4px; }
      </style>
      ${this._renderContent(disabled)}
    `;
    this._attachListeners();
  }

  private _renderContent(disabled: boolean): string {
    switch (this._mode) {
      case 'automatic':
        return '<span role="status">Auto-confirming…</span>';

      case 'single':
        return `<button type="button"${disabled ? ' disabled' : ''}>Confirm</button>`;

      case 'typed':
        return `
          <div class="hint">Type <strong>${this._typedPhrase}</strong> to confirm</div>
          <input type="text" aria-label="Confirmation phrase" autocomplete="off"${disabled ? ' disabled' : ''} />
          <button type="button" disabled>Confirm</button>
        `;

      case 'multi_party':
        return this._pending
          ? '<span class="pending" role="status">Awaiting co-approval…</span>'
          : `<button type="button"${disabled ? ' disabled' : ''}>Send for approval</button>`;

      case 'cooling_off':
        return this._remaining > 0
          ? `
            <button type="button" disabled>Confirm (${this._remaining}s)</button>
            <div class="countdown">Cooling-off period active</div>
          `
          : `<button type="button"${disabled ? ' disabled' : ''}>Confirm</button>`;
    }
  }

  private _attachListeners(): void {
    const btn = this._shadow.querySelector('button');
    if (!btn) return;

    if (this._mode === 'single' || (this._mode === 'cooling_off' && this._remaining === 0)) {
      btn.addEventListener('click', () => this._fireConfirm());
    } else if (this._mode === 'multi_party' && !this._pending) {
      btn.addEventListener('click', () => {
        this._pending = true;
        this._render();
        this.dispatchEvent(
          new CustomEvent('aui:send-for-approval', { bubbles: true, composed: true }),
        );
      });
    } else if (this._mode === 'typed') {
      const input = this._shadow.querySelector('input');
      input?.addEventListener('input', () => {
        if (btn) btn.disabled = input.value !== this._typedPhrase;
      });
      btn.addEventListener('click', () => this._fireConfirm());
    }
  }

  private _fireConfirm(): void {
    this.dispatchEvent(new CustomEvent('aui:confirm', { bubbles: true, composed: true }));
  }

  private _startCooldown(): void {
    this._remaining = this._coolingSecs;
    this._render();
    this._timer = setInterval(() => {
      this._remaining = Math.max(0, this._remaining - 1);
      this._render();
      if (this._remaining === 0) this._stopCooldown();
    }, 1000);
  }

  private _stopCooldown(): void {
    if (this._timer !== null) {
      clearInterval(this._timer);
      this._timer = null;
    }
  }
}

customElements.define('aui-confirm-button', AuiConfirmButton);
