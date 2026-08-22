// === errors.ts =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

export class GatewayError extends Error {
  constructor(message: string, options?: ErrorOptions) {
    super(message, options);
    this.name = new.target.name;
  }
}

export class GatewayInputError extends GatewayError {}

export class GatewayStateError extends GatewayError {}

export class KernelDisconnectedError extends GatewayStateError {
  constructor(kernelName: string) {
    super(`kernel '${kernelName}' is not connected (was it disconnected? call listKernels)`);
  }
}

// Shares KernelDisconnectedError's surface message so callers see one uniform diagnostic
// regardless of which race (never-registered vs. disconnected) the gateway lost.
export class KernelNotFoundError extends GatewayStateError {
  constructor(kernelName: string) {
    super(`kernel '${kernelName}' is not connected (was it disconnected? call listKernels)`);
  }
}
