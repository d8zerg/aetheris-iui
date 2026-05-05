import { useState, useEffect, useCallback } from 'react';
import type { AetherisSession } from '@aetheris-iui/core';
import { useAetherisClient } from './AetherisContext.js';

export interface UseAetherisSessionResult {
  session: AetherisSession | null;
  fillSlot: (slotName: string, valueJson: string) => void;
  preview: () => void;
  confirm: () => void;
  cancel: (note?: string) => void;
}

export function useAetherisSession(sessionId: string | null): UseAetherisSessionResult {
  const client = useAetherisClient();
  const [session, setSession] = useState<AetherisSession | null>(
    sessionId ? (client.getSession(sessionId) ?? null) : null,
  );

  useEffect(() => {
    if (!sessionId) { setSession(null); return; }
    const s = client.getSession(sessionId) ?? null;
    setSession(s);
    if (!s) return;

    const onChange = () => setSession((prev) => (prev ? Object.assign(Object.create(Object.getPrototypeOf(prev)), prev) : prev));
    s.addEventListener('change', onChange);
    return () => s.removeEventListener('change', onChange);
  }, [client, sessionId]);

  const fillSlot = useCallback(
    (slotName: string, valueJson: string) => {
      if (sessionId) client.fillSlot(sessionId, slotName, valueJson);
    },
    [client, sessionId],
  );

  const preview = useCallback(() => {
    if (sessionId) client.preview(sessionId);
  }, [client, sessionId]);

  const confirm = useCallback(() => {
    if (sessionId) client.confirm(sessionId);
  }, [client, sessionId]);

  const cancel = useCallback(
    (note?: string) => {
      if (sessionId) client.cancel(sessionId, note);
    },
    [client, sessionId],
  );

  return { session, fillSlot, preview, confirm, cancel };
}
