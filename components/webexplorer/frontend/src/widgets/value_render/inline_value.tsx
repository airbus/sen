// === InlineValue.tsx =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { Fragment, useMemo } from "react";

import type { Client, Var, Variant } from "@sen/client";
import {
  isPrimitive,
  isQuantity,
  isSequence,
  isStruct,
  isVariant,
} from "@sen/client";

import { Mono, Muted } from "../../ui/text_primitives.js";
import { PrimitiveValue, QuantityValue } from "./primitive_values.js";

const SEQUENCE_PREVIEW = 6;

export function InlineValue({
  client,
  declaredType,
  value,
}: {
  client: Client | null;
  declaredType: string;
  value: Var | undefined;
}) {
  if (value === undefined) return <Muted>--</Muted>;
  if (value === null) return <Muted>none</Muted>;

  if (isQuantity(value)) return <QuantityValue q={value} />;
  if (isVariant(value)) return <InlineVariant client={client} v={value} />;
  if (isPrimitive(value)) {
    return <PrimitiveValue declaredType={declaredType} value={value} client={client} />;
  }
  if (isSequence(value)) {
    return (
      <InlineSequence client={client} declaredType={declaredType} value={value} />
    );
  }
  if (isStruct(value)) {
    return (
      <InlineStruct client={client} declaredType={declaredType} value={value} />
    );
  }
  return <Mono>{JSON.stringify(value)}</Mono>;
}

function InlineStruct({
  client,
  declaredType,
  value,
}: {
  client: Client | null;
  declaredType: string;
  value: Record<string, Var>;
}) {
  // Split memos so spec lookup runs only on type change, not on every value identity flip.
  const spec = useMemo<{ specOrder: readonly string[]; types: Readonly<Record<string, string>> } | null>(() => {
    const s = client?.getType(declaredType);
    if (!s || s.data.type !== "sen.kernel.StructTypeSpec") return null;
    const fields = s.data.value.fields;
    const types: Record<string, string> = {};
    const specOrder: string[] = [];
    for (const f of fields) {
      types[f.name] = f.type;
      specOrder.push(f.name);
    }
    return { specOrder, types };
  }, [client, declaredType]);
  const meta = useMemo<{ order: readonly string[]; types: Readonly<Record<string, string>> }>(() => {
    if (!spec) return { order: Object.keys(value), types: {} };
    return { order: spec.specOrder.filter((n) => n in value), types: spec.types };
  }, [spec, value]);

  if (meta.order.length === 0) return <Muted>{"{}"}</Muted>;

  return (
    <span style={braceWrap}>
      <Punct>{"{"}</Punct>
      {meta.order.map((name, i) => {
        const fieldType = meta.types[name] ?? "unknown";
        return (
          <Fragment key={name}>
            {i > 0 && <Punct>,&nbsp;</Punct>}
            <span style={fieldNameStyle}>{name}:&nbsp;</span>
            <InlineValue client={client} declaredType={fieldType} value={value[name]} />
          </Fragment>
        );
      })}
      <Punct>{"}"}</Punct>
    </span>
  );
}

function InlineSequence({
  client,
  declaredType,
  value,
}: {
  client: Client | null;
  declaredType: string;
  value: readonly Var[];
}) {
  const elementType = useMemo(() => {
    const spec = client?.getType(declaredType);
    if (spec && spec.data.type === "sen.kernel.SequenceTypeSpec") {
      return spec.data.value.elementType;
    }
    return "unknown";
  }, [client, declaredType]);

  if (value.length === 0) return <Muted>[]</Muted>;

  const visible = value.slice(0, SEQUENCE_PREVIEW);
  const overflow = value.length - visible.length;
  return (
    <span style={braceWrap}>
      <Punct>[</Punct>
      {visible.map((v, i) => (
        <Fragment key={i}>
          {i > 0 && <Punct>,&nbsp;</Punct>}
          <InlineValue client={client} declaredType={elementType} value={v} />
        </Fragment>
      ))}
      {overflow > 0 && <Muted>,&nbsp;...+{overflow}</Muted>}
      <Punct>]</Punct>
    </span>
  );
}

function InlineVariant({ client, v }: { client: Client | null; v: Variant }) {
  const armType = v.type;
  const payload = v.value as Var;
  return (
    <span style={braceWrap}>
      <span style={fieldNameStyle}>{armType}</span>
      <Punct>(</Punct>
      <InlineValue client={client} declaredType={armType} value={payload} />
      <Punct>)</Punct>
    </span>
  );
}

function Punct({ children }: { children: React.ReactNode }) {
  return <span style={{ color: "var(--fg-subtle)" }}>{children}</span>;
}

const braceWrap: React.CSSProperties = {
  display: "inline",
  fontFamily: "var(--font-mono)",
};

const fieldNameStyle: React.CSSProperties = {
  color: "var(--fg-muted)",
  fontFamily: "var(--font-mono)",
};
