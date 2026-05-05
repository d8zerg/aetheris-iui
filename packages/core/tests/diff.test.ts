import { describe, expect, it } from 'vitest';
import * as fc from 'fast-check';
import { computeDiff, changedEntries, renderDiffTable, diffFromPreviewJson } from '../src/diff.js';

describe('computeDiff', () => {
  it('returns unchanged entry for identical primitives', () => {
    const d = computeDiff('hello', 'hello');
    expect(d).toHaveLength(1);
    expect(d[0].changed).toBe(false);
  });

  it('returns changed entry for differing primitives', () => {
    const d = computeDiff(1, 2);
    expect(d).toHaveLength(1);
    expect(d[0].changed).toBe(true);
    expect(d[0].before).toBe('1');
    expect(d[0].after).toBe('2');
  });

  it('returns empty changes for identical objects', () => {
    const obj = { a: 1, b: 'hello' };
    const d = computeDiff(obj, obj);
    expect(d.every((e) => !e.changed)).toBe(true);
  });

  it('detects nested object change', () => {
    const d = computeDiff({ user: { name: 'Alice' } }, { user: { name: 'Bob' } });
    expect(d.some((e) => e.changed && e.path === 'user.name')).toBe(true);
  });

  it('detects added key', () => {
    const d = computeDiff({ a: 1 }, { a: 1, b: 2 });
    const changed = changedEntries(d);
    expect(changed.some((e) => e.path === 'b')).toBe(true);
  });

  it('detects removed key', () => {
    const d = computeDiff({ a: 1, b: 2 }, { a: 1 });
    const changed = changedEntries(d);
    expect(changed.some((e) => e.path === 'b')).toBe(true);
  });

  it('uses dot-notation paths', () => {
    const d = computeDiff({ x: { y: { z: 1 } } }, { x: { y: { z: 2 } } });
    expect(d.some((e) => e.path === 'x.y.z')).toBe(true);
  });
});

describe('changedEntries', () => {
  it('filters to only changed entries', () => {
    const d = computeDiff({ a: 1, b: 2 }, { a: 1, b: 99 });
    const changed = changedEntries(d);
    expect(changed.every((e) => e.changed)).toBe(true);
    expect(changed.some((e) => e.path === 'b')).toBe(true);
    expect(changed.some((e) => e.path === 'a')).toBe(false);
  });
});

describe('renderDiffTable', () => {
  it('produces a markdown table with header', () => {
    const d = computeDiff({ a: 1 }, { a: 2 });
    const table = renderDiffTable(d);
    expect(table).toContain('| Path |');
    expect(table).toContain('| Before |');
    expect(table).toContain('| After |');
    expect(table).toContain('a');
  });
});

describe('diffFromPreviewJson', () => {
  it('parses before/after structure', () => {
    const json = JSON.stringify({ before: { status: 'active' }, after: { status: 'disabled' } });
    const d = diffFromPreviewJson(json);
    expect(d.some((e) => e.changed && e.path === 'status')).toBe(true);
  });

  it('returns empty array for invalid JSON', () => {
    expect(diffFromPreviewJson('not-json')).toEqual([]);
  });
});

// ---- Property-based tests ----

describe('computeDiff properties', () => {
  it('identical objects always produce zero changed entries', () => {
    fc.assert(
      fc.property(fc.object({ maxDepth: 2 }), (obj) => {
        const d = computeDiff(obj, obj);
        return d.every((e) => !e.changed);
      }),
    );
  });

  it('changed entries are always a subset of all entries', () => {
    fc.assert(
      fc.property(fc.object({ maxDepth: 2 }), fc.object({ maxDepth: 2 }), (a, b) => {
        const all = computeDiff(a, b);
        const changed = changedEntries(all);
        return changed.length <= all.length;
      }),
    );
  });

  it('diff is symmetric in the "changed" flag', () => {
    fc.assert(
      fc.property(fc.object({ maxDepth: 2 }), fc.object({ maxDepth: 2 }), (a, b) => {
        const ab = computeDiff(a, b);
        const ba = computeDiff(b, a);
        return changedEntries(ab).length === changedEntries(ba).length;
      }),
    );
  });
});
