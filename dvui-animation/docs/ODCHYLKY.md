# Odchylky od DVUI

Čím se dvui-animation liší od DVUI (commit `2efa35e`, kopie
v `dvui-original/`). Krátké shrnutí je v
[README.md](../README.md#odchylky-od-dvui). Jak animace fungují, popisuje
[ANIMACE.md](ANIMACE.md).

dvui-animation vznikl kopií dvui-layout, takže přebírá i jeho odchylky
(kap. 4); ty jsou podrobně popsané v
[dvui-layout/docs/ODCHYLKY.md](../../dvui-layout/docs/ODCHYLKY.md).

Obsah:

1. [Co je stejné](#1-co-je-stejné)
2. [Převod animací ze Zigu do C](#2-převod-animací-ze-zigu-do-c)
3. [Vynechané části animací](#3-vynechané-části-animací)
4. [Odchylky převzaté z dvui-layout](#4-odchylky-převzaté-z-dvui-layout)
5. [Co oproti dvui-layout chybí](#5-co-oproti-dvui-layout-chybí)
6. [Demo oproti DVUI Examples](#6-demo-oproti-dvui-examples)

---

## 1. Co je stejné

- `Animation.value` a `Animation.done` – stejné podmínky a vzorec
  (`lerp` s easingem, `clamp` podílu na [0, 1]).
- Všech 31 funkcí z `easing.zig` se stejnými konstantami.
- Mapa animací v okně se stejnou sémantikou jako DVUI
  `TrackingAutoHashMap(.get_and_put)`: `used` nastavuje zápis i čtení,
  stárnutí jen použitých animací, mazání animací dokončených už minulý
  snímek, `reset` na konci stárnutí.
- Klíče: `id.update(key)`, časovače pod `"_timer"`, AnimateWidget pod
  `"_start"` a `"_end"`.
- `dvui_window_end`: přednost `extra_frames_needed`, pak 0 pro běžící
  animaci, jinak nejmenší `start_time` budoucích animací, jinak null.
- `Window.waitTime` a `Window.beginWait` včetně `max_fps`, odečtení času
  snímku, `loop_target_slop`, zdvojování kroku a „dotáčení“ do cíle.
- `reduce_motion` (animace končí v příštím snímku, časovače beze změny,
  `stateFade` rovnou na cíl).
- AnimateWidget: pořadí kroků v `init` a `deinit`, podmínka „větší než
  min. velikost“ s tolerancí 0,001, posun podle gravity, `max(val, 0)`
  u min. velikosti.
- `waitEventTimeout` backendu SDL 3 GPU včetně návratové hodnoty
  „přerušeno událostí“ a obejití chyby SDL #6539.

## 2. Převod animací ze Zigu do C

| Zig (DVUI) | C (dvui-animation) | Poznámka |
|---|---|---|
| `Animation` s výchozími hodnotami polí (`easing = linear`, `start_val = 0`, `end_val = 1`, `start_time = 0`) | `dvui_Animation` bez výchozích hodnot | C je nemá; nezadané pole je 0. `easing == NULL` se chová jako `linear`, ale **`end_val` je nutné zadat** (0 ≠ 1). Inicializátor s výchozími hodnotami a přepisem (`{.end_val = 1, __VA_ARGS__}`) by s `warning_level=3` hlásil `-Woverride-init`. |
| `*const EasingFn` | `dvui_EasingFn*` (může být NULL) | NULL = linear |
| `animationGet(id, key) ?Animation` | `bool dvui_animation_get(id, key, dvui_Animation* out)` | `?T` → návratová hodnota + výstupní parametr |
| `timerGet(id) ?i32` | `bool dvui_timer_get(id, int32_t* out)` | dtto; `out` smí být NULL |
| `Window.end() !?u32` | `int32_t dvui_window_end(win)`, −1 = null | rozhodnutí vlastníka; hodnoty z animací jsou `i32`, takže se vejdou |
| `Window.timer`, `Window.timerRemove` | `dvui_timer`, `dvui_timer_remove` | volají se mezi `begin` a `end` na aktuálním okně |
| `Window.max_fps: ?f32` | `dvui_window_max_fps_set(win, fps)`, 0 = null | |
| `Window.loop_wait_target: ?i128` | `has_loop_wait_target` + `int64_t` | čas v ns jako `int64_t` |
| `Window.loop_target_slop` (pole) | `dvui_window_loop_target_slop(win)` | jen pro čtení (DVUI ho ukazuje v Examples) |
| `frameTimeNS() i128` | `int64_t dvui_frame_time_ns()` | |
| `micros -|= …` (saturace) | `sub_sat_i32` | |
| `AnimateWidget.InitOptions` (`kind`, `duration` povinné) | vlastnosti `kind` (výchozí `NONE`), `duration` (výchozí 0), `easing` | v seznamu vlastností nejde vynutit povinnou položku; bez `kind` widget nic neanimuje (jako `.none`) |
| `AnimateWidget.val: ?f32` | `has_val` + `val` | |
| `hover_fade_secs` (`pub var`) | `dvui_hover_fade_secs` (globální proměnná) | v dvui-layout je to konstanta |
| seznam easingů v Examples přes `std.meta.declarations` | pole `dvui_easings` + X-makro `DVUI_EASINGS` | C nemá reflexi |
| alokace v mapě (`put` může selhat → log) | `malloc`, při nedostatku paměti `abort()` | jako zbytek repozitáře |

## 3. Vynechané části animací

- **Podřízená okna OS** (`child_os_wins`): `waitTime` v DVUI na konci bere
  minimum s jejich `waitTime`. dvui-animation má jedno okno, část chybí.
- **Retain tokeny** mapy (`TrackingAutoHashMap.retain`) – animace je
  nepoužívají.
- **Ladicí kontrola typu** dat a animací v debug buildu (`DebugInfo`).
- **Mutex** datového úložiště (vícevláknový přístup).
- **Ostatní animované widgety DVUI** (Spinner, animované dialogy
  a plovoucí okna, Paned, Tree, toasty) – nejsou v zadání; plovoucí okna
  dvui-animation nemá vůbec (kap. 5).
- **Měření doby snímku** (`ft_total_start`, `frame_timing_last`) – jen pro
  ladicí okno DVUI.

## 4. Odchylky převzaté z dvui-layout

Platí beze změny (podrobně v
[dvui-layout/docs/ODCHYLKY.md](../../dvui-layout/docs/ODCHYLKY.md)):

- **API vlastností:** jeden seznam značek `dvui_Prop` místo struktur
  `Options` a `InitOptions`.
- **Id:** do id vstupuje adresa literálu `"soubor:řádek:čítač"` z
  `DVUI_SRC`.
- **Vynecháno:** text a fonty (místo popisku je zástupce s `label_size`),
  zaoblené rohy, stíny, gradienty, ikony, textury, klávesová navigace
  tabulátorem, dotyk (jeden capture, jedno tažení), AccessKit, dialogy.
- **Změněno:** jediné téma (Adwaita Light), `Color.lighten` v CIE LCh(uv)
  místo HSLuv, datové úložiště kontroluje typ jen velikostí.
- **Paměť:** bez vlastních alokátorů (`malloc`/`free`, vlastní aréna
  snímku), při nedostatku paměti `abort()`.
- **Backendy** (sdílené, `../shared/`): zúžené rozhraní bez textur,
  schránky a víc oken; SDL 3 GPU maže okno barvou pozadí, má jen bílou
  texturu 1×1 a 32bitové indexy. Rozhraní má navíc `sleep` (DVUI
  `Backend.sleep`), které dvui-animation používá.

## 5. Co oproti dvui-layout chybí

Zadání požaduje jen box a button (+ pomocné widgety), proto byly
z kopie dvui-layout odstraněny:

- scroll (ScrollInfo, ScrollBar, ScrollContainer, ScrollArea),
  `dvui_scroll_drag`; `dvui_scroll_to` zůstal jako prázdná funkce, protože
  ho volá `WidgetData.register` pro fokusovaný widget (DVUI: bez
  ScrollContainer také nic neudělá);
- grid a `Font.sizeM` (`dvui_font_size_m`);
- plovoucí widgety (Floating, FloatingTooltip, FloatingWindow, hlavička
  okna), `placeOnScreen`, `RenderFrontToBack`;
- ScaleWidget.

Okno zůstalo celé (subwindows, capture, tažení, fokus, události), aby bylo
věrné DVUI. Zůstaly widgety box, button, overlay, spacer, separator
a zástupce popisku.

## 6. Demo oproti DVUI Examples

Demo (`examples/demo.c`) napodobuje `src/Examples/animations.zig` bez
textu:

| DVUI Examples | Demo |
|---|---|
| checkbox „Reduce Motion“ s tooltipem | přepínač (čtvercové tlačítko), bez tooltipu |
| tlačítko „Wiggle“ | stejné, místo textu zástupce popisku |
| „Animating Window (Rect)“, „Animating Dialog (drop)“ | vynecháno (plovoucí okna a dialogy nejsou) |
| expander „Easings“ | bez expanderu, vždy zobrazeno |
| dropdown druhu animace | čtyři barevná tlačítka (alpha, horizontal, vertical, none), vybrané má silný okraj |
| dropdown easingu | 31 malých tlačítek ve dvou řadách |
| `sliderEntry` délky (50–2000 ms po 10 ms) | tlačítka −/+ po 100 ms (100–2000 ms) a pruh délky |
| checkbox „Center“ | přepínač |
| `plotXY` průběhu easingu | 100 svislých obdélníků (sloupcový graf) s osami, stejné rozsahy os |
| overlay 100×80 s paddingem 6 | overlay roztažený přes zbytek okna, aby byl graf vidět |
| „Spinner“, „Texture Frames“ | vynecháno |
| „Clock“: popisek „ms into second“ s časovačem | pruh „ms do sekundy“ + deset čtverců (sekunda mod 10); stejný časovač |
| odhad režie smyčky (`loop_target_slop`) jako text | vynecháno (je dostupný přes `dvui_window_loop_target_slop`) |
