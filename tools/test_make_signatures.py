"""Tests for make_signatures.  Run: python -I tools/test_make_signatures.py

The classifier tests always run. The tests that read the game's code need OOT3D_CANONICAL_CODE (a code image
extracted from your own copy of the game, not distributed); without it they are reported as SKIP and the run
still exits 0.
"""
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_signatures as ms  # noqa: E402

SIGS_H = os.path.join(HERE, "..", "src", "sigs.h")
_path = ms.image_path()
IMAGE = open(_path, "rb").read() if _path else None


class Skip(Exception):
    pass


def need_image():
    if IMAGE is None:
        raise Skip("%s not set" % ms.ENV_IMAGE)


def count_matches(sig, mask, image):
    """Full masked scan at every byte offset (no first-byte pre-filter)."""
    L = len(sig)
    msig = [sig[k] & mask[k] for k in range(L)]
    n = 0
    for i in range(len(image) - L + 1):
        for k in range(L):
            if (image[i + k] & mask[k]) != msig[k]:
                break
        else:
            n += 1
    return n


def test_default_is_96_bytes_and_masks_branches_and_literals():
    need_image()
    for t in ms.TARGETS:
        sig, mask = ms.make_signature(IMAGE, t.addr, 24)
        assert len(sig) == 96 and len(mask) == 96
    sig, mask = ms.make_signature(IMAGE, 0x41E968, 24)
    # bl at 0x41E984 is instruction 7: low 24 bits masked, cond+opcode kept
    assert mask[28:32] == bytes([0, 0, 0, 0xFF])
    # ldr r2,[pc,#0x2ac] at instruction 0: imm12 masked
    assert mask[0:4] == bytes([0, 0xF0, 0xFF, 0xFF])
    # non-pc-relative instruction fully compared
    assert mask[4:8] == b"\xff\xff\xff\xff"


def test_each_signature_unique():
    need_image()
    for t in ms.TARGETS:
        sig, mask = ms.make_signature(IMAGE, t.addr, t.count, t.keep_literal_imm)
        assert len(sig) == 4 * t.count
        assert count_matches(sig, mask, IMAGE) == 1, t.name
    sig, mask = ms.make_signature(IMAGE, ms.PADHOLD_START, ms.PADHOLD_COUNT)
    assert count_matches(sig, mask, IMAGE) == 1, "PadHold"


def test_altering_unmasked_byte_gives_zero_matches():
    need_image()
    for t in ms.TARGETS:
        sig, mask = ms.make_signature(IMAGE, t.addr, t.count, t.keep_literal_imm)
        idx = next(i for i, m in enumerate(mask) if m == 0xFF)
        bad = bytearray(sig)
        bad[idx] ^= 0x01
        assert count_matches(bytes(bad), mask, IMAGE) == 0, t.name


def test_signature_is_instructions_only_inside_function():
    need_image()
    for t in ms.TARGETS:
        lo, hi = t.addr, t.addr + 4 * t.count
        for i in range(t.count):
            w = struct.unpack_from("<I", IMAGE, t.addr - ms.BASE + 4 * i)[0]
            tgt = ms.pc_literal_target(w, t.addr + 4 * i)
            assert tgt is None or not (lo <= tgt < hi), (t.name, i)  # no pool inside window
        assert hi <= t.func_end, t.name  # window stays before the literal pool / function end


def test_message_context_literal():
    need_image()
    off = ms.literal_offset(IMAGE, 0x3769D8, ms.GETSTATE_LITERAL_INSN)
    assert off == ms.MSG_CTX_LITERAL_OFFSET == 0x5C
    word = struct.unpack_from("<I", IMAGE, 0x3769D8 - ms.BASE + off)[0]
    assert word == ms.MSG_CTX_ADDR


def test_halfword_pc_relative_loads_masked():
    # ldrh/ldrsb/ldrsh/ldrd [pc,#0x12] (imm split hi=1, lo=2), AL and EQ forms
    for w in (0xE1DF01B2, 0xE1DF01D2, 0xE1DF01F2, 0xE1CF01D2, 0x01DF01B2, 0xE15F01B2):
        assert ms.classify(w) == "literal_h", hex(w)
        m = ms.word_mask(w)
        assert m & 0x00000F0F == 0, hex(w)          # both imm nibbles masked
        assert m & 0xFFFF00F0 == 0xFFFF00F0, hex(w)  # opcode/regs/SH kept
    # register-offset halfword load and plain ldrh r0,[r1] must not be masked
    assert ms.word_mask(0xE19100B2) == 0xFFFFFFFF
    assert ms.word_mask(0xE1D100B0) == 0xFFFFFFFF


def test_branch_and_literal_classifier():
    assert ms.classify(0xEB001234) == "branch"         # bl
    assert ms.classify(0x0A000010) == "branch"         # beq
    assert ms.word_mask(0xEB001234) == 0xFF000000
    assert ms.classify(0xE59F22AC) == "literal"        # ldr r2,[pc,#0x2ac]
    assert ms.word_mask(0xE59F22AC) == 0xFFFFF000
    assert ms.word_mask(0xE59F22AC, keep_imm=True) == 0xFFFFFFFF
    assert ms.pc_literal_target(0xE59F22AC, 0x41E968) == 0x41E968 + 8 + 0x2AC
    assert ms.classify(0xE5950000) is None             # ldr r0,[r5]
    assert ms.classify(0xE92D4030) is None             # push {r4,r5,lr}


def test_classifier_matches_capstone_when_available():
    need_image()
    try:
        from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM
    except ImportError:
        raise Skip("capstone not installed (python -I drops user site-packages)")
    md = Cs(CS_ARCH_ARM, CS_MODE_ARM)
    for t in ms.TARGETS:
        for i in range(24):
            a = t.addr + 4 * i
            raw = IMAGE[a - ms.BASE:a - ms.BASE + 4]
            w = struct.unpack("<I", raw)[0]
            ins = next(md.disasm(raw, a), None)
            if ins is None:
                continue
            cap = ins.mnemonic.startswith("b") and ins.mnemonic[:2] in ("b", "bl", "be", "bn", "bg", "bl", "bh", "bc", "bm", "bp", "bv", "bi", "bs", "bt", "bx") and ins.op_str.startswith("#")
            mine = ms.classify(w) == "branch"
            if cap:
                assert mine, hex(a)


def test_generated_header_equals_src_sigs_h():
    need_image()
    want = open(SIGS_H, "rb").read().decode("ascii").replace("\r", "")
    assert ms.c_header_text(IMAGE) == want


if __name__ == "__main__":
    fails = skips = 0
    for name, fn in sorted(globals().items()):
        if name.startswith("test_") and callable(fn):
            try:
                fn()
                print("PASS", name)
            except Skip as s:
                skips += 1
                print("SKIP", name, "-", s)
            except Exception as e:  # noqa: BLE001
                fails += 1
                print("FAIL", name, type(e).__name__, e)
    print("%d failed, %d skipped" % (fails, skips))
    sys.exit(1 if fails else 0)
