# PLAN.md – plán projektu dvui-essence

## Cíl repozitáře

Postupně vytvořit sadu malých, čitelných projektů v C23, které věrně převádějí
jednotlivé části knihovny [DVUI](https://github.com/david-vanderson/dvui)
(immediate-mode GUI v jazyce Zig). Každý projekt izoluje jednu „esenci“ DVUI:

1. **dvui-layout** – layout widgetů (hotovo).
2. **dvui-animation** – animace, easing, časovače a AnimateWidget (hotovo).
3. dvui-text – vykreslování textu (později).
4. dvui-text-selection – výběr textu (později).
5. dvui-text-edit – editace textu (později).

Kód společný všem projektům je v `shared/` (viz níže a
[shared/README.md](shared/README.md)).

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
  meson.build                  používá soubory z ../shared
  include/dvui/dvui.h          veřejné API (Options, Window, widgety); vkládá dvui/base.h
  src/
    core.c                     Options, téma, placeIn, geometrie závislá na okně
    frame.c                    alokace v aréně snímku, seznamy příkazů kreslení
    window.c                   Window.begin/end, události, subwindows, capture,
                               focus, dragging, refresh, renderovací fronta,
                               dvui_data_* nad sdíleným dvui_DataStore
    widget_data.c              WidgetData (init/register/minSize*, borderAndBackground)
    layout.c                   BasicLayout, placeOnScreen
    widgets/box.c, button.c, simple.c (spacer, separator, label placeholder,
                               overlay, scale), scroll_info.c, scroll_bar.c,
                               scroll_container.c, scroll_area.c, grid.c,
                               floating.c (floating, tooltip, floating window,
                               window header)
  examples/demo.c              demo se všemi widgety
  tests/test_layout.c          testy layoutu (víc snímků, kontrola obdélníků)
```

Mapování DVUI → C:

| DVUI (Zig) | dvui-layout (C) |
|---|---|
| `Rect.zig`, `Size.zig`, `Point.zig`, `RectScale.zig`, `Color.zig` | `shared/src/geometry.c` (+ `core.c`: `visible`, `toNatural`) |
| `Data.zig`, `tracking_hash_map.zig` | `shared/src/data.c` |
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
| `widgets/FloatingWidget.zig`, `FloatingTooltipWidget.zig`, `FloatingWindowWidget.zig` | `widgets/floating.c` |
| `widgets/OverlayWidget.zig`, `ScaleWidget.zig`, `LabelWidget.zig` (bez textu) | `widgets/simple.c` |
| `backends/sdl3gpu.zig`, `backends/testing.zig` | `shared/src/backends/` |

### Fáze

- [x] Fáze 0: snapshot DVUI, AGENTS.md, PLAN.md.
- [x] Fáze 1: jádro – geometrie, Options, Id, datové úložiště, Window,
      WidgetData, události, capture, subwindows, renderovací fronta.
- [x] Fáze 2: backend SDL3 GPU + testovací backend.
- [x] Fáze 3: widgety box, button, overlay, scale, spacer.
- [x] Fáze 4: scroll info / bar / container / area.
- [x] Fáze 5: floating, floating tooltip, floating window + window header.
- [x] Fáze 6: grid (vícepruchodový layout).
- [x] Fáze 7: demo, testy, dokumentace (README česky), ověření buildu.

### Co se z DVUI vědomě vynechává

Text a fonty, ikony, obrázky, textury (kromě bílé 1×1 pro shader), zaoblené rohy,
stíny, gradienty, ninepatch, AccessKit, dialogy a toasty, animace (kromě
`hoverFade`), dotykové události a setrvačné scrollování prstem, klávesová
navigace tabulátorem a v gridu, editace a řazení v gridu, témata kromě
jednoho výchozího (barvy podle Adwaita Light). Podrobnosti v
`dvui-layout/README.md`, sekce „Odchylky od DVUI“.

### Stav

Hotovo a ověřeno na Linuxu (GCC 13, Clang 18, ASan/UBSan, demo přes Vulkan –
lavapipe pod Xvfb). Windows a macOS zatím nejsou otestované.

API vlastností widgetů je seznam značek (`dvui_Prop`) – jeden seznam pro
DVUI `Options` i `InitOptions` (viz `dvui-layout/docs/ROZHRANI.md`, kap. 3).

Možné další kroky:
- Ověřit build na Windows (Clang/MinGW, D3D12) a macOS (Metal).
- Případně doplnit klávesovou navigaci (tab_index) a kurzor v gridu.

## Sdílený kód (shared/)

Vytažen z dvui-layout, aby ho mohly používat další projekty. Obsah,
použití v `meson.build` a rozhraní mezi backendem a oknem popisuje
[shared/README.md](shared/README.md). Zásada: zobecňovat jen málo, jinak
kód zduplikovat do projektu (viz AGENTS.md).

```
shared/
  include/dvui/base.h          Id, DVUI_SRC, geometrie, barvy, vstupní výčty,
                               dvui_Vertex, dvui_Backend, funkce okna volané backendem
  include/dvui/util.h          logování, min/max
  include/dvui/data.h          aréna, hash mapa, dvui_DataStore
  include/dvui/backend_sdl3gpu.h, backend_testing.h
  src/base.c                   id, pomocné funkce vstupu
  src/geometry.c               Rect, Size, Point, RectScale, Color
  src/data.c                   aréna, hash mapa, datové úložiště
  src/backends/sdl3gpu.c, testing.c
  shaders/, tools/embed.c
```

- [x] Krok 1: přesun sdíleného kódu z dvui-layout do `shared/`; dvui-layout
      ho používá, testy (159 kontrol) projdou, demo je pixelově stejné.
- [x] Krok 2: `dvui_Backend.sleep`, `dvui_sdl3gpu_wait_event_timeout`
      s `uint32_t` a návratovou hodnotou (kvůli `Window.waitTime`
      v dvui-animation).

## Projekt dvui-animation

### Zadání

Animace z DVUI nad backendem SDL 3 GPU (+ testovací backend). Widgety box
a button jako obdélníky bez textu, pomocné `label_placeholder` a `spacer`.
Výchozí bod je kopie dvui-layout bez scrollu, gridu a plovoucích widgetů.

Implementovat (zdroje v `dvui-original/src`):
- `dvui_Animation` (dvui.zig – `Animation`: `value`, `done`);
- `dvui_animation`, `dvui_animation_get`, mapa animací v okně
  (Window.zig – `animations`), stárnutí a mazání ve `window_begin`;
- časovače `dvui_timer`, `timer_get`, `timer_done`, `timer_done_or_none`;
- `reduce_motion`;
- `dvui_window_end` vracející čas do dalšího snímku a `Window.waitTime`;
- všechny funkce z `easing.zig`;
- AnimateWidget (`dvui.animate`): druhy alpha, vertical, horizontal, none.

### Rozhodnutí (odsouhlasená vlastníkem)

- `dvui_window_end` vrací `int32_t` – počet mikrosekund, −1 = DVUI `null`
  (čekat na událost). Totéž platí pro dvui-layout (vrací jen 0 / −1).
- `Window.waitTime` a `beginWait` celé (včetně `max_fps` a slop); do
  sdíleného rozhraní backendu přibyla funkce `sleep` a
  `dvui_sdl3gpu_wait_event_timeout` má jako DVUI parametr `uint32_t`
  (`UINT32_MAX` = bez limitu) a vrací „přerušeno událostí“. dvui-layout
  se přizpůsobil (převod −1 → `UINT32_MAX` v demu).
- Overlay je převzatý jako pomocný widget (používá ho DVUI Examples).

### Architektura

```
dvui-animation/
  meson.build                  používá soubory z ../shared
  include/dvui/dvui.h          veřejné API; vkládá dvui/base.h
  src/
    animation.c                Animation, animation/animationGet, časovače, reduce_motion
    easing.c                   easing.zig
    window.c                   Window (stárnutí animací v begin, end, waitTime, beginWait)
    core.c, frame.c, widget_data.c, layout.c   jako dvui-layout (bez placeOnScreen, fontu)
    widgets/animate.c          AnimateWidget
    widgets/box.c, button.c, simple.c (spacer, separator, label placeholder, overlay)
  examples/demo.c              demo podle Examples/animations.zig
  tests/test_animation.c       testy animací (+ základní testy layoutu)
  docs/ANIMACE.md, ROZHRANI.md, ODCHYLKY.md
```

### Fáze

- [x] Fáze 1: kopie dvui-layout → dvui-animation, odstranění scrollu,
      gridu, plovoucích widgetů a Scale.
- [x] Fáze 2: easing, `dvui_Animation`, mapa animací, stárnutí v `begin`,
      časovače, `reduce_motion`.
- [x] Fáze 3: návratová hodnota `dvui_window_end`, `waitTime`,
      `beginWait`, úprava sdíleného backendu a smyčky dem.
- [x] Fáze 4: AnimateWidget a alfa při kreslení.
- [x] Fáze 5: testy (easing, window_end, životnost animací, časovače,
      AnimateWidget, reduce_motion, waitTime).
- [x] Fáze 6: demo podle `Examples/animations.zig` bez textu.
- [x] Fáze 7: dokumentace, ověření buildu (GCC 13, Clang 18, ASan/UBSan,
      bez varování), snímek dema pod Xvfb (lavapipe).

### Stav

Hotovo a ověřeno na Linuxu. Windows a macOS zatím nejsou otestované.
