# dvui-essence

Sada malých projektů v C23, které věrně převádějí jednotlivé části knihovny
[DVUI](https://github.com/david-vanderson/dvui) (immediate-mode GUI v jazyce Zig).

| Adresář | Obsah |
|---|---|
| `dvui-original/` | neměnná kopie původního DVUI (commit `2efa35e`, 12. 11. 2026) |
| [`dvui-layout/`](dvui-layout/README.md) | layout widgetů nad SDL 3 GPU |
| [`dvui-animation/`](dvui-animation/README.md) | animace, easing, časovače a AnimateWidget nad SDL 3 GPU |
| [`shared/`](shared/README.md) | kód sdílený projekty: základní typy, geometrie, datové úložiště, backendy SDL 3 GPU a testovací |

Pravidla práce jsou v [AGENTS.md](AGENTS.md), plán v [PLAN.md](PLAN.md).
