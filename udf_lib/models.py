"""UDF data models."""

from __future__ import annotations

from dataclasses import dataclass, field


@dataclass
class AnimationSlot:
    name: str                       # e.g. "stay", "move"
    direction_count: int            # 1 or 8
    frame_count: int                # frames per direction
    atime_ms: int                   # total animation time in milliseconds
    anchor_x: int                   # pointx in engine pixels
    anchor_y: int                   # pointy in engine pixels
    frame_width: int                # pixels
    frame_height: int               # pixels
    # pixel_data[direction_index][frame_index] = bytes (RGBA, row-major)
    pixel_data: list[list[bytes]]
    alias_of: str | None = None     # slot name this slot aliases, or None


@dataclass
class SlotAlias:
    slot_name: str    # the slot that is aliased (has no pixel data)
    source_slot: str  # the slot whose pixel data is reused


@dataclass
class UnitDef:
    format_version: int
    unit_id: str
    unit_class: str          # "fighter" | "worker" | "building" | "factory"
    display_name: str
    slots: dict[str, AnimationSlot]
    slot_aliases: list[SlotAlias] = field(default_factory=list)
