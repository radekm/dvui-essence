# Držení stavu mezi snímky

Jak dvui-layout (stejně jako DVUI) pamatuje informace o widgetech z jednoho
snímku do dalšího. Navazuje na [ROZHRANI.md](ROZHRANI.md) (id, datové
úložiště) a [LAYOUT.md](LAYOUT.md) (jak se z min. velikostí počítá layout).

Obsah:

1. [Princip](#1-princip)
2. [Id – klíč ke všemu stavu](#2-id--klíč-ke-všemu-stavu)
3. [Hashovací mapa](#3-hashovací-mapa)
4. [Min. velikosti](#4-min-velikosti)
5. [Datové úložiště](#5-datové-úložiště)
6. [Úklid: co nepoužitého zmizí](#6-úklid-co-nepoužitého-zmizí)
7. [Stav v okně mimo mapy](#7-stav-v-okně-mimo-mapy)
8. [Co žije jen jeden snímek](#8-co-žije-jen-jeden-snímek)
9. [Klíče používané widgety](#9-klíče-používané-widgety)
10. [Rozdíly proti DVUI](#10-rozdíly-proti-dvui)
11. [Na co si dát pozor](#11-na-co-si-dát-pozor)

---

## 1. Princip

Immediate-mode GUI nemá strom objektů, který by přežíval mezi snímky. Každý
snímek program widgety znovu „deklaruje“: zavolá `*_init`, vloží děti a zavolá
`*_deinit`. Struktura widgetu (`dvui_BoxWidget`, `dvui_GridWidget`, …) žije jen
mezi `init` a `deinit`, buď na zásobníku volajícího, nebo v `calloc` pohodlného
makra, které se uvolní v `deinit`.

Všechno, co si widget potřebuje pamatovat do dalšího snímku, proto ukládá do
**okna** (`dvui_Window`) pod svým **id**:

```
snímek N                         okno (přežívá)                 snímek N+1
────────                         ──────────────                 ──────────
box init  ───────────── čte ───► min_sizes[id]  ◄──── čte ───── box init
box deinit ──────────── píše ──► min_sizes[id]
                                 data_store[id+"_data"] ◄─ čte ─ box rectFor
box deinit ──────────── píše ──► data_store[id+"_data"]
```

Ve stavu jsou dvě hashovací mapy (min. velikosti a datové úložiště) a několik
obyčejných polí okna pro stav, který nepatří jednomu widgetu (capture, fokus,
tažení, subwindows).

## 2. Id – klíč ke všemu stavu

Id je `uint64_t` (`dvui_Id`) odvozené při `dvui_widget_data_init`:

```
id = hash(id rodiče, adresa literálu "soubor:řádek:čítač", id_extra)
```

`DVUI_SRC` (DVUI `@src()`) vytvoří pro každé místo ve zdrojáku jiný
řetězcový literál a do id vstupuje jeho adresa – stálá po celý běh
programu, ale jiná v každém běhu (ASLR). Proto id ani pořadí položek
v mapách nejsou mezi běhy stejné; stav mezi běhy se neukládá, takže to
nevadí. `id_extra` rozliší víc widgetů ze stejného místa ve zdrojáku (smyčka). Protože
id obsahuje id rodiče, stejný widget pod jiným rodičem má jiné id – a tedy
jiný (prázdný) stav.

Z id se odvozují i klíče pro data: `dvui_id_update(id, "_klíč")` zahashuje
id s textovým klíčem. Jeden widget tak může mít libovolně mnoho uložených
hodnot.

Id 0 znamená „žádné“ (Zig `?Id = null`). Podrobnosti o id v
[ROZHRANI.md, kap. 4](ROZHRANI.md#4-id-a-dvui_src).

## 3. Hashovací mapa

Obě mapy jsou stejného typu `dvui_Map`, náhrada za DVUI
`TrackingAutoHashMap`. Mapa je sdílená s ostatními projekty
(`../shared/include/dvui/data.h`, `../shared/src/data.c`, viz
[shared/README.md](../../shared/README.md)):

- **otevřené adresování** s lineárním hledáním; hash klíče je finalizér
  z MurmurHash3 (`mix`);
- kapacita je mocnina dvou a začíná na 64; když obsazené a smazané položky
  dohromady překročí ¾ kapacity, mapa se přestaví – na dvojnásobnou
  kapacitu, nebo na stejnou, pokud jde hlavně o smazané položky;
- smazaná položka se jen označí jako „náhrobek“ (`state = 2`), aby se
  nepřerušily řetězce hledání; náhrobky zmizí při přestavbě mapy;
- položka (`dvui_MapEntry`) nese klíč, ukazatel na blob z `malloc`, jeho
  velikost a příznak **`used`** – „použito od posledního úklidu“.

| Funkce | Význam |
|---|---|
| `dvui_map_get(m, key)` | najde položku, nebo NULL |
| `dvui_map_get_or_put(m, key)` | najde, nebo vloží prázdnou (`ptr = NULL`) |
| `dvui_map_remove(m, key)` | uvolní blob a položku označí jako smazanou |
| `dvui_map_reset(m)` | úklid podle `used` (kap. 6) |
| `dvui_map_free(m)` | uvolní všechno (při zničení okna) |

Mapa sama `used` nenastavuje – to dělají funkce nad ní (kap. 4 a 5).

## 4. Min. velikosti

`win->min_sizes` (DVUI `Window.min_sizes`): **klíč = id widgetu, hodnota =
`dvui_Size`**. Je to stav, na kterém stojí celý vícesnímkový layout.

- **Zápis:** `dvui_widget_data_min_size_set_and_refresh` v `deinit` widgetu.
  Uloží min. velikost, kterou widget tento snímek spočítal (vlastní
  `min_size_content` + co nahlásily děti, omezené `max_size_content`),
  a nastaví `used = true`.
- **Čtení:** `dvui_min_size_get(id, &out)`, hlavně ve
  `dvui_widget_data_init` dalšího snímku – widget si od rodiče řekne o místo
  podle **minulé** min. velikosti (děti v tu chvíli ještě neznáme).
- **První snímek:** `dvui_first_frame(id)` je pravda, když pro id ještě
  min. velikost uložená není. Widget pak obvykle vyžádá další snímek
  (`dvui_refresh`), protože jeho velikost zatím neodpovídá obsahu.
- **Změna:** když se min. velikost liší od minulé a omezovala přidělený
  obdélník, `minSizeSetAndRefresh` vyžádá další snímek – layout se ustaluje
  postupně (viz [LAYOUT.md](LAYOUT.md)).
- Při `dvui_widget_data_register` se pozná **duplicitní id**: min. velikost
  pro toto id už byla tento snímek uložena (`used = true`) → hláška na
  stderr (chybí `id_extra`).

## 5. Datové úložiště

`win->data_store` (DVUI `Window.data_store`, `Data.zig`): **klíč =
`dvui_id_update(id, "_klíč")`, hodnota = libovolný blob**.

Úložiště je sdílená struktura `dvui_DataStore` (mapa `storage` + koš
`trash`, `../shared/include/dvui/data.h`) s funkcemi
`dvui_data_store_set/get/get_ptr_default/get_slice/remove/reset`. Okno ji
obsahuje a funkce `dvui_data_*` v `src/window.c` z id a jména spočítají
klíč a zavolají sdílenou funkci. Chování popsané níže je tedy ve všech
projektech stejné.

| Funkce / makro | Význam |
|---|---|
| `dvui_data_set(win, id, key, &v, size)` / `DVUI_DATA_SET(id, key, T, v)` | uloží kopii hodnoty |
| `dvui_data_get(win, id, key, size)` / `DVUI_DATA_GET(id, key, T)` | ukazatel na uloženou hodnotu, nebo NULL |
| `dvui_data_get_ptr_default(…)` / `DVUI_DATA_GET_PTR_DEFAULT(id, key, T, def)` | ukazatel; když hodnota není, nejdřív uloží `def` |
| `dvui_data_get_slice(win, id, key, &size)` | pole proměnné délky (velikost v bajtech) |
| `dvui_data_remove(win, id, key)` | odstraní hodnotu |

`win = NULL` znamená aktuální okno. Každé čtení i zápis nastaví položce
`used = true`.

**Typová kontrola jen podle velikosti.** Blob nemá typ; `dvui_data_get`
porovná požadovanou velikost s uloženou a při nesouladu zaloguje chybu
a vrátí NULL. DVUI kontroluje i jméno typu (v debug buildu).

**Zápis na místě.** Když se zapisuje hodnota **stejné velikosti**, přepíše
se do existujícího bloku. Ukazatele získané z `get` / `get_ptr_default`
tak zůstávají platné a widget může stav měnit přímo přes ukazatel – takhle
pracuje např. ScrollArea s `dvui_ScrollInfo` nebo grid se šířkami sloupců:

```c
dvui_ScrollInfo* si = dvui_data_get_ptr_default(nullptr, id, "_scroll_info", sizeof *si, &def);
si->viewport.y += 10;          // změna rovnou v úložišti
```

**Koš.** Když se změní velikost hodnoty (např. grid přidá sloupec a pole
`"__col_widths"` se prodlouží) nebo se hodnota odstraní, starý blob se
**neuvolní hned**, ale přesune do koše (`win->data_store.trash`). Koš se vysype až
na začátku dalšího snímku, takže kód, který si tento snímek ukazatel na
starou hodnotu už vzal, nespadne. Až do konce snímku ale vidí starou kopii.

**Pole.** `dvui_data_get_slice` vrací blob i s jeho velikostí; grid si
z ní dopočítá počet prvků. Při změně délky grid vytvoří nové pole, zkopíruje
starý obsah a doplní výchozí hodnoty (DVUI `dataSetSliceCopies`).

## 6. Úklid: co nepoužitého zmizí

Na začátku každého snímku (`dvui_window_begin`) se projdou obě mapy
(DVUI `Data.reset`, `TrackingAutoHashMap.reset`; v kódu
`dvui_data_store_reset` a `dvui_map_reset`):

```
pro každou položku:
    used == true  → used = false   (zůstává, další snímek se znovu ověří)
    used == false → smazat         (nikdo ji celý minulý snímek nepoužil)
pak vysypat koš datového úložiště
```

Důsledek: **stav widgetu přežije jen tehdy, když widget vznikne v každém
snímku.** Časová osa pro widget, který jeden snímek vynechá:

| snímek | widget | po `begin` | během snímku |
|---|---|---|---|
| N | vznikne | – | zapíše min. velikost a data (`used = true`) |
| N+1 | **nevznikne** | položky zůstanou, `used = false` | nikdo je nepoužije |
| N+2 | vznikne | položky **smazány** | `dvui_first_frame` = true, data jsou výchozí |

Widget, který se skryje a zase ukáže (podmínka v kódu, zavřené okno, řádek
gridu mimo viditelnou oblast), tedy začíná znovu „od nuly“. Kdo chce stav
zachovat i přes skrytí, musí ho držet sám (vlastní proměnnou, `rect_io`
u plovoucího okna, `scroll_info` u scroll oblasti).

Stejně se uklízejí **subwindows** (`dvui_subwindows_reset` v
`dvui_window_end`): subwindow, které se tento snímek nepřidalo, ze seznamu
zmizí.

## 7. Stav v okně mimo mapy

Stav, který nepatří jednomu widgetu, je v obyčejných polích `dvui_Window`
(`src/internal.h`). Odkazuje na widgety přes id, takže přežije, i když
struktury widgetů zaniknou.

| Pole | DVUI | Co drží | Jak se ukončí |
|---|---|---|---|
| `capture` + `has_capture` | `Window.capture` | id widgetu, který „chytil“ myš (tažení posuvníku, tlačítko při stisku), a jeho obdélník | widget ho každý snímek potvrdí `dvui_capture_mouse_maintain` (volá `register`); když se nepotvrdí (`captured_last_frame == false`), na začátku dalšího snímku se zruší |
| `dragging` | `Window.dragging` | stav tažení: tlačítko, výchozí bod, offset, jméno, kurzor | `dvui_drag_end`, nebo puštění tlačítka, které nikdo nezpracoval (`dvui_window_end`) |
| `subwindows` | `Window.subwindows` | zásobník subwindows (okno, plovoucí okna, tooltipy): id, obdélník, modálnost, fronty kreslení, **fokusovaný widget v subwindow** | úklid podle `used` (kap. 6); zmizí-li fokusované subwindow, fokus přejde na nejvyšší zbylé |
| `subwindows.focused_id` | `focused_subwindowId` | které subwindow má fokus | viz výše |
| `mouse_pt`, `mouse_pt_prev` | stejně | poslední pozice myši (fyzické pixely) | přepisuje se událostmi |
| `extra_frames_needed` | stejně | kolik snímků ještě udělat hned (bez čekání na událost) | `dvui_refresh` nastaví 1, `begin` sníží; `dvui_window_end` vrací 0 („kresli hned“), jinak −1 („čekej na událost“) |
| `frame_time_ns`, `secs_since_last_frame` | stejně | čas snímku (pro `hoverFade`, setrvačné scrollování) | každý `begin` |
| `theme`, `content_scale`, `open_flag` | stejně | nastavení okna | mění program |

Fokus jednotlivých widgetů je tedy uložený **v subwindow** (každé plovoucí
okno si pamatuje svůj fokusovaný widget), ne v mapě.

## 8. Co žije jen jeden snímek

Pro úplnost – tohle mezi snímky **nepřežívá**:

- **struktury widgetů** – od `init` do `deinit`; pohodlná makra je alokují
  `calloc` a eviduje je okno (`widget_allocs`), aby `dvui_window_end` uvolnil
  i ty, na které program zapomněl zavolat `deinit`;
- **aréna snímku** (`dvui_frame_alloc`, `dvui_frame_grow`): seznam událostí,
  pomocná pole gridu, dočasné kopie; vynuluje ji `dvui_window_end`;
- **události** – každý snímek nový seznam (`init_events`);
- **fronty kreslení** subwindows;
- `last_focused_id_this_frame`, `current_parent`, `layout` okna,
  vyžádaný kurzor.

## 9. Klíče používané widgety

Klíče s jedním podtržítkem jsou z DVUI pro „obyčejné“ widgety, se dvěma pro
grid a sdílené pomocné funkce – stejně jako v DVUI.

| Widget | Klíč | Typ | Co drží |
|---|---|---|---|
| Box | `"_data"` | `dvui_BoxData` | počet „zabalených“ dětí, součet vah roztažených dětí a místo zabrané neroztaženými – z toho se v dalším snímku počítá, kolik pixelů připadne na jednotku váhy (`pixels_per_w`) |
| ScrollArea | `"_scroll_info"` | `dvui_ScrollInfo` | posun, virtuální velikost, viewport, rychlost (jen když program nepředá vlastní `scroll_info`) |
| | `"_linsets"` | `dvui_Size` | levé/horní odsazení scroll kontejneru (zmenšuje viewport v dalším snímku) |
| | `"_scroll_id"` | `dvui_Id` | id vnitřního ScrollContaineru (cíl fokusu při kliknutí na posuvník) |
| ScrollContainer | `"_fv_id"`, `"_fv_offset"`, `"_fv_viewport"` | `dvui_Id`, `dvui_Point`, `dvui_Point` | první viditelné dítě pro `lock_visible` (udržení obsahu na místě, když se nad ním změní velikosti) |
| Scale | `"_scale"` | `float` | měřítko (když ho program nepředá) |
| FloatingWindow | `"_rect"` | `dvui_Rect` | pozice a velikost okna (když program nepředá `rect_io`) |
| | `"_auto_size"`, `"_auto_pos"` | `bool` | zda se okno ještě přizpůsobuje obsahu / centruje |
| | `"_drag_part"` | `dvui_DragPart` | za kterou část se okno táhne (střed, roh, hrana) |
| FloatingTooltip | `"_showing"` | `bool` | tooltip byl minulý snímek vidět |
| | `"_sticky_pt"` | `dvui_Point` | u pozice `STICKY`: pozice myši v prvním snímku tooltipu, u které tooltip zůstane |
| Grid | `"__cols"`, `"__rows"` | `size_t` | počet sloupců a řádků z minula |
| | `"__col_widths"` | pole `float` | šířky sloupců |
| | `"__col_expands"` | pole `bool` | které sloupce mají roztažené buňky |
| | `"__col_resizable"` | pole `bool` | které sloupce lze měnit tažením |
| | `"__row_heights"` | pole `dvui_GridRowHeight` | výšky řádků lišících se od výchozí |
| | `"__row_height_default"` | `dvui_GridRowHeightDefault` | výchozí výška řádku |
| | `"__col_header_height"` | `float` | výška hlavičky |
| | `"__auto_size"` | `dvui_GridAutoSize` | požadavek na auto-size v dalším snímku |
| | `"__csi"` | `dvui_ScrollInfo` | scroll hlavičky (synchronizovaný s tělem) |
| | `"__mouse_mode"` | `bool` | ovládá se myší (vs. klávesnicí) |
| všechny (hover) | `"__hover_fade"` | `float` | průběh plynulého zvýraznění při najetí myší (`dvui_hover_fade`) |

Vnořené widgety (hbox/vbox ScrollArey, scroll kontejner gridu) mají vlastní
id, takže i vlastní `"_data"` a min. velikosti.

## 10. Rozdíly proti DVUI

Mechanismus je stejný (min. velikosti, datové úložiště, úklid podle
použití, klíče). Liší se:

- **Chybí mapa animací** (`Window.animations`, `dvui.animation`,
  `animationGet`) a s ní časovače (`dvui.timer`). Jediný animovaný stav,
  `hoverFade`, je uložený v datovém úložišti. Až se budou portovat widgety,
  které animace potřebují (např. PanedWidget), přibude třetí mapa stejného
  druhu a `dvui_window_end` bude vracet čas do konce nejbližší animace.
- **Chybí mapa tagů** (`Window.tags`, `dvui.tag`/`tagGet` – pojmenovaná
  místa na obrazovce, používá je Docking).
- **Jeden slot tažení a jeden capture** místo slotu pro každé tlačítko/prst.
- **Typ dat** se kontroluje jen velikostí (DVUI navíc jménem typu).
- **Paměť:** DVUI ukládá data do vlastního alokátoru s vlastním košem;
  zde má každá hodnota vlastní `malloc` a koš je pole ukazatelů
  (viz [ROZHRANI.md, kap. 11](ROZHRANI.md#11-správa-paměti)).

## 11. Na co si dát pozor

- **Ve smyčce vždy `DVUI_ID_EXTRA(i)`.** Jinak mají widgety stejné id, sdílí
  min. velikost i data a layout „skáče“ (na stderr je hláška o duplicitním id).
- **`id_extra` má odpovídat identitě položky, ne pozici.** Když se ze
  seznamu odebere položka a `id_extra` je index, všechny další položky
  „zdědí“ stav té předchozí.
- **Podmíněně skrytý widget ztrácí stav** po jednom vynechaném snímku
  (kap. 6). Trvalý stav držte v programu.
- **Ukazatel do úložiště platí jen do konce snímku** a jen dokud se nezmění
  velikost hodnoty. Mezi snímky si ho nepamatujte – v dalším snímku si ho
  vezměte znovu.
- **Stejný klíč, jiný typ stejné velikosti se nepozná.** Klíče proto volte
  jako DVUI (`"_jméno"`) a pro jeden klíč používejte vždy jeden typ.
- **Přesun widgetu pod jiného rodiče mění id**, takže widget začíná
  s prázdným stavem.
