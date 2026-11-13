# Jak funguje layout

Tento dokument popisuje, jak dvui-layout (a stejně tak DVUI, ze kterého je
převzatý) počítá polohu a velikost widgetů. Odkazy typu `BoxWidget.zig – rectFor`
míří do `../../dvui-original/src`, odkazy na C funkce do `../src`.

Obsah:

1. [Základní myšlenka](#1-základní-myšlenka)
2. [Souřadnice a jednotky](#2-souřadnice-a-jednotky)
3. [Vrstvy obdélníku widgetu](#3-vrstvy-obdélníku-widgetu)
4. [Co z WidgetData a Options vstupuje do layoutu](#4-co-z-widgetdata-a-options-vstupuje-do-layoutu)
5. [Životní cyklus widgetu v jednom snímku](#5-životní-cyklus-widgetu-v-jednom-snímku)
6. [placeIn – umístění do dostupného místa](#6-placein--umístění-do-dostupného-místa)
7. [Kontejnery](#7-kontejnery)
   - [Window a BasicLayout](#71-window-a-basiclayout)
   - [Box](#72-box)
   - [Overlay, Button a další „jednodětné“ kontejnery](#73-overlay-button-a-další-jednodětné-kontejnery)
   - [ScrollContainer](#74-scrollcontainer)
   - [ScrollArea a ScrollBar](#75-scrollarea-a-scrollbar)
   - [Grid](#76-grid)
   - [Plovoucí widgety (subwindows)](#77-plovoucí-widgety-subwindows)
8. [Konvergence: refresh a více snímků](#8-konvergence-refresh-a-více-snímků)
9. [Ořez (clip) a pořadí kreslení](#9-ořez-clip-a-pořadí-kreslení)
10. [Časté situace a pasti](#10-časté-situace-a-pasti)

---

## 1. Základní myšlenka

dvui je *immediate-mode* GUI: program každý snímek znovu zavolá funkce
widgetů a strom widgetů vznikne znovu. Mezi snímky si dvui pamatuje jen
data uložená podle **id** widgetu.

Layout se počítá **jedním průchodem shora dolů**, přímo během volání
widgetů:

- **Shora dolů jdou obdélníky.** Když widget vzniká (`init`), zeptá se
  rodiče, kde má být (`rect_for`). Rodič mu odpoví hned – dřív, než
  widget vůbec ví, jaké bude mít děti.
- **Zdola nahoru jdou minimální velikosti.** Když widget končí (`deinit`),
  už zná své děti, spočítá svou min. velikost a nahlásí ji rodiči
  (`min_size_for_child`). Rodič ji použije až pro **svou** min. velikost.
- Rodič ale obdélník dítěti přidělil dřív, než dítě nahlásilo min. velikost.
  Proto se min. velikost každého widgetu **uloží podle id** a v **dalším
  snímku** se použije při přidělování obdélníku.

Důsledek: když se něco změní (nový widget, jiný obsah, jiná velikost okna),
layout se ustálí až po jednom nebo několika dalších snímcích. Widgety si samy
vyžádají další snímek (`dvui_refresh`), když poznají, že jejich min. velikost
z minula neplatí. `dvui_window_end` pak vrátí 0 („kresli hned znovu“).
Uživatel obvykle mezisnímky nevidí, protože se vykreslí bez čekání na
událost.

```
snímek N:    rodič.init → rect_for(dítě, min_size z N-1) → dítě.init → … → dítě.deinit
                                                                        → min_size_for_child(min_size N)
             rodič.deinit → uloží min_size N, porovná s N-1 → případně refresh
snímek N+1:  rodič.init dostane rect podle min_size N …
```

## 2. Souřadnice a jednotky

| Pojem | Význam |
|---|---|
| **fyzické pixely** (`Physical`) | pixely framebufferu; v nich jsou události myši, clip a vše, co jde do backendu |
| **natural** (`Natural`) | pixely okna tak, jak je hlásí OS (bez HiDPI násobku) |
| **logické** (bez přívlastku) | jednotky, ve kterých widget zadává velikosti (`min_size_content`, `margin`, …) |

- `dvui_Rect` widgetu (`wd.rect`) je v **logických jednotkách rodiče**
  a v **souřadnicích obsahu rodiče**: (0, 0) je levý horní roh content rectu
  rodiče (u ScrollContaineru roh virtuální plochy).
- `dvui_RectScale` = fyzický obdélník `r` + měřítko `s` (kolik fyzických
  pixelů je jedna logická jednotka). Každý widget si při `register` spočítá
  `wd.rect_scale` tak, že rodiči předá svůj `rect` a rodič (`screen_rect_scale`)
  ho přepočítá na obrazovku:
  `r_phys = rect · s_rodiče + počátek_obsahu_rodiče`.
- Okno: `wd.rect` = velikost okna v natural jednotkách děleno
  `content_scale` systému; `natural_scale = šířka_v_pixelech / wd.rect.w`
  (zaokrouhleno na setiny). Kořenový RectScale je `{rect_pixels, natural_scale}`.
- Měřítko mění jen `dvui_ScaleWidget` (násobí `s`) – plovoucí widgety ho
  používají, aby převzaly měřítko místa, odkud vznikly.
- ScrollContainer navíc posouvá počátek o `frame_viewport` (viz 7.4).

Převody: `dvui_rect_scale_rect_to_physical`, `dvui_rect_scale_rect_from_physical`,
`dvui_rect_scale_point_to_physical`, `dvui_rect_scale_point_from_physical`.

## 3. Vrstvy obdélníku widgetu

```
wd.rect  ─────────────────────────────────────────────┐  ← přidělí rodič
│ margin                                              │
│   ┌ border rect ─────────────────────────────────┐  │  ← sem patří události myši (clicked, eventMatch)
│   │ border                                       │  │
│   │   ┌ background rect ───────────────────────┐ │  │  ← sem se kreslí pozadí (background = true)
│   │   │ padding                                │ │  │
│   │   │   ┌ content rect ────────────────────┐ │ │  │  ← sem se umisťují děti
│   │   │   │                                  │ │ │  │
│   │   │   └──────────────────────────────────┘ │ │  │
│   │   └────────────────────────────────────────┘ │  │
│   └──────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────┘
```

Všechny čtyři okraje jsou `dvui_Rect`, kde `x` = vlevo, `y` = nahoře,
`w` = vpravo, `h` = dole. Funkce (`src/widget_data.c`):

| Obdélník | Výpočet | Funkce |
|---|---|---|
| border rect | `inset(rect, margin)` | `dvui_widget_data_border_rect`, `…_border_rect_scale` |
| background rect | `inset(border rect, border)` | `dvui_widget_data_background_rect`, `…_background_rect_scale` |
| content rect | `inset(background rect, padding)` | `dvui_widget_data_content_rect`, `…_content_rect_scale` |

`inset` nikdy nevrátí zápornou šířku nebo výšku (ořízne na 0).

Opačný směr – z velikosti obsahu na velikost celého widgetu – dělá
`dvui_options_pad_size(o, s) = s + padding + border + margin` (v obou osách
součet levé a pravé, resp. horní a dolní strany).

## 4. Co z WidgetData a Options vstupuje do layoutu

### Options (`dvui_Options`)

| Položka | Kdy se čte | Vliv |
|---|---|---|
| `min_size_content` | `WidgetData.init` | spodní mez velikosti obsahu; `min_size = pad(min(min_size_content, max_size_content))` |
| `max_size_content` | `init`, `minSizeSetAndRefresh` | horní mez min. velikosti, kterou widget nahlásí: `min_size = min(min_size, pad(max_size_content))`. **Neomezuje přidělený obdélník** – jen to, co widget hlásí rodiči |
| `margin`, `border`, `padding` | všude | vrstvy z kap. 3; vstupují do `pad_size` |
| `expand` | `init` → `rect_for` | zda a jak roztáhnout do dostupného místa (kap. 6); u boxu i váha při dělení místa |
| `gravity_x`, `gravity_y` | `init` → `rect_for` | kam v dostupném místě widget posunout (0 = začátek, 0.5 = střed, 1 = konec); u boxu i zda se dítě „balí“ (7.2) |
| `rect` | `init` | widget **mimo layout rodiče**: obdélník se nepočítá přes `rect_for` a min. velikost se rodiči nehlásí (7.6, 7.7) |
| `id_extra` | `init` | součást id → podle id se hledá min. velikost z minula a data |
| `background`, barvy, `style` | kreslení | do layoutu nevstupují |
| `name` | ladění | do layoutu nevstupuje |

Výchozí hodnoty nenastavených položek: okraje 0, `expand = NONE`,
gravitace 0, `min_size_content = 0×0`, `max_size_content = 2 000 000`
(`DVUI_MAX_FLOAT_SAFE`), `background = false`. Každý widget má vlastní
výchozí seznam vlastností (`dvui_button_widget_defaults` apod.), přes který se
aplikuje seznam uživatele (`dvui_props_resolve`, DVUI `defaults.override(opts)`).
Zda byla položka zadána, říká maska `options.set` (např. `rect`).

### WidgetData (`dvui_WidgetData`)

| Položka | Kdo ji nastavuje | Význam pro layout |
|---|---|---|
| `id` | `init` | klíč, pod kterým se ukládá min. velikost (`min_sizes`) a data widgetu |
| `parent` | `init` (= `dvui_parent_get()`) | komu se volá `rect_for` a `min_size_for_child` |
| `rect` | `init` (od rodiče nebo z `options.rect`) | přidělený obdélník; widget ho smí před `register` změnit (plovoucí okno) |
| `min_size` | `init` = min. velikost z Options; při `deinit` zvětšená podle dětí | to, co se uloží a nahlásí rodiči |
| `options` | `init` | viz výše |
| `rect_scale` | `register` (`rect_set`) | obrazovkový obdélník a měřítko; používají ho děti i kreslení |
| `init_options.subwindow` | widget | rect_scale se nepočítá přes rodiče, ale od okna (plovoucí widgety) |
| `init_options.id_override` | widget | použije se toto id místo odvozeného |

## 5. Životní cyklus widgetu v jednom snímku

Pořadí je u všech widgetů stejné (DVUI `WidgetData.zig`, `Widget.zig`):

### 5.1 `init` – `dvui_widget_data_init(src, init_options, opts)`

1. `parent = dvui_parent_get()` – aktuální rodič.
2. `id = id_override` nebo `hash(id rodiče, adresa místa ve zdrojáku, id_extra)` (viz `DVUI_SRC`).
3. `min_size = pad(min(min_size_content, max_size_content))` – co víme
   o sobě bez dětí.
4. `ms = max(min_size, min. velikost z minulého snímku)` – pokud widget
   minule existoval. Tady se projeví děti z minulého snímku.
5. Obdélník:
   - je-li `options.rect`: `rect = options.rect` s velikostí
     `w = (expand vodorovně) ? šířka obsahu rodiče : (rect.w == 0 ? ms.w : rect.w)`
     a obdobně `h`;
   - jinak `rect = parent.rect_for(id, ms, expand, gravity)`.

### 5.2 `register` – `dvui_widget_data_register(wd)`

- spočítá `rect_scale` (od rodiče nebo od okna u subwindow),
- udrží capture myši, pokud ho widget měl (`capture_mouse_maintain`),
- zapamatuje si, že tento widget je naposledy zaregistrovaný (kvůli fokusu),
- pokud má fokus a fokus se právě změnil, požádá nejbližší ScrollContainer,
  aby ho odscrolloval do viditelné oblasti (`dvui_scroll_to`),
- ohlásí chybu, pokud se id tento snímek už vyskytlo (duplicitní id).

Kontejnery pak zavolají `dvui_parent_set(self)` – od té chvíle jsou
rodičem pro další widgety.

### 5.3 Děti

Každé dítě projde stejným cyklem. Kontejner při tom pro každé dítě dostane
právě jedno volání `rect_for` (v `init` dítěte) a pak právě jedno volání
`min_size_for_child` (v `deinit` dítěte), v tomto pořadí. Box to kontroluje
(`child_id`).

### 5.4 `deinit`

1. Kontejner spočítá velikost obsahu z nahlášených min. velikostí dětí
   a zvětší svou: `wd.min_size = max(wd.min_size, pad(velikost dětí))`
   (`dvui_widget_data_min_size_max`).
2. `dvui_widget_data_min_size_set_and_refresh`:
   - `min_size = min(min_size, pad(max_size_content))`,
   - porovná s min. velikostí z minula a případně vyžádá další snímek
     (podmínky v kap. 8),
   - uloží `min_size` pod `id`.
3. `dvui_widget_data_min_size_report_to_parent` – zavolá
   `parent.min_size_for_child(min_size)`, **pokud** widget nemá
   `options.rect`.
4. `dvui_parent_reset(id, parent)` – vrátí rodiče (a zkontroluje, že se
   widgety ukončují ve správném pořadí).

## 6. placeIn – umístění do dostupného místa

`dvui_place_in(avail, min_size, expand, gravity)` (DVUI `dvui.placeIn`) je
základní stavební kámen všech kontejnerů:

1. `size = min_size`, ale nikdy víc než `avail` (v každé ose zvlášť).
2. Podle `expand`:
   - `HORIZONTAL`: `size.w = avail.w`,
   - `VERTICAL`: `size.h = avail.h`,
   - `BOTH`: `size = avail`,
   - `RATIO`: zachová poměr stran `min_size` a zvětší (nebo zmenší, pokud je
     `min_size` větší než `avail`) na největší velikost, která se vejde,
   - `NONE`: beze změny.
3. Pozice: `x = avail.x + gravity.x · (avail.w − size.w)`, obdobně `y`.

Příklady (`avail = {10, 20, 100, 50}`, `min = 30×10`):

| expand | gravity | výsledek |
|---|---|---|
| NONE | (0, 0) | {10, 20, 30, 10} |
| NONE | (1, 1) | {80, 60, 30, 10} |
| NONE | (0.5, 0.5) | {45, 40, 30, 10} |
| HORIZONTAL | (0, 0) | {10, 20, 100, 10} |
| BOTH | – | {10, 20, 100, 50} |

## 7. Kontejnery

Kontejner implementuje rozhraní `dvui_Widget` (viz `ROZHRANI.md`):
`rect_for` rozhoduje, kam dát dítě; `screen_rect_scale` převádí obdélník
dítěte na obrazovku; `min_size_for_child` sbírá min. velikosti dětí.

### 7.1 Window a BasicLayout

Okno (`src/window.c`) je kořen stromu: jeho `rect_for` používá
`dvui_BasicLayout` se směrem `VERTICAL` nad `wd.rect` okna, `min_size_for_child`
nic nedělá (okno OS se dětem nepřizpůsobuje).

**BasicLayout** (DVUI `layout.zig`) je nejjednodušší skládání – používá ho
okno, `ScaleWidget` a buňka gridu:

- `rect_for`: z dostupného obdélníku odečte už použitou část (`pos`) ve směru
  skládání a na zbytek zavolá `placeIn`; `pos += výška (šířka) výsledku`.
- Dítě roztažené ve směru skládání dostane celý zbytek; další dítě za ním
  už nedostane nic (hlásí se chyba „child after expanded child“).
- `min_size_for_child`: ve směru skládání sčítá, v druhém směru bere maximum.

### 7.2 Box

`dvui_BoxWidget` (DVUI `BoxWidget.zig`) skládá děti ve směru `dir`
(vodorovně/svisle). Na rozdíl od BasicLayout umí rozdělit volné místo mezi
**víc** roztažených dětí, balit děti od začátku i od konce a dávat všem
dětem stejně místa (`equal_space`).

**Data z minulého snímku** – klíč `"_data"` (`dvui_BoxData`):

| Položka | Význam |
|---|---|
| `packed_children` | kolik dětí se minule „balilo“ (viz níže) |
| `total_weight` | kolik z nich bylo roztažených ve směru skládání |
| `min_space_taken` | součet min. velikostí dětí ve směru skládání |

**Při prvním `rect_for`** (líně, aby šlo po `init` změnit rect):

- `child_rect` = content rect posunutý do (0, 0) – zbývající volné místo,
- `pixels_per_w` – kolik pixelů navíc dostane roztažené dítě:
  - běžně `max(0, dostupná_délka − min_space_taken) / total_weight`
    (místo `total_weight` lze vnutit `num_packed_expanded`),
  - u `equal_space`: `dostupná_délka / packed_children`.

**Pro každé dítě** (`rect_for`):

1. **Pozicované dítě:** má-li gravitaci ve směru skládání ostře mezi 0 a 1
   (např. `gravity_x = 0.5` ve vodorovném boxu), nebalí se – umístí se
   `placeIn` do celého content rectu (jako v overlay) a místo neubírá.
2. Jinak se balí: `packed_children += 1`; je-li roztažené ve směru skládání
   (HORIZONTAL/BOTH ve vodorovném boxu), má váhu 1 a `total_weight += 1`.
3. `EXPAND_RATIO`: min. velikost se přepočítá tak, aby dítě vyplnilo tloušťku
   boxu při zachování poměru stran; navýšení se pamatuje jako `ratio_extra`.
4. Obdélník:
   - **běžný box:**
     - první snímek boxu (nejsou data) a dítě je roztažené → `placeIn` s jeho
       `expand`, tj. první roztažené dítě dostane všechno zbývající místo;
     - jinak `ms.délka += pixels_per_w · váha` a `placeIn(child_rect, ms, ee, g)`,
       kde `ee` zachová jen roztažení **napříč** směrem skládání;
   - **equal_space:** dítěti se nejdřív vymezí úsek délky `pixels_per_w`
     (přes celou tloušťku) a dítě se do něj umístí `placeIn` se svým
     `expand`.
5. **Odebrání místa** (`remove_space`): z `child_rect` se ubere délka
   výsledku. Je-li gravitace ve směru skládání ≤ 0.5, ubírá se **zepředu**
   (další dítě je za ním); je-li 1, ubírá se **zezadu** – dítě se „přilepí“
   na konec a další děti zůstávají před ním.
6. Když dítě dostalo méně než svou min. velikost, nastaví se `ran_off`.

**`min_size_for_child`**: ve směru skládání sčítá (`min_space_taken`)
a hledá maximum (`max_space`, pro `equal_space`), napříč hledá maximum
(`max_thick`). Pozicované děti jen zvětšují min. velikost boxu jako overlay.

**`deinit`**:

- min. velikost obsahu = `(min_space_taken, max_thick)` (u `equal_space`
  `max_space · packed_children`), převedená na směr boxu,
- **refresh**, pokud box má roztažené děti (nebo `equal_space`) a
  - zbylo nevyužité místo (`child_rect` má délku > 0.001) – dítě zmizelo,
    přestalo být roztažené nebo se zmenšilo; nebo
  - `pixels_per_w > 0` a některé dítě přeteklo (`ran_off`),
- uloží `"_data"` pro další snímek.

**Příklad** (test `test_box_horizontal_expand`, okno 400×300): vodorovný box
s `expand = HORIZONTAL`, děti A (30×10, roztažená), B (50×20), C (10×10,
roztažená do obou směrů).

| snímek | box | A | B | C | proč |
|---|---|---|---|---|---|
| 1 | 400×0 | {0,0,400,0} | {400,0,0,0} | {400,0,0,0} | box nezná svou min. výšku (0); bez `"_data"` dostane první roztažené dítě všechno |
| 2 | 400×20 | {0,0,185,10} | {185,0,50,20} | {235,0,165,20} | min. výška 20 z minula; `pixels_per_w = (400 − 90) / 2 = 155` |
| 3 | stejné | | | | ustáleno, `dvui_window_end` vrátí −1 |

### 7.3 Overlay, Button a další „jednodětné“ kontejnery

`OverlayWidget`, `ButtonWidget`, `FloatingWidget`, `FloatingTooltipWidget`,
`FloatingWindowWidget` a `GridWidget` (pro svou scroll oblast) mají stejné
chování:

- `rect_for` = `placeIn(content rect posunutý do 0,0, min_size, expand, gravity)` –
  každé dítě dostane celý content rect, děti se překrývají,
- `min_size_for_child` = `min_size = max(min_size, pad(dítě))`.

Tlačítko: `dvui_button` vloží jako dítě zástupce popisku s
`gravity = (0.5, 0.5)`, takže je popisek uprostřed. Min. velikost tlačítka
z testu: popisek 40×12 + padding popisku 6 → 52×24; + padding tlačítka 6
→ 64×36; + margin 4 → `wd.rect` 72×44.

### 7.4 ScrollContainer

`dvui_ScrollContainerWidget` (DVUI `ScrollContainerWidget.zig`) skládá děti
**svisle** na virtuální plochu a ukazuje z ní výřez (`viewport`). Stav je
v `dvui_ScrollInfo`:

| Položka | Význam |
|---|---|
| `vertical`, `horizontal` | režim: `NONE` (neskroluje se, plocha = viewport), `AUTO` (plocha podle obsahu), `GIVEN` (velikost plochy zadá program) |
| `virtual_size` | velikost virtuální plochy (u AUTO z minulého snímku) |
| `viewport` | výřez: `x, y` = posun, `w, h` = velikost content rectu kontejneru |

**`init`**: `viewport.w/h` = velikost content rectu; v režimu `NONE` se
`virtual_size` v dané ose nastaví na viewport; zapne se clip na content rect;
`frame_viewport` = posun viewportu, se kterým se tento snímek kreslí
(obvykle `viewport.x/y`, ScrollArea může zadat jiný).

**`rect_for`** (dítě se skládá pod předchozí):

- `y = next_virtual_size.h` – součet **min. výšek** dosud ukončených dětí
  (ne jejich skutečných obdélníků!),
- výška dostupného místa podle `vertical`:
  `NONE` → `viewport.h − y`; `AUTO` → `max(viewport.h − y, min_size.h)`;
  `GIVEN` → `virtual_size.h − y`,
- šířka podle `horizontal`: `NONE` → `viewport.w`; `AUTO` →
  `max(viewport.w, min_size.w)`; `GIVEN` → `virtual_size.w`,
- obdélník = `placeIn({0, y, šířka, výška}, min_size, expand, gravity)`.
- Svisle roztažené smí být jen poslední dítě.

**`screen_rect_scale`**: obdélník dítěte posune o `−frame_viewport`
a převede přes content rect kontejneru. Proto jsou děti s `y` větším než
viewport mimo obrazovku (a odříznuté clipem).

**`min_size_for_child`**: `next_virtual_size.h += h`,
`next_virtual_size.w = max(…, w)`.

**`deinit`**:

- v režimu `NONE` a `AUTO` je min. velikost kontejneru = **celá velikost
  obsahu** (`pad(next_virtual_size)`); v režimu `GIVEN` zůstává min. velikost
  z Options,
- v režimu `AUTO`: když se `next_virtual_size` liší od `virtual_size`,
  přepíše ji a vyžádá refresh,
- zpracuje kolečko myši a klávesy (pokud ho nepředběhl potomek) a
  „odpruží“ viewport zpět do mezí `0 … scroll_max`, pokud z nich vyjel
  (např. obsah se zmenšil).

`scroll_max = max(0, virtual_size − viewport)` v každé ose.

**`lock_visible`**: kontejner si pamatuje první viditelné dítě a jeho posun
(`"_fv_id"`, `"_fv_offset"`, `"_fv_viewport"`). S `lock_visible` v dalším
snímku posune viewport tak, aby toto dítě zůstalo na stejném místě, i když
se nad ním přidal nebo ubral obsah. Dokud dítě nenajde, kreslí děti mimo
obrazovku.

### 7.5 ScrollArea a ScrollBar

`dvui_ScrollAreaWidget` (DVUI `ScrollAreaWidget.zig`) je složenina:

```
hbox (vodorovný box, Options uživatele)
├── svislý posuvník (gravity_x = 1, expand VERTICAL)   – jen když je potřeba
└── vbox (svislý box, expand BOTH)
    ├── vodorovný posuvník (gravity_y = 1, expand HORIZONTAL) – jen když je potřeba
    └── ScrollContainer (expand BOTH)
```

Posuvníky se vytvářejí **před** obsahem s gravitací 1, takže je box zabalí
na konec (vpravo/dole) a obsah dostane zbytek. Díky tomu jsou vidět i při
nedostatku místa.

**Rozhodnutí o posuvnících** (v `init`, ještě před obsahem):

1. Odhad viewportu = content rect hboxu minus levý/horní okraj kontejneru
   z minulého snímku (`"_linsets"`).
2. Svislý posuvník, je-li `vertical != NONE` a buď `vertical_bar = SHOW`,
   nebo režim `AUTO*` a `virtual_size.h > viewport.h + 0.001`
   (`virtual_size` je **z minulého snímku**). Posuvník ubere viewportu
   šířku 10 (min. velikost posuvníku), kromě režimu `AUTO_OVERLAY`
   (posuvník se kreslí přes obsah).
3. Stejně vodorovný posuvník.
4. Svislý posuvník se zkontroluje ještě jednou – vodorovný mohl ubrat
   výšku.

Min. velikost ScrollArea je min. velikost hboxu, tedy **velikost obsahu**
(+ posuvníky). Aby oblast skutečně scrollovala, musí jí rodič dát méně
místa: roztažením do omezeného prostoru, `max_size_content`, nebo pevnou
velikostí plovoucího okna. Viz kap. 10.

Stav (`dvui_ScrollInfo`) se ukládá pod `"_scroll_info"`, pokud ho
nepředá program (`scroll_info`).

**ScrollBar** (`dvui_ScrollBarWidget`, min. velikost 10×10) není kontejner.
Úchyt (`grab_rect`) se počítá z content rectu posuvníku:

- délka úchytu = `min(délka, max(20, délka · visible_fraction))`,
- `visible_fraction = viewport / max(viewport, virtual_size)` (při přejetí
  mimo meze se přičte i přejetá část),
- poloha = `(délka − délka_úchytu) · offset_fraction`, kde
  `offset_fraction = posun / scroll_max`.

Tažení úchytu převádí polohu myši zpět na `offset_fraction`; kliknutí mimo
úchyt posune o stránku (`dvui_scroll_info_scroll_page`).

### 7.6 Grid

`dvui_GridWidget` (DVUI `GridWidget.zig`) je tabulka. Na rozdíl od boxu
**nepoužívá `rect_for` pro buňky** – každé buňce spočítá obdélník sám
z šířek sloupců a výšek řádků a předá ho přes `options.rect`. Buňky tedy
nehlásí min. velikost rodiči, ale přímo gridu (`cell_min_size`). Šířky
a výšky se z min. velikostí buněk dopočítají až pro **další snímek** – to je
vícepruchodový layout.

**Struktura:**

```
grid (Options uživatele, výchozí border 1, pozadí)
└── scroll (ScrollArea bez vlastního ScrollContaineru)
    └── vbox
        ├── cscroll (ScrollArea hlaviček, jen vodorovně, bez posuvníků)   – pokud jsou hlavičky
        │   └── hlavičky sloupců (CellWidget s options.rect)
        └── bscroll (ScrollContainer těla)
            └── buňky (CellWidget s options.rect)
```

Hlavičky musí přijít před první buňkou. Vodorovný posun hlaviček a těla se
na konci snímku synchronizuje.

**Data mezi snímky:**

| Klíč | Obsah |
|---|---|
| `"__cols"`, `"__rows"` | počet sloupců/řádků (největší viděný index + 1) |
| `"__col_widths"` | šířka každého sloupce (bez roztažení) |
| `"__col_expands"` | který sloupec má roztažené buňky (`expand` vodorovně) |
| `"__col_resizable"` | zda jde šířka sloupce měnit tažením |
| `"__row_heights"` | seřazené dvojice (řádek, výška) pro řádky s jinou než výchozí výškou |
| `"__row_height_default"` | výchozí výška řádku (nejmenší viděná výška) |
| `"__col_header_height"` | výška hlaviček |
| `"__auto_size"` | zda se má i příští snímek přizpůsobovat obsahu |
| `"__csi"` | ScrollInfo hlaviček |

**Geometrie buňky** (`dvui_grid_widget_cell`):

```
x = col_offset(col) = Σ col_width(i) pro i < col
y = row_offset(row) = Σ row_height(r) pro r < row
w = col_width(col)
h = row_height(row)
```

- `col_width(c) = max(6, col_widths[c] + col_expand · col_weight(c))`;
  pro neznámý sloupec 100.
- `row_height(r)` = hodnota z `"__row_heights"`, jinak výchozí výška řádku,
  jinak `default_min.h` (výška písmene M + 2 × 6 = 24).
- `row_offset` si u dlouhých tabulek každých 1000 pixelů zapamatuje
  mezivýsledek (`row_offsets`), aby se nesčítalo vždy od začátku.
- Hlavička: `{col_offset(col), 0, col_width(col), col_header_height}`.

**Roztažení a zmenšení sloupců** (v `init`, z dat minulého snímku): pokud
nějaký sloupec má roztažené buňky, nebo grid neskroluje vodorovně, spočítá se

```
total       = Σ col_widths
shrink      = viewport.w < total          (pak mají váhu všechny měnitelné sloupce)
col_weight  = col_widths[c], pokud je sloupec roztažený (nebo shrink a měnitelný)
              a širší než 6; jinak 0
col_expand  = (viewport.w − total) / Σ col_weight
```

Pak se omezí: při vodorovném scrollování jen zvětšovat (`≥ 0`), bez
roztažených sloupců jen zmenšovat (`≤ 0`), nikdy pod `−0.99`. Každý
roztažený sloupec tedy dostane díl volného místa **úměrný své šířce**.

**Auto-size** (`dvui_grid_widget_auto_size`, automaticky v prvním snímku
a při `layout_only`): buňky během snímku hlásí min. velikosti
(`cell_min_size`):

- `col_widths_auto[col] = max(…, 6, min_size.w)`,
- hlavička: `col_header_height_auto = max(…, min_size.h)`,
- buňka těla: `row_heights_auto[row] = max(…, max(6, min_size.h))` a výchozí
  výška řádku se sníží na nejmenší viděnou výšku.

V `deinit` se `col_widths_auto` uloží jako nové `"__col_widths"`,
`row_heights_auto` (doplněné o dříve známé řádky, které tento snímek nebyly
vidět) jako `"__row_heights"`. Pokud se nějaká hodnota liší od toho, s čím
se tento snímek kreslilo, nastaví se `"__auto_size"` a vyžádá refresh –
přizpůsobování pokračuje i v dalším snímku.

Bez auto-size (nebo jen řádků) se šířky sloupců nemění podle obsahu, jen
ruční změnou šířky: tažení za pravý okraj hlavičky sčítá posun
(`col_resize_amount`); záporný posun ubírá ze sloupce a případně ze sloupců
vlevo, kladný přidává; u roztažených sloupců se změna vyrovná na ostatních
roztažených (nebo měnitelných) sloupcích, aby celková šířka seděla.

**Min. velikost těla** se gridu nahlásí ručně: `bscroll.min_size_for_child(
{Σ col_widths, row_offset(rows)})`.

**`dvui_grid_widget_rows_visible`** (vyžaduje `rows`): z `frame_viewport.y`
a výšky viewportu najde první a poslední viditelný řádek (+1 řádek na každou
stranu), takže program může vytvářet jen viditelné buňky. V prvním snímku
(viewport ještě 0) vrátí prvních 50 řádků.

**Příklad** (2×2 buňky, šířka `20 + 10·col + 5·row`, výška `10 + 4·row`,
grid bez `expand`):

| snímek | r0 c0 | r0 c1 | r1 c0 | r1 c1 | co se děje |
|---|---|---|---|---|---|
| 1 | {0,0,100,24} | {100,0,100,10} | {0,10,100,10} | {100,10,100,10} | nic není známo: sloupce 100 (neznámé), první řádek `default_min.h` = 24, pak už výchozí výška 10 z první buňky; auto-size zjistí šířky 25/35 a výšky 10/14 |
| 2 | {0,0,6,10} | {6,0,6,10} | {0,10,6,14} | {6,10,6,14} | šířky jsou známé, ale grid z minula hlásil min. šířku ~0 → viewport ~0 → `shrink`, sloupce se smrsknou na minimum 6; grid teď nahlásí šířku 60 |
| 3 | {0,0,25,10} | {25,0,35,10} | {0,10,25,14} | {25,10,35,14} | ustáleno |

### 7.7 Plovoucí widgety (subwindows)

`FloatingWidget`, `FloatingTooltipWidget` a `FloatingWindowWidget` jsou
**subwindows**: nejsou součástí layoutu rodiče a mají vlastní pořadí
vykreslení a vlastní zpracování myši.

Společné:

- `WidgetData.init` s `subwindow = true` a `options.rect` (pokud není zadán,
  `{0,0,0,0}`) → obdélník se **nepočítá přes rodiče**, velikost se vezme
  z min. velikosti z minulého snímku (`rect.w == 0 → ms.w`). Souřadnice jsou
  v **natural jednotkách okna**, ne rodiče.
- rect_scale se počítá od okna (`dvui_window_rect_scale`).
- Min. velikost se rodiči **nehlásí**.
- Uvnitř je `ScaleWidget`, který převezme měřítko rodiče
  (`scale_val = s rodiče / natural_scale`), takže obsah plovoucího widgetu
  vznikajícího ze zvětšené oblasti je zvětšený stejně.
- Clip se nastaví na celé okno a nejbližší ScrollContainer se „zapomene“
  (`dvui_scroll_container_scroll_set(nullptr)`).

Umístění:

- **FloatingWidget** s `from`: levý horní roh se spočítá z bodu `from`
  a `from_gravity` (např. 0.5 = bod uprostřed widgetu) a výsledek se
  `dvui_place_on_screen` posune, aby byl celý vidět.
- **FloatingTooltipWidget**: `HORIZONTAL`/`VERTICAL` – začne v levém horním
  rohu `active_rect` a `place_on_screen` ho odsune vpravo (dolů) od
  `active_rect`; když se nevejde, přeskočí na druhou stranu, kde je víc místa.
  `STICKY` – vedle myši v okamžiku zobrazení. `ABSOLUTE` – kde určí program.
  Tooltip se ukazuje, dokud je myš nad `active_rect` (nebo nad tooltipem či
  jeho vnořeným tooltipem u `interactive`); stav je v `"_showing"`.
- **FloatingWindowWidget**:
  - obdélník je uložený v `"_rect"` (nebo v `rect_io` programu),
  - **první snímek** se jen uloží a okno má velikost 0 – nekreslí se,
    protože ještě nezná svou velikost,
  - **auto-size** (okno bez zadané velikosti): `rect.w/h = min(max(min. velikost
    z minula, min. velikost z Options), velikost okna)`; dokud se velikost
    mění, okno žádá další snímky; jakmile v snímku nikdo refresh nechtěl,
    auto-size se vypne (`"_auto_size"`),
  - **auto-pos** (okno bez zadané pozice): vycentruje se na `center_on` nebo
    na rodičovské subwindow; volitelně se posune o 24 px, aby nezakrylo
    rodiče nebo jiné okno na stejném místě,
  - pak `place_on_screen` na obrazovku zvětšenou tak, aby z okna zbylo vidět
    aspoň 48 px na šířku a 24 px na výšku (jde ho chytit),
  - tažením se mění pozice nebo velikost (min. 40×10) podle toho, za kterou
    část okna uživatel chytil (`dvui_DragPart`: okraj 4 px, roh 15 px,
    střed jen v `drag_area`, typicky v liště okna).

Pořadí subwindows (`Subwindows.zig`): nové okno se přidá navrch (nebo hned
nad rodiče, pokud `stay_above_parent_window`); `dvui_raise_subwindow` ho
přesune navrch. Myš patří nejvyššímu subwindow, které ji obsahuje (nebo
modálnímu oknu): `floating_win` události musí odpovídat aktuálnímu
subwindow, jinak ji widget nedostane.

## 8. Konvergence: refresh a více snímků

`dvui_refresh` nastaví „potřebuji další snímek“ a `dvui_window_end` vrátí 0.
Layout si ho vyžádá v těchto případech:

| Kde | Podmínka |
|---|---|
| `minSizeSetAndRefresh` (všechny widgety) | widget nemá min. velikost z minula (první snímek) |
| | widget dostal přesně min. velikost z minula (`rect.w == ms.w`), ale jeho nová min. velikost je jiná – byla omezující a změnila se; obdobně výška |
| Box `deinit` | má roztažené děti a zbylo nevyužité místo, nebo některé dítě přeteklo |
| ScrollContainer `deinit` | v režimu AUTO se změnila virtuální velikost |
| ScrollContainer | viewport je mimo meze a pruží zpět |
| Grid `deinit` | auto-size zjistil jiné šířky/výšky |
| FloatingWindow | první snímek; auto-size (dočasně potlačí refresh ostatních, aby poznal ustálení) |
| Floating s `from` | nezná min. velikost |
| Tooltip `deinit` | myš odešla – zavře se příští snímek |
| hover / události | `hoverFade` během přechodu, kliknutí, scroll, tažení |

Typicky se layout ustálí za **2 snímky** (box, tlačítka, scroll), grid
s auto-size za **3**. Testy to ověřují funkcí `settle`, která opakuje snímky,
dokud `dvui_window_end` nevrátí −1.

Proč to konverguje: min. velikosti se počítají jen z min. velikostí dětí
(ne z přidělených obdélníků), takže po ustálení listů se ustálí i rodiče.
Výjimkou jsou cykly, kdy min. velikost závisí na přiděleném místě (grid
s `shrink`, `EXPAND_RATIO`); i ty se ustálí, protože se porovnává s minulou
hodnotou a refresh se žádá jen při změně.

## 9. Ořez (clip) a pořadí kreslení

- **Clip** je obdélník ve fyzických pixelech. ScrollContainer ho v `init`
  zúží na svůj content rect a v `deinit` vrátí. Plovoucí widgety ho nastaví
  na celé okno. Kreslení i události myši mimo clip se ignorují
  (`dvui_event_match` vyžaduje, aby byla myš uvnitř clipu).
- **Kreslení hlavního okna** jde rovnou do backendu (`rendering = true`).
  **Subwindows** kreslí do fronty příkazů (`dvui_RenderFrontToBack`);
  v `dvui_window_end` se fronty vykreslí v pořadí subwindows zdola nahoru,
  takže okno navrchu překryje ostatní bez ohledu na to, kdy ho program
  vytvořil.
- `after = true` (např. rámeček fokusu) jde do druhé fronty, která se
  kreslí po hlavní frontě daného subwindow.
- Hrany obdélníků se zaokrouhlují na celé pixely (`snap_to_pixels`).

## 10. Časté situace a pasti

- **ScrollArea zabere všechno místo pod sebou.** Její min. výška je výška
  obsahu, box jí tedy přidělí celý zbytek a další děti nedostanou nic.
  Řešení jako v DVUI: prvky, které mají být pod ní, deklarovat **dřív**
  s `gravity_y = 1` (box je zabalí dolů), nebo jí dát `max_size_content`.
- **Dítě po roztaženém dítěti** v BasicLayout/ScrollContaineru nedostane
  místo – roztažené smí být jen poslední, jinak použijte Box.
- **Prázdný první snímek.** Kontejner bez min. velikosti z minula má v prvním
  snímku velikost 0 ve směru, ve kterém není roztažený. Vždy počítejte
  s tím, že layout je správně až po ustálení.
- **Duplicitní id.** Dva widgety ze stejného řádku zdrojáku (smyčka)
  potřebují různé `id_extra`, jinak sdílejí min. velikost i data a layout
  se rozbije (hlásí se chyba).
- **`max_size_content` neomezuje přidělený obdélník.** Omezuje jen min.
  velikost, kterou widget hlásí rodiči. S `expand` widget pořád dostane
  celé dostupné místo.
- **`options.rect` vyřadí widget z layoutu rodiče** – rodič o něm neví
  a jeho min. velikost se nezapočítá.
