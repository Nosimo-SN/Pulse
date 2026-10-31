# Third-party notices

Pulse (`Pulse.dll`) is licensed under the GNU General Public License v3.0 or later (see `LICENSE`). The DLL
statically links the components below; their license terms follow.

| Component | License |
|---|---|
| CommonLibSSE-NG (alandtse `ng`) | GPL-3.0-or-later with Modding Exception and GPL-3.0 Linking Exception |
| spdlog | MIT |
| {fmt} | MIT (with an exception for embedded object code) |
| DirectXTK | MIT |
| rapidcsv | BSD-3-Clause |
| xbyak | BSD-3-Clause |

rapidcsv and xbyak are build dependencies of CommonLibSSE-NG and are listed for completeness.

The source repository also carries SkyrimNet's plugin API headers (`include/SkyrimNet/`, MIT, Fateless.Ai;
licence in `include/SkyrimNet/LICENSE`). They are compiled into the DLL as declarations only.

Not included or redistributed: SkyrimNet itself, SKSE, and Skyrim.
