// === env_flags.ts ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Boolean environment flags, parsed strictly in both directions.
//
// The flags this parses are security gates: SEN_MCP_GATEWAY_READONLY withdraws the write tools
// and the python child, SEN_MCP_GATEWAY_NO_RECORDING withdraws the python child. A parser that
// answers "false" to a spelling it does not recognise turns an operator's typo into a gateway
// that serves writes and runs arbitrary python while its operator believes it is locked down,
// and says nothing. So an unrecognised value is an error rather than a guess, which is the same
// choice SEN_RECORDING_TIMEOUT_MS already makes for a value outside its range.
//
// Whitespace is trimmed because it never carries meaning here and `FLAG="true "` is a shell
// accident, not a different intent. An empty value reads as unset, matching envOrUndefined.

const TRUE_VALUES = ["1", "true", "yes", "on"] as const;
const FALSE_VALUES = ["0", "false", "no", "off"] as const;

/// Thrown at startup for a flag whose value is neither truthy nor falsey. Fatal on purpose:
/// the caller cannot pick a safe default, because the permissive reading is the unsafe one.
export class EnvFlagError extends Error {
  constructor(name: string, value: string) {
    super(
      `${name} must be one of ${TRUE_VALUES.join(", ")} to enable, ` +
        `or ${FALSE_VALUES.join(", ")} to disable; got ${JSON.stringify(value)}`,
    );
    this.name = "EnvFlagError";
  }
}

/// True when `name` is set to a recognised truthy value, false when unset, empty, or set to a
/// recognised falsey value. Throws EnvFlagError on anything else.
export function isEnvFlagSet(name: string, env: NodeJS.ProcessEnv = process.env): boolean {
  const raw = env[name];
  if (raw === undefined) return false;
  const value = raw.trim().toLowerCase();
  if (value === "") return false;
  if ((TRUE_VALUES as readonly string[]).includes(value)) return true;
  if ((FALSE_VALUES as readonly string[]).includes(value)) return false;
  throw new EnvFlagError(name, raw);
}
