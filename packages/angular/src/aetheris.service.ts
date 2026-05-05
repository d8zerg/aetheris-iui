import { Injectable, OnDestroy, InjectionToken, Inject } from '@angular/core';
import { BehaviorSubject, Observable } from 'rxjs';
import type { AetherisClient, AetherisSession, ProcessIntentRequest } from '@aetheris-iui/core';

export const AETHERIS_CLIENT = new InjectionToken<AetherisClient>('AetherisClient');

@Injectable({ providedIn: 'root' })
export class AetherisService implements OnDestroy {
  private readonly _sessions = new Map<string, BehaviorSubject<AetherisSession | null>>();
  private readonly _unsubs = new Map<string, () => void>();

  constructor(@Inject(AETHERIS_CLIENT) private readonly client: AetherisClient) {}

  session$(sessionId: string): Observable<AetherisSession | null> {
    if (!this._sessions.has(sessionId)) {
      const subj = new BehaviorSubject<AetherisSession | null>(
        this.client.getSession(sessionId) ?? null,
      );
      this._sessions.set(sessionId, subj);

      const s = this.client.getSession(sessionId);
      if (s) {
        const onChange = () => subj.next(s);
        s.addEventListener('change', onChange);
        this._unsubs.set(sessionId, () => s.removeEventListener('change', onChange));
      }
    }
    return this._sessions.get(sessionId)!.asObservable();
  }

  fillSlot(sessionId: string, slotName: string, valueJson: string): void {
    this.client.fillSlot(sessionId, slotName, valueJson);
  }

  preview(sessionId: string): void { this.client.preview(sessionId); }
  confirm(sessionId: string): void { this.client.confirm(sessionId); }
  cancel(sessionId: string, note?: string): void { this.client.cancel(sessionId, note); }

  async processIntent(request: ProcessIntentRequest): Promise<AetherisSession> {
    return this.client.processIntent(request);
  }

  ngOnDestroy(): void {
    this._unsubs.forEach((fn) => fn());
    this._sessions.forEach((subj) => subj.complete());
  }
}
