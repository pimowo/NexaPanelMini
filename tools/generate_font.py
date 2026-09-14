from io import BytesIO
from pathlib import Path
from urllib.request import urlopen

from PIL import Image, ImageDraw, ImageFont


FONT_URL = (
    "https://raw.githubusercontent.com/google/fonts/main/ofl/notosans/"
    "NotoSans%5Bwdth%2Cwght%5D.ttf"
)
FONT_SIZES = (15, 24)
POLISH_LETTERS = "ĄĆĘŁŃÓŚŹŻąćęłńóśźż"
CHARACTERS = "".join(chr(code) for code in range(32, 127)) + "°" + POLISH_LETTERS
OUTPUT = Path(__file__).parents[1] / "include" / "display" / "NotoSansPl.h"


def format_bytes(values, indent="    ", width=16):
    lines = []
    for index in range(0, len(values), width):
        chunk = ", ".join(f"0x{value:02X}" for value in values[index:index + width])
        lines.append(f"{indent}{chunk},")
    return "\n".join(lines)


def build_font(ttf_data, size):
    font = ImageFont.truetype(BytesIO(ttf_data), size=size)
    glyphs = []
    bitmap = []
    top = 0
    bottom = 0

    for character in sorted(set(CHARACTERS), key=ord):
        left, glyph_top, right, glyph_bottom = font.getbbox(character, anchor="ls")
        width = max(0, right - left)
        height = max(0, glyph_bottom - glyph_top)
        offset = len(bitmap)

        if width and height:
            image = Image.new("L", (width, height))
            draw = ImageDraw.Draw(image)
            draw.text((-left, -glyph_top), character, font=font, fill=255,
                      anchor="ls")
            bitmap.extend(image.get_flattened_data())

        advance = round(font.getlength(character))
        glyphs.append((ord(character), offset, width, height, advance,
                       left, glyph_top))
        top = min(top, glyph_top)
        bottom = max(bottom, glyph_bottom)

    return glyphs, bitmap, -top, bottom


def int32(value):
    return value.to_bytes(4, byteorder="big", signed=True)


def build_vlw(ttf_data, size):
    glyphs, bitmap, ascent, descent = build_font(ttf_data, size)
    data = bytearray()
    for value in (len(glyphs), 11, size, 0, ascent, descent):
        data.extend(int32(value))
    for code_point, _, width, height, advance, x_offset, y_offset in glyphs:
        for value in (code_point, height, width, advance, -y_offset,
                      x_offset, 0):
            data.extend(int32(value))
    data.extend(bitmap)
    name = f"NotoSansPl{size}".encode("ascii")
    data.extend((len(name),))
    data.extend(name + b"\0")
    data.extend((len(name),))
    data.extend(name + b"\0\1")
    return data


def main():
    with urlopen(FONT_URL, timeout=30) as response:
        ttf_data = response.read()

    output = [
        "#pragma once",
        "#include <Arduino.h>",
        "",
        "// Generated from Noto Sans (OFL-1.1) by tools/generate_font.py.",
        "",
    ]

    for size in FONT_SIZES:
        data = build_vlw(ttf_data, size)
        output.extend([
            f"const uint8_t NotoSansPl{size}[] PROGMEM = {{",
            format_bytes(data),
            "};",
            "",
        ])

    OUTPUT.write_text("\n".join(output) + "\n", encoding="ascii")
    print(f"Generated {OUTPUT} ({OUTPUT.stat().st_size} bytes)")


if __name__ == "__main__":
    main()