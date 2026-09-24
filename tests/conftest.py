"""Shared Hypothesis strategies for UDF tests."""

from __future__ import annotations

import io

from hypothesis import strategies as st
from PIL import Image

from udf_lib.models import AnimationSlot, SlotAlias, UnitDef

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

UNIT_CLASSES = ["fighter", "worker", "building", "factory"]
DIRECTION_COUNTS = [1, 8]


def _rgba_pixel_data(
    direction_count: int,
    frame_count: int,
    frame_width: int,
    frame_height: int,
) -> st.SearchStrategy[list[list[bytes]]]:
    """Strategy producing pixel_data[dir][frame] = RGBA bytes."""
    frame_size = frame_width * frame_height * 4
    frame_st = st.binary(min_size=frame_size, max_size=frame_size)
    direction_st = st.lists(frame_st, min_size=frame_count, max_size=frame_count)
    return st.lists(direction_st, min_size=direction_count, max_size=direction_count)


# ---------------------------------------------------------------------------
# Public strategies
# ---------------------------------------------------------------------------


@st.composite
def st_animation_slot(draw: st.DrawFn, name: str | None = None) -> AnimationSlot:
    """Arbitrary AnimationSlot with random but consistent pixel data."""
    slot_name = name if name is not None else draw(
        st.text(alphabet=st.characters(whitelist_categories=("Ll",)), min_size=1, max_size=12)
    )
    direction_count = draw(st.sampled_from(DIRECTION_COUNTS))
    frame_count = draw(st.integers(min_value=1, max_value=16))
    atime_ms = draw(st.integers(min_value=0, max_value=10000))
    anchor_x = draw(st.integers(min_value=-256, max_value=256))
    anchor_y = draw(st.integers(min_value=-256, max_value=256))
    frame_width = draw(st.sampled_from([16, 32, 64]))
    frame_height = draw(st.sampled_from([16, 32, 64]))
    pixel_data = draw(_rgba_pixel_data(direction_count, frame_count, frame_width, frame_height))
    return AnimationSlot(
        name=slot_name,
        direction_count=direction_count,
        frame_count=frame_count,
        atime_ms=atime_ms,
        anchor_x=anchor_x,
        anchor_y=anchor_y,
        frame_width=frame_width,
        frame_height=frame_height,
        pixel_data=pixel_data,
    )


@st.composite
def st_unit_def(draw: st.DrawFn) -> UnitDef:
    """Arbitrary UnitDef with random slots and pixel data."""
    unit_class = draw(st.sampled_from(UNIT_CLASSES))
    unit_id = draw(
        st.text(alphabet=st.characters(whitelist_categories=("Ll", "Nd"), whitelist_characters="-_"),
                min_size=1, max_size=20)
    )
    display_name = draw(st.text(min_size=1, max_size=40))
    format_version = 1

    # Generate 1–5 unique slot names
    slot_names = draw(
        st.lists(
            st.text(alphabet=st.characters(whitelist_categories=("Ll",)), min_size=1, max_size=10),
            min_size=1,
            max_size=5,
            unique=True,
        )
    )

    slots: dict[str, AnimationSlot] = {}
    for sname in slot_names:
        slots[sname] = draw(st_animation_slot(name=sname))

    return UnitDef(
        format_version=format_version,
        unit_id=unit_id,
        unit_class=unit_class,
        display_name=display_name,
        slots=slots,
        slot_aliases=[],
    )


