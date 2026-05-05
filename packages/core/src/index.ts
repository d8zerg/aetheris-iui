export type {
  Session,
  SessionState,
  ConfirmationMode,
  ArchiveReason,
  Slot,
  ServerMessage,
  ClientMessage,
  ProcessIntentRequest,
  FillSlotRequest,
} from './types.js';

export { MockTransport, WebSocketTransport } from './transport.js';
export type { Transport } from './transport.js';

export { AetherisSession } from './session.js';
export type { SessionChangeDetail } from './session.js';

export { AetherisClient } from './client.js';
export type { ClientOptions, ErrorHandler } from './client.js';

export {
  computeDiff,
  changedEntries,
  renderDiffTable,
  diffFromPreviewJson,
} from './diff.js';
export type { DiffEntry } from './diff.js';
