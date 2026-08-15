// === SequenceInput.tsx ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useEffect, useMemo, useRef, useState } from "react";
import type * as React from "react";

import {
  DndContext,
  DragOverlay,
  KeyboardSensor,
  PointerSensor,
  closestCenter,
  useSensor,
  useSensors,
  type DragEndEvent,
  type DragStartEvent,
} from "@dnd-kit/core";
import {
  SortableContext,
  sortableKeyboardCoordinates,
  useSortable,
  verticalListSortingStrategy,
} from "@dnd-kit/sortable";
import { CSS } from "@dnd-kit/utilities";

import type { Client, SequenceTypeSpec, Var } from "@sen/client";

import { defaultFor } from "../../core/types.js";
import { HoverIconButton } from "../../ui/buttons.js";
import { CloseIcon, GripIcon, PlusIcon } from "../../ui/icons.js";
import { Muted } from "../../ui/text_primitives.js";
import { baseInputStyle } from "./_styles.js";
import { ValueInput } from "./value_input.js";

export function SequenceInput({
  client,
  spec,
  value,
  onChange,
}: {
  client: Client | null;
  spec: SequenceTypeSpec;
  value: Var[];
  onChange: (v: Var[]) => void;
}) {
  if (spec.elementType === "u8") {
    return <ByteSequenceInput spec={spec} value={value as number[]} onChange={onChange} />;
  }
  return <GenericSequenceInput client={client} spec={spec} value={value} onChange={onChange} />;
}

// Stable per-row IDs (not array index) so dnd-kit can distinguish reorder from in-place edit.
interface SeqRow {
  id: string;
  value: Var;
}

let nextSeqRowId = 0;
function freshSeqRowId(): string {
  return `seq-row-${++nextSeqRowId}`;
}

function GenericSequenceInput({
  client,
  spec,
  value,
  onChange,
}: {
  client: Client | null;
  spec: SequenceTypeSpec;
  value: Var[];
  onChange: (v: Var[]) => void;
}) {
  const canMutate = !spec.fixedSize;
  const canAddMore = canMutate && (spec.maxSize === null || value.length < spec.maxSize);
  const canReorder = canMutate && value.length > 1;

  // Same-length value replacements keep IDs in place; length mismatches resync via the effect.
  const [ids, setIds] = useState<string[]>(() => value.map(() => freshSeqRowId()));
  const idsLengthRef = useRef(ids.length);
  idsLengthRef.current = ids.length;
  useEffect(() => {
    if (value.length === idsLengthRef.current) return;
    setIds((prev) => {
      if (prev.length === value.length) return prev;
      if (prev.length < value.length) {
        const padded = prev.slice();
        while (padded.length < value.length) padded.push(freshSeqRowId());
        return padded;
      }
      return prev.slice(0, value.length);
    });
  }, [value.length]);

  const rows = useMemo<SeqRow[]>(
    () => value.map((v, i) => ({ id: ids[i] ?? `seq-row-pending-${i}`, value: v })),
    [value, ids],
  );

  const updateAt = (i: number, next: Var) => {
    const copy = value.slice();
    copy[i] = next;
    onChange(copy);
  };
  const insertAt = (i: number) => {
    if (!canAddMore) return;
    const copyValues = value.slice();
    copyValues.splice(i, 0, defaultFor(client, spec.elementType));
    const copyIds = ids.slice();
    copyIds.splice(i, 0, freshSeqRowId());
    setIds(copyIds);
    onChange(copyValues);
  };
  const removeAt = (i: number) => {
    const copyValues = value.slice();
    copyValues.splice(i, 1);
    const copyIds = ids.slice();
    copyIds.splice(i, 1);
    setIds(copyIds);
    onChange(copyValues);
  };
  const move = (from: number, to: number) => {
    if (from === to) return;
    const copyValues = value.slice();
    const [item] = copyValues.splice(from, 1);
    if (item === undefined) return;
    copyValues.splice(to, 0, item);
    const copyIds = ids.slice();
    const [movedId] = copyIds.splice(from, 1);
    if (movedId !== undefined) copyIds.splice(to, 0, movedId);
    setIds(copyIds);
    onChange(copyValues);
  };

  // 4px activation distance so a click on the input doesn't start a drag.
  const sensors = useSensors(
    useSensor(PointerSensor, { activationConstraint: { distance: 4 } }),
    useSensor(KeyboardSensor, { coordinateGetter: sortableKeyboardCoordinates }),
  );

  // Portal-mounted DragOverlay escapes ancestor stacking contexts so the row floats above.
  const [activeId, setActiveId] = useState<string | null>(null);
  const activeRow = activeId !== null ? rows.find((r) => r.id === activeId) ?? null : null;
  const activeIndex = activeId !== null ? ids.indexOf(activeId) : -1;

  const handleDragStart = (event: DragStartEvent) => {
    setActiveId(String(event.active.id));
  };
  const handleDragEnd = (event: DragEndEvent) => {
    setActiveId(null);
    const { active, over } = event;
    if (!over || active.id === over.id) return;
    const from = ids.indexOf(String(active.id));
    const to = ids.indexOf(String(over.id));
    if (from < 0 || to < 0) return;
    move(from, to);
  };

  return (
    <div style={{ display: "flex", flexDirection: "column", gap: 4 }}>
      {value.length === 0 ? (
        <Muted>[]</Muted>
      ) : (
        <DndContext
          sensors={sensors}
          collisionDetection={closestCenter}
          onDragStart={handleDragStart}
          onDragEnd={handleDragEnd}
          onDragCancel={() => setActiveId(null)}
        >
          <SortableContext items={ids} strategy={verticalListSortingStrategy}>
            {rows.map((row, index) => (
              <SequenceRow
                key={row.id}
                id={row.id}
                index={index}
                value={row.value}
                client={client}
                elementType={spec.elementType}
                canRemove={canMutate}
                canInsert={canAddMore}
                canReorder={canReorder}
                onUpdate={(n) => updateAt(index, n)}
                onInsertAfter={() => insertAt(index + 1)}
                onRemove={() => removeAt(index)}
              />
            ))}
          </SortableContext>
          <DragOverlay dropAnimation={null}>
            {activeRow ? (
              <SequenceRow
                id={activeRow.id}
                index={Math.max(activeIndex, 0)}
                value={activeRow.value}
                client={client}
                elementType={spec.elementType}
                canRemove={canMutate}
                canInsert={canAddMore}
                canReorder={canReorder}
                onUpdate={() => {}}
                onInsertAfter={() => {}}
                onRemove={() => {}}
                overlay
              />
            ) : null}
          </DragOverlay>
        </DndContext>
      )}
      {canAddMore && (
        <span style={{ alignSelf: "flex-start" }}>
          <HoverIconButton
            onClick={() => insertAt(value.length)}
            ariaLabel="add element"
            tooltip="Append element"
            icon={<PlusIcon />}
            size={22}
          />
        </span>
      )}
    </div>
  );
}

