# shared – sdílený kód projektů

Kód, který je ve všech projektech repozitáře (dvui-layout, dvui-animation,
…) stejný. Není to samostatný projekt: nemá vlastní `meson.build`, každý
projekt si potřebné soubory přidá do svého buildu sám.

Do `shared/` patří jen to, co lze sdílet bez výrazného zobecnění. Okno,
WidgetData, vlastnosti widgetů, téma, layout a widgety má každý projekt
vlastní (zkopírované a upravené) – viz [AGENTS.md](../AGENTS.md).

## Obsah

```
include/dvui/base.h              základní typy, rozhraní backendu, funkce okna volané backendem
include/dvui/util.h              logování (DVUI_LOG_ERR), DVUI_MIN/MAX, dvui_clampf
include/dvui/data.h              aréna, hash mapa (dvui_Map), datové úložiště (dvui_DataStore)
include/dvui/backend_sdl3gpu.h   backend SDL 3 GPU
include/dvui/backend_testing.h   testovací backend bez GPU
src/base.c                       id (dvui_id_extend, dvui_id_update), pomocné funkce vstupu
src/geometry.c                   Rect, Size, Point, RectScale, Color (vč. dvui_color_lighten)
src/data.c                       aréna, hash mapa, datové úložiště
src/backends/sdl3gpu.c           backend SDL 3 GPU (DVUI: backends/sdl3gpu.zig)
src/backends/testing.c           testovací backend (DVUI: backends/testing.zig)
shaders/                         předkompilované shadery z DVUI (SPIR-V, MSL, DXIL)
tools/embed.c                    nástroj pro build: vloží shader do C pole (náhrada #embed)
```

### `dvui/base.h`

- `dvui_Id`, `dvui_Src` a makro `DVUI_SRC` (náhrada Zig `@src()`),
  `dvui_id_extend`, `dvui_id_update`;
- `dvui_Point`, `dvui_Size`, `dvui_Rect` a aliasy `…Physical`, `…Natural`,
  `dvui_RectScale`, `dvui_Color` s makry `DVUI_RGB`, `DVUI_RGBA`,
  `DVUI_MAX_FLOAT_SAFE`, `dvui_Direction` a funkce nad nimi;
- vstup: `dvui_Button`, `dvui_Mod`, `dvui_MouseActionKind`, `dvui_Key`,
  `dvui_KeyAction`, `dvui_WindowAction`, `dvui_Cursor`;
- `dvui_Vertex` a `dvui_Backend` (rozhraní backendu);
- `typedef struct dvui_Window dvui_Window;` a deklarace funkcí okna, které
  volá backend (níže).

Funkce geometrie, které potřebují aktuální okno (`dvui_rect_visible`,
`dvui_rect_to_natural`), sdílené nejsou – má je každý projekt.

### `dvui/data.h`

- **aréna** `dvui_Arena` (`dvui_arena_alloc/reset/free_all`) – bloky
  ≥ 64 KiB, jen posun ukazatele; okno v ní drží data jednoho snímku;
- **hash mapa** `dvui_Map` (DVUI `TrackingAutoHashMap`) – u64 → blob
  z `malloc` se značkou `used`; `dvui_map_reset` smaže položky nepoužité od
  minulého resetu. Okno v ní drží např. min. velikosti widgetů;
- **datové úložiště** `dvui_DataStore` (DVUI `Data.zig`) = mapa + koš.
  Funkce `dvui_data_store_set/get/get_ptr_default/get_slice/remove/reset/
  deinit` pracují s hotovým klíčem (u64). Okno projektu úložiště obsahuje
  a jeho funkce `dvui_data_set(win, id, "jméno", …)` spočítají klíč
  `dvui_id_update(id, "jméno")` a zavolají sdílenou funkci. Hodnota stejné
  velikosti se přepisuje na místě, jiná velikost nebo smazání dá starý blob
  do koše, který vysype až `dvui_data_store_reset` na začátku dalšího
  snímku.

Podrobný popis chování (úklid podle `used`, koš, platnost ukazatelů) je
v dokumentaci projektů, např.
[dvui-layout/docs/STAV.md](../dvui-layout/docs/STAV.md).

## Použití v projektu (meson.build)

Meson dovoluje v `files()` a `include_directories()` cesty mimo adresář
projektu, takže projekt uvádí sdílené soubory cestou `../shared/…`:

