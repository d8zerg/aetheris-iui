import { describe, expect, it } from 'vitest';
import { AetherisSession } from '../src/session.js';
import type { Session } from '../src/types.js';

function makeSession(overrides: Partial<Session> = {}): Session {
  return {
    id: 'sess-1',
    action_id: 'camera.disable',
    operator_id: 'op-1',
    tenant_id: 'tenant-1',
    state: 'fill',
    confirmation_mode: 'single',
    slots: [
      { name: 'cameraId', required: true, value_json: null },
      { name: 'reason', required: false, value_json: null },
    ],
    clarification_question: '',
    preview_result_json: '',
    archive_reason: null,
    created_at_us: 1_000_000,
    updated_at_us: 1_000_000,
    ...overrides,
  };
}

describe('AetherisSession', () => {
  it('exposes session properties', () => {
    const s = new AetherisSession(makeSession());
    expect(s.id).toBe('sess-1');
    expect(s.action_id).toBe('camera.disable');
    expect(s.state).toBe('fill');
    expect(s.confirmation_mode).toBe('single');
    expect(s.slots).toHaveLength(2);
  });

  it('unfilled_required returns only required unfilled slots', () => {
    const s = new AetherisSession(makeSession());
    expect(s.unfilled_required).toHaveLength(1);
    expect(s.unfilled_required[0].name).toBe('cameraId');
  });

  it('all_required_filled is false when required slots are empty', () => {
    const s = new AetherisSession(makeSession());
    expect(s.all_required_filled).toBe(false);
  });

  it('all_required_filled is true when all required slots have values', () => {
    const s = new AetherisSession(
      makeSession({
        slots: [
          { name: 'cameraId', required: true, value_json: '"cam-1"' },
          { name: 'reason', required: false, value_json: null },
        ],
      }),
    );
    expect(s.all_required_filled).toBe(true);
  });

  it('is_archived is false for non-archive states', () => {
    for (const state of ['fill', 'clarification', 'preview', 'commit'] as const) {
      const s = new AetherisSession(makeSession({ state }));
      expect(s.is_archived).toBe(false);
    }
  });

  it('is_archived is true for archive state', () => {
    const s = new AetherisSession(makeSession({ state: 'archive' }));
    expect(s.is_archived).toBe(true);
  });

  it('_update changes state and fires change event', () => {
    const s = new AetherisSession(makeSession());
    let changed = false;
    s.addEventListener('change', () => { changed = true; });

    s._update(makeSession({ state: 'preview' }));

    expect(s.state).toBe('preview');
    expect(changed).toBe(true);
  });

  it('raw returns the underlying snapshot', () => {
    const raw = makeSession();
    const s = new AetherisSession(raw);
    expect(s.raw).toEqual(raw);
  });
});
