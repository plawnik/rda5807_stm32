"""Compare real C framebuffer captures with the original designer export."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
project = json.loads((ROOT / "assets/menu/radio-menu.json").read_text())
regions = {r["name"]: r for r in project["home"]["regions"]}
assets = {a["id"]: a for a in project["assets"]}
fonts = {f["id"]: f for f in project["fonts"]}


def capture(name):
    data = (ROOT / f"build/tests/{name}.bin").read_bytes()
    assert len(data) == 504
    return [[int(bool(data[(y // 8) * 84 + x] & (1 << (y % 8))))
             for x in range(84)] for y in range(48)]


def crop(image, name):
    r = regions[name]
    return [row[r["x"]:r["x"] + r["width"]]
            for row in image[r["y"]:r["y"] + r["height"]]]


def graphic(image, name, index):
    r = regions[name]
    a = assets[r["graphics"][index]["assetId"]]
    rows = a["frames"][0]["bitmap"]["rows"]
    expected = [[int(rows[y * a["height"] // r["height"]][x * a["width"] // r["width"]])
                 for x in range(r["width"])] for y in range(r["height"])]
    assert crop(image, name) == expected, f"{name}: pixels differ from uploaded JSON"


def text(image, name, string, font_id, alignment="left", scroll_step=0):
    """Independent reference renderer for exact glyphs, clipping and five-space loop."""
    r, f = regions[name], fonts[font_id]
    advances = [2 if font_id == "font-3x4" and c in ".:" else f["width"] + f["spacing"]
                for c in string]
    width = sum(advances) - f["spacing"] if string else 0
    expected = [[0] * r["width"] for _ in range(r["height"])]
    x = max(0, (r["width"] - width) // 2) if alignment == "center" else 0
    if alignment == "right": x = max(0, r["width"] - width)
    if scroll_step and width > r["width"]:
        cycle = string + " " * 5
        start = scroll_step % len(cycle)
        string = cycle[start:] + cycle[:start]
    for c in string:
        glyph = f["glyphs"].get(c, f["glyphs"].get(c.upper(), f["glyphs"]["?"]))
        shift = 1 if font_id == "font-3x4" and c in ".:" else 0
        for yy, row in enumerate(glyph["rows"]):
            for xx, value in enumerate(row):
                dx = x + xx - shift
                if value == "1" and 0 <= dx < r["width"] and yy < r["height"]:
                    expected[yy][dx] = 1
        x += 2 if shift else f["width"] + f["spacing"]
        if x >= r["width"]: break
    assert crop(image, name) == expected, f"{name}: text differs from JSON font/position"


home = capture("home-rds")
for name, variant in [("Siła sygnału FM", 8), ("Poziom głośności", 8),
                      ("RDS status", 0), ("Bass boost", 0), ("Mono/stereo", 0),
                      ("Signal_logo", 0), ("Ikona glosnika", 0), ("Tryb_przełączania", 1)]:
    graphic(home, name, variant)
text(home, "Zegar", "13:47", "font-3x4", "center")
text(home, "Częstotliwość", "106.1MHz", "font-5x7", "center")
text(home, "Siła sygnału", "99", "font-3x4", "center")
text(home, "Poziom glosnosci", "15", "font-3x4", "center")
text(home, "Nazwa stacji PS", "RADIO123", "font-3x4")
text(home, "Rodzaj programu PTY", "ROCK", "font-3x4", "right")
station = "RADIO ULUBIONA STACJA Z PAMIECI"
radio_text = "ARTYSTA I TYTUL UTWORU PRZEWIJANY Z ODSTEPEM"
text(home, "Nazwa zapisanej stacji", station, "font-3x4", "center")
text(home, "Radio text", radio_text, "font-3x4")
text(home, "Radio tekst plus", radio_text, "font-3x4")
scrolled = capture("home-scroll")
text(scrolled, "Radio text", radio_text, "font-3x4")
text(scrolled, "Radio tekst plus", radio_text, "font-3x4", scroll_step=6)
text(scrolled, "Nazwa zapisanej stacji", station, "font-3x4", "center", scroll_step=6)
text(capture("home-loop-gap"), "Radio tekst plus", radio_text, "font-3x4", scroll_step=len(radio_text))
text(capture("home-loop-restart"), "Radio tekst plus", radio_text, "font-3x4", scroll_step=len(radio_text) + 5)
logo = capture("home-logo")
assert not any(any(row) for row in crop(logo, "RDS status"))
for name, variant in [("Bass boost", 1), ("Mono/stereo", 1), ("Signal_logo", 1), ("Ikona glosnika", 1)]:
    graphic(logo, name, variant)
text(capture("home-integer-frequency"), "Częstotliwość", "100MHz", "font-5x7", "center")
text(capture("home-fraction-frequency"), "Częstotliwość", "97.55MHz", "font-5x7", "center")
text(capture("home-short-name"), "Nazwa zapisanej stacji", "FM", "font-3x4", "center")
print("Framebuffer pixels match uploaded JSON: assets, fonts, fields, scrolling and frequency format.")
