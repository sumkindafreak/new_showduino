"""Paint dark panels over baked-in dynamic text on themed BMP backgrounds."""
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
THEME = ROOT / "sd_card" / "ui" / "themes" / "default"
BLANK = (6, 10, 14)

COMMON = [(648, 8, 146, 26)]
DASHBOARD = [
    (218, 46, 204, 20),
    (226, 116, 284, 24),
    (226, 140, 284, 32),
    (226, 284, 172, 22),
    (416, 284, 172, 22),
    (606, 284, 172, 22),
    (606, 306, 172, 22),
    (606, 326, 172, 22),
    (606, 346, 172, 42),
]
LIVE_EXTRA = [(226, 106, 172, 22), (398, 106, 80, 22), (486, 106, 82, 22)]
MODAL = [(18, 450, 154, 22), (14, 450, 772, 26), (218, 70, 560, 22)]

PAGE_REGIONS = {
    "desktop.bmp": COMMON + DASHBOARD,
    "live.bmp": COMMON + DASHBOARD + LIVE_EXTRA,
    "diagnostics.bmp": COMMON + DASHBOARD,
    "nodes.bmp": COMMON + DASHBOARD,
    "complete.bmp": COMMON + MODAL,
    "locked.bmp": COMMON + MODAL,
    "unlock.bmp": COMMON + MODAL,
    "connection_lost.bmp": COMMON + MODAL,
    "no_network.bmp": COMMON + MODAL,
    "no_sd.bmp": COMMON + MODAL,
    "reboot.bmp": COMMON + MODAL,
    "firmware_update.bmp": COMMON + MODAL,
    "backup.bmp": COMMON + MODAL,
    "recovery.bmp": COMMON + MODAL,
    "discovery.bmp": COMMON + MODAL,
}

def blank_bmp(path, regions):
    img = Image.open(path).convert("RGB")
    draw = ImageDraw.Draw(img)
    for x, y, w, h in regions:
        draw.rectangle([x, y, x + w - 1, y + h - 1], fill=BLANK)
    img.save(path, "BMP")
    print("blanked %s (%d regions)" % (path.name, len(regions)))

def main():
    if not THEME.is_dir():
        raise SystemExit("theme folder missing: %s" % THEME)
    for name, regions in PAGE_REGIONS.items():
        bmp = THEME / name
        if not bmp.is_file():
            print("skip missing %s" % name)
            continue
        blank_bmp(bmp, regions)
    print("done")

if __name__ == "__main__":
    main()