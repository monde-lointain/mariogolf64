"""Encode the F3DEX2 display list commands that character models use.

Each function returns a command's two words, or a list of them for a macro
that expands to several, as the gs* macros of the SDK's gbi.h build them.
to_bytes() writes them big-endian, as the RSP reads them.
"""

import struct
from typing import NamedTuple

Command = tuple[int, int]


class TileWrap(NamedTuple):
    """How a tile repeats in S and T: G_TX_WRAP or G_TX_CLAMP, and coordinate masks."""

    mode_s: int
    mode_t: int
    mask_s: int
    mask_t: int


OPCODE_SHIFT = 24

G_VTX = 0x01
G_TRI1 = 0x05
G_TRI2 = 0x06
G_TEXTURE = 0xD7
G_POPMTX = 0xD8
G_GEOMETRYMODE = 0xD9
G_MTX = 0xDA
G_DL = 0xDE
G_ENDDL = 0xDF
G_SETOTHERMODE_H = 0xE3
G_RDPLOADSYNC = 0xE6
G_RDPPIPESYNC = 0xE7
G_SETTILESIZE = 0xF2
G_LOADBLOCK = 0xF3
G_SETTILE = 0xF5
G_SETPRIMCOLOR = 0xFA
G_SETCOMBINE = 0xFC
G_SETTIMG = 0xFD

# Geometry mode bits
G_ZBUFFER = 0x00000001
G_SHADE = 0x00000004
G_CULL_FRONT = 0x00000200
G_CULL_BACK = 0x00000400
G_CULL_BOTH = G_CULL_FRONT | G_CULL_BACK
G_FOG = 0x00010000
G_LIGHTING = 0x00020000
G_SHADING_SMOOTH = 0x00200000
GEOMETRY_MODE_MASK = 0x00FFFFFF

# gsSPMatrix's parameters; the command stores them with PUSH inverted
G_MTX_PUSH = 0x01
G_MTX_SIZE_FIELD = 0x380000
MATRIX_SIZE = 0x40
G_DL_PUSH = 0x00
G_DL_NOPUSH = 0x01

# Other mode fields: each one's shift and width
G_MDSFT_TEXTLUT = 14
G_MDSFT_CYCLETYPE = 20
OTHER_MODE_FIELD_WIDTH = 2
OTHER_MODE_BITS = 32
G_CYC_1CYCLE = 0 << G_MDSFT_CYCLETYPE
G_TT_NONE = 0 << G_MDSFT_TEXTLUT

# Texture image formats and texel sizes
G_IM_FMT_RGBA = 0
G_IM_SIZ_16B = 2
G_IM_SIZ_16B_BYTES = 2
# A load block counts texels in units of 64 bits
LOAD_WORD_BYTES = 8
G_TX_LOADTILE = 7
G_TX_RENDERTILE = 0
G_TX_WRAP = 0
G_TX_CLAMP = 2
G_TX_DXT_FRAC = 11
G_TEXTURE_IMAGE_FRAC = 2
G_TX_LDBLK_MAX_TXL = 2047
G_ON = 1
G_OFF = 0

# Combiner inputs. The color's a, b, c and d and the alpha's take different
# numbers for zero.
G_CCMUX_TEXEL0 = 1
G_CCMUX_PRIMITIVE = 3
G_CCMUX_SHADE = 4
G_CCMUX_0_AB = 15
G_CCMUX_0_D = 7
G_ACMUX_TEXEL0 = 1
G_ACMUX_SHADE = 4
G_ACMUX_ENVIRONMENT = 5
G_ACMUX_0 = 7

# A triangle's vertex indices are stored doubled
TRIANGLE_INDEX_SCALE = 2
VERTEX_BUFFER_SIZE = 32


def _field(value: int, shift: int, width: int) -> int:
    return (value & ((1 << width) - 1)) << shift


def _opcode(opcode: int) -> int:
    return opcode << OPCODE_SHIFT


def to_bytes(commands: list[Command]) -> bytes:
    """Write commands as the RSP reads them."""
    return b"".join(struct.pack(">II", word0, word1) for word0, word1 in commands)


