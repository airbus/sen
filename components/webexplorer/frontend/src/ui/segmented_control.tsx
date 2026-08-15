// === SegmentedControl.tsx ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

export interface SegmentedControlOption<T extends string> {
  value: T;
  label: string;
  tooltip?: string;
}

export interface SegmentedControlProps<T extends string> {
  ariaLabel: string;
  value: T;
  onChange: (next: T) => void;
  options: ReadonlyArray<SegmentedControlOption<T>>;
  /** Stretch each segment to fill container width; defaults to natural width. */
  fillWidth?: boolean;
  /** Skip the uppercase transform; use for labels with literal unit letters (`5s 1m 1h`)
   *  where uppercasing turns `m` into ambiguous `M`. */
  preserveCase?: boolean;
}

export function SegmentedControl<T extends string>({
  ariaLabel,
  value,
  onChange,
  options,
  fillWidth = false,
  preserveCase = false,
}: SegmentedControlProps<T>) {
  return (
    <div
      role="radiogroup"
      aria-label={ariaLabel}
      style={{
        display: fillWidth ? "flex" : "inline-flex",
        gap: 2,
        padding: 2,
        border: "1px solid var(--border-default)",
        borderRadius: "var(--radius-md)",
        background: "rgba(0, 0, 0, 0.15)",
      }}
    >
      {options.map((opt) => {
        const active = value === opt.value;
        // Active segment uses `.pressable` chrome so it lifts off the inset tray; inactive
        // segments stay transparent so the eye lands on the current mode.
        return (
          <button
            key={opt.value}
            type="button"
            role="radio"
            aria-checked={active}
            aria-pressed={active}
            className={active ? "pressable" : undefined}
            onClick={() => onChange(opt.value)}
            title={opt.tooltip ?? opt.label}
            style={{
              flex: fillWidth ? 1 : undefined,
              padding: "3px 9px",
              fontFamily: "var(--font-ui)",
              fontSize: "var(--fs-xs)",
              color: active ? "var(--accent-text-wash)" : "var(--fg-subtle)",
              background: active ? undefined : "transparent",
              border: active ? undefined : "none",
              cursor: "pointer",
              borderRadius: "var(--radius-sm)",
              letterSpacing: preserveCase ? undefined : "0.04em",
              textTransform: preserveCase ? "none" : "uppercase",
              transition: "color 120ms ease",
            }}
          >
            {opt.label}
          </button>
        );
      })}
    </div>
  );
}
