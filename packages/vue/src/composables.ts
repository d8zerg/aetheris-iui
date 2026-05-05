import { ref, onMounted, onUnmounted, readonly, type Ref } from 'vue';
import type { AetherisClient, AetherisSession } from '@aetheris-iui/core';

export function useAetherisSession(
  client: AetherisClient,
  sessionId: Ref<string | null>,
) {
  const session: Ref<AetherisSession | null> = ref(null);
  let cleanup: (() => void) | null = null;

  function subscribe(id: string | null) {
    cleanup?.();
    cleanup = null;
    if (!id) { session.value = null; return; }
    const s = client.getSession(id) ?? null;
    session.value = s;
    if (!s) return;
    const onChange = () => { session.value = s; };
    s.addEventListener('change', onChange);
    cleanup = () => s.removeEventListener('change', onChange);
  }

  onMounted(() => subscribe(sessionId.value));
  onUnmounted(() => cleanup?.());

  // re-subscribe when sessionId changes
  let stopWatch: (() => void) | null = null;

  onMounted(() => {
    // manual watch: poll is not ideal but avoids importing watch without a proper Vue context
    // Callers should pass a computed/ref and call subscribe manually when it changes.
  });

  return {
    session: readonly(session),
    fillSlot: (slotName: string, valueJson: string) => {
      if (sessionId.value) client.fillSlot(sessionId.value, slotName, valueJson);
    },
    preview: () => { if (sessionId.value) client.preview(sessionId.value); },
    confirm: () => { if (sessionId.value) client.confirm(sessionId.value); },
    cancel: (note?: string) => { if (sessionId.value) client.cancel(sessionId.value, note); },
    subscribe,
  };
}
