import { describe, expect, it, beforeEach } from 'vitest';
import { AetherisClient } from '../src/client.js';
import { MockTransport } from '../src/transport.js';
import type { Session } from '../src/types.js';

function makeSession(id = 'sess-1'): Session {
  return {
    id,
    action_id: 'camera.disable',
    operator_id: 'op-1',
    tenant_id: 'tenant-1',
    state: 'fill',
    confirmation_mode: 'single',
    slots: [{ name: 'cameraId', required: true, value_json: null }],
    clarification_question: '',
    preview_result_json: '',
    archive_reason: null,
    created_at_us: 0,
    updated_at_us: 0,
  };
}

describe('AetherisClient', () => {
  let transport: MockTransport;
  let client: AetherisClient;

  beforeEach(async () => {
    transport = new MockTransport();
    client = new AetherisClient(transport);
    await client.connect();
  });

  it('connect sets connected to true', () => {
    expect(client.connected).toBe(true);
  });

  it('disconnect sets connected to false', () => {
    client.disconnect();
    expect(client.connected).toBe(false);
  });

  it('processIntent sends process_intent message', async () => {
    const promise = client.processIntent({
      intent_text: 'disable camera 1',
      operator_id: 'op-1',
      tenant_id: 'tenant-1',
    });
    transport.simulateMessage({ type: 'session_created', session: makeSession() });
    const session = await promise;
    expect(session.id).toBe('sess-1');
    expect(transport.sent[0]).toMatchObject({
      type: 'process_intent',
      intent_text: 'disable camera 1',
      locale: 'en',
    });
  });

  it('processIntent rejects on server error', async () => {
    const promise = client.processIntent({
      intent_text: 'unknown intent',
      operator_id: 'op-1',
      tenant_id: 'tenant-1',
    });
    transport.simulateMessage({
      type: 'error',
      code: 'classifier.ambiguity.low_confidence',
      message: 'Confidence too low.',
    });
    await expect(promise).rejects.toThrow('classifier.ambiguity.low_confidence');
  });

  it('fillSlot sends fill_slot message', () => {
    client.fillSlot('sess-1', 'cameraId', '"cam-3"');
    expect(transport.sent[0]).toEqual({
      type: 'fill_slot',
      session_id: 'sess-1',
      slot_name: 'cameraId',
      value_json: '"cam-3"',
    });
  });

  it('preview sends preview message', () => {
    client.preview('sess-1');
    expect(transport.sent[0]).toEqual({ type: 'preview', session_id: 'sess-1' });
  });

  it('confirm sends confirm message', () => {
    client.confirm('sess-1');
    expect(transport.sent[0]).toEqual({ type: 'confirm', session_id: 'sess-1' });
  });

  it('cancel sends cancel message with note', () => {
    client.cancel('sess-1', 'operator cancelled');
    expect(transport.sent[0]).toEqual({
      type: 'cancel',
      session_id: 'sess-1',
      note: 'operator cancelled',
    });
  });

  it('session_updated message updates existing session', async () => {
    // Create the session
    const promise = client.processIntent({
      intent_text: 'disable camera',
      operator_id: 'op-1',
      tenant_id: 'tenant-1',
    });
    transport.simulateMessage({ type: 'session_created', session: makeSession() });
    const session = await promise;
    expect(session.state).toBe('fill');

    // Update it
    transport.simulateMessage({
      type: 'session_updated',
      session: { ...makeSession(), state: 'preview' },
    });
    expect(session.state).toBe('preview');
  });

  it('getSession returns created session by id', async () => {
    const promise = client.processIntent({
      intent_text: 'test',
      operator_id: 'op-1',
      tenant_id: 'tenant-1',
    });
    transport.simulateMessage({ type: 'session_created', session: makeSession('sess-99') });
    await promise;
    expect(client.getSession('sess-99')).toBeDefined();
    expect(client.getSession('nonexistent')).toBeUndefined();
  });

  it('error message calls onError handler', async () => {
    let errorCode = '';
    const c = new AetherisClient(transport, {
      onError: (code) => { errorCode = code; },
    });
    await c.connect();
    transport.simulateMessage({
      type: 'error',
      code: 'intent_engine.scopes.insufficient',
      message: 'Missing scope.',
    });
    expect(errorCode).toBe('intent_engine.scopes.insufficient');
  });
});
