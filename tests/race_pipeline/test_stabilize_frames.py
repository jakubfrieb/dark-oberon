import numpy as np

from stabilize_frames import freeze, motion_mask, stabilize


def frame(color, prop=None):
    """40x40 RGBA frame: a 20x10 body plus an optional 4x4 'propeller' block."""
    a = np.zeros((40, 40, 4), np.uint8)
    a[15:25, 10:30] = (*color, 255)
    if prop is not None:
        a[16:20, 30:34] = (*prop, 255)
    return a


def test_motion_mask_marks_only_pixels_that_change_in_reference():
    ref = [frame((200, 200, 200), (0, 0, 0)), frame((200, 200, 200), (255, 255, 255))]
    m = motion_mask(ref, grow=0)
    assert m[17, 31] and not m[20, 15]


def test_stabilize_keeps_motion_and_freezes_the_rest():
    # race frames: the body was repainted slightly differently in frame 1 (shimmer),
    # the propeller moves as intended
    race = [frame((100, 150, 100), (10, 10, 10)), frame((104, 146, 97), (240, 240, 240))]
    ref = [frame((200, 200, 200), (0, 0, 0)), frame((200, 200, 200), (255, 255, 255))]
    out = stabilize(race, ref, grow=0)
    assert (out[1][20, 15] == out[0][20, 15]).all()          # body frozen to frame 0
    assert tuple(out[1][17, 31][:3]) == (240, 240, 240)       # propeller animation kept
    assert (out[0] == race[0]).all()                          # first frame untouched


def test_stabilize_rejects_mismatched_sizes():
    import pytest
    with pytest.raises(ValueError):
        stabilize([frame((1, 1, 1))], [frame((1, 1, 1)), frame((2, 2, 2))])


def test_freeze_repeats_first_frame():
    race = [frame((100, 150, 100)), frame((104, 146, 97)), frame((90, 160, 110))]
    out = freeze(race)
    assert len(out) == 3 and all((f == race[0]).all() for f in out)
