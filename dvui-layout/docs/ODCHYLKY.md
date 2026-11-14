# Odchylky od DVUI

Podrobný přehled, čím se dvui-layout liší od DVUI (commit `2efa35e`, kopie
v `dvui-original/`). Krátké shrnutí je v [README.md](../README.md#odchylky-od-dvui);
chybějící widgety podrobně popisuje [NEIMPLEMENTOVANO.md](NEIMPLEMENTOVANO.md).

Obsah:

1. [Co je stejné](#1-co-je-stejné)
2. [Převod ze Zigu do C](#2-převod-ze-zigu-do-c)
3. [API vlastností widgetů](#3-api-vlastností-widgetů)
4. [Id widgetů](#4-id-widgetů)
5. [Vynechané vlastnosti v Options a tématu](#5-vynechané-vlastnosti-v-options-a-tématu)
6. [Vynechané funkce](#6-vynechané-funkce)
7. [Změněné chování](#7-změněné-chování)
8. [Paměť](#8-paměť)
9. [Backendy](#9-backendy)
10. [Má některá odchylka vliv na layout?](#10-má-některá-odchylka-vliv-na-layout)

---

## 1. Co je stejné

Cílem projektu je věrný port layoutu, proto se **neliší**:

- životní cyklus widgetu: `init` → `register` → `rectFor` /
  `minSizeForChild` dětí → `deinit` → `minSizeSetAndRefresh` →
  `minSizeReportToParent`;
- `placeIn`, `placeOnScreen`, `BasicLayout`, vrstvy obdélníku (margin,
  border, padding) a `padSize`;
- algoritmy BoxWidgetu (`pixels_per_w`, balení podle gravitace,
  `equal_space`, `num_packed_expanded`), ScrollContaineru a ScrollArey
  (posuvníky, virtuální velikost, `lock_visible`, `frame_viewport`),
  GridWidgetu (auto-size sloupců a řádků přes víc snímků, roztahování
  a zmenšování sloupců, `rowsVisible`) a plovoucích widgetů
  (FloatingWidget, FloatingTooltip, FloatingWindow s auto-size,
  centrováním a vyhýbáním se oknům);
- min. velikosti z minulého snímku, `refresh` a `extra_frames_needed`;
- datové úložiště a jeho klíče (`"_data"`, `"_scroll_info"`,
  `"__col_widths"`, …), úklid nepoužitých položek (viz [STAV.md](STAV.md));
- subwindows, `RenderFrontToBack`, capture myši, tažení, fokus widgetu
  a subwindow, `eventMatch`, `clicked`, `hoverFade`;
- výchozí hodnoty vlastností a výchozí Options jednotlivých widgetů.

Testy (`tests/test_layout.c`) kontrolují přesné obdélníky, které dává
DVUI.

## 2. Převod ze Zigu do C

Mechanické rozdíly, které plynou z jazyka. Nejsou to změny chování.

| Zig (DVUI) | C (dvui-layout) |
|---|---|
| `BoxWidget.init`, `dvui.placeIn` | `dvui_box_widget_init`, `dvui_place_in` |
| `enums.Expand.both` | `DVUI_EXPAND_BOTH` |
| `?T` u parametrů a polí | podle situace: ukazatel (`NULL` = nezadáno), id 0, příznak `has_*`, bit v `dvui_Options.set` (kap. 3) |
| `[]T` (slice) | ukazatel + délka (`col_widths` + `col_widths_len`) |
| `Rect.Physical`, `Rect.Natural`, `Point.Physical` … | aliasy stejného typu (`typedef dvui_Rect dvui_RectPhysical`); jednotky hlídá jen jméno typu, ne překladač |
| `Widget` (ukazatel + vtable) | `dvui_Widget {void* ptr; const dvui_WidgetVTable* vtable;}` |
| `self.data()` s `validate()` | přímý přístup `&self->wd`; kontrola platnosti (`validate`) chybí |
| chyby (`!T`, `try`) | nejsou; při nedostatku paměti `abort()`, ostatní chyby se logují |
| `@src()` | makro `DVUI_SRC` (kap. 4) |
| `undefined` | nulová inicializace |
| `std.log` | `DVUI_LOG_ERR` na stderr |
| `i128` čas v nanosekundách | `int64_t` |

Konvence pojmenování a komentářů popisuje [AGENTS.md](../../AGENTS.md).

**Rozdělení kódu.** Část kódu je sdílená s ostatními projekty repozitáře
v adresáři `../shared/` (viz [shared/README.md](../../shared/README.md)):
základní typy (`dvui/base.h`), geometrie a barvy, id, aréna, hash mapa,
datové úložiště a oba backendy. Chování to nemění; jen DVUI `Data`
(`Window.data_store`) je v C samostatná struktura `dvui_DataStore`, kterou
okno obsahuje, a funkce `dvui_data_*` okna z id a jména spočítají klíč
a zavolají `dvui_data_store_*` (DVUI dělá totéž v `dvui.dataSet` →
`Data.set`).

## 3. API vlastností widgetů

DVUI předává widgetu dvě struktury: `InitOptions` (vlastnosti konkrétního
widgetu) a `Options` (společné vlastnosti). Volitelné položky jsou typu
`?T` a výchozí hodnoty se doplní `defaults.override(opts)`.

dvui-layout předává **jeden seznam značek** (`dvui_Prop`), ve kterém jsou
obě skupiny:

```c
// DVUI:  dvui.box(@src(), .{ .dir = .horizontal }, .{ .expand = .horizontal, .margin = .all(4) })
dvui_box(DVUI_DIR(DVUI_HORIZONTAL), DVUI_EXPAND(DVUI_EXPAND_HORIZONTAL), DVUI_MARGIN_ALL(4));
```

- Vyhodnocení (`dvui_props_resolve`) odpovídá DVUI: obecné výchozí
  hodnoty → výchozí seznam widgetu (DVUI `defaults`) → seznam uživatele.
- Které společné vlastnosti byly zadány (Zig `!= null`), si
  `dvui_Options` pamatuje v bitové masce `set`; nezadané pole má výchozí
  hodnotu a čte se přímo (`o->margin`). Getry DVUI (`marginGet()`, …)
  proto nejsou potřeba.
- Vlastnosti widgetu se rozbalí do `dvui_WidgetProps` (pole `has[]` říká,
  co bylo zadáno) a z nich do interní struktury `*InitOptions` widgetu.
  Kde DVUI rozlišuje `null`, má interní struktura příznak `has_*`
  (např. `has_frame_viewport`).
- Vlastnost, které widget nerozumí, se ohlásí na stderr (DVUI to hlídá
  překladač).
- Vložení hotových Options do seznamu: `DVUI_OPTIONS(&opts)` – náhrada za
  skládání `strip().override(...)` při tvorbě vnitřních widgetů.
- Typ hodnoty kontroluje překladač jen přes pole unie `dvui_Prop`;
  implicitní převody čísel projdou (`DVUI_P(expand, 0.5f)`).

**Přejmenované vlastnosti** (aby jedno jméno v seznamu znamenalo jednu
věc):

| DVUI | dvui-layout | Důvod |
|---|---|---|
| `ScrollBarWidget.InitOptions.direction` | `dir` | stejně jako Box |
| `FloatingTooltipWidget.InitOptions.scale` | `tooltip_scale` | `scale` je ukazatel u ScaleWidgetu |
| `FloatingWindowWidget.InitOptions.rect` | `rect_io` | `rect` je společná vlastnost |
| `GridWidget.CellOptions.col_resizable` | `resizable` | jako u hlavičky (`ColHeaderInitOptions.resizable`) |
| text popisku / titulku (`[]const u8`) | `label_size` | text není, jen jeho velikost (kap. 6.1) |
| `GridWidget.InitOptions.scroll_opts` (vnořená struktura) | vlastnosti ScrollArey přímo v seznamu gridu | seznam je jeden |

Vlastnosti s výchozí hodnotou `true` mají stejné jméno i výchozí hodnotu
jako v DVUI (`draw_focus`, `container`, `process_events_after`,
`resizable`, `mouse_events`, …). Výjimkou je interní
`dvui_WidgetDataInitOptions.no_scroll_when_focused` (DVUI
`scroll_when_focused = true`), kterou uživatel nezadává.

Pohodlné funkce DVUI (`dvui.box(@src(), …)`) jsou makra (`dvui_box(…)`),
která sama doplní `DVUI_SRC`; skutečné funkce mají příponu `_impl`.

## 4. Id widgetů

- DVUI: `hash(id rodiče, ukazatel na jméno modulu, ukazatel na jméno
  souboru, řádek, sloupec, id_extra)`.
- dvui-layout: `hash(id rodiče, adresa literálu, id_extra)`. `DVUI_SRC`
  vytvoří pro každé místo ve zdrojáku řetězcový literál
  `"soubor:řádek:čítač"` (čítač = `__COUNTER__`, náhrada sloupce) a do id
  jde jeho adresa.

Chování je stejné: každé místo ve zdrojáku dává po celý běh jiné stálé id.
Id se liší mezi běhy programu (ASLR) a mezi moduly (EXE a DLL mají vlastní
kopie literálů) – v DVUI je to stejné, protože hashuje také ukazatele.
Id 0 znamená „žádné id“ (Zig `?Id = null`).

## 5. Vynechané vlastnosti v Options a tématu

### Options

| Vlastnost DVUI | Proč chybí |
|---|---|
| `font` | není text |
| `label` (popisek pro AccessKit) | není AccessKit |
| `role` | není AccessKit |
| `tab_index` | není klávesová navigace (kap. 6.3) |
| `tag` | nejsou tagy (`dvui.tag`, `tagGet`) |
| `data_out` | nepotřebné – WidgetData je dostupná přímo |
| `theme` (jiné téma pro podstrom) | téma je jen v okně |
| `ninepatch_fill`, `ninepatch_hover`, `ninepatch_press` | nejsou textury |
| `corners` | jen ostré rohy |
| `box_shadow` | nejsou stíny |
| `rotation` | kreslí se jen osově zarovnané obdélníky |

Barvy jsou `dvui_Color`, ne `ColorOrGradient` (bez gradientů).
`max_size_content` je `dvui_Size`, ne `MaxSize` (stejné hodnoty, jen bez
pomocných konstruktorů `width()`/`height()` – místo nich
`DVUI_MAX_SIZE(w, DVUI_MAX_FLOAT_SAFE)`).

### Téma

- Je jen jedno: barvy podle Adwaita Light (`dvui_theme_adwaita_light`);
  lze ho nahradit `dvui_window_theme_set`.
- Chybí fonty (`font_body`, `font_heading`, `font_title`, `font_mono`,
  `embedded_fonts`), `corner` a ninepatch.
- Nepovinné barvy (`?Color`) jsou obyčejné `dvui_Color`; hodnota
  `{0, 0, 0, 0}` znamená „nenastaveno“. Plně průhlednou barvu proto
  v tématu nastavit nelze (v Options ano – tam rozhoduje bit v `set`).

## 6. Vynechané funkce

### 6.1 Text

Není text ani fonty. Místo `LabelWidget` je `dvui_label_placeholder`,
widget s min. velikostí `label_size` a výchozím paddingem 6 jako popisek.
Používají ho `dvui_button`, `dvui_tooltip` a `dvui_window_header`.
Velikost písmene „M“ (`Font.sizeM`, potřebuje ji grid pro výchozí rozměry
buněk) je konstanta 9×12 – přibližně Vera Sans velikosti 10, výchozí font
DVUI. Text je cílem dalších projektů (dvui-text, dvui-text-selection,
dvui-text-edit).

### 6.2 Kreslení

Kreslí se jen plné osově zarovnané obdélníky:
- okraj (`border`) je tah složený ze 4 obdélníků; nestejnoměrný okraj se
  kreslí jako celý border rect barvou okraje, přes který se nakreslí pozadí;
- úchyt posuvníku je obdélník (v DVUI zaoblený);
- chybí zaoblené rohy, stíny, gradienty, ninepatch, rotace, `fade`
  (vyhlazení hran), cesty (`Path`), triangulace, ikony (TinyVG), obrázky,
  textury a render targety;
- zavírací tlačítko okna má místo ikony čtvercový zástupce.

### 6.3 Klávesnice a fokus

- Chybí klávesová navigace: `tab_index`, `tabIndexNext/Prev`, šipky ve
  `Window.end`, `FocusGroupWidget`. Fokus se mění kliknutím a funkcemi
  `dvui_focus_widget` / `dvui_focus_subwindow`.
- Chybí textové události (`Event.Text`) a vstup textu
  (`textInputRect`).
- Kódy kláves a modifikátory jsou jen ty, které widgety používají
  (Enter / mezerník = „activate“, šipky, PageUp/PageDown, Home/End, …).

### 6.4 Dotyk

V DVUI má každý prst vlastní capture a tažení (`dragging.slot(b)`). Zde je
jen myš: **jeden capture a jeden slot tažení**. Chybí proto
`processMotionScroll` a dotyková část `ScrollContainerWidget.processEvent`.
Setrvačné scrollování (`velocity`) je zachované, ale bez doteku se
nespustí.

### 6.5 Animace, časovače, dialogy

- Chybí animace (`dvui.animation`, `animationGet`, `easing`) a časovače
  (`dvui.timer`). Zachovaný je jen `stateFade` / `hoverFade`, který svůj
  stav drží v datovém úložišti.
- `FloatingTooltipWidget.InitOptions.delay` chybí (potřebuje
  AnimateWidget). Výchozí chování DVUI (`delay = null`, tooltip hned) je
  stejné.
- Chybí dialogy, toasty, `popup`, `dvui.expander`.

### 6.6 Grid

Chybí kurzor buněk, klávesová navigace, fokus buněk (`grid_focus`,
`focusOnWidget`), editace buněk, řazení podle hlavičky (`headerSortable`)
a `cellActivated`. `cellHovered` funguje jen v režimu myši. Layout gridu
je celý: hlavičky, auto-size, roztahování a zmenšování sloupců, změna
šířky tažením, `rowsVisible`, `cellFromPoint`.

### 6.7 Ostatní

- AccessKit (přístupnost) a vše kolem `role` / `label`.
- Ladicí okno (`Debug`), `errorOutline`, profiler.
- Víc oken operačního systému (`OsWindowWidget`, `popWindow`).
- Schránka, `openURL`, preferované barevné schéma, `prefersReducedMotion`.
- Widgety neuvedené v README – viz [NEIMPLEMENTOVANO.md](NEIMPLEMENTOVANO.md).

## 7. Změněné chování

- **`dvui_window_end`** vrací `int32_t`: `0` („udělej hned další snímek“)
  nebo `−1` („čekej na událost“, DVUI `null`). DVUI vrací `?u32` – kolik mikrosekund má uplynout
  mezi tímto a dalším `begin` (0 = hned, jiné hodnoty kvůli animacím
  a časovačům), nebo `null` („čekej na událost“). Bez animací a časovačů
  jiné hodnoty než 0 a `null` nevznikají. Chybí i `Window.waitTime` (omezení FPS),
  proto program převádí −1 na `UINT32_MAX` pro
  `dvui_sdl3gpu_wait_event_timeout` sám (DVUI to dělá ve `waitTime`).
  Animace a `waitTime` má projekt dvui-animation.
- **`Color.lighten`**: DVUI mění světlost v HSLuv, zde se mění L v CIE
  LCh(uv) se zachovaným chroma. Pro šedé barvy je výsledek stejný; pro
  barevné se může nepatrně lišit odstín ztmavené barvy (hover, press).
- **Datové úložiště** kontroluje typ hodnoty jen podle velikosti; DVUI
  v debug buildu porovnává i jméno typu.
- **Logování**: `Event.handle` nepíše ladicí log, `refresh` neloguje zdroj.
  Chybové hlášky (duplicitní id, widget neuzavřený v rodiči, neznámá
  vlastnost) jdou na stderr.
- **Neznámá vlastnost v seznamu** se ohlásí za běhu (v DVUI to není možné –
  zachytí to překladač).

## 8. Paměť

DVUI dostává `std.mem.Allocator` (gpa) a používá arénu snímku, LIFO arénu
a zásobník widgetů. dvui-layout:

- **nepodporuje vlastní alokátory** – používá `malloc` / `free`;
- má vlastní arénu snímku (`dvui_frame_alloc`; aréna sama je sdílená
  v `../shared/src/data.c`), kterou vynuluje `dvui_window_end`;
- LIFO aréna není potřeba (nekreslí se cesty);
- widgety z pohodlných maker mají každý vlastní `calloc` místo zásobníku;
  okno je eviduje a ty, které program zapomněl uvolnit, uvolní na konci
  snímku;
- každá hodnota v datovém úložišti má vlastní `malloc`; nahrazené hodnoty
  jdou do koše, který se vysype na začátku dalšího snímku;
- při nedostatku paměti se volá `abort()` (DVUI vrací chybu).

Podrobně v [ROZHRANI.md, kap. 11](ROZHRANI.md#11-správa-paměti)
a [STAV.md](STAV.md).

## 9. Backendy

Rozhraní backendu i oba backendy jsou sdílené s ostatními projekty
(`../shared/`, viz [shared/README.md](../../shared/README.md)); odchylky
níže proto platí pro všechny projekty.

**Rozhraní backendu** (`dvui_Backend`) má jen funkce, které layout
a kreslení obdélníků potřebují: `begin`, `end`, `pixel_size`,
`window_size`, `content_scale`, `draw_clipped_triangles` (bez textury),
`set_cursor`, `render_present`, `nano_time`, `sleep` (dvui-layout ji
nevolá). Chybí textury, render
targety, schránka, `textInputRect`, `openURL`, `refresh` z jiného vlákna,
AccessKit a správa víc oken.

**SDL 3 GPU** (`../shared/src/backends/sdl3gpu.c`, DVUI `sdl3gpu.zig`):
- shadery jsou převzaté z DVUI (předkompilované SPIR-V, MSL, DXIL);
- okno se na začátku snímku maže barvou pozadí (výchozí `#f0f0f0`) místo
  `LOADOP_LOAD`;
- jediná textura je bílá 1×1, sampler nearest;
- indexy jsou 32bitové;
- nepovinné parametry `dvui_Sdl3GpuInitOptions` (`min_size`,
  `clear_color`) jsou ukazatele.

**Testovací backend** (`../shared/src/backends/testing.c`, DVUI
`backends/testing.zig`): bez GPU; velikost okna zadává test a vykreslené
obdélníky se ukládají do pole `draws` (obdélník, barva, clip). Pomocné
funkce `dvui.testing` (`settle`, `moveTo`, `click`) nejsou součástí
knihovny – testy mají vlastní (`tests/test_layout.c`).

## 10. Má některá odchylka vliv na layout?

Záměrně žádná: pro stejný strom widgetů a stejné vlastnosti dává
dvui-layout stejné obdélníky jako DVUI. Výsledek se může lišit jen tam,
kde DVUI do layoutu vnáší něco, co zde chybí:

- **text** – popisek má v DVUI velikost podle fontu a textu, zde
  `label_size`; grid odvozuje výchozí šířku a výšku buňky z `Font.sizeM`
  (zde konstanta 9×12);
- **ikony** – zavírací tlačítko okna má velikost podle výšky písma
  (stejně jako v DVUI, ale z konstanty);
- **animace** – widgety, které je v DVUI používají k layoutu (Paned,
  Animate, rozbalení ve Tree), zde zatím nejsou;
- **dotyk a klávesnice** – scrollování a fokus vyvolané dotykem nebo
  klávesnicí (`scroll_to_focused` při navigaci tabulátorem) nenastanou.
