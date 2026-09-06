// === errors.ts =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

export class GatewayError extends Error {
  // Declared per class rather than taken from new.target.name, which is the class's runtime name
  // and so whatever a minifier chose. Audit entries record this, and a mangled one says nothing.
  static readonly errorName: string = "GatewayError";

  constructor(message: string, options?: ErrorOptions) {
    super(message, options);
    this.name = new.target.errorName;
  }
}

export class GatewayInputError extends GatewayError {
  static override readonly errorName: string = "GatewayInputError";
}

export class GatewayStateError extends GatewayError {
  static override readonly errorName: string = "GatewayStateError";
}

export class KernelDisconnectedError extends GatewayStateError {
  static override readonly errorName: string = "KernelDisconnectedError";

  constructor(kernelName: string) {
    super(`kernel '${kernelName}' is not connected (was it disconnected? call listKernels)`);
  }
}

// Shares KernelDisconnectedError's surface message so callers see one uniform diagnostic
// regardless of which race (never-registered vs. disconnected) the gateway lost.
export class KernelNotFoundError extends GatewayStateError {
  static override readonly errorName: string = "KernelNotFoundError";

  constructor(kernelName: string) {
    super(`kernel '${kernelName}' is not connected (was it disconnected? call listKernels)`);
  }
}
