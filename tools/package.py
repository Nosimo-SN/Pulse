# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Nosimo-SN. Part of Pulse.
"""Builds the Pulse mod zip.

Usage: python -I package.py <project root> <version>
Writes build/dist/Pulse-<version>.zip:
  SKSE/Plugins/Pulse.dll (+ licence files)
  SKSE/Plugins/SkyrimNet/external/nosimo.pulse/...   (SkyrimNet plugin: manifest + settings page)
"""
import sys
import zipfile
from pathlib import Path

root = Path(sys.argv[1])
version = sys.argv[2]
dist = root / "build" / "dist"
dist.mkdir(parents=True, exist_ok=True)
out = dist / f"Pulse-{version}.zip"

files = {
    "SKSE/Plugins/Pulse.dll": root / "build/Pulse.dll",
    "SKSE/Plugins/Pulse_LICENSE.txt": root / "LICENSE",
    "SKSE/Plugins/Pulse_THIRD_PARTY_NOTICES.md": root / "THIRD_PARTY_NOTICES.md",
}
plugin_dir = root / "skyrimnet/nosimo.pulse"
for f in plugin_dir.rglob("*"):
    if f.is_file():
        files["SKSE/Plugins/SkyrimNet/external/nosimo.pulse/" + f.relative_to(plugin_dir).as_posix()] = f

with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    for arc, src in sorted(files.items()):
        z.write(src, arc)

print(f"{out}: {len(files)} files")