```meson
shared_dir = '../shared'

inc = include_directories('include', shared_dir / 'include')

core_sources = files(
  shared_dir / 'src/base.c',
  shared_dir / 'src/geometry.c',
  shared_dir / 'src/data.c',
  shared_dir / 'src/backends/testing.c',
  'src/window.c',
  # … vlastní soubory projektu
)

# backend SDL 3 GPU (jen když je SDL 3 v systému)
embed = executable('embed', shared_dir / 'tools/embed.c', native: true)
# pro každý shader: custom_target s input: shared_dir / 'shaders/default.<stage>.<fmt>'
#   a command: [embed, '@INPUT@', '@OUTPUT@', 'dvui_shader_<stage>_<fmt>']
sdl3gpu_lib = static_library('…-sdl3gpu', shared_dir / 'src/backends/sdl3gpu.c', shader_sources, …)
```

Celý příklad je v [dvui-layout/meson.build](../dvui-layout/meson.build).
Projekt se sestavuje ze svého adresáře (`cd dvui-layout && meson setup
build`); `shared/` se sestavuje vždy jako jeho součást.

Kód projektu vkládá `dvui/base.h` ze své hlavičky `dvui/dvui.h`
a `dvui/data.h` s `dvui/util.h` ze svého `internal.h`.

## Rozhraní mezi backendem a oknem

Backend zná okno jen jako neúplný typ `dvui_Window` a volá tyto funkce,
které **musí implementovat každý projekt** (v DVUI jsou to metody
`Window`):

| Funkce | DVUI | Kdy ji backend volá |
|---|---|---|
| `bool dvui_window_add_event_mouse_motion(win, pt)` | `Window.addEventMouseMotion` | pohyb myši (fyzické pixely) |
| `bool dvui_window_add_event_mouse_button(win, b, action)` | `Window.addEventMouseButton` | stisk / puštění tlačítka |
| `bool dvui_window_add_event_mouse_wheel(win, ticks, dir)` | `Window.addEventMouseWheel` | kolečko (`ticks` už vynásobené `dvui_scroll_speed`) |
| `bool dvui_window_add_event_key(win, code, action, mod)` | `Window.addEventKey` | klávesa |
| `void dvui_window_add_event_window(win, action)` | `Window.addEventWindow` | fokus, zavření, opuštění okna |
| `void dvui_window_add_event_app_quit(win)` | `Window.addEventAppQuit` | ukončení aplikace |
| `void dvui_window_modifiers_set(win, mod)` | `Window.modifiers` | změna modifikátorů |
| `extern float dvui_scroll_speed` | `dvui.scroll_speed` | proměnná; čte ji při převodu kolečka |

Funkce `add_event_*` vrací `true`, pokud je událost nad plovoucím oknem
(DVUI tím říká, že ji „spotřebovalo“). Volají se mezi `dvui_window_begin`
a vytvořením prvního widgetu.

Okno naopak volá backend přes `dvui_Backend` (`begin`, `end`,
`pixel_size`, `window_size`, `content_scale`, `draw_clipped_triangles`,
`set_cursor`, `render_present`, `nano_time`, `sleep`). `sleep` používá jen
`Window.waitTime` / `beginWait` (dvui-animation); v testovacím backendu
nic nedělá. Kdy se která funkce volá,
popisuje [dvui-layout/docs/ROZHRANI.md, kap. 10](../dvui-layout/docs/ROZHRANI.md#10-backend).

Navíc backend SDL 3 GPU nabízí programu `dvui_sdl3gpu_add_all_events`,
`dvui_sdl3gpu_add_event` a `dvui_sdl3gpu_wait_event_timeout(b,
timeout_micros)` (DVUI `waitEventTimeout`): `0` = nečekat, `UINT32_MAX` =
čekat na událost bez limitu, jinak nejvýš tolik mikrosekund. Vrací `true`,
když čekání přerušila událost. Projekty vracejí z `dvui_window_end`
`int32_t` (−1 = DVUI `null`); dvui-animation ho převádí přes
`dvui_window_wait_time` (DVUI `Window.waitTime`), dvui-layout převodem
`−1 → UINT32_MAX` přímo v programu.

## Odchylky od DVUI

Sdílený kód má stejné odchylky jako dosud v dvui-layout: zúžené rozhraní
backendu (bez textur, schránky a víc oken), SDL 3 GPU maže okno barvou
pozadí a má jen bílou texturu 1×1 a 32bitové indexy, datové úložiště
kontroluje typ jen velikostí, `dvui_color_lighten` počítá v CIE LCh(uv)
místo HSLuv, bez vlastních alokátorů (`malloc`/`free`, při nedostatku
paměti `abort()`). Podrobně v
[dvui-layout/docs/ODCHYLKY.md](../dvui-layout/docs/ODCHYLKY.md)
(kap. 2, 7, 8 a 9).
