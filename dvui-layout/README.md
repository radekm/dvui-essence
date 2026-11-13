# dvui-layout

Ořezaná verze [DVUI](https://github.com/david-vanderson/dvui) v jazyce C23,
která dělá **layout widgetů** a kreslí je přes **SDL 3 GPU**. Widgety jsou
obdélníky s výplní a okrajem (bez textu, s ostrými rohy). Kód layoutu je věrný
port DVUI (commit `2efa35e`, kopie je v `../dvui-original`).

![Demo](docs/demo.png)

## Dokumentace

- [docs/LAYOUT.md](docs/LAYOUT.md) – jak přesně funguje layout: vrstvy obdélníku,
  co z WidgetData a Options vstupuje do výpočtu, algoritmy jednotlivých
  kontejnerů (box, scroll, grid, plovoucí okna), konvergence přes více snímků.
- [docs/ROZHRANI.md](docs/ROZHRANI.md) – základní rozhraní: smyčka snímku,
  `dvui_Widget` a `WidgetData`, Options, id, události, capture a fokus,
  datové úložiště, kreslení, backend, správa paměti a příklad vlastního widgetu.

## Widgety

| Widget | C API | DVUI originál |
|---|---|---|
| box (vodorovný i svislý, `equal_space`) | `dvui_box`, `dvui_BoxWidget` | `widgets/BoxWidget.zig` |
| tlačítko (bez textu, klikatelné) | `dvui_button`, `dvui_ButtonWidget` | `widgets/ButtonWidget.zig` |
| scroll area / container / bar | `dvui_scroll_area`, `dvui_ScrollContainerWidget`, `dvui_ScrollBarWidget` | `widgets/Scroll*Widget.zig`, `ScrollInfo.zig` |
| grid (vícepruchodový layout) | `dvui_grid`, `dvui_GridWidget` | `widgets/GridWidget.zig` |
| tooltip | `dvui_tooltip`, `dvui_FloatingTooltipWidget` | `widgets/FloatingTooltipWidget.zig` |
| plovoucí okno + lišta | `dvui_floating_window`, `dvui_window_header` | `widgets/FloatingWindowWidget.zig`, `dvui.windowHeader` |
| pomocné | `dvui_spacer`, `dvui_separator`, `dvui_label_placeholder`, `dvui_overlay`, `dvui_ScaleWidget`, `dvui_FloatingWidget` | `dvui.zig`, `LabelWidget.zig`, `OverlayWidget.zig`, `ScaleWidget.zig`, `FloatingWidget.zig` |

## Sestavení

Potřeba: Meson ≥ 1.3, Ninja, překladač C23 (GCC ≥ 13 nebo Clang ≥ 18;
na Windows Clang nebo MinGW-GCC) a pro backend a demo **SDL 3** v systému
(hledá se přes pkg-config/CMake jako `sdl3`).

```sh
cd dvui-layout
meson setup build
ninja -C build
meson test -C build          # testy layoutu, nepotřebují GPU ani SDL
./build/dvui-layout-demo     # demo (Vulkan / Metal / D3D12 přes SDL 3 GPU)
```

Bez SDL 3 se sestaví jen knihovna a testy (`-Dsdl3=disabled` to vynutí).

## Jak se používá

Stejně jako DVUI je to immediate-mode GUI: každý snímek se widgety znovu
„deklarují“ a layout se ustálí během několika snímků (min. velikosti se pamatují
z minulého snímku).

```c
#include "dvui/backend_sdl3gpu.h"

dvui_Sdl3GpuBackend* backend = dvui_sdl3gpu_create((dvui_Sdl3GpuInitOptions){.title = "app", .size = {800, 600}});
dvui_Window* win = dvui_window_create(dvui_sdl3gpu_backend(backend));
bool open = true;
dvui_window_open_flag_set(win, &open);

while (open) {
    dvui_window_begin(win, (int64_t)SDL_GetTicksNS());
    dvui_sdl3gpu_add_all_events(backend, win);

    dvui_BoxWidget* row = dvui_box(DVUI_DIR(DVUI_HORIZONTAL), DVUI_EXPAND(DVUI_EXPAND_HORIZONTAL));
    if (dvui_button(DVUI_LABEL_SIZE(40, 12), DVUI_FILL(DVUI_RGB(200, 60, 30)))) {
        // kliknuto
    }
    dvui_box_widget_deinit(row);

    dvui_sdl3gpu_wait_event_timeout(backend, dvui_window_end(win));
}

dvui_window_destroy(win);
dvui_sdl3gpu_destroy(backend);
```

Poznámky k API:

- Widget dostává **jeden seznam vlastností** (značka + hodnota), ve kterém
  jsou společné vlastnosti (DVUI `Options`) i vlastnosti widgetu (DVUI
  `InitOptions`). Co v seznamu není, má výchozí hodnotu jako v DVUI.
  Podrobně v [docs/ROZHRANI.md](docs/ROZHRANI.md), kap. 3.
- `DVUI_SRC` nahrazuje Zig `@src()`; id widgetu vzniká z id rodiče, souboru,
  řádku, `__COUNTER__` a `id_extra`. Ve smyčce předávejte `DVUI_ID_EXTRA(i)`.
  Makra `dvui_box(…)` apod. doplňují `DVUI_SRC` sama.
- Widget lze vytvořit buď ve vlastní paměti
  (`dvui_box_widget_init(&box, DVUI_SRC, DVUI_PROPS(…))`), nebo „pohodlným“
  makrem (`dvui_box(…)`), které ho alokuje; v obou případech se ukončuje
  `*_deinit`.

## Struktura

```
include/dvui/dvui.h               veřejné API
include/dvui/backend_sdl3gpu.h    backend SDL 3 GPU
include/dvui/backend_testing.h    testovací backend (bez GPU)
src/core.c                        geometrie, barvy, Options, téma, placeIn
src/window.c                      Window, události, capture, fokus, subwindows,
                                  datové úložiště, renderovací fronta
src/widget_data.c                 WidgetData
src/layout.c                      BasicLayout, placeOnScreen
src/widgets/                      widgety
src/backends/                     SDL 3 GPU a testovací backend
shaders/                          předkompilované shadery z DVUI
tools/embed.c                     vložení shaderů do C (náhrada #embed)
tests/test_layout.c               testy layoutu
examples/demo.c                   demo
```

## Odchylky od DVUI

Layout (placeIn, BasicLayout, BoxWidget, ScrollContainer/ScrollArea,
GridWidget, plovoucí widgety, min. velikosti z minulého snímku, refresh,
datové úložiště a jeho klíče) odpovídá DVUI. Vědomě se liší:

**Vynecháno úplně**
- Text, fonty, ikony, obrázky a textury. Místo `LabelWidget` je
  `dvui_label_placeholder` se zadanou „velikostí textu“; `dvui_button`,
  `dvui_tooltip` a `dvui_window_header` ho používají místo popisku.
  Velikost písmene „M“ (DVUI `Font.sizeM`, používá ji grid) je konstanta 9×12.
- Zaoblené rohy (`corners`), stíny (`box_shadow`), gradienty, ninepatch,
  rotace, `fade` (vyhlazení okrajů). Kreslí se jen obdélníky; okraj je tah
  ze 4 obdélníků. Úchyt posuvníku je obdélník.
- AccessKit, dialogy, toasty, animace a časovače (kromě `hoverFade`),
  zpoždění tooltipu (`delay`), `tag`, `data_out`, `theme` v Options,
  ladicí okno.
- Klávesová navigace tabulátorem (`tab_index`, `tabIndexNext/Prev`,
  šipky v `Window.end`) a `FocusGroupWidget`.
- Dotykové ovládání: v DVUI má každý prst vlastní capture a tažení; zde je
  jen myš (jeden capture, jedno tažení). Proto chybí i `processMotionScroll`
  a dotyková část `ScrollContainerWidget.processEvent`. Setrvačné
  scrollování (`velocity`) je zachované, ale bez doteku se nespustí.
- Grid: kurzor buněk, klávesová navigace, fokus buněk, `focusOnWidget`,
  editace buněk (`editable`), řazení (`headerSortable`), `cellActivated`.
  `cellHovered` funguje jen v režimu myši. Zůstává celý layout: hlavičky,
  auto-size sloupců i řádků přes více snímků, roztahování a zmenšování
  sloupců, ruční změna šířky tažením, `rowsVisible`, `cellFromPoint`.
- Render targety a vykreslování do textury.

**Změněno**
- API vlastností: místo struktur `Options` a `InitOptions` s volitelnými
  poli (Zig `?T`) dostává widget jeden seznam značek (`dvui_Prop`).
  Vyhodnocení odpovídá DVUI `defaults.override(opts)`; zadané společné
  vlastnosti si `dvui_Options` pamatuje v bitové masce `set`. Vlastnosti
  widgetu z DVUI s výchozí hodnotou `null` mají v interních
  `*InitOptions` příznak `has_*`. Jména vlastností jsou převzatá z DVUI
  s výjimkami: `ScrollBarWidget` `direction` → `dir`,
  `FloatingTooltipWidget` `scale` → `tooltip_scale`,
  `FloatingWindowWidget` `rect` → `rect_io`, `CellOptions.col_resizable`
  → `resizable` (jako u hlavičky), velikost popisku/titulku → `label_size`.
  `GridWidget.InitOptions.scroll_opts` není vnořená struktura – vlastnosti
  ScrollArea se gridu dávají přímo do jeho seznamu. Cizí vlastnost se
  ohlásí na stderr.
- Téma: barva `{0, 0, 0, 0}` znamená „nenastaveno“ (DVUI `?Color = null`).
- Téma: jen jedno (barvy podle Adwaita Light). `Color.lighten` v DVUI mění
  světlost v HSLuv; zde se mění L v CIE LCh(uv) se zachovaným chroma
  (pro šedé barvy je výsledek stejný).
- Id: DVUI hashuje ukazatel na jméno souboru a sloupec; zde obsah jména
  souboru, řádek a `__COUNTER__`. Hodnota id 0 znamená „žádné id“
  (v Zigu `?Id = null`).
- Backend SDL 3 GPU maže okno barvou pozadí (výchozí `#f0f0f0`) místo
  `LOADOP_LOAD`; textura je jen bílá 1×1, sampler nearest; indexy 32bitové.
- `Event.handle` nepíše ladicí log; `refresh` neloguje zdroj.
- Kódy kláves a modifikátory: jen ty, které widgety používají
  (Enter/mezerník = „activate“, šipky, PageUp/PageDown, …).
- Alokátory: DVUI dostává `std.mem.Allocator` (gpa) a používá arénu snímku,
  LIFO arénu a zásobník widgetů. dvui-layout nepodporuje vlastní alokátory:
  používá `malloc`/`free` a vlastní arénu snímku; LIFO aréna není potřeba
  (nekreslí se cesty); widgety z „pohodlných“ funkcí mají každý vlastní
  `calloc` místo zásobníku. Při nedostatku paměti se volá `abort()`.
  Podrobně v [docs/ROZHRANI.md](docs/ROZHRANI.md), kap. 11.

## Testy

`tests/test_layout.c` běží s testovacím backendem (bez GPU) a kontroluje
obdélníky po ustálení layoutu: `placeIn`, `placeOnScreen`, box (svislý,
vodorovný s roztažením, `equal_space`), velikost a kliknutí tlačítka,
scroll area (virtuální velikost, posuvník, kolečko, mez scrollování), grid
(auto-size sloupců a řádků přes víc snímků), plovoucí okno (auto-size,
centrování, přetažení) a tooltip (zobrazení a skrytí podle myši).
