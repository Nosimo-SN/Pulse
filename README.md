# Pulse

Timers for your own [SkyrimNet](https://fateless.ai) triggers.

SkyrimNet triggers react to events, but there is no event for "every so often". Pulse adds one: a small SKSE
plugin that sends a mod event on a timer, which a trigger you build in the dashboard can match like any other
`mod_event`. Pulse decides *when*; your trigger decides everything else.

| Part | What it does |
|---|---|
| **SKSE plugin** (`Pulse.dll`) | Runs the timers and sends the events. No Papyrus scripts, no game plugin. |
| **SkyrimNet plugin** (`nosimo.pulse`) | The Settings page where you switch timers on and set their times. |

## Requirements

- Skyrim SE / AE (1.5.97 and 1.6.x) with [SKSE](https://skse.silverlock.org/)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
- SkyrimNet Beta 28 or later

## The timers

All timers start **off**. Switch on the ones you need on Pulse's Settings page in the SkyrimNet dashboard.

| Timer | `event_name` | Fires | `num_arg` | `str_arg` |
|---|---|---|---|---|
| Fixed 1–4 | `Pulse_Fixed1` … `Pulse_Fixed4` | every *interval* seconds (1–1200) | the interval | `fixed` |
| Random 1–4 | `Pulse_Random1` … `Pulse_Random4` | after a random delay between *min* and *max* (1–1200), drawn again after every firing | the delay that just passed | `random` |

`sender_name` / `sender_form_id` are the player (`00000014`).

- Time counts only while you are playing: not in the main menu, loading screens, or menus that pause the game
  (inventory, map, journal, console). Dialogue and barter don't pause the game, so they count.
- Every timer starts counting again from zero when a save loads or a new game starts.
- A timer that is switched off sends nothing, and starts from zero when switched back on.
- Changes apply within about five seconds, no restart needed. A shorter interval than the time already
  counted fires at once.
- A random min above its max is swapped; set them equal for a fixed delay.

Pulse itself posts nothing to SkyrimNet. What NPCs hear about is your trigger's response: `player_thought`
and `npc_thought` stay private, while `direct_narration` and `persistent_generic` enter the event history.

## Example trigger

A random thought every two to ten minutes (Random 3, min 120, max 600):

```yaml
name: "pulse_idle_thought"
description: "Now and then, the player has a passing thought"

eventCriteria:
  eventType: "mod_event"
  schemaConditions:
    - fieldPath: "event_name"
      operator: "equals"
      value: "Pulse_Random3"

response:
  type: "player_thought"
  content: "A passing thought about what is happening around {{ player_name }} in {{ location }}."

audience: "player"
enabled: true
probability: 1.0
cooldownSeconds: 0
priority: 1
```

Use the trigger's own `probability` and `cooldownSeconds` on top of the timer to thin it out further. Several
triggers can share one timer.

## Troubleshooting

`Pulse.log` sits next to `skse64.log` (`Documents/My Games/Skyrim Special Edition/SKSE`). It lists each
timer's settings at startup; switch on **Log Every Event** (Debug) to log every event sent. SkyrimNet's MCP
`get_monitored_events` with `event_type: mod_event` shows the events as SkyrimNet receives them.

## Building

CMake + vcpkg + Ninja with VS 2022, against CommonLibSSE-NG (alandtse `ng`):

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake -DCOMMONLIB_DIR=<commonlibsse-ng>
cmake --build build
python tools/gen_settings.py . 1.0.0
python tools/package.py . 1.0.0
```

## License

GPL-3.0-or-later. See `LICENSE` and `THIRD_PARTY_NOTICES.md`.