def vertex(address: int, count: int, first: int) -> Command:
    """gsSPVertex: load COUNT vertices into the buffer from index FIRST."""
    if not 0 < count <= VERTEX_BUFFER_SIZE - first:
        msg = f"{count} vertices from index {first} overflow the vertex buffer"
        raise ValueError(msg)
    return (
        _opcode(G_VTX) | _field(count, 12, 8) | _field((first + count) * 2, 0, 8),
        address,
    )


def _triangle_word(a: int, b: int, c: int) -> int:
    return (
        _field(a * TRIANGLE_INDEX_SCALE, 16, 8)
        | _field(b * TRIANGLE_INDEX_SCALE, 8, 8)
        | _field(c * TRIANGLE_INDEX_SCALE, 0, 8)
    )


def triangle(a: int, b: int, c: int) -> Command:
    """gsSP1Triangle with flag 0."""
    return (_opcode(G_TRI1) | _triangle_word(a, b, c), 0)


def triangles(first: tuple[int, int, int], second: tuple[int, int, int]) -> Command:
    """gsSP2Triangles with flags 0."""
    return (_opcode(G_TRI2) | _triangle_word(*first), _triangle_word(*second))


def texture(scale_s: int, scale_t: int, tile: int, *, on: bool) -> Command:
    """GsSPTexture with level 0; the scales are 0.16 fixed point."""
    return (
        _opcode(G_TEXTURE) | _field(tile, 8, 3) | _field(G_ON if on else G_OFF, 1, 7),
        _field(scale_s, 16, 16) | _field(scale_t, 0, 16),
    )


def geometry_mode(clear: int, set_bits: int) -> Command:
    """gsSPGeometryMode: clear some bits, then set others."""
    return (_opcode(G_GEOMETRYMODE) | (~clear & GEOMETRY_MODE_MASK), set_bits)


def matrix(address: int, parameters: int) -> Command:
    """GsSPMatrix."""
    return (
        _opcode(G_MTX) | G_MTX_SIZE_FIELD | ((parameters ^ G_MTX_PUSH) & 0xFF),
        address,
    )


def pop_matrix(count: int = 1) -> Command:
    """GsSPPopMatrixN for the modelview stack."""
    return (_opcode(G_POPMTX) | G_MTX_SIZE_FIELD | 2, count * MATRIX_SIZE)


def call(address: int) -> Command:
    """GsSPDisplayList."""
    return (_opcode(G_DL) | _field(G_DL_PUSH, 16, 8), address)


def end() -> Command:
    """GsSPEndDisplayList."""
    return (_opcode(G_ENDDL), 0)


def _other_mode_h(shift: int, value: int) -> Command:
    return (
        _opcode(G_SETOTHERMODE_H)
        | _field(OTHER_MODE_BITS - shift - OTHER_MODE_FIELD_WIDTH, 8, 8)
        | _field(OTHER_MODE_FIELD_WIDTH - 1, 0, 8),
        value,
    )


def cycle_type(mode: int) -> Command:
    """GsDPSetCycleType."""
    return _other_mode_h(G_MDSFT_CYCLETYPE, mode)


def texture_lut(mode: int) -> Command:
    """GsDPSetTextureLUT."""
    return _other_mode_h(G_MDSFT_TEXTLUT, mode)


def pipe_sync() -> Command:
    """GsDPPipeSync."""
    return (_opcode(G_RDPPIPESYNC), 0)


def load_sync() -> Command:
    """GsDPLoadSync."""
    return (_opcode(G_RDPLOADSYNC), 0)


def prim_color(red: int, green: int, blue: int, alpha: int) -> Command:
    """GsDPSetPrimColor with minimum level and fraction 0."""
    return (
        _opcode(G_SETPRIMCOLOR),
        _field(red, 24, 8)
        | _field(green, 16, 8)
        | _field(blue, 8, 8)
        | _field(alpha, 0, 8),
    )