@st.composite
def st_slot_manifest(draw: st.DrawFn) -> dict:
    """Arbitrary slot manifest dict with random source image bytes."""
    unit_class = draw(st.sampled_from(UNIT_CLASSES))
    unit_id = draw(st.text(min_size=1, max_size=20))
    display_name = draw(st.text(min_size=1, max_size=40))

    slot_names = draw(
        st.lists(
            st.text(alphabet=st.characters(whitelist_categories=("Ll",)), min_size=1, max_size=10),
            min_size=1,
            max_size=4,
            unique=True,
        )
    )

    slots: dict = {}
    for sname in slot_names:
        direction_count = draw(st.sampled_from(DIRECTION_COUNTS))
        frame_count = draw(st.integers(min_value=1, max_value=4))
        frame_w = draw(st.sampled_from([16, 32]))
        frame_h = draw(st.sampled_from([16, 32]))
        img_w = frame_w  # hcount=1
        img_h = frame_h * frame_count  # vcount=frame_count

        sources = []
        for _ in range(direction_count):
            img = Image.new("RGBA", (img_w, img_h))
            buf = io.BytesIO()
            img.save(buf, format="PNG")
            sources.append(buf.getvalue())

        slots[sname] = {
            "sources": sources,
            "hcount": 1,
            "vcount": frame_count,
            "atime_ms": draw(st.integers(min_value=0, max_value=10000)),
            "anchor_x": draw(st.integers(min_value=0, max_value=64)),
            "anchor_y": draw(st.integers(min_value=0, max_value=64)),
        }

    return {
        "unit_id": unit_id,
        "unit_class": unit_class,
        "display_name": display_name,
        "slots": slots,
        "slot_aliases": [],
    }


@st.composite
def st_invalid_json(draw: st.DrawFn) -> str:
    """Strings that are not valid JSON."""
    # Mix of clearly invalid strings
    invalid_examples = [
        "{bad json",
        "[1, 2,]",
        "{'key': 'value'}",  # single quotes
        "undefined",
        "",
        "{key: value}",
        "...",
        "<!-- comment -->",
    ]
    base = draw(st.sampled_from(invalid_examples))
    # Optionally append random garbage
    suffix = draw(st.text(min_size=0, max_size=10))
    candidate = base + suffix
    # Verify it's actually invalid JSON (filter out accidental valid cases)
    import json
    try:
        json.loads(candidate)
        # If it parsed, return something definitely invalid
        return "{definitely: invalid json!!!"
    except (json.JSONDecodeError, ValueError):
        return candidate


def st_unsupported_version() -> st.SearchStrategy[int]:
    """Integers outside the supported version set {1}."""
    return st.integers().filter(lambda v: v != 1)


@st.composite
def st_rgb_image(draw: st.DrawFn) -> Image.Image:
    """Random RGB image (no alpha channel), returned as PIL Image."""
    width = draw(st.sampled_from([16, 32, 64]))
    height = draw(st.sampled_from([16, 32, 64]))
    pixel_bytes = draw(st.binary(min_size=width * height * 3, max_size=width * height * 3))
    img = Image.frombytes("RGB", (width, height), pixel_bytes)
    return img


@st.composite
def st_frame_grid_image(draw: st.DrawFn, hcount: int, vcount: int) -> Image.Image:
    """Image whose dimensions are exactly hcount×frame_w by vcount×frame_h."""
    frame_w = draw(st.sampled_from([16, 32]))
    frame_h = draw(st.sampled_from([16, 32]))
    width = hcount * frame_w
    height = vcount * frame_h
    pixel_bytes = draw(st.binary(min_size=width * height * 4, max_size=width * height * 4))
    img = Image.frombytes("RGBA", (width, height), pixel_bytes)
    return img


@st.composite
def st_mismatched_image(draw: st.DrawFn, hcount: int, vcount: int) -> Image.Image:
    """Image whose dimensions do NOT evenly divide by hcount/vcount."""
    # Pick a base frame size, then add 1 to make it non-divisible
    frame_w = draw(st.sampled_from([16, 32]))
    frame_h = draw(st.sampled_from([16, 32]))
    # Add a non-zero offset that breaks divisibility
    offset_w = draw(st.integers(min_value=1, max_value=frame_w - 1))
    offset_h = draw(st.integers(min_value=1, max_value=frame_h - 1))
    width = hcount * frame_w + offset_w
    height = vcount * frame_h + offset_h
    pixel_bytes = draw(st.binary(min_size=width * height * 4, max_size=width * height * 4))
    img = Image.frombytes("RGBA", (width, height), pixel_bytes)
    return img
