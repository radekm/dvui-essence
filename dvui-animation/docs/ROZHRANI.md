# Základní rozhraní

Přehled rozhraní dvui-animation: smyčka snímku s čekáním na animace,
animace a časovače, AnimateWidget, widget a jeho data, vlastnosti, id,
rodiče, události, datové úložiště, kreslení, backend a správa paměti. Jak
animace fungují uvnitř (časy, stárnutí, mazání, výpočet čekání), popisuje
podrobně [ANIMACE.md](ANIMACE.md).

Deklarace jsou v `include/dvui/dvui.h`. Základní typy (id, `DVUI_SRC`,
geometrie, barvy, tlačítka, klávesy, kurzory), rozhraní backendu a funkce
okna volané backendem jsou ve sdílené hlavičce `dvui/base.h`, kterou
`dvui.h` vkládá; leží v `../shared/include/` spolu s backendy (viz
[shared/README.md](../../shared/README.md)).

Layout (BasicLayout, box, placeIn, min. velikosti přes víc snímků) je
stejný jako v projektu dvui-layout; podrobně ho popisuje
[dvui-layout/docs/LAYOUT.md](../../dvui-layout/docs/LAYOUT.md).

Obsah:

1. [Smyčka snímku (Window)](#1-smyčka-snímku-window)
2. [Animace, časovače a Animate](#2-animace-časovače-a-animate)
3. [Widget a WidgetData](#3-widget-a-widgetdata)
4. [Vlastnosti widgetů (seznam značek)](#4-vlastnosti-widgetů-seznam-značek)
5. [Id a DVUI_SRC](#5-id-a-dvui_src)
6. [Rodič (parent)](#6-rodič-parent)
7. [Události](#7-události)
8. [Capture, tažení a fokus](#8-capture-tažení-a-fokus)
9. [Datové úložiště](#9-datové-úložiště)
10. [Kreslení, clip a subwindows](#10-kreslení-clip-a-subwindows)
11. [Backend](#11-backend)
12. [Správa paměti](#12-správa-paměti)
13. [Příklad: vlastní kontejner](#13-příklad-vlastní-kontejner)

---

## 1. Smyčka snímku (Window)

Smyčka je stejná jako v DVUI (`backends/sdl3gpu.zig` – `main`): čas snímku
dává `dvui_window_begin_wait`, výsledek `dvui_window_end` převede
`dvui_window_wait_time` na dobu čekání backendu a backend vrátí, zda čekání
přerušila událost.

```c
dvui_Window* win = dvui_window_create(backend);      // DVUI: Window.init
dvui_window_open_flag_set(win, &open);               // zavření okna / app quit → open = false

bool interrupted = false;
while (open) {
    int64_t nstime = dvui_window_begin_wait(win, interrupted);   // DVUI: Window.beginWait
    dvui_window_begin(win, nstime);                              // DVUI: Window.begin
    /* přidání událostí: dvui_window_add_event_* nebo dvui_sdl3gpu_add_all_events */
    /* widgety */
    int32_t end_micros = dvui_window_end(win);                   // DVUI: Window.end
    uint32_t wait = dvui_window_wait_time(win, end_micros);      // DVUI: Window.waitTime
    interrupted = dvui_sdl3gpu_wait_event_timeout(backend, wait); // DVUI: waitEventTimeout
}
dvui_window_destroy(win);
```

| Funkce | Co dělá |
|---|---|
| `dvui_window_begin_wait(win, interrupted)` | vrátí čas pro `begin` (max. z minulého času snímku a `nano_time`); když smyčka spala na konkrétní čas a nepřerušila ji událost, upraví odhad režie `loop_target_slop`, případně krátce „dotočí“ do cíle |
| `dvui_window_begin(win, time_ns)` | nastaví okno jako aktuální, zahodí nepoužitá data a min. velikosti, **posune animace o uplynulý čas a smaže dokončené**, zavolá `backend.begin`, zjistí velikost okna, zaregistruje okno jako kořenový widget |
| `dvui_window_add_event_*` | přidá událost do seznamu snímku (volat **po** `begin`, **před** widgety) |
| `dvui_window_end(win)` | ověří, že jsou všechny widgety ukončené, vykreslí fronty subwindows, zpracuje nezpracované události, zavolá `backend.end`, resetuje arénu snímku, nastaví kurzor, `backend.render_present`; **vrátí čas do dalšího snímku** (níže) |
| `dvui_window_wait_time(win, end_micros)` | kolik µs má backend čekat na událost; `UINT32_MAX` = bez limitu. Při `max_fps` tu rovnou spí (`backend.sleep`) |
| `dvui_window_max_fps_set(win, fps)` | omezení počtu snímků za sekundu (0 = bez omezení) |
| `dvui_window_loop_target_slop(win)` | odhad režie smyčky v µs (DVUI `loop_target_slop`) |
| `dvui_window_cursor_requested` | kurzor, který si widgety v tomto snímku vyžádaly |
| `dvui_window_theme_set` | změna tématu (`dvui_Theme`) |

Návratová hodnota `dvui_window_end` (DVUI `?u32`; zde `int32_t`, kde −1 je
`null`):

| Hodnota | Význam | Kdy |
|---|---|---|
| `0` | další snímek hned | někdo zavolal `dvui_refresh` (layout se ustaluje, stateFade…), nebo běží animace |
| `> 0` | další snímek nejpozději za tolik µs | všechny použité animace a časovače začínají v budoucnu; vrátí se nejbližší začátek |
| `−1` | čekat na událost | nic neběží ani není naplánované |

Mezi `begin` a `end` je okno „aktuální“ a fungují globální funkce
(`dvui_parent_get`, `dvui_events`, `dvui_data_get`, `dvui_animation`, …).
Mimo tento úsek je jejich volání chyba (program skončí s hlášením).
`dvui_frame_time_ns()` vrací čas začátku snímku (DVUI `frameTimeNS`)
a `dvui_seconds_since_last_frame()` dobu od minulého snímku.

## 2. Animace, časovače a Animate

Podrobně (časová osa, stárnutí, životnost, čekání) v [ANIMACE.md](ANIMACE.md).

**Animace** (DVUI `Animation`) – hodnota, která se mění od `start_val`
k `end_val` v čase `start_time` … `end_time` (µs **relativně k aktuálnímu
snímku**, okno je každý snímek posune):

```c
typedef struct dvui_Animation {
    dvui_EasingFn* easing;   // NULL = dvui_easing_linear
    float start_val;
    float end_val;           // v C nutno zadat (DVUI výchozí 1)
    int32_t start_time;      // 0 = teď
    int32_t end_time;        // délka v µs
} dvui_Animation;
```

| Funkce | DVUI | Význam |
|---|---|---|
| `dvui_animation(id, "klíč", a)` | `dvui.animation` | uloží (nebo přepíše) animaci pod `(id, klíč)` |
| `dvui_animation_get(id, "klíč", &a)` | `dvui.animationGet` | false, když animace není; čtení ji udrží naživu |
| `dvui_animation_value(&a)` | `Animation.value` | aktuální hodnota (easing, mezi `start_val` a `end_val`) |
| `dvui_animation_done(&a)` | `Animation.done` | true v posledním snímku (`end_time <= 0`) |
| `dvui_reduce_motion` | `dvui.reduce_motion` | globální proměnná: animace skončí hned v příštím snímku |

```c
// kliknutí spustí „wiggle“ na 0,5 s
if (dvui_button_widget_clicked(&bw))
    dvui_animation(bw.wd.id, "xoffset", (dvui_Animation){.start_val = 0, .end_val = 1, .end_time = 500000});

dvui_Animation a;
if (dvui_animation_get(bw.wd.id, "xoffset", &a)) {
    const float v = dvui_animation_value(&a);
    /* … posun podle v … */
}
```

Animace žije, dokud se na ni někdo každý snímek ptá (nebo ji nastavuje)
a dokud neskončí; v posledním snímku je `done` a v dalším zmizí.

**Easing** (DVUI `easing.zig`) – funkce `float f(float t)`:
`dvui_easing_linear`, `…_in_quad`, `…_out_quad`, `…_in_out_quad`, stejně
`cubic`, `quart`, `quint`, `sine`, `expo`, `circ`, `elastic`, `back`,
`bounce` (31 funkcí). Seznam s názvy je v poli `dvui_easings`
(`dvui_easings_len`), X-makro `DVUI_EASINGS`. `elastic` a `back`
přesahují interval [0, 1].

**Časovače** (DVUI `timer`) – animace s klíčem `"_timer"`, která začíná
i končí za `micros`. Nežádá snímky průběžně; `dvui_window_end` jen vrátí
čas do vypršení:

| Funkce | Význam |
|---|---|
| `dvui_timer(id, micros)` | časovač na `micros` µs |
| `dvui_timer_get(id, &left)` | zbývající µs (≤ 0 po vypršení); false = žádný časovač |
| `dvui_timer_done(id)` | true v prvním snímku po vypršení (právě jednou) |
| `dvui_timer_done_or_none(id)` | `timer_done` nebo žádný časovač – pro periodické akce |
| `dvui_timer_remove(id)` | zruší časovač (DVUI `Window.timerRemove`) |

```c
// DVUI Examples – hodiny: snímek na začátku každé sekundy
const int32_t left = (int32_t)((dvui_frame_time_ns() / 1000000) % 1000);
if (dvui_timer_done_or_none(id)) dvui_timer(id, 1000 * (1000 - left));
```

**AnimateWidget** (DVUI `AnimateWidget`, `dvui.animate`) – obal, který
animuje svůj obsah:

| Vlastnost | Typ | Význam |
|---|---|---|
| `kind` (`DVUI_KIND`) | `dvui_AnimateKind` | `DVUI_ANIMATE_ALPHA` (průhlednost), `…_VERTICAL` (výška), `…_HORIZONTAL` (šířka), `…_NONE` (výchozí) |
| `duration` (`DVUI_DURATION`) | `int32_t` | délka v µs (v DVUI povinná) |
| `easing` (`DVUI_EASING`) | `dvui_EasingFn*` | NULL = linear |

```c
dvui_AnimateWidget* a = dvui_animate(DVUI_KIND(DVUI_ANIMATE_VERTICAL), DVUI_DURATION(300000),
                                     DVUI_EASING(dvui_easing_out_bounce), DVUI_EXPAND(DVUI_EXPAND_BOTH));
if (start) dvui_animate_widget_start(a);       // znovu „_start“ 0 → 1
if (end) dvui_animate_widget_start_end(a);     // „_end“ 1 → 0
/* obsah */
if (dvui_animate_widget_end(a)) { /* poslední snímek „_end“ – obsah lze skrýt */ }
dvui_animate_widget_deinit(a);
```

V prvním snímku widget sám spustí `"_start"`. `alpha` násobí průhlednost
všeho kreslení uvnitř (`dvui_alpha`), `vertical`/`horizontal` zmenší
obdélník widgetu (jen když je větší než jeho min. velikost, tj. roztažený)
a v `deinit` stejně zmenší nahlášenou min. velikost.

## 3. Widget a WidgetData

### 3.1 Rozhraní `dvui_Widget`

Každý widget, který může mít děti, poskytuje tabulku funkcí:

```c
typedef struct dvui_WidgetVTable {
    dvui_WidgetData* (*data)(void* ptr);
    dvui_Rect (*rect_for)(void* ptr, dvui_Id id, dvui_Size min_size, dvui_Expand e, dvui_Gravity g);
    dvui_RectScale (*screen_rect_scale)(void* ptr, dvui_Rect r);
    void (*min_size_for_child)(void* ptr, dvui_Size s);
} dvui_WidgetVTable;

typedef struct dvui_Widget { void* ptr; const dvui_WidgetVTable* vtable; } dvui_Widget;
```

| Funkce | Kontrakt |
|---|---|
| `data` | vrátí `WidgetData` widgetu (pro id, rect, options rodiče) |
| `rect_for(id, min_size, e, g)` | dítě s id `id` a min. velikostí `min_size` (už včetně margin/border/padding) se ptá, kde má být. Vrací obdélník **v souřadnicích content rectu tohoto widgetu**. Volá se jednou za dítě, z `dvui_widget_data_init` dítěte. |
| `screen_rect_scale(r)` | převede obdélník dítěte na fyzické pixely + měřítko. Typicky `rect_to_rect_scale(content_rect_scale, r)`; v DVUI ho některé kontejnery mění (ScrollContainer, ScaleWidget), v dvui-animation žádný. |
| `min_size_for_child(s)` | dítě v `deinit` hlásí svou min. velikost. Volá se jednou za dítě, po jeho `rect_for`. |

Volání přes rozhraní: `dvui_widget_data(w)`, `dvui_widget_rect_for(w, …)`,
`dvui_widget_screen_rect_scale(w, r)`, `dvui_widget_min_size_for_child(w, s)`,
`dvui_widget_extend_id(w, src, id_extra)`.

Každý widget má funkci `dvui_<widget>_widget_widget(self)`, která vrátí
jeho `dvui_Widget`.

### 3.2 `dvui_WidgetData`

Společná data všech widgetů (podrobně v
[dvui-layout/docs/LAYOUT.md](../../dvui-layout/docs/LAYOUT.md), kap. 4):

```c
struct dvui_WidgetData {
    dvui_Id id;
    dvui_Widget parent;
    dvui_WidgetDataInitOptions init_options;  // subwindow, no_scroll_when_focused, id_override
    dvui_Rect rect;                           // v souřadnicích obsahu rodiče
    dvui_Size min_size;
    dvui_Options options;
    dvui_Src src;
    bool has_rect_scale;
    dvui_RectScale rect_scale;                // platí po register
};
```

Funkce v pořadí, v jakém je widget volá:

| Funkce | Kdy |
|---|---|
| `dvui_widget_data_init(src, init_options, opts)` | na začátku `init`; zeptá se rodiče na obdélník |
| `dvui_widget_data_register(wd)` | po případné úpravě `rect`; spočítá `rect_scale` |
| `dvui_widget_data_border_and_background(wd, opts)` | kreslení okraje a pozadí |
| `dvui_widget_data_focus_border(wd)` | rámeček fokusu (kreslí se „after“) |
| `dvui_widget_data_min_size_max(wd, s)` | v `min_size_for_child`/`deinit`: zvětší `min_size` |
| `dvui_widget_data_min_size_set_and_refresh(wd)` | v `deinit`: omezí `max_size`, uloží, případně refresh |
| `dvui_widget_data_min_size_report_to_parent(wd)` | v `deinit`: nahlásí rodiči (pokud nemá `options.rect`) |

Obdélníky: `…_border_rect`, `…_background_rect`, `…_content_rect`
a jejich `…_rect_scale` varianty, `dvui_widget_data_rect_scale`,
`dvui_widget_data_visible` (průnik s clipem není prázdný),
`dvui_widget_data_is_root`.

### 3.3 Životní cyklus widgetu

Každý widget má `*_init` a `*_deinit`; mezi nimi jsou funkce specifické
pro widget (`process_events`, `draw_background`, …):

```c
dvui_ButtonWidget bw;
dvui_button_widget_init(&bw, DVUI_SRC, DVUI_PROPS(DVUI_GRAYED(true)));  // init + register + parent_set
dvui_button_widget_process_events(&bw);                                 // události
dvui_button_widget_draw_background(&bw);                                // kreslení
/* děti */
dvui_button_widget_deinit(&bw);                                         // min. velikost + parent_reset
```

„Pohodlná“ makra (`dvui_box`, `dvui_overlay`, `dvui_animate`) doplní `DVUI_SRC`, widget alokují, zavolají `init`
a obvyklé kroky a vrátí ukazatel; ukončuje se stejným `*_deinit`, který
paměť uvolní. Funkce, které widget celý vytvoří i ukončí, vracejí výsledek
přímo: `dvui_button` (kliknuto), `dvui_spacer`/`dvui_separator`/
`dvui_label_placeholder` (kopie `WidgetData` pro zjištění obdélníku).

Widget ve vlastní paměti se nesmí během snímku přesunout (děti si drží
ukazatel na rodiče).

## 4. Vlastnosti widgetů (seznam značek)

Widget dostane **jeden seznam vlastností** (tag list). Každá položka je
dvojice značka + hodnota (`dvui_Prop`); co v seznamu není, je „nenastaveno“
a platí výchozí hodnota widgetu. Když je vlastnost v seznamu víckrát, platí
poslední.

```c
dvui_BoxWidget* row = dvui_box(DVUI_DIR(DVUI_HORIZONTAL), DVUI_EXPAND(DVUI_EXPAND_HORIZONTAL), DVUI_MARGIN_ALL(4));
if (dvui_button(DVUI_LABEL_SIZE(40, 12), DVUI_FILL(DVUI_RGB(200, 60, 30)), DVUI_STYLE(DVUI_STYLE_HIGHLIGHT))) { … }
dvui_spacer(DVUI_P(min_size_content, s), DVUI_ID_EXTRA(i));   // obecný tvar DVUI_P(jméno, hodnota)
```

Vlastnosti jsou dvojího druhu (obojí je ve stejném seznamu):

- **společné** (DVUI `Options`) – rozumí jim všechny widgety: `id_extra`,
  `name`, `rect`, `expand`, `gravity_x/y`, barvy (`color_fill`,
  `color_fill_hover`, `color_fill_press`, `color_text*`, `color_border`),
  `style`, `margin`, `border`, `padding`, `min_size_content`,
  `max_size_content`, `background`. Po vyhodnocení se uloží do
  `wd.options` (`dvui_Options`).
- **vlastnosti widgetu** (DVUI `InitOptions`) – `dir`, `equal_space`,
  `num_packed_expanded` u boxu, `label_size`, `draw_focus`, `grayed`
  u tlačítka a zástupce popisku, `kind`, `duration`, `easing` u Animate. Které
  vlastnosti widget zná, je uvedeno u jeho deklarace v `dvui.h`. Vlastnost,
  které widget nerozumí, se ohlásí na stderr a ignoruje.

Všechny vlastnosti jsou vyjmenované na jednom místě (X-makra
`DVUI_OPTION_PROPS` a `DVUI_WIDGET_PROPS` v `dvui.h`) i s typem a obecnou
výchozí hodnotou. Pro časté vlastnosti jsou zkratky (`DVUI_EXPAND`,
`DVUI_MARGIN_ALL`, `DVUI_MIN_SIZE(w, h)`, `DVUI_FILL`, `DVUI_DIR`, `DVUI_KIND`, …);
`DVUI_P(jméno, hodnota)` funguje pro každou. Hodnota se kontroluje typem
pole v unii `dvui_Prop` – pozor jen na implicitní převody čísel
(`DVUI_P(expand, 0.5f)` projde).

**Vyhodnocení** (`dvui_props_resolve`): obecné výchozí hodnoty → výchozí
seznam widgetu (`dvui_button_widget_defaults` apod., DVUI `defaults`) →
seznam uživatele. Výsledkem je `dvui_Options` (společné vlastnosti; maska
`set` říká, které byly zadány – DVUI `?T != null`) a `dvui_WidgetProps`
(vlastnosti widgetu; `has[značka]` říká, které byly zadány).

**Předání stylu dál:** značka `DVUI_OPTIONS(&opts)` vloží do seznamu hotové
`dvui_Options` (přepíše jen zadané položky). Tak widgety skládají vnitřní
widgety: `DVUI_PROPS(DVUI_OPTIONS(&strip), DVUI_EXPAND(DVUI_EXPAND_BOTH))`.

| Funkce / makro | Význam |
|---|---|
| `DVUI_PROPS(…)` | seznam jako výraz typu `dvui_Props` (pro `*_widget_init`) |
| `dvui_options_default()` | Options bez zadaných položek |
| `dvui_options_with(o, …)` | kopie `o` s aplikovaným seznamem (jen společné vlastnosti) |
| `dvui_options_has(&o, DVUI_BIT(x))` | byla vlastnost `x` zadána? |
| `dvui_options_override(a, b)` | položky zadané v `b` přepíší `a` |
| `dvui_options_strip(o)` | ponechá jen styl a barvy, okraje vynuluje (pro vnitřní pomocné widgety) |
| `dvui_options_style_only(o)` | jen styl a barvy |
| `dvui_options_pad_size(o, s)` | `s + padding + border + margin` |
| `dvui_options_min_size_get`, `…_max_size_get`, `…_gravity_get` | odvozené hodnoty |
| `dvui_options_color(o, ask)` | barva z Options, jinak z tématu podle `style` |
| `dvui_props_forward(&wp, tags, out)` | zkopíruje zadané vlastnosti widgetu (předání vnitřnímu widgetu) |

Pole `dvui_Options` se čtou přímo (`wd->options.margin`); nezadané pole má
výchozí hodnotu.

**Barvy:** nezadané hover/press barvy se odvodí ze zadané `color_fill`
ztmavením (`dvui_theme_adjust_color_for_state`), jinak z tématu
(`dvui_theme_color`). Styly: `CONTENT` (výchozí), `CONTROL` (tlačítka),
`WINDOW` (pozadí oken a scroll oblastí), `HIGHLIGHT`, `ERR`. V tématu
znamená barva `{0, 0, 0, 0}` „nenastaveno“.

## 5. Id a DVUI_SRC

Id identifikuje widget mezi snímky – podle něj se hledá min. velikost
z minula, data, capture i fokus.

```c
#define DVUI_SRC  ((dvui_Src){__FILE__, __LINE__, c, __FILE__ ":" "řádek" ":" "c"})   // c = __COUNTER__
id = hash(id rodiče, adresa literálu "soubor:řádek:c", id_extra)
```

`DVUI_SRC` kromě souboru a řádku (pro chybové hlášky) vytvoří řetězcový
literál `"soubor:řádek:čítač"`, který je pro každé místo ve zdrojáku jiný.
Do id vstupuje jeho **adresa** (`src.anchor`): literály s různým obsahem
nikdy nezačínají na stejné adrese a adresa je po celý běh programu stálá.
DVUI hashuje podobně ukazatel na jméno souboru, řádek a sloupec.

- Každé místo ve zdrojáku dává jiné id; stejné místo volané víckrát (smyčka,
  pomocná funkce) potřebuje různé `id_extra` (`DVUI_ID_EXTRA(i)`).
- `dvui_id_extend(start, src, id_extra)` – odvození id;
  `dvui_id_update(id, "jméno")` – odvození klíče pro data.
- `init_options.id_override` – vlastní id.
- Id se liší mezi běhy programu (ASLR) a mezi moduly (EXE, DLL mají vlastní
  kopie literálů); v rámci jednoho běhu jsou stálá. Stav mezi běhy se
  neukládá, takže to nevadí.
- Id 0 znamená „žádné“ (fokus, capture, cíl události).
- Duplicitní id v jednom snímku se hlásí na stderr.

## 6. Rodič (parent)

| Funkce | Význam |
|---|---|
| `dvui_parent_get()` | aktuální rodič – nový widget se ho v `init` zeptá na obdélník |
| `dvui_parent_set(w)` | kontejner se po `register` stane rodičem |
| `dvui_parent_reset(id, prev)` | v `deinit` vrátí předchozího rodiče; kontroluje, že `id` je opravdu aktuální rodič (jinak hlásí neukončené widgety) |

Kořenem je okno. Widgety se musí ukončovat v opačném pořadí, než vznikly.

## 7. Události

Události jednoho snímku jsou v poli `dvui_events(&count)`. Každý widget
projde celé pole a vybere si ty, které mu patří:

```c
size_t n;
dvui_Event* evts = dvui_events(&n);
for (size_t i = 0; i < n; i++) {
    dvui_Event* e = &evts[i];
    if (!dvui_event_match_simple(e, &self->wd)) continue;   // patří mi?
    if (e->kind == DVUI_EVENT_MOUSE && e->evt.mouse.action == DVUI_MOUSE_PRESS) {
        dvui_event_handle(e, &self->wd);                     // zpracováno – ostatní ji už neuvidí
        /* … */
    }
}
```

**Druhy** (`dvui_Event.kind`): `MOUSE`, `KEY`, `WINDOW` (zavření, odchod
myši), `APP_QUIT`.

**Akce myši** (`dvui_EventMouse.action`):

| Akce | Kdy |
|---|---|
| `FOCUS` | těsně před `PRESS` levým tlačítkem; widget si podle ní bere fokus |
| `PRESS`, `RELEASE` | tlačítko myši |
| `MOTION` | pohyb (`motion` = posun ve fyzických pixelech) |
| `POSITION` | **jedna na konci seznamu každého snímku** s aktuální pozicí myši – pro hover |
| `WHEEL_X`, `WHEEL_Y` | kolečko (`wheel` v logických pixelech; Shift + svislé = vodorovné) |

`p` je pozice ve fyzických pixelech, `floating_win` je subwindow pod myší.

**`dvui_event_match(e, {id, focus_id, r, drag_name})`** (DVUI `eventMatch`)
vrátí true, pokud:

- událost ještě není zpracovaná,
- klávesa: cílí na widget `id` (nebo `focus_id`) – cílem je fokusovaný widget,
- myš s capture: capture má `id` (pak vždy true); má-li ho jiný widget,
  projde jen `POSITION` pro widget, který se s ním překrývá ve stejném
  subwindow,
- myš: neprobíhá cizí pojmenované tažení, myš je nad aktuálním subwindow,
  uvnitř `r` a uvnitř clipu.

`dvui_event_match_simple(e, wd)` = match s `id` widgetu a jeho border rectem.

**`dvui_clicked(wd, opts)`** (DVUI `clicked`) – kompletní obsluha
„tlačítka“: fokus při `FOCUS`, capture při `PRESS`, kliknutí při `RELEASE`
uvnitř, kurzor ruky a `hovered` při `POSITION`, Enter/mezerník u fokusu.

Události přidává backend (`dvui_sdl3gpu_add_all_events`) nebo test
(`dvui_window_add_event_mouse_motion`, `…_mouse_button`, `…_mouse_wheel`,
`…_key`, `…_window`, `…_app_quit`).

## 8. Capture, tažení a fokus

**Capture myši** – widget si „přivlastní“ myš, aby dostával pohyb
a puštění tlačítka i mimo svůj obdélník:

- `dvui_capture_mouse(wd, e->num)` – od události `num` dál cílí události
  myši (kromě kolečka) na tento widget; `dvui_capture_mouse(nullptr, num)` uvolní,
- `dvui_captured(id)` – má widget capture?
- capture se udržuje jen dokud widget každý snímek existuje (`register`
  volá `dvui_capture_mouse_maintain`) a dokud nad ním není modální okno.

**Tažení** (DVUI `Dragging.zig`):

- `dvui_drag_pre_start(button, p, opts)` – možné tažení; skutečné začne po
  posunu o víc než 3 natural pixely,
- `dvui_drag_start(…)` – tažení hned,
- `dvui_dragging(me, name, &dp)` – probíhá tažení? vrátí posun od minula,
- `dvui_drag_offset(button)`, `dvui_drag_end(button)`.
- `opts.name` pojmenuje tažení mezi widgety (drag & drop); widgety, které
  jméno neznají, pak události myši nedostanou.

**Fokus**:

- každé subwindow má fokusovaný widget; `dvui_focus_widget(id, subwindow, &num)`
  ho nastaví (a přesměruje klávesy od události `num`),
- `dvui_focus_subwindow(id, &num)` – fokus okna; `dvui_raise_subwindow(id)` –
  okno navrch,
- `dvui_focused_widget_id()`, `dvui_last_focused_id_in_frame()` – kontejnery
  podle toho poznají, že fokus získal jejich potomek.

## 9. Datové úložiště

Widget si mezi snímky pamatuje stav pod klíčem `(id, "jméno")`
(DVUI `dataGet`/`dataSet`). Jak to funguje uvnitř (mapy, úklid, koš,
seznam klíčů), popisuje dokumentace dvui-layout
([STAV.md](../../dvui-layout/docs/STAV.md)) – mechanismus je stejný. Samotné úložiště
(`dvui_DataStore`) je sdílené (`../shared/include/dvui/data.h`); okno ho
obsahuje a funkce níže jen spočítají klíč z id a jména a zavolají ho.

| Funkce | Význam |
|---|---|
| `dvui_data_set(win, id, key, &value, size)` | uloží kopii |
| `dvui_data_get(win, id, key, size)` | ukazatel na uloženou hodnotu, nebo `NULL` |
| `dvui_data_get_ptr_default(win, id, key, size, &def)` | ukazatel; když hodnota není, uloží `def` |
| `dvui_data_get_slice(win, id, key, &size)` | pole proměnné délky (délka v bajtech) |
| `dvui_data_remove(win, id, key)` | smaže |

`win` může být `nullptr` (= aktuální okno). Typová makra:
`DVUI_DATA_GET(id, key, T)`, `DVUI_DATA_SET(id, key, T, val)`,
`DVUI_DATA_GET_PTR_DEFAULT(id, key, T, def)`.

Pravidla:

- **Hodnota, na kterou se během snímku nikdo nezeptal ani ji nenastavil,
  se na začátku dalšího snímku smaže.** Widget, který zmizí, tak
  automaticky přijde o svá data.
- Ukazatel z `get` platí, dokud se pod stejným klíčem neuloží hodnota
  **jiné velikosti** – a i pak až do konce snímku (starý blok se uvolní až
  v dalším `begin`). Zápis stejné velikosti přepisuje na místě, takže
  ukazatel lze používat jako „proměnnou widgetu“ (tak to dělá stateFade).
- Při `get` s jinou velikostí, než má uložená hodnota, se vrátí `NULL`
  a ohlásí chyba.

Klíče používané widgety (shodné s DVUI): box `"_data"`, hover
`"__hover_fade"`. Animace mají vlastní mapu (kap. 2) s klíči `"_start"`,
`"_end"` (Animate) a `"_timer"` (časovače).

## 10. Kreslení, clip a subwindows

| Funkce | Význam |
|---|---|
| `dvui_render_fill_rect(r, color, after)` | plný obdélník ve fyzických pixelech |
| `dvui_render_stroke_rect(r, thickness, color, after)` | obrys; `r` je středová čára |
| `dvui_clip_get()`, `dvui_clip(r)` (průnik, vrací starý), `dvui_clip_set(r)` | ořez ve fyzických pixelech |
| `dvui_alpha(mult)`, `dvui_alpha_set(a)` | průhlednost dalšího kreslení: `dvui_alpha` vynásobí aktuální alfu a vrátí starou (tak to dělá Animate `alpha`) |
| `dvui_subwindow_add`, `dvui_subwindow_current_set`, `dvui_subwindow_current_id` | registrace subwindow a přepnutí aktuálního |

`after = true` znamená „nakreslit po ostatním obsahu subwindow“ (rámeček
fokusu). Barva se při kreslení násobí alfou okna (`dvui_color_opacity`)
a teprve pak premultiplikuje; odložené příkazy si alfu pamatují.

dvui-animation nemá plovoucí widgety, takže jediné subwindow je okno samo.

## 11. Backend

Backend je struktura ukazatelů na funkce (DVUI `Backend.zig`). Rozhraní
i oba backendy jsou sdílené s ostatními projekty – leží v `../shared/`
(hlavičky `dvui/base.h`, `dvui/backend_sdl3gpu.h`,
`dvui/backend_testing.h`). Backend volá funkce okna
`dvui_window_add_event_*`, `dvui_window_modifiers_set` a čte
`dvui_scroll_speed`; ty implementuje `src/window.c` (rozhraní mezi
backendem a oknem popisuje [shared/README.md](../../shared/README.md)).

```c
typedef struct dvui_Backend {
    void* ctx;
    void (*begin)(void* ctx);
    void (*end)(void* ctx);
    dvui_SizePhysical (*pixel_size)(void* ctx);
    dvui_SizeNatural (*window_size)(void* ctx);
    float (*content_scale)(void* ctx);
    void (*draw_clipped_triangles)(void* ctx, const dvui_Vertex* vtx, size_t vtx_count,
                                   const uint32_t* idx, size_t idx_count, const dvui_RectPhysical* clip);
    void (*set_cursor)(void* ctx, dvui_Cursor cursor);
    void (*render_present)(void* ctx);
    int64_t (*nano_time)(void* ctx);
    void (*sleep)(void* ctx, uint64_t ns);
} dvui_Backend;
```

| Funkce | Kdy se volá | Co má udělat |
|---|---|---|
| `begin` | v `dvui_window_begin`, před `pixel_size` | připravit snímek (SDL3 GPU: command buffer, swapchain textura) |
| `pixel_size` | v `begin` | velikost framebufferu ve fyzických pixelech |
| `window_size` | v `begin` | velikost okna v natural jednotkách |
| `content_scale` | v `begin` (může být `NULL` = 1) | měřítko displeje; dělí se jím velikost okna |
| `draw_clipped_triangles` | během snímku i v `end` (fronty subwindows) | vykreslit trojúhelníky; `clip == NULL` = bez ořezu, jinak scissor v celých pixelech. Vrcholy: pozice ve fyzických pixelech, barva **premultiplikovaná**, UV (u obdélníků 0) |
| `end` | v `dvui_window_end` po vykreslení front | odeslat vykreslení (SDL3 GPU: nahrát buffery, render pass) |
| `set_cursor` | v `end` | nastavit kurzor |
| `render_present` | na konci `end` | zobrazit snímek |
| `nano_time` | `dvui_window_begin_wait`, `dvui_window_wait_time` | monotónní čas v ns |
| `sleep` | `dvui_window_wait_time` (při `max_fps`), `dvui_window_begin_wait` (`sleep(0)` při dotáčení) | uspat vlákno na `ns` nanosekund; smí být NULL |

**SDL3 GPU** (`../shared/include/dvui/backend_sdl3gpu.h`): `dvui_sdl3gpu_create`
(okno, GPU zařízení, pipeline, shadery), `dvui_sdl3gpu_backend`,
`dvui_sdl3gpu_add_all_events` / `dvui_sdl3gpu_add_event` (převod událostí
SDL), `dvui_sdl3gpu_wait_event_timeout(b, timeout_micros)` (čekání na
událost; `UINT32_MAX` = bez limitu, 0 = nečekat; vrací, zda čekání
přerušila událost), `dvui_sdl3gpu_destroy`.

**Testovací backend** (`../shared/include/dvui/backend_testing.h`): bez GPU;
velikost okna zadává test, vykreslené obdélníky ukládá do `draws`
(ohraničení, barva, clip) pro kontrolu.

## 12. Správa paměti

dvui-animation nepodporuje vlastní alokátory; používá `malloc`/`free` a jednu
vlastní arénu (aréna, hash mapa a datové úložiště jsou sdílené –
`../shared/src/data.c`). Při nedostatku paměti program skončí (`abort`).

| Co | Jak | Jak dlouho žije |
|---|---|---|
| **aréna snímku** (`dvui_frame_alloc`, `dvui_frame_grow`) | bloky ≥ 64 KiB, jen posun ukazatele; `dvui_window_end` ji vynuluje a bloky se použijí znovu | do konce snímku |
| – co v ní je | seznam událostí, fronty kreslení subwindows, dočasné kopie | |
| **datové úložiště** | hash mapa → každá hodnota vlastní `malloc`; nahrazené/smazané bloky jdou do koše, uvolní se v dalším `begin` | dokud se hodnota používá |
| **min. velikosti** | hash mapa → `malloc` po `dvui_Size` | do snímku, kdy widget nevznikne |
| **animace a časovače** | hash mapa → `malloc` po `dvui_Animation` | do konce animace, nebo do snímku, kdy se na ni nikdo nezeptá |
| **pohodlné widgety** (`dvui_box`, …) | `calloc` + seznam v okně; `*_deinit` uvolní, co zbude, uvolní `dvui_window_end` | do `*_deinit` |
| **subwindows, koš, seznam widgetů** | `realloc` | po dobu života okna |
| **backend SDL3 GPU** | vrcholy, indexy a volání v polích přes `realloc` (nezmenšují se); GPU buffery jen rostou | po dobu života backendu |

Pole z arény se při růstu kopírují a staré místo zůstane v aréně do konce
snímku (jako `ArrayList` nad arénou v Zigu). Ukazatele do arény proto
nedržte přes `dvui_frame_grow` ani přes konec snímku.

## 13. Příklad: vlastní kontejner

Kontejner, který skládá děti vedle sebe (`BasicLayout`), s paddingem
a pozadím. Ukazuje celé rozhraní: vtable, `init` (WidgetData → register →
kreslení → `parent_set`) a `deinit` (min. velikost → nahlášení → `parent_reset`).

```c
typedef struct RowWidget {
    dvui_WidgetData wd;
    dvui_BasicLayout layout;
} RowWidget;

static dvui_WidgetData* row_data(void* ptr) { return &((RowWidget*)ptr)->wd; }

static dvui_Rect row_rect_for(void* ptr, dvui_Id id, dvui_Size min_size, dvui_Expand e, dvui_Gravity g) {
    RowWidget* self = ptr;
    // děti dostávají místo v content rectu posunutém do (0, 0)
    return dvui_basic_layout_rect_for(&self->layout, dvui_rect_just_size(dvui_widget_data_content_rect(&self->wd)),
                                      id, min_size, e, g);
}

static dvui_RectScale row_screen_rect_scale(void* ptr, dvui_Rect r) {
    RowWidget* self = ptr;
    return dvui_rect_scale_rect_to_rect_scale(dvui_widget_data_content_rect_scale(&self->wd), r);
}

static void row_min_size_for_child(void* ptr, dvui_Size s) {
    RowWidget* self = ptr;
    const dvui_Size children = dvui_basic_layout_min_size_for_child(&self->layout, s);
    dvui_widget_data_min_size_max(&self->wd, dvui_options_pad_size(&self->wd.options, children));
}

static const dvui_WidgetVTable row_vtable = {row_data, row_rect_for, row_screen_rect_scale, row_min_size_for_child};

void row_init(RowWidget* self, dvui_Src src, dvui_Props props) {
    static const dvui_Prop defaults[] = {DVUI_NAME("Row"), DVUI_PADDING_ALL(4), DVUI_BACKGROUND(true), {0}};
    dvui_Options opts;
    // bez vlastních vlastností: allowed = NULL, wp = NULL
    dvui_props_resolve("Row", defaults, props, nullptr, &opts, nullptr);
    *self = (RowWidget){
        .wd = dvui_widget_data_init(src, (dvui_WidgetDataInitOptions){0}, opts),
        .layout = {.dir = DVUI_HORIZONTAL},
    };
    dvui_widget_data_register(&self->wd);
    dvui_widget_data_border_and_background(&self->wd, (dvui_BorderAndBackgroundOptions){0});
    dvui_parent_set((dvui_Widget){self, &row_vtable});
}

void row_deinit(RowWidget* self) {
    dvui_widget_data_min_size_set_and_refresh(&self->wd);
    dvui_widget_data_min_size_report_to_parent(&self->wd);
    dvui_parent_reset(self->wd.id, self->wd.parent);
}
```

Použití – dva spacery 30×10 a 20×15:

```c
#define row(self, ...) row_init((self), DVUI_SRC, DVUI_PROPS(__VA_ARGS__))

RowWidget r;
row(&r);
dvui_spacer(DVUI_MIN_SIZE(30, 10));
dvui_spacer(DVUI_MIN_SIZE(20, 15));
row_deinit(&r);
```

| snímek | řádek | druhý spacer | `dvui_window_end` |
|---|---|---|---|
| 1 | 8×8 (jen padding, děti ještě neznámé) | {4, 4, 0, 0} | 0 (první snímek → refresh) |
| 2 | 58×23 (= 30 + 20 + 2·4, 15 + 2·4) | {34, 4, 20, 15} | −1 (ustáleno) |