function SequenceRow({
  id,
  index,
  value,
  client,
  elementType,
  canRemove,
  canInsert,
  canReorder,
  onUpdate,
  onInsertAfter,
  onRemove,
  overlay = false,
}: {
  id: string;
  index: number;
  value: Var;
  client: Client | null;
  elementType: string;
  canRemove: boolean;
  canInsert: boolean;
  canReorder: boolean;
  onUpdate: (next: Var) => void;
  onInsertAfter: () => void;
  onRemove: () => void;
  overlay?: boolean;
}) {
  const sortable = useSortable({ id, disabled: !canReorder || overlay });
  const isDragging = !overlay && sortable.isDragging;
  const rowStyle: React.CSSProperties = overlay
    ? {
        display: "flex",
        alignItems: "center",
        gap: 6,
        background: "var(--bg-elevated)",
        boxShadow: "0 8px 20px rgba(0, 0, 0, 0.32)",
        borderRadius: "var(--radius-sm)",
        padding: "1px 2px",
        cursor: "grabbing",
      }
    : {
        display: "flex",
        alignItems: "center",
        gap: 6,
        transform: CSS.Transform.toString(sortable.transform),
        transition: sortable.transition,
        // In-list placeholder while the overlay carries the visual.
        opacity: isDragging ? 0.25 : 1,
      };
  const refToUse = overlay ? undefined : sortable.setNodeRef;
  const attrs = overlay ? {} : sortable.attributes;
  return (
    <div ref={refToUse} style={rowStyle} {...attrs}>
      {canReorder ? (
        <span
          ref={overlay ? undefined : sortable.setActivatorNodeRef}
          {...(overlay ? {} : sortable.listeners)}
          title="Drag to reorder"
          style={{
            color: "var(--fg-subtle)",
            cursor: isDragging || overlay ? "grabbing" : "grab",
            display: "inline-flex",
            alignItems: "center",
            padding: "2px 1px",
            touchAction: "none",
          }}
        >
          <GripIcon />
        </span>
      ) : (
        <span style={{ width: 10, display: "inline-block" }} />
      )}
      <span
        style={{
          fontFamily: "var(--font-mono)",
          fontSize: "var(--fs-sm)",
          color: "var(--fg-subtle)",
          minWidth: 18,
        }}
      >
        [{index}]
      </span>
      <span style={{ flex: "0 0 auto", minWidth: 0 }}>
        <ValueInput
          client={client}
          declaredType={elementType}
          value={value}
          onChange={onUpdate}
        />
      </span>
      {canInsert && (
        <HoverIconButton
          onClick={onInsertAfter}
          ariaLabel={`insert element after [${index}]`}
          tooltip="Insert below"
          icon={<PlusIcon />}
          size={22}
        />
      )}
      {canRemove && (
        <HoverIconButton
          onClick={onRemove}
          ariaLabel={`remove element [${index}]`}
          tooltip="Remove"
          icon={<CloseIcon />}
          danger
          size={22}
        />
      )}
    </div>
  );
}

