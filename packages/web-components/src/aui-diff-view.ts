import { computeDiff, changedEntries, type DiffEntry } from '@aetheris-iui/core';

type DiffViewMode = 'table' | 'tree' | 'json';

/**
 * <aui-diff-view> - renders a diff between two JSON values.
 *
 * Attributes:
 *   before-json - JSON string of the "before" state (default: null)
 *   after-json  - JSON string of the "after" state (default: null)
 *   mode        - 'table' | 'tree' | 'json'  (default: 'table')
 *   show-unchanged - boolean; include unchanged entries in output
 */
export class AuiDiffView extends HTMLElement {
  private _shadow!: ShadowRoot;
  private _mode: DiffViewMode = 'table';
  private _showUnchanged = false;

  static get observedAttributes(): string[] {
    return ['before-json', 'after-json', 'mode', 'show-unchanged'];
  }

  connectedCallback(): void {
    this._shadow = this.attachShadow({ mode: 'open' });
    this._render();
  }

  attributeChangedCallback(name: string, _old: string | null, value: string | null): void {
    if (name === 'mode') this._mode = (value as DiffViewMode) ?? 'table';
    if (name === 'show-unchanged') this._showUnchanged = value !== null;
    if (this._shadow) this._render();
  }

  private _parse(attr: string): unknown {
    const raw = this.getAttribute(attr);
    if (!raw) return null;
    try { return JSON.parse(raw); } catch { return null; }
  }

  private _render(): void {
    const before = this._parse('before-json');
    const after = this._parse('after-json');
    const diff = computeDiff(before, after);
    const entries = this._showUnchanged ? diff : changedEntries(diff);

    this._shadow.innerHTML = `
      <style>
        :host { display: block; font-family: monospace; font-size: 0.85rem; }
        table { width: 100%; border-collapse: collapse; }
        th { text-align: left; padding: 4px 8px; background: #f0f0f0; border-bottom: 1px solid #ccc; }
        td { padding: 4px 8px; border-bottom: 1px solid #eee; }
        .changed-row { background: #fff8e1; }
        .removed { color: #c00; text-decoration: line-through; }
        .added { color: #2a7d2a; }
        .path { color: #555; }
        pre { background: #f8f8f8; padding: 8px; border-radius: 4px; overflow: auto; }
        .tree-node { margin-left: 16px; }
        .tree-changed { color: #c00; }
        .tree-label { font-weight: bold; }
      </style>
      ${this._renderContent(entries)}
    `;
  }

  private _renderContent(entries: readonly DiffEntry[]): string {
    if (entries.length === 0) {
      return '<em>No changes</em>';
    }
    switch (this._mode) {
      case 'table': return this._renderTable(entries);
      case 'tree': return this._renderTree(entries);
      case 'json': return this._renderJson(entries);
    }
  }

  private _renderTable(entries: readonly DiffEntry[]): string {
    const rows = entries
      .map(
        (e) =>
          `<tr class="${e.changed ? 'changed-row' : ''}">
            <td class="path">${e.path}</td>
            <td class="removed">${e.changed ? e.before : e.before}</td>
            <td class="added">${e.after}</td>
          </tr>`,
      )
      .join('');
    return `
      <table>
        <thead><tr><th>Path</th><th>Before</th><th>After</th></tr></thead>
        <tbody>${rows}</tbody>
      </table>
    `;
  }

  private _renderTree(entries: readonly DiffEntry[]): string {
    const lines = entries
      .map(
        (e) =>
          `<div class="tree-node ${e.changed ? 'tree-changed' : ''}">
            <span class="tree-label">${e.path}:</span>
            ${e.changed ? `<span class="removed">${e.before}</span> -> <span class="added">${e.after}</span>`
                        : `<span>${e.after}</span>`}
          </div>`,
      )
      .join('');
    return `<div class="tree">${lines}</div>`;
  }

  private _renderJson(entries: readonly DiffEntry[]): string {
    return `<pre>${JSON.stringify(entries, null, 2)}</pre>`;
  }
}

customElements.define('aui-diff-view', AuiDiffView);
