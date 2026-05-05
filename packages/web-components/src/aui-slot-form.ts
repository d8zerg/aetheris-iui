import type { Slot } from '@aetheris-iui/core';

/**
 * <aui-slot-form> - renders a form for filling intent session slots.
 *
 * Attributes:
 *   slots-json - JSON array of Slot objects (see @aetheris-iui/core Slot type)
 *   disabled   - boolean; disables all inputs
 *
 * Events:
 *   aui:slot-change - detail: { slot_name: string; value_json: string }
 *   aui:submit      - fired when all required slots are filled and submit is clicked
 */
export class AuiSlotForm extends HTMLElement {
  private _shadow!: ShadowRoot;
  private _slots: Slot[] = [];

  static get observedAttributes(): string[] {
    return ['slots-json', 'disabled'];
  }

  connectedCallback(): void {
    this._shadow = this.attachShadow({ mode: 'open' });
    this._tryParseSlots();
    this._render();
  }

  attributeChangedCallback(name: string, _old: string | null, _value: string | null): void {
    if (name === 'slots-json') this._tryParseSlots();
    if (this._shadow) this._render();
  }

  private _tryParseSlots(): void {
    const raw = this.getAttribute('slots-json');
    if (!raw) { this._slots = []; return; }
    try {
      this._slots = JSON.parse(raw) as Slot[];
    } catch {
      this._slots = [];
    }
  }

  private _render(): void {
    const disabled = this.hasAttribute('disabled');
    const allFilled = this._slots.every((s) => !s.required || s.value_json !== null);

    this._shadow.innerHTML = `
      <style>
        :host { display: block; font-family: inherit; }
        .slot { margin-bottom: 12px; }
        label { display: block; font-size: 0.85rem; color: #444; margin-bottom: 3px; }
        label .required { color: #c00; }
        input, select {
          width: 100%; box-sizing: border-box;
          padding: 6px 8px; border: 1px solid #bbb; border-radius: 4px;
          font-size: 0.9rem;
        }
        input:disabled, select:disabled { background: #f5f5f5; }
        input.filled { border-color: #2a9d2a; }
        .submit-row { margin-top: 16px; }
        button {
          padding: 8px 20px; border: 1px solid #0057b7; border-radius: 4px;
          background: #0057b7; color: #fff; font-size: 0.9rem;
          cursor: pointer;
        }
        button:disabled { opacity: 0.45; cursor: not-allowed; }
      </style>
      <form id="slot-form">
        ${this._slots.map((s) => this._renderSlot(s, disabled)).join('')}
        <div class="submit-row">
          <button type="submit"${!allFilled || disabled ? ' disabled' : ''}>
            Continue
          </button>
        </div>
      </form>
    `;
    this._shadow.querySelector('form')?.addEventListener('submit', (e) => {
      e.preventDefault();
      this.dispatchEvent(new CustomEvent('aui:submit', { bubbles: true, composed: true }));
    });
    this._shadow.querySelectorAll<HTMLInputElement>('input[data-slot]').forEach((input) => {
      input.addEventListener('change', () => {
        const slotName = input.dataset['slot'] ?? '';
        const valueJson = JSON.stringify(input.value);
        this.dispatchEvent(
          new CustomEvent('aui:slot-change', {
            bubbles: true,
            composed: true,
            detail: { slot_name: slotName, value_json: valueJson },
          }),
        );
      });
    });
  }

  private _renderSlot(slot: Slot, disabled: boolean): string {
    const label = slot.name.replace(/_/g, ' ');
    const filled = slot.value_json !== null;
    let currentValue = '';
    if (slot.value_json !== null) {
      try { currentValue = JSON.parse(slot.value_json) as string; } catch { currentValue = ''; }
    }
    return `
      <div class="slot">
        <label>
          ${label}${slot.required ? ' <span class="required">*</span>' : ''}
        </label>
        <input
          type="text"
          data-slot="${slot.name}"
          value="${currentValue}"
          class="${filled ? 'filled' : ''}"
          ${disabled ? 'disabled' : ''}
          aria-required="${slot.required}"
        />
      </div>
    `;
  }
}

customElements.define('aui-slot-form', AuiSlotForm);
