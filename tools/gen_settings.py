# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Nosimo-SN. Part of Pulse.
"""Writes the Pulse settings schema (the plugin's Settings page in the SkyrimNet dashboard).

Usage: python -I gen_settings.py <project root> <version>
Writes skyrimnet/nosimo.pulse/settings/Pulse.yaml.
"""
import sys
from pathlib import Path

root = Path(sys.argv[1])
version = sys.argv[2]
out = root / "skyrimnet" / "nosimo.pulse" / "settings" / "Pulse.yaml"

FIXED_DEFAULTS = [30, 60, 300, 600]
RANDOM_DEFAULTS = [(30, 90), (60, 180), (120, 600), (300, 1200)]

lines = [
    "plugin:",
    '  name: "Pulse"',
    f'  version: "{version}"',
    "  description: >-",
    "    Timers for your own SkyrimNet triggers. Each timer you switch on sends a mod event when it fires:",
    "    Pulse_Fixed1 to Pulse_Fixed4 every set number of seconds, Pulse_Random1 to Pulse_Random4 after a new",
    "    random delay between a min and a max each time. In the trigger editor, use event type mod_event and",
    "    match event_name (num_arg holds the seconds counted). Pulse itself posts nothing to SkyrimNet; what",
    "    people hear about is your trigger's response. Time counts only while you play, not in menus or loading screens.",
    "    Changes apply within about five seconds, no restart needed.",
    '  icon: "⏱️"',
    "",
    "schema:",
    "  fields:",
]


def field(name, path, ftype, description, default, category, lo=None, hi=None):
    lines.append(f'    - name: "{name}"')
    lines.append(f'      path: "{path}"')
    lines.append(f'      type: "{ftype}"')
    lines.append(f'      description: "{description}"')
    lines.append(f"      defaultValue: {default}")
    if lo is not None:
        lines.append(f"      min: {lo}")
        lines.append(f"      max: {hi}")
    lines.append(f'      category: "{category}"')
    lines.append("")


lines.append("    # --- Fixed timers ----------------------------------------------------------------------------")
for i, interval in enumerate(FIXED_DEFAULTS, 1):
    field(f"Fixed {i}", f"fixed{i}.enabled", "bool",
          f"Sends the mod event Pulse_Fixed{i} every interval below. num_arg = the interval, str_arg = fixed.",
          "false", "Fixed Timers")
    field(f"Fixed {i} Interval (seconds)", f"fixed{i}.interval", "int",
          f"Seconds between Pulse_Fixed{i} events, 1 to 1200.",
          interval, "Fixed Timers", 1, 1200)

lines.append("    # --- Random timers ---------------------------------------------------------------------------")
for i, (lo, hi) in enumerate(RANDOM_DEFAULTS, 1):
    field(f"Random {i}", f"random{i}.enabled", "bool",
          f"Sends the mod event Pulse_Random{i} after a random delay between the min and max below, drawn again after every event. num_arg = the delay that just passed, str_arg = random.",
          "false", "Random Timers")
    field(f"Random {i} Min (seconds)", f"random{i}.min", "int",
          f"Shortest delay before Pulse_Random{i}, 1 to 1200. If it is above the max, the two are swapped.",
          lo, "Random Timers", 1, 1200)
    field(f"Random {i} Max (seconds)", f"random{i}.max", "int",
          f"Longest delay before Pulse_Random{i}, 1 to 1200. Set it equal to the min for a fixed delay.",
          hi, "Random Timers", 1, 1200)

lines.append("    # --- Debug -----------------------------------------------------------------------------------")
field("Log Every Event", "debug.log_fires", "bool",
      "Write each event Pulse sends to Pulse.log (next to skse64.log).",
      "false", "Debug")

out.parent.mkdir(parents=True, exist_ok=True)
out.write_text("\n".join(lines).rstrip() + "\n", encoding="utf-8")
print(f"wrote {out}")
