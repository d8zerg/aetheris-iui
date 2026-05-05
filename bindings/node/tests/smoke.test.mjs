/**
 * Node.js binding smoke tests.
 * Usage: node smoke.test.mjs <path-to-aetheris_stdio_server>
 */

import { AetherisClient } from '../index.mjs';

const SAMPLE_SESSION = {
  id: 'sess-001',
  action_id: 'camera.start_recording',
  operator_id: 'op-alice',
  tenant_id: 'tenant-acme',
  state: 'fill',
  confirmation_mode: 'single',
  slots: [
    { name: 'camera_id', required: true, value_json: null },
    { name: 'resolution', required: false, value_json: '"1080p"' },
  ],
  clarification_question: null,
  preview_result_json: null,
  archive_reason: null,
  created_at_us: 1_000_000_000,
  updated_at_us: 1_000_000_000,
};

function assert(condition, message) {
  if (!condition) throw new Error(`Assertion failed: ${message}`);
}

const tests = [
  async function test_ping(client) {
    const resp = await client.ping();
    assert(resp.type === 'pong', `expected pong, got ${resp.type}`);
    assert(typeof resp.version === 'string', 'version must be a string');
    console.log(`  ok  ping -> version=${resp.version}`);
  },

  async function test_version(client) {
    const resp = await client.version();
    assert(resp.type === 'version', `expected version, got ${resp.type}`);
    assert(typeof resp.abi === 'number' && resp.abi > 0, 'abi must be positive number');
    console.log(`  ok  version=${resp.version} abi=${resp.abi}`);
  },

  async function test_session_snapshot(client) {
    const resp = await client.sessionSnapshot(SAMPLE_SESSION);
    assert(resp.type === 'snapshot', `expected snapshot, got ${resp.type}`);
    const snap = resp.snapshot;
    assert(snap.id === 'sess-001', 'id mismatch');
    assert(snap.action_id === 'camera.start_recording', 'action_id mismatch');
    assert(snap.state === 'fill', 'state mismatch');
    assert(snap.confirmation_mode === 'single', 'confirmation_mode mismatch');
    assert(Array.isArray(snap.slots) && snap.slots.length === 2, 'slots mismatch');
    assert(snap.created_at_us === 1_000_000_000, 'created_at_us mismatch');
    console.log(`  ok  snapshot id=${snap.id} slots=${snap.slots.length}`);
  },

  async function test_multiple_requests(client) {
    const pong = await client.ping();
    const ver = await client.version();
    const snap = await client.sessionSnapshot(SAMPLE_SESSION);
    assert(pong.type === 'pong', 'ping failed');
    assert(ver.type === 'version', 'version failed');
    assert(snap.type === 'snapshot', 'snapshot failed');
    console.log('  ok  multiple sequential requests');
  },
];

async function main() {
  const serverPath = process.argv[2];
  if (!serverPath) {
    console.error('Usage: node smoke.test.mjs <path-to-aetheris_stdio_server>');
    process.exit(1);
  }

  let failed = 0;
  for (const testFn of tests) {
    const client = new AetherisClient(serverPath);
    await client.connect();
    try {
      await testFn(client);
    } catch (err) {
      console.log(` FAIL ${testFn.name}: ${err.message}`);
      failed++;
    } finally {
      try { await client.close(); } catch { /* ignore */ }
    }
  }

  const total = tests.length;
  console.log(`\n${total - failed}/${total} smoke tests passed`);
  process.exit(failed > 0 ? 1 : 0);
}

main();