def combine(
    color: tuple[int, int, int, int], alpha: tuple[int, int, int, int]
) -> Command:
    """GsDPSetCombineLERP with the same (a - b) * c + d for both cycles."""
    a, b, c, d = color
    alpha_a, alpha_b, alpha_c, alpha_d = alpha
    word0 = (
        _opcode(G_SETCOMBINE)
        | _field(a, 20, 4)
        | _field(c, 15, 5)
        | _field(alpha_a, 12, 3)
        | _field(alpha_c, 9, 3)
        | _field(a, 5, 4)
        | _field(c, 0, 5)
    )
    word1 = (
        _field(b, 28, 4)
        | _field(b, 24, 4)
        | _field(alpha_a, 21, 3)
        | _field(alpha_c, 18, 3)
        | _field(d, 15, 3)
        | _field(alpha_b, 12, 3)
        | _field(alpha_d, 9, 3)
        | _field(d, 6, 3)
        | _field(alpha_b, 3, 3)
        | _field(alpha_d, 0, 3)
    )
    return (word0, word1)


# The character models' two combines: a material's color, and a texture's,
# each lit by the shade and faded by the environment alpha
COMBINE_PRIMITIVE_SHADE = combine(
    (G_CCMUX_PRIMITIVE, G_CCMUX_0_AB, G_CCMUX_SHADE, G_CCMUX_0_D),
    (G_ACMUX_ENVIRONMENT, G_ACMUX_0, G_ACMUX_SHADE, G_ACMUX_0),
)
COMBINE_TEXEL_SHADE = combine(
    (G_CCMUX_TEXEL0, G_CCMUX_0_AB, G_CCMUX_SHADE, G_CCMUX_0_D),
    (G_ACMUX_ENVIRONMENT, G_ACMUX_0, G_ACMUX_TEXEL0, G_ACMUX_0),
)


def _set_tile(size: int, line: int, tile: int, wrap: TileWrap) -> Command:
    return (
        _opcode(G_SETTILE)
        | _field(G_IM_FMT_RGBA, 21, 3)
        | _field(size, 19, 2)
        | _field(line, 9, 9),
        _field(tile, 24, 3)
        | _field(wrap.mode_t, 18, 2)
        | _field(wrap.mask_t, 14, 4)
        | _field(wrap.mode_s, 8, 2)
        | _field(wrap.mask_s, 4, 4),
    )


def load_rgba16_block(
    address: int, width: int, height: int, wrap: TileWrap
) -> list[Command]:
    """GsDPLoadTextureBlock for an RGBA16 texture, with no shift or LOD."""
    texel_count = width * height
    if texel_count - 1 > G_TX_LDBLK_MAX_TXL:
        msg = f"a {width}x{height} texture is too big to load as one block"
        raise ValueError(msg)
    words_per_line = max(1, width * G_IM_SIZ_16B_BYTES // LOAD_WORD_BYTES)
    dxt = ((1 << G_TX_DXT_FRAC) + words_per_line - 1) // words_per_line
    line = (width * G_IM_SIZ_16B_BYTES + LOAD_WORD_BYTES - 1) // LOAD_WORD_BYTES
    return [
        (
            _opcode(G_SETTIMG)
            | _field(G_IM_FMT_RGBA, 21, 3)
            | _field(G_IM_SIZ_16B, 19, 2),
            address,
        ),
        _set_tile(G_IM_SIZ_16B, 0, G_TX_LOADTILE, wrap),
        load_sync(),
        (
            _opcode(G_LOADBLOCK),
            _field(G_TX_LOADTILE, 24, 3)
            | _field(texel_count - 1, 12, 12)
            | _field(dxt, 0, 12),
        ),
        pipe_sync(),
        _set_tile(G_IM_SIZ_16B, line, G_TX_RENDERTILE, wrap),
        (
            _opcode(G_SETTILESIZE),
            _field(G_TX_RENDERTILE, 24, 3)
            | _field((width - 1) << G_TEXTURE_IMAGE_FRAC, 12, 12)
            | _field((height - 1) << G_TEXTURE_IMAGE_FRAC, 0, 12),
        ),
    ]
