import { createContext, useContext } from 'react';
import type { AetherisClient } from '@aetheris-iui/core';

export const AetherisContext = createContext<AetherisClient | null>(null);

export function useAetherisClient(): AetherisClient {
  const client = useContext(AetherisContext);
  if (!client) {
    throw new Error('useAetherisClient must be used inside <AetherisProvider>');
  }
  return client;
}
