// === errors.ts =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

/**
 * Wire error codes the Sen server emits. Use these to switch on `JsonRpcError.code` rather
 * than hard-coding magic numbers. The first block is the JSON-RPC 2.0 reserved range; the
 * second block is Sen application codes in the spec's "Server error" -32000..-32099 range,
 * used when the envelope and params are well-formed but the request semantically fails.
 */
export const JsonRpcErrorCode = {
  parseError: -32700,
  invalidRequest: -32600,
  methodNotFound: -32601,
  invalidParams: -32602,
  internalError: -32603,

  unknownInterest: -32001,
  objectNotInInterest: -32002,
  unknownMember: -32003,
  unknownType: -32004,
  notWritable: -32005,
} as const;

export type JsonRpcErrorCode = (typeof JsonRpcErrorCode)[keyof typeof JsonRpcErrorCode];

/** Base class for all errors thrown by this library. */
export class SenClientError extends Error {
  constructor(message: string, options?: ErrorOptions) {
    super(message, options);
    this.name = new.target.name;
  }
}

/**
 * The server returned a JSON-RPC error. `code` follows the JSON-RPC 2.0 reserved ranges plus
 * any application codes the Sen server defines.
 */
export class JsonRpcError extends SenClientError {
  constructor(
    public readonly code: number,
    message: string,
    public readonly data?: unknown,
    options?: ErrorOptions,
  ) {
    super(message, options);
  }
}

/** Connection-level failure: lost connection, malformed frame, call while not connected. */
export class TransportError extends SenClientError {
  constructor(message: string, options?: ErrorOptions) {
    super(message, options);
  }
}

/** No response within the per-call timeout. `timeoutMs` is the elapsed deadline. */
export class TimeoutError extends SenClientError {
  constructor(
    message: string,
    public readonly timeoutMs: number,
    options?: ErrorOptions,
  ) {
    super(message, options);
  }
}

/**
 * Thrown when an `InterestHandle` method is called after `release()` (or after the initial
 * `declareInterest` was rejected). Cleanup paths that race release can catch this and
 * ignore it.
 */
export class InterestReleasedError extends SenClientError {
  constructor(
    public readonly interestName: string,
    operation: string,
    options?: ErrorOptions,
  ) {
    super(`Interest "${interestName}" is released; ${operation} is no longer valid`, options);
  }
}
