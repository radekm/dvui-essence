# Jak fungují animace

Podrobný popis animací, časovačů a řízení smyčky v dvui-animation. Kód je
věrný port DVUI (commit `2efa35e`): `Animation`, `animation`,
`animationGet`, `timer*` a `reduce_motion` z `dvui.zig`, mapa `animations`,
`begin`, `end`, `waitTime` a `beginWait` z `Window.zig`, `easing.zig`
a `widgets/AnimateWidget.zig`. Přehled API je v [ROZHRANI.md](ROZHRANI.md),
odchylky v [ODCHYLKY.md](ODCHYLKY.md).

Obsah:

1. [Základní myšlenka](#1-základní-myšlenka)
2. [dvui_Animation](#2-dvui_animation)
3. [Easing](#3-easing)
4. [Mapa animací v okně](#4-mapa-animací-v-okně)
5. [Stárnutí a mazání v dvui_window_begin](#5-stárnutí-a-mazání-v-dvui_window_begin)
6. [Životnost animace](#6-životnost-animace)
7. [Návratová hodnota dvui_window_end](#7-návratová-hodnota-dvui_window_end)
8. [Časovače](#8-časovače)
9. [reduce_motion](#9-reduce_motion)
10. [waitTime a beginWait – řízení smyčky](#10-waittime-a-beginwait--řízení-smyčky)
11. [AnimateWidget](#11-animatewidget)
12. [stateFade a hoverFade](#12-statefade-a-hoverfade)
13. [Na co si dát pozor](#13-na-co-si-dát-pozor)

---

## 1. Základní myšlenka

Immediate-mode GUI kreslí celý snímek znovu a mezi snímky si pamatuje jen
malý stav pod id widgetů. Animace je další takový stav: **hodnota, která
se mění v čase** (od `start_val` k `end_val`). Widget animaci v nějakém
snímku založí (`dvui_animation`) a v každém dalším snímku si přečte její
aktuální hodnotu (`dvui_animation_get` + `dvui_animation_value`) a podle
ní se nakreslí (posun, velikost, průhlednost…).

Aby animace běžela plynule, musí okno vědět, že má kreslit další snímky,
i když nepřichází žádná událost. Proto `dvui_window_end` vrací, **za jak
dlouho chce další snímek**: hned (běží animace), za určitou dobu (animace
nebo časovač začíná v budoucnu), nebo vůbec (čekat na událost). Smyčka
programu pak čeká na událost nejvýš tak dlouho (`dvui_window_wait_time`
a backend).

Časy animací jsou **relativní k aktuálnímu snímku**. Okno je na začátku
každého snímku posune o uplynulý čas – animace tedy „stárnou“ samy, nikdo
nemusí počítat absolutní časy.

## 2. dvui_Animation

```c
typedef struct dvui_Animation {
    dvui_EasingFn* easing;   // NULL = linear (DVUI výchozí easing.linear)
    float start_val;         // DVUI výchozí 0
    float end_val;           // DVUI výchozí 1 – v C nutno zadat!
    int32_t start_time;      // µs od aktuálního snímku; DVUI výchozí 0
    int32_t end_time;        // µs od aktuálního snímku; v DVUI povinné
} dvui_Animation;
```

- `start_time` a `end_time` jsou v **mikrosekundách** relativně
  k začátku aktuálního snímku: 0 = teď, kladné = v budoucnu, záporné =
  v minulosti. Animace na 0,5 s založená teď je `{.start_time = 0,
  .end_time = 500000}`; za 16 ms (další snímek) bude `{−16000, 484000}`.
- Typ je `int32_t` jako v DVUI (`i32`), takže nejdelší animace je asi 35
  minut.

**`dvui_animation_value(&a)`** (DVUI `Animation.value`):

| Stav | Podmínka | Hodnota |
|---|---|---|
| ještě nezačala (nebo začíná právě teď) | `start_time >= 0` | `start_val` |
| skončila | `end_time <= 0` (`done`) | `end_val` |
| běží | jinak | `lerp(start_val, end_val, easing(frac))` |

kde `frac = −start_time / (end_time − start_time)` je podíl uplynulé doby
(omezený na [0, 1]) a `lerp(a, b, t) = a + (b − a) · t`. Výsledek se
neořezává: easing `elastic` nebo `back` dá hodnotu mimo interval
`start_val` … `end_val`.

**`dvui_animation_done(&a)`** (DVUI `Animation.done`) je true, když
`end_time <= 0` – v **posledním snímku**, ve kterém animace existuje
(kap. 6). Widget tak může v tomto snímku dokončit akci (zavřít okno, skrýt
obsah).

## 3. Easing

Easing (DVUI `easing.zig`, převzato z
[gistu Kryzarel](https://gist.github.com/Kryzarel/bba64622057f21a1d6d44879f9cd7bd4))
převádí lineární průběh času `t ∈ [0, 1]` na „průběh“ animace. Všechny
funkce jsou čisté `float f(float t)`:

| Rodina | `in` | `out` | `in_out` |
|---|---|---|---|
| quad, cubic, quart, quint | `tⁿ` | `1 − in(1 − t)` | první půlka `in(2t)/2`, druhá `1 − in(2(1 − t))/2` |
| sine | `1 − cos(t·π/2)` | `sin(t·π/2)` | `(cos(t·π) − 1) / −2` |
| expo | `2^(10(t − 1))` | `1 − in(1 − t)` | jako výše |
| circ | `−(√(1 − t²) − 1)` | `1 − in(1 − t)` | jako výše |
| elastic | `1 − out(1 − t)` | `2^(−10t)·sin((t − p/4)·2π/p) + 1`, `p = 0,3` | jako výše |
| back | `t²((s + 1)t − s)`, `s = 1,70158` | `1 − in(1 − t)` | jako výše |
| bounce | `1 − out(1 − t)` | po částech parabolické (4 „odskoky“) | jako výše |

Plus `linear(t) = t`. Funkce `expo` a `elastic` nekončí přesně v 0 a 1
(odchylka nejvýš 2⁻¹⁰); `elastic` a `back` během průběhu přesahují [0, 1].

V C se funkce jmenují `dvui_easing_<jméno>` (`dvui_easing_in_out_quad`).
Pole `dvui_easings` obsahuje všech 31 funkcí s názvy v pořadí X-makra
`DVUI_EASINGS` (DVUI Examples si stejný seznam skládá v době překladu přes
`std.meta.declarations`).

## 4. Mapa animací v okně

Okno má třetí hashovací mapu (vedle min. velikostí a datového úložiště):

```
win->animations : dvui_id_update(id, "klíč") → dvui_Animation
```

DVUI ji má jako `TrackingAutoHashMap(Id, Animation, .get_and_put, void)`.
V C je to sdílená `dvui_Map` (viz [shared/README.md](../../shared/README.md));
každá hodnota je blob z `malloc` velikosti `dvui_Animation`.

Režim `.get_and_put` znamená, že značku **`used`** nastavuje zápis
(`dvui_animation`, `dvui_timer`) **i čtení** (`dvui_animation_get`,
`dvui_timer_get`, `dvui_timer_done`, …). `dvui_map_reset` pak smaže
položky, které od minulého resetu nikdo nepoužil, a ostatním značku
vynuluje (stejně jako u datového úložiště).

| Funkce | Co dělá s mapou |
|---|---|
| `dvui_animation(id, key, a)` | `put` (přepíše i běžící animaci), `used = true` |
| `dvui_animation_get(id, key, &a)` | `get`, `used = true` |
| `dvui_timer(id, micros)` | `put` pod klíčem `"_timer"` |
| `dvui_timer_get/done/done_or_none(id)` | `get` pod klíčem `"_timer"` |
| `dvui_timer_remove(id)` | `remove` |
| `dvui_window_begin` | stárnutí použitých, mazání (kap. 5) |
| `dvui_window_end` | čte použité (bez změny `used`) – kap. 7 |

## 5. Stárnutí a mazání v dvui_window_begin

DVUI `Window.begin` („update animations and remove dead ones“). Probíhá po
nastavení `secs_since_last_frame`:

```
micros = min(micros_since_last, INT32_MAX)
pro každou animaci s used == true:           // DVUI iterator.next_used
    pokud end_time <= 0:
        used = false                          // byla „done“ už minulý snímek → smazat
    jinak:
        start_time -= micros  (saturace)
        end_time   -= micros  (saturace)      // i když je teď ≤ 0, zůstává – tento snímek je „done“
dvui_map_reset(animations)                    // smaže used == false, ostatním used = false
```

`micros_since_last` je doba od minulého `dvui_window_begin` (z `time_ns`
předaného do `begin`, omezená na `UINT32_MAX` µs, nejméně 1). Když čas
skočí dozadu, okno zůstane na starém čase a pro animace použije 10 ms.

Důležité:

- Stárnou **jen animace použité minulý snímek**. Nepoužitá animace se
  neposune a hned se smaže (měla `used = false` už po minulém resetu).
- Po resetu mají všechny přeživší animace `used = false`; kdo je chce
  zachovat, musí se na ně v tomto snímku zeptat (kap. 6).
- Odečítání je saturované (Zig `-|=`), čas nepřeteče.

## 6. Životnost animace

Animace přežije do dalšího snímku, jen když platí obojí:

1. v tomto snímku ji někdo **použil** (nastavil nebo přečetl),
2. **nebyla dokončená už na začátku** tohoto snímku (`end_time > 0` před
   odečtením).

Časová osa: animace na 32 ms, snímky po 16 ms, widget ji každý snímek čte
(odpovídá testu `test_animation_lifetime`):

| Snímek | po stárnutí v `begin` | `get` | `value` | `done` | `dvui_window_end` |
|---|---|---|---|---|---|
| N (založení) | – (`put`: `{0, 32000}`) | ano | 0 (`start_time >= 0`) | ne | 0 |
| N+1 | `{−16000, 16000}` | ano | 0,5 | ne | 0 |
| N+2 | `{−32000, 0}` | ano | 1 (`end_val`) | **ano** | −1 |
| N+3 | smazána (`end_time <= 0` už před odečtením) | ne | – | – | −1 |

Snímek N+2 je **poslední snímek** animace: `done` je true a hodnota je
přesně `end_val`. V něm může widget dokončit, co animace doprovázela.
`dvui_window_end` v něm vrací −1 (animace už neběží), takže když se nic
jiného neděje, další snímek přijde až s událostí – animace se tedy smaže
až v ní.

Když se na animaci jeden snímek nikdo nezeptá, zmizí:

| Snímek | co se stane |
|---|---|
| N | `put` (`used = true`) |
| N+1 | stárnutí, reset → `used = false`; nikdo se neptá; `end` ji nevidí |
| N+2 | `begin`: nepoužitá → nestárne a reset ji smaže; `get` vrátí false |

Widget, který animaci potřebuje, ji proto musí číst v každém snímku (tak to
dělá AnimateWidget i tlačítko „wiggle“ v DVUI Examples).

## 7. Návratová hodnota dvui_window_end

DVUI `Window.end` vrací `?u32` – kolik mikrosekund má uplynout mezi tímto
a dalším `begin`. V C je to `int32_t`, kde **−1 je `null`** (čekat na
událost). Počítá se na konci `end`:

```
pokud extra_frames_needed > 0:     // dvui_refresh, první snímek, …
    vrať 0
ret = null
pro každou animaci s used == true:  // jen použité v tomto snímku
    pokud start_time > 0:           // začíná v budoucnu
        ret = min(ret, start_time)
    jinak pokud end_time > 0:       // právě běží
        vrať 0
vrať ret                            // null (−1), nebo čas nejbližšího začátku
```

| Situace | Výsledek |
|---|---|
| refresh (ustalování layoutu, stateFade, kliknutí…) | 0 |
| běží aspoň jedna použitá animace | 0 |
| všechny použité animace začínají v budoucnu | nejmenší `start_time` (µs) |
| časovač (začíná i končí v budoucnu) | čas do vypršení |
| animace v posledním snímku (`end_time <= 0`) | neovlivní (null) |
| nic | −1 |

Protože se berou jen použité animace, animace, na kterou se nikdo nezeptal,
snímky nevyžaduje (a v dalším snímku zmizí).

## 8. Časovače

Časovač je animace pod klíčem `"_timer"` s `start_time = end_time =
micros` (DVUI `Window.timer`). Protože `start_time > 0`, `dvui_window_end`
nevrací 0, ale čas do vypršení – časovač **nespouští snímky průběžně**,
naplánuje jen jeden snímek v okamžiku vypršení.

| Funkce | Implementace |
|---|---|
| `dvui_timer(id, micros)` | `put("_timer", {.start_time = micros, .end_time = micros})` |
| `dvui_timer_get(id, &left)` | `left = end_time` (≤ 0 = vypršel, o kolik µs je snímek pozdě) |
| `dvui_timer_done(id)` | časovač existuje a `end_time <= 0` |
| `dvui_timer_done_or_none(id)` | `timer_done` nebo časovač není |
| `dvui_timer_remove(id)` | smaže položku |

Časovač na 40 ms, snímky po 16 ms (test `test_timer`):

| Snímek | `left` | `timer_done` | `done_or_none` | `dvui_window_end` |
|---|---|---|---|---|
| N (`timer`) | 40000 | ne | ne | 40000 |
| N+1 | 24000 | ne | ne | 24000 |
| N+2 | 8000 | ne | ne | 8000 |
| N+3 | −8000 | **ano** | ano | −1 |
| N+4 | – (smazán) | ne | ano | −1 |

`timer_done` je tedy true **právě v jednom snímku** (prvním po vypršení).
V DVUI Examples se tím dělají periodické akce – hodiny, které kreslí snímek
na začátku každé sekundy:

```c
const int32_t left = (int32_t)((dvui_frame_time_ns() / 1000000) % 1000);  // ms do sekundy
if (dvui_timer_done_or_none(id)) {
    dvui_timer(id, 1000 * (1000 - left));    // další snímek na začátku příští sekundy
}
```

Volání `dvui_timer_done_or_none` každý snímek časovač zároveň udržuje
naživu (kap. 6). Časovače se netýká `reduce_motion`.

## 9. reduce_motion

`dvui_reduce_motion` (DVUI `dvui.reduce_motion`) je globální proměnná pro
uživatele, kterým pohyb vadí. Když je true:

- `dvui_animation` přepíše `end_time = start_time + 1` – animace (ihned
  začínající) skončí hned v dalším snímku; v něm je `done` a hodnota
  `end_val`, takže widget skočí rovnou do cílového stavu;
- `dvui_state_fade` / `dvui_hover_fade` vracejí rovnou cíl (0 nebo 1);
- **časovače** se nemění (zakládají se přes `Window.timer`, ne přes
  `dvui_animation`).

## 10. waitTime a beginWait – řízení smyčky

DVUI nepředává výsledek `end` backendu přímo. `Window.waitTime` ho přepočítá
podle toho, kolik času už snímek zabral a jak přesně se smyčka probouzí,
a volitelně omezí počet snímků za sekundu. `Window.beginWait` pak na
začátku dalšího snímku vyhodnotí, jestli se probuzení trefilo.

Stav v okně (DVUI `Window`):

| Pole | Výchozí | Význam |
|---|---|---|
| `max_fps` | 0 (null) | omezení snímků za sekundu (`dvui_window_max_fps_set`) |
| `loop_target_slop` | 1000 µs | odhad, o kolik se smyčka probouzí pozdě (režie mimo snímek) |
| `loop_target_slop_frames` | 0 | krok úpravy slop (zdvojuje se, ±1 … ±1000) |
| `loop_wait_target` | null | absolutní čas (ns), kdy chceme začít další snímek |
| `loop_wait_target_can_interrupt` | false | cíl platí jen pro čekání, které nepřerušila událost |

**`dvui_window_wait_time(win, end_micros)`** (DVUI `waitTime`):

```
min_micros  = max_fps ? 1e6 / max_fps : 0            // nejkratší doba snímku
wait_micros = (end_micros nebo 0) -| min_micros       // podmíněné čekání navíc
loop_wait_target = null; can_interrupt = false
target_min = min_micros; target = min_micros + wait_micros
so_far = (max(frame_time, nano_time) − frame_time) / 1000   // µs od begin
odečti so_far nejdřív z min_micros, zbytek z wait_micros
odečti slop nejdřív z min_micros, zbytek z wait_micros (slop může být záporný → přičte)
pokud min_micros > 0:                                 // kvůli max_fps spíme bezpodmínečně
    backend.sleep(min_micros · 1000)
    loop_wait_target = frame_time + target_min · 1000
pokud end_micros je null:   vrať UINT32_MAX, loop_wait_target = null   // čekat bez limitu
pokud wait_micros > 0:      loop_wait_target = frame_time + target · 1000,
                            can_interrupt = true; vrať wait_micros      // čekat s limitem
jinak:                      vrať 0                                       // už jsme pozdě
```

Výsledek se předá `dvui_sdl3gpu_wait_event_timeout`: `UINT32_MAX` = čekat
na událost (`SDL_WaitEvent`), `0` = nečekat, jinak
`SDL_WaitEventTimeout` s milisekundami zaokrouhlenými nahoru. Backend vrátí
`true`, když čekání přerušila událost.

**`dvui_window_begin_wait(win, interrupted)`** (DVUI `beginWait`) vrací čas
pro `dvui_window_begin` (`max(frame_time, nano_time)`) a když byl nastaven
cíl `loop_wait_target`:

- čekání přerušila událost a cíl to dovoloval → slop se nemění;
- probudili jsme se **pozdě** → `slop_frames = clamp(2·slop_frames, 1, 1000)`,
  `slop += slop_frames` (příště budeme čekat kratší dobu);
- probudili jsme se **brzy** → `slop_frames = clamp(2·slop_frames, −1000, −1)`,
  `slop += slop_frames`; když chybí méně než 1 ms, smyčka „dotáčí“
  (`backend.sleep(0)` a znovu `nano_time`), dokud cíl nenastane;
- slop se drží v rozsahu −1 ms … 100 ms.

Příklad (test `test_wait_time`, `nano_time` = čas `begin`, slop 1000 µs):
`wait_time(−1) = UINT32_MAX`, `wait_time(0) = 0`,
`wait_time(100000) = 99000`; když od `begin` uběhlo 30 ms,
`wait_time(100000) = 69000`. S `max_fps = 50` je cíl `begin + 20 ms`;
probuzení v `begin + 25 ms` zvětší slop na 1001, další pozdní probuzení na
1003.

## 11. AnimateWidget

DVUI `widgets/AnimateWidget.zig`, `dvui.animate`. Obal s `BasicLayout`
(děti pod sebou jako v okně), který animuje svůj obsah. Vlastnosti `kind`,
`duration` (µs), `easing`.

**init** (pořadí jako v DVUI):

1. `WidgetData.init` – rodič přidělí obdélník podle **minulé** min.
   velikosti.
2. Pokud `kind != none`:
   - v prvním snímku (`dvui_first_frame`) spustí `"_start"` (`start`);
   - přečte `"_end"`, jinak `"_start"`; existuje-li, `val = value()`.
3. Pokud je `val`:
   - `alpha`: `prev_alpha = dvui_alpha(val)` – vynásobí alfu okna, takže
     vše uvnitř (pozadí widgetu i děti) se kreslí průhledněji;
   - `vertical`: pokud je obdélník **vyšší než min. velikost** (je
     roztažený), `rect.h *= max(val, 0)` a `rect.y` se posune podle
     `gravity_y` (střed při 0,5, spodek při 1);
   - `horizontal`: totéž pro šířku a `gravity_x`.
4. `parent_set`, `register`, `border_and_background`.

**deinit**:

- `alpha`: vrátí alfu (`dvui_alpha_set(prev_alpha)`);
- `vertical` / `horizontal`: `min_size.h` (`w`) `*= max(val, 0)` – rodič
  dostane zmenšenou min. velikost, takže se při animaci zmenšuje
  i okolní layout (bez záporných hodnot, které by layout rozbily);
- pak obvyklé `min_size_set_and_refresh`, `min_size_report_to_parent`,
  `parent_reset`.

Proč podmínka „větší než min. velikost“: obdélník se počítá z minulé min.
velikosti, která už je zmenšená hodnotou animace. Neroztažený widget je
proto zmenšený už přes min. velikost a druhé násobení by ho zmenšilo
dvakrát. Roztažený widget (`expand`) dostane celé místo rodiče, a proto se
zmenšuje přímo obdélník.

**Animace widgetu:**

| Funkce | Animace | Hodnoty |
|---|---|---|
| (první snímek) / `dvui_animate_widget_start` | `"_start"` | 0 → 1 za `duration`, `easing` |
| `dvui_animate_widget_start_end` | `"_end"` | 1 → 0 za `duration`, `easing` |
| `dvui_animate_widget_end` | – | true v posledním snímku `"_end"` (`done`) |

`"_end"` má přednost před `"_start"`. Po skončení `"_start"` (a zmizení
animace) má obsah plnou velikost a alfu. Po skončení `"_end"` program
obvykle obsah přestane kreslit (`end()` je signál k tomu).

Průběh při `vertical`, `duration` 160 ms, snímky po 16 ms, roztažený obal
výšky 60 (test `test_animate_vertical`): snímek 0 výška 60 (min. velikost
ještě neznámá), snímky 1–9 výška `60 · k/10`, snímek 10 výška 60
(`done`, hodnota 1), pak animace zmizí.

## 12. stateFade a hoverFade

`dvui_state_fade(id, key, on, secs)` (DVUI `stateFade`) a
`dvui_hover_fade(id, hovered)` (`hoverFade`, doba `dvui_hover_fade_secs` =
0,12 s) nejsou animace v mapě animací. Stav (číslo 0…1, nebo −1/−2 =
„stojí na 0/1“) je v **datovém úložišti** pod klíčem `key`
(`"__hover_fade"`) a posouvá se o `secs_since_last_frame / secs`. Dokud
přechod běží, volá `dvui_refresh`, takže `dvui_window_end` vrací 0. Přechod
začíná snímkem, který drží výchozí hodnotu, protože doba od minulého
snímku může zahrnovat dlouhou nečinnost. Při `reduce_motion` vrací rovnou
cíl. Tlačítko je používá pro hover.

## 13. Na co si dát pozor

- **Animaci čtěte každý snímek.** Nepřečtená animace jeden snímek přežije
  a pak zmizí (kap. 6) – i když ještě neskončila.
- **`end_val` zadávejte vždy.** V C nemá struktura výchozí hodnoty;
  `{.end_time = 500000}` animuje od 0 k **0**, ne k 1 jako v DVUI.
- **Časy jsou relativní.** `start_time = 100000` znamená „za 100 ms od
  teď“, ne absolutní čas. Pro „začátek příští sekundy“ použijte
  `dvui_frame_time_ns()`.
- **Poslední snímek nežádá další.** V posledním snímku (`done`) vrací
  `dvui_window_end` −1; kdo chce po skončení animace ještě něco
  překreslit, zavolá `dvui_refresh`.
- **Smyčka musí použít `dvui_window_wait_time`.** Předat výsledek
  `dvui_window_end` backendu přímo nejde (−1 je „čekat bez limitu“ =
  `UINT32_MAX`); `waitTime` navíc odečte čas, který snímek zabral.
- **Animace se v testovacím backendu řídí časem předaným do `begin`.**
  Testy posouvají čas o pevný krok (16 ms), proto jsou hodnoty přesně
  předvídatelné.
