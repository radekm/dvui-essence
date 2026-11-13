# PLAN.md – plán projektu dvui-essence

## Cíl repozitáře

Postupně vytvořit sadu malých, čitelných projektů v C23, které věrně převádějí
jednotlivé části knihovny [DVUI](https://github.com/david-vanderson/dvui)
(immediate-mode GUI v jazyce Zig). Každý projekt izoluje jednu „esenci“ DVUI:

1. **dvui-layout** – layout widgetů (tento plán, viz níže).
2. dvui-text – vykreslování textu (později).
3. dvui-text-selection – výběr textu (později).
4. dvui-text-edit – editace textu (později).

## Původní DVUI

- Uloženo v `dvui-original/` jako obyčejné soubory.
- Zdroj: https://github.com/david-vanderson/dvui, větev `master`,
  commit `2efa35ee6459ca61b8e92787db4f1616d532f9b9` (12. 11. 2026), verze `0.6.0-dev`.

## Projekt dvui-layout

### Zadání

Ořezaná verze DVUI nad backendem **SDL 3 GPU**, která dělá layout widgetů.
Widgety jsou obdélníky s různými výplněmi a barvami okrajů (bez textu, ostré rohy).
Kód layoutu musí zůstat věrný DVUI.

Požadované widgety:

- **button** – bez textu, ale klikatelné (hover, press, click),
- **box** – horizontální i vertikální (včetně `equal_space`, expand, gravity),
- **scroll bar** (a scroll area / scroll container, bez nichž nedává smysl),
- **grid** – vícepruchodový layout (auto-size sloupců/řádků přes více snímků),
- **tooltip** (FloatingTooltipWidget),
- **floating window** (FloatingWindowWidget – přesun, změna velikosti, pořadí).

### Rozhodnutí (odsouhlasená vlastníkem)

- Shadery: převzaté předkompilované shadery DVUI (SPIR-V, MSL, DXIL).
- Rohy: jen ostré; `corner_radius` z Options se nepřenáší.
- Platformy: Linux, Windows, macOS.
- `dvui-original`: celý snapshot repozitáře bez `.git`.

### Architektura

```
dvui-layout/
  meson.build
  include/dvui/dvui.h          veřejné API (typy, Options, Window, widgety)
  include/dvui/backend_sdl3gpu.h
  src/
    core.c                     Id, Rect, Size, Point, Color, Options, placeIn
    data.c                     hash mapa pro min_sizes a datové úložiště (dataGet/dataSet)
    window.c                   Window.begin/end, události, subwindows, capture,
                               focus, dragging, refresh, renderovací fronta
    widget_data.c              WidgetData (init/register/minSize*, borderAndBackground)
    layout.c                   BasicLayout, placeOnScreen
    widgets/*.c                box, button, overlay, scale, spacer/separator,
                               scroll_info, scroll_bar, scroll_container,
                               scroll_area, grid, floating, floating_tooltip,
                               floating_window, window_header
    backends/sdl3gpu.c         backend nad SDL3 GPU + mapování událostí SDL
    backends/testing.c         backend bez GPU pro testy
  shaders/                     zkompilované shadery převzaté z DVUI
  tools/embed.c                malý nástroj (překládaný Mesonem) pro vložení
                               shaderů do C pole
  examples/demo.c              demo se všemi widgety
  tests/test_layout.c          testy layoutu (víc snímků, kontrola obdélníků)
```

Mapování DVUI → C:

| DVUI (Zig) | dvui-layout (C) |
|---|---|
| `Rect.zig`, `Size.zig`, `Point.zig`, `RectScale.zig` | `core.c` |
| `Options.zig` | `core.c` (`dvui_Options`, `dvui_options_override`) |
| `dvui.zig` – `placeIn`, `events`, `eventMatch`, `clicked`, `captureMouse`, `dataGet/Set`, `parentSet/Reset`, `refresh`, `hoverFade` | `window.c`, `core.c` |
| `Window.zig` | `window.c` |
| `Subwindows.zig`, `Dragging.zig` | `window.c` |
| `WidgetData.zig`, `Widget.zig` | `widget_data.c`, `dvui.h` (vtable) |
| `layout.zig` | `layout.c` |
| `widgets/BoxWidget.zig` | `widgets/box.c` |
| `widgets/ButtonWidget.zig` | `widgets/button.c` |
| `widgets/ScrollAreaWidget.zig`, `ScrollContainerWidget.zig`, `ScrollBarWidget.zig`, `ScrollInfo.zig` | `widgets/scroll_*.c` |
| `widgets/GridWidget.zig` | `widgets/grid.c` |
| `widgets/FloatingWidget.zig`, `FloatingTooltipWidget.zig`, `FloatingWindowWidget.zig` | `widgets/floating*.c` |
| `widgets/OverlayWidget.zig`, `ScaleWidget.zig` | `widgets/overlay.c`, `widgets/scale.c` |
| `backends/sdl3gpu.zig` | `backends/sdl3gpu.c` |

### Fáze

- [x] Fáze 0: snapshot DVUI, AGENTS.md, PLAN.md.
- [ ] Fáze 1: jádro – geometrie, Options, Id, datové úložiště, Window,
      WidgetData, události, capture, subwindows, renderovací fronta.
- [ ] Fáze 2: backend SDL3 GPU + testovací backend.
- [ ] Fáze 3: widgety box, button, overlay, scale, spacer.
- [ ] Fáze 4: scroll info / bar / container / area.
- [ ] Fáze 5: floating, floating tooltip, floating window + window header.
- [ ] Fáze 6: grid (vícepruchodový layout).
- [ ] Fáze 7: demo, testy, dokumentace (README česky), ověření buildu.

### Co se z DVUI vědomě vynechává

Text a fonty, ikony, obrázky, textury (kromě bílé 1×1 pro shader), zaoblené rohy,
stíny, gradienty, ninepatch, AccessKit, dialogy a toasty, animace (kromě
`hoverFade`), dotykové události a setrvačné scrollování prstem, klávesová
navigace tabulátorem a v gridu, editace a řazení v gridu, témata kromě
jednoho výchozího (barvy podle Adwaita Light). Podrobnosti v
`dvui-layout/README.md`, sekce „Odchylky od DVUI“.
