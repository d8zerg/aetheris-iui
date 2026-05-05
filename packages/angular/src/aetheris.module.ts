import { NgModule, ModuleWithProviders } from '@angular/core';
import type { AetherisClient } from '@aetheris-iui/core';
import { AETHERIS_CLIENT } from './aetheris.service.js';
import { AetherisService } from './aetheris.service.js';

@NgModule({
  providers: [AetherisService],
})
export class AetherisModule {
  static forRoot(client: AetherisClient): ModuleWithProviders<AetherisModule> {
    return {
      ngModule: AetherisModule,
      providers: [
        { provide: AETHERIS_CLIENT, useValue: client },
        AetherisService,
      ],
    };
  }
}