type ByteEncoding = "ascii" | "hex" | "dec";

function ByteSequenceInput({
  spec,
  value,
  onChange,
}: {
  spec: SequenceTypeSpec;
  value: number[];
  onChange: (v: number[]) => void;
}) {
  const [encoding, setEncoding] = useState<ByteEncoding>("hex");
  const reEncoded = useMemo(() => encodeBytes(value, encoding), [value, encoding]);
  // dirty protects in-progress draft from external value updates until commit.
  const [draft, setDraft] = useState(reEncoded);
  const [dirty, setDirty] = useState(false);
  useEffect(() => {
    if (!dirty) setDraft(reEncoded);
  }, [reEncoded, dirty]);

  const commit = () => {
    const bytes = decodeBytes(draft, encoding);
    setDirty(false);
    if (bytes === null) return;
    if (spec.fixedSize && bytes.length !== value.length) return;
    if (spec.maxSize !== null && bytes.length > spec.maxSize) return;
    onChange(bytes);
  };

  return (
    <div style={{ display: "flex", flexDirection: "column", gap: 4 }}>
      <span style={{ display: "inline-flex", alignItems: "center", gap: 8 }}>
        <span style={{ fontSize: "var(--fs-sm)", color: "var(--fg-subtle)", fontFamily: "var(--font-mono)" }}>
          {value.length} byte{value.length === 1 ? "" : "s"}
        </span>
        <ByteEncodingPicker value={encoding} onChange={(next) => { setEncoding(next); setDirty(false); }} />
      </span>
      <textarea
        value={draft}
        onChange={(e) => {
          setDraft(e.target.value);
          setDirty(true);
        }}
        onBlur={commit}
        onKeyDown={(e) => {
          if (e.key === "Enter") {
            if (e.metaKey || e.ctrlKey) {
              e.preventDefault();
              commit();
              return;
            }
            e.stopPropagation();
          }
        }}
        title="Cmd/Ctrl+Enter to commit"
        style={baseInputStyle({
          width: 360,
          minHeight: 60,
          fontFamily: "var(--font-mono)",
          resize: "vertical",
        })}
      />
    </div>
  );
}

function ByteEncodingPicker({
  value,
  onChange,
}: {
  value: ByteEncoding;
  onChange: (next: ByteEncoding) => void;
}) {
  const options: ByteEncoding[] = ["ascii", "hex", "dec"];
  return (
    <span
      style={{
        display: "inline-flex",
        border: "1px solid var(--accent-border)",
        borderRadius: "var(--radius-sm)",
        overflow: "hidden",
      }}
    >
      {options.map((enc, i) => {
        const active = enc === value;
        return (
          <button
            key={enc}
            type="button"
            onClick={() => onChange(enc)}
            style={{
              padding: "2px 8px",
              fontSize: "var(--fs-sm)",
              fontFamily: "var(--font-mono)",
              border: "none",
              borderLeft: i > 0 ? "1px solid var(--border-default)" : "none",
              background: active ? "var(--accent-gradient)" : "var(--bg-input)",
              color: active ? "var(--accent-fg)" : "var(--fg-base)",
              cursor: "pointer",
            }}
          >
            {enc}
          </button>
        );
      })}
    </span>
  );
}

function encodeBytes(bytes: number[], encoding: ByteEncoding): string {
  if (encoding === "ascii") {
    return bytes
      .map((b) => (b >= 0x20 && b < 0x7f ? String.fromCharCode(b) : `\\x${b.toString(16).padStart(2, "0")}`))
      .join("");
  }
  if (encoding === "hex") {
    return bytes.map((b) => b.toString(16).padStart(2, "0")).join(" ");
  }
  return bytes.map((b) => String(b)).join(", ");
}

// Returns null on malformed input; caller rejects the commit.
function decodeBytes(text: string, encoding: ByteEncoding): number[] | null {
  if (encoding === "ascii") {
    const out: number[] = [];
    let i = 0;
    while (i < text.length) {
      if (text[i] === "\\" && text[i + 1] === "x") {
        const hex = text.slice(i + 2, i + 4);
        const b = parseInt(hex, 16);
        if (!Number.isFinite(b)) return null;
        out.push(b);
        i += 4;
      } else {
        out.push(text.charCodeAt(i));
        i++;
      }
    }
    return out;
  }
  const tokens = text.trim().split(/[\s,]+/).filter(Boolean);
  const out: number[] = [];
  for (const t of tokens) {
    const b = parseInt(t, encoding === "hex" ? 16 : 10);
    if (!Number.isFinite(b) || b < 0 || b > 255) return null;
    out.push(b);
  }
  return out;
}
