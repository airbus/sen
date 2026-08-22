// === kernel_registry.ts ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { KernelNotFoundError } from "./errors.js";
import { Kernel } from "./kernel.js";

export class KernelRegistry {
  private readonly kernels = new Map<string, Kernel>();

  constructor(private readonly log: (msg: string) => void) {}

  async connect(name: string, url: string, openTimeoutMs: number): Promise<Kernel> {
    validateName(name);
    if (this.kernels.has(name)) throw new Error(`kernel name already in use: ${name}`);
    const kernel = new Kernel(name, url, openTimeoutMs, this.log);
    // Reserve the slot before the await so a concurrent connect with the same name hits
    // the duplicate-name guard instead of racing past it and leaking the loser's WebSocket.
    this.kernels.set(name, kernel);
    try {
      await kernel.openWs();
    } catch (err) {
      this.releaseName(name, kernel);
      throw err;
    }
    return kernel;
  }

  async disconnect(name: string): Promise<void> {
    const kernel = this.kernels.get(name);
    if (kernel === undefined) throw new KernelNotFoundError(name);
    // Free the name before the await so a reconnect isn't blocked by a slow teardown; the
    // kernel we hand to shutdown() is the one we just took out of the map.
    this.releaseName(name, kernel);
    await kernel.shutdown();
  }

  resolve(name: string | undefined): Kernel {
    if (name !== undefined) {
      const kernel = this.kernels.get(name);
      if (kernel === undefined) throw new KernelNotFoundError(name);
      return kernel;
    }
    const all = Array.from(this.kernels.values());
    if (all.length === 0) {
      throw new Error("no kernels connected; call connectToKernel first");
    }
    if (all.length > 1) {
      const names = all.map((k) => k.name).join(", ");
      throw new Error(`multiple kernels connected; specify 'kernel'. Available: ${names}`);
    }
    return all[0]!;
  }

  list(): Kernel[] {
    return Array.from(this.kernels.values());
  }

  async shutdown(): Promise<void> {
    const all = Array.from(this.kernels.values());
    this.kernels.clear();
    await Promise.allSettled(all.map((k) => k.shutdown()));
  }

  // A name outlives the kernel that held it: by the time a slow dial fails, the same name may
  // already belong to a later kernel. Deleting blind would evict that one from the map while
  // its socket, timers and interests stay alive with nothing able to reach them.
  private releaseName(name: string, kernel: Kernel): void {
    if (this.kernels.get(name) === kernel) this.kernels.delete(name);
  }
}

const NAME_PATTERN = /^[A-Za-z0-9._-]{1,64}$/;

function validateName(name: string): void {
  if (!NAME_PATTERN.test(name)) {
    throw new Error(
      `kernel name must match ${NAME_PATTERN.source} (got ${JSON.stringify(name)})`,
    );
  }
}
