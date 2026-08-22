"""Director theme helper.

Theme BMP backgrounds are retired. This writes a LVGL-only theme.json.
"""
import json
from pathlib import Path

ROOT = Path(r"c:\Users\tjpro\Downloads\new_showduino-main\new_showduino-main")
DST = ROOT / "sd_card" / "ui" / "themes" / "default"
DST.mkdir(parents=True, exist_ok=True)
theme = {
    "version": 1,
    "name": "Director HUD",
    "resolution": "800x480",
    "backgroundImages": False,
    "pages": {},
}
(DST / "theme.json").write_text(json.dumps(theme, indent=2) + "\n", encoding="utf-8")
print("wrote", DST / "theme.json")
