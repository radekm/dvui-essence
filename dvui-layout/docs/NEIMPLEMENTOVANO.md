# Neimplementované layouty a kontejnery

Přehled widgetů z DVUI (`dvui-original/src/widgets/`), které dvui-layout
zatím nemá: co dělají, co je na nich z pohledu layoutu zajímavé, jak těžké by
bylo je doplnit a co by se kvůli nim muselo přidat nebo změnit v jádru.
Jak se drží stav, popisuje [STAV.md](STAV.md); jak funguje layout
[LAYOUT.md](LAYOUT.md).

Obsah:

1. [Přehled](#1-přehled)
2. [Co chybí v jádru](#2-co-chybí-v-jádru)
3. [Kontejnery s vlastním layoutem](#3-kontejnery-s-vlastním-layoutem)
4. [Kontejnery, které skládají existující widgety](#4-kontejnery-které-skládají-existující-widgety)
5. [Widgety mimo rozsah dvui-layout](#5-widgety-mimo-rozsah-dvui-layout)
6. [Doporučené pořadí](#6-doporučené-pořadí)

---

## 1. Přehled

Hotové jsou: Box, Button, Overlay, Scale, ScrollBar, ScrollContainer,
ScrollArea, Grid, Floating, FloatingTooltip, FloatingWindow, spacer,
separator, zástupce Label a windowHeader.

Obtížnost: **snadné** = hodiny, nic nového v jádru; **střední** = den až
dva, drobnosti v jádru; **těžké** = několik dní, nová infrastruktura;
**velmi těžké** = samostatný projekt. Řádky jsou délka zdrojáku v Zigu.

| Widget | Řádky | Co dělá | Obtížnost | Co chybí v jádru |
|---|---|---|---|---|
| VirtualParentWidget | 66 | průhledný rodič, jen mění id dětí | snadné | nic |
| CacheSizeWidget | 96 | přeskočí deklaraci obsahu, dokud se nemění velikost | snadné | `data_get_default` (triviální) |
| FlexBoxWidget | 145 | řádky se zalamováním (CSS flex-wrap) | snadné | nic |
| PanedWidget | 473 | dvě oblasti s přetahovatelnou příčkou | střední | animace |
| AnimateWidget | 161 | animace průhlednosti / výšky / šířky obsahu | střední | animace, easing, alfa při kreslení |
| ReorderWidget | 435 | přeuspořádání položek tažením | střední až těžké | `drag_name`, zástupce ikony |
| TreeWidget | 539 | strom s rozbalováním a přetahováním větví | těžké | Reorder, animace, FocusGroup, expander |
| TabsWidget | 210 | řada záložek nad obsahem | střední | FocusGroup, kreslení cest (obrys záložky) |
| MenuWidget, FloatingMenuWidget, MenuItemWidget | 983 | menu a podmenu | těžké | FocusGroup, klávesová navigace, kořen menu |
| ContextWidget | 229 | kontextové menu (pravé tlačítko, podržení) | střední | menu, časovače |
| DockingWidget | 1 858 | dokovací panely (strom rozdělení + záložky) | velmi těžké | Paned, Tabs, menu, context, tagy |
| CacheWidget | 199 | kreslí obsah do textury a pak jen ji | těžké | render targety, textury |
| FocusGroupWidget | 220 | skupina, ve které se fokus posouvá šipkami | střední | klávesová navigace (`tab_index`) |
| OsWindowWidget | 197 | další okno operačního systému | těžké | víc oken v backendu |
| Label, TextLayout, TextEntry, Suggestion, Dropdown | 5 089 | text | – | patří do dvui-text |
| Plot, ColorPicker, Icon | 1 381 | grafy, výběr barvy, ikony | – | mimo rozsah (kap. 5) |

## 2. Co chybí v jádru

Sdílené věci, které potřebuje víc widgetů. Doplnit je dává smysl dřív než
samotné widgety.

### 2.1 Animace a časovače

> Animace, časovače, `waitTime` a AnimateWidget jsou implementované
> v samostatném projektu [dvui-animation](../../dvui-animation/README.md).
> Níže je původní rozbor pro dvui-layout.

DVUI: `dvui.animation`, `dvui.animationGet`, `Animation` (`start_val`,
`end_val`, `start_time`, `end_time`, `easing`), `easing.zig`, `dvui.timer`,
`timerGet`, `timerDone`. Potřebují je Paned (sbalení), Animate, Tree
(rozbalení větve), Context (podržení prstu = časovač).

Co přidat:
- třetí mapu v okně `animations` (klíč `dvui_id_update(id, key)`, hodnota
  `dvui_Animation`), stejného druhu jako min. velikosti (viz
  [STAV.md](STAV.md)) – úklid ale není podle `used`, nýbrž podle času:
  v `window_begin` se všem animacím odečte uplynulý čas (časy jsou relativní
  k aktuálnímu snímku); animace, která skončila už v minulém snímku, se
  smaže, takže každá je ještě jeden snímek vidět jako dokončená (DVUI
  `Window.begin` – „update animations and remove dead ones“);
- `dvui_window_end` dnes vrací jen 0 / −1. S animacemi musí vracet **čas
  do konce nejbližší animace nebo časovače** (DVUI `Window.end` vrací
  `?u32` mikrosekund), aby
  backend věděl, kdy probudit smyčku. Backend s tím už počítá
  (`dvui_sdl3gpu_wait_event_timeout`, `sleep`);
- `easing.zig` (240 řádků čistých funkcí) – stačí přepsat jen použité
  (`linear`, `outQuad`, `inOutQuad`, …);
- testy: testovací backend musí umět posunout čas (už umí – `step` přičítá
  16 ms) a ověřit, že se po skončení animace přestanou žádat snímky.

Obtížnost: **střední**. Je to izolovaná změna ve `window.c` a nové API.

### 2.2 Tažení: jméno a sloty

- `dvui_drag_name(name)` – je tažení s tímto jménem aktivní? Pole
  `dragging.name` už existuje, chybí jen funkce. **Triviální.**
- DVUI má **slot tažení pro každé tlačítko/prst** (`dragging.slot(b)`),
  my jeden. Reorder a Tree se ptají na slot podle tlačítka – pro myš stačí
  vracet ten jeden. Plná podpora by znamenala i capture pro každý prst
  (dotykové ovládání, které jsme vynechali). **Pro myš nic neměnit.**

### 2.3 Drobnosti v datovém úložišti

- `dvui_data_get_default` (hodnota, ne ukazatel; při chybějící hodnotě
  vrátí výchozí a nic neukládá) – CacheSize, Tree. **Triviální.**
- přístup k `extra_frames_needed` mimo `window.c` (CacheSize ho dočasně
  nuluje, stejně jako to už dělá FloatingWindow při auto-size) – stačí
  funkce v `internal.h`.

### 2.4 Fokus a klávesová navigace

DVUI: `tab_index` v Options, `tabIndexNext/Prev`, šipky ve
`Window.end`, `FocusGroupWidget`. Vynechali jsme je (viz README). Potřebují
je Tabs, Menu, Tree; bez nich tyto widgety fungují jen myší.

Co přidat:
- vlastnost `tab_index` a seznam fokusovatelných widgetů v pořadí
  registrace (DVUI `Window.tab_index`, `tab_index_prev`);
- obsluhu Tab / Shift+Tab a šipek v `dvui_window_end`;
- `FocusGroupWidget` (≈ 220 řádků) – skupina, ve které šipky přesouvají
  fokus mezi dětmi.

Obtížnost: **střední**. Layoutu se netýká, ale je to předpoklad pro menu.

### 2.5 Menu jako systém

Menu v DVUI není jen widget, ale i globální stav: `MenuWidget.Root`
(kdo menu otevřel a jak ho zavřít), řetěz rodičovských menu
(`parentMenu`, `close_chain`), podmenu jako samostatná subwindows
(`FloatingMenuWidget`), `subwindowCurrentSet`, klávesová navigace
(`FloatingMenuWidget.Style`). Plovoucí subwindows už máme
(FloatingWidget, FloatingTooltip), takže layoutová část je hotová; chybí
logika otevírání a zavírání.

Obtížnost: **těžké** (≈ 1 000 řádků Zigu + FocusGroup).

### 2.6 Tagy

DVUI: vlastnost `tag` v Options, `dvui.tag` / `tagGet` – widget se při
`register` zapíše pod jménem do mapy `Window.tags` (obdélník, viditelnost).
Používá je Docking (cíle přetažení panelu) a testy DVUI. Je to další mapa
stejného druhu jako min. velikosti a nová společná vlastnost.

Obtížnost: **snadné**.

### 2.7 Kreslení

Kreslíme jen plné obdélníky. Chybí:
- **cesty** (`Path.Builder`, `stroke`, triangulace) – obrys záložky
  u Tabs, šipky expanderu, zaoblené rohy obecně;
- **alfa vrstvy** – Animate mění průhlednost obsahu (`dvui.alpha`);
  jednoduché: násobit alfa barvy v renderovací frontě;
- **render targety a textury** – CacheWidget, rozmazané pozadí.
  Vyžaduje změnu backendu SDL3 GPU (textura jako cíl kreslení) i
  testovacího backendu.

Tabs lze udělat bez cest (záložka jako obdélník – stejná odchylka jako
dosud u rohů). Render targety jsou **těžké** a do layoutu nic nepřinášejí.

### 2.8 Ikony

Reorder (úchyt), Tree a expander (šipka), Docking (zavírací křížek) kreslí
ikony (TinyVG). Stačí zástupce jako u zavíracího tlačítka okna – čtverec
o velikosti písma. **Snadné.**

## 3. Kontejnery s vlastním layoutem

Tyto widgety mají vlastní `rectFor` / `minSizeForChild`, tedy nový
algoritmus layoutu. Z hlediska cíle projektu (věrný port layoutu) jsou
nejcennější.

### 3.1 PanedWidget – rozdělovač

Dvě oblasti (`showFirst()`, `showSecond()`) oddělené příčkou, kterou lze
táhnout myší.

Zajímavé na layoutu:
- `rectFor` dělí content rect podle `split_ratio` (0–1) a odečte mezeru pro
  příčku (`handleGap` – zmenší se až na 0, když je příčka u okraje);
- uvnitř každé strany je `BasicLayout`, takže děti se skládají jako
  v Overlay;
- **min. velikost je maximum, ne součet stran** (`minSizeForChild` jde přes
  jeden `BasicLayout`) – Paned nikdy nepožaduje místo pro obě strany
  současně;
- **sbalení:** když je widget v daném směru menší než `collapsed_size`,
  ukazuje jen jednu stranu (podle toho, ke které byl poměr blíž) a druhou
  „vysune“ mimo obdélník; přechod je animovaný (250 ms);
- **`autofit_first`:** v prvním snímku dá první straně celé místo, ve druhém
  změří min. velikost jejích dětí (`layout.min_size_children`) a nastaví
  podle ní `split_ratio` – další vícesnímkový mechanismus, podobný gridu;
- stav: `"_split_ratio"`, `"_collapsed"`, `"_collapsing"`,
  `"_autofit_next_frame"`.

Co chybí: **animace** (2.1). Bez nich by šlo poměr nastavit skokem, ale
byla by to odchylka od DVUI. `BasicLayout.min_size_children` už máme.

Obtížnost: **střední**. Testy: rozdělení, tažení příčky, sbalení pod
`collapsed_size`, autofit přes dva snímky.

### 3.2 FlexBoxWidget – zalamování

Děti se skládají zleva doprava; když se další nevejde, začne nový řádek.

Zajímavé na layoutu:
- **výška závisí na šířce.** `rectFor` potřebuje šířku kontejneru; když je
  0 (první snímek), vezme `min_size_content.w`, jinak odhad 500;
- **hlášená min. velikost:** šířka = všechny děti v jednom řádku
  (`width_nobreak`), výška = skutečná výška po zalomení. Rodič tedy dostane
  „ideální“ šířku, ale FlexBox se zalomí do té, kterou opravdu dostane;
- `justify_content = center` centruje řádky podle **minulé** nejširší řady
  (`"_mrw"`) – další hodnota z minulého snímku;
- `border_collapse` – překrytí sousedních okrajů.

Je to dobrá příprava na zalamovaný text (dvui-text) – stejný problém
„výška závisí na šířce“.

Co chybí: nic. Obtížnost: **snadné**.

### 3.3 VirtualParentWidget – průhledný rodič

`rectFor` a `minSizeForChild` předává svému rodiči, jen si sjednocuje
obdélníky dětí (a ukládá je do `"_rect"`). Slouží ke změně id skupiny
widgetů (vlastní `id_extra`) bez zásahu do layoutu.

Zajímavé: ukazuje, že rozhraní `dvui_Widget` nemusí patřit „skutečnému“
kontejneru. Vlastní WidgetData má `rect` z minulého snímku (sjednocení
dětí), takže `register` / capture fungují na správném místě.

Co chybí: nic. Obtížnost: **snadné**.

### 3.4 CacheSizeWidget – přeskočení obsahu

Pamatuje si velikost obsahu; dokud se nezmění, `uncached()` vrací false
a program obsah vůbec nedeklaruje (a widget hlásí uloženou min. velikost).
Optimalizace pro drahý obsah.

Zajímavé: pracuje přímo s `min_sizes` a `extra_frames_needed` – musí
poznat, že se layout ustálil.

Co chybí: `data_get_default`, přístup k `extra_frames_needed` (2.3).
Obtížnost: **snadné**.

### 3.5 ReorderWidget – přeuspořádání tažením

`ReorderWidget` obsahuje `Reorderable` položky. Při tažení:
- tažená položka se **vyjme z layoutu** a vloží do `FloatingWidget`
  u myši (`reinstall1/2` – jeden widget během snímku mění rodiče);
- na místě pod myší vznikne **prázdný slot** o velikosti tažené položky
  (`reorderable_size`), ostatní se posunou;
- poslední slot (`last_slot`) umožní vložit na konec;
- při puštění program dostane, odkud kam se položka přesunula, a změní
  pořadí ve svých datech;
- během tažení u okraje scroll oblasti se scrolluje (`scrollDrag` – máme).

Zajímavé: jediné místo v DVUI, kde widget přechází mezi rodiči během
snímku; WidgetData se inicializuje dvakrát (nejdřív s `rect = {}`, pak
skutečně při `install`).

Co chybí: `drag_name` (2.2), zástupce ikony úchytu (2.8). FloatingWidget
máme.

Obtížnost: **střední až těžké** – kódu není moc, ale je choulostivý na
pořadí kroků. Testy: tažení přes testovací backend (posun myši, stisk,
puštění) a kontrola obdélníků slotů.

### 3.6 TreeWidget – strom

Větve se rozbalují (expander s animací) a přetahují (stejný mechanismus
jako Reorder, včetně vkládání do jiné větve). Navigace šipkami přes
FocusGroup.

Co chybí: Reorder, animace, FocusGroup (2.4), expander (tlačítko se šipkou
a stavem `"__expand"`), ikony.

Obtížnost: **těžké**. Layoutově nepřináší nic nového oproti Reorder + Box.

### 3.7 AnimateWidget – animovaný obsah

Obsah se při objevení/zmizení animuje: průhledností (`alpha`), nebo
výškou/šířkou (`vertical`, `horizontal`) – během animace hlásí rodiči jen
část své min. velikosti, takže okolí plynule „odjíždí“.

Zajímavé: min. velikost se mění v čase, ne podle obsahu.

Co chybí: animace a easing (2.1), alfa při kreslení (2.7).
Obtížnost: **střední** (po doplnění animací snadné).

## 4. Kontejnery, které skládají existující widgety

Layoutově nic nového – jsou postavené z Boxu, ScrollArey a plovoucích
widgetů. Náročné jsou kvůli interakci.

### 4.1 TabsWidget

ScrollArea (bez posuvníku) s Boxem tlačítek; vybraná záložka má jiný obrys
a „splyne“ s obsahem pod ní. Obsah záložky kreslí program sám.

Co chybí: FocusGroup (šipky mezi záložkami), kreslení cest pro obrys
(lze nahradit obdélníkem), popisky (zástupce). Obtížnost: **střední**.

### 4.2 Menu, FloatingMenu, MenuItem

Menu je Box s položkami; podmenu je `FloatingMenuWidget` – subwindow
umístěné vedle položky (`placeOnScreen` s `avoid`). Viz 2.5.

Co chybí: celý systém menu (2.5), FocusGroup (2.4). Obtížnost: **těžké**.

### 4.3 ContextWidget

Obalí oblast; pravé tlačítko (nebo podržení prstu – časovač 100 ms)
otevře FloatingMenu v místě kliknutí.

Co chybí: menu, časovače (2.1). Obtížnost: **střední** po menu.

### 4.4 DockingWidget

Dokovací panely jako v IDE: binární strom rozdělení (`DockLayout`), každý
uzel je PanedWidget, každý list záložky (Tabs) s obsahem; panely lze
odtrhnout do plovoucích oken a přetáhnout jinam (cíle přetažení přes
tagy). Změny stromu se během snímku jen řadí do fronty a provedou se
v `deinit`, aby ukazatele na `split_ratio` zůstaly platné.

Co chybí: Paned, Tabs, menu, Context, tagy, ikony – tedy skoro všechno
výše. Samotný strom (`Layout.zig`, ≈ 800 řádků) je čistá datová struktura
bez GUI a dá se portovat a testovat samostatně.

Obtížnost: **velmi těžké** – vhodné jako samostatný projekt
(`dvui-docking`) až po Paned a Tabs.

### 4.5 FocusGroupWidget

Viz 2.4. Layoutově je to obyčejný kontejner s `BasicLayout`.

## 5. Widgety mimo rozsah dvui-layout

- **Text:** `LabelWidget`, `TextLayoutWidget` (zalamování, výběr),
  `TextEntryWidget`, `SuggestionWidget`, `DropdownWidget` – plánované
  projekty dvui-text, dvui-text-selection a dvui-text-edit. Dropdown
  a Suggestion navíc potřebují FloatingMenu.
- **CacheWidget** – render do textury (2.7); layoutově nic nového.
- **OsWindowWidget** – další okno operačního systému; vyžaduje víc
  `dvui_Window` nad jedním backendem a změnu backendu SDL3 GPU (víc
  swapchainů). Layoutově nic nového.
- **PlotWidget, ColorPickerWidget** – kreslení (čáry, gradienty);
  layoutově jen Box.
- **IconWidget** – TinyVG; nahrazuje se zástupcem.
- **Dialogy, toasty, `popup`** (funkce v `dvui.zig`) – stojí na
  FloatingWindow a časovačích; po doplnění časovačů snadné, ale bez textu
  mají malý smysl.

## 6. Doporučené pořadí

1. **FlexBox, VirtualParent, CacheSize** – bez změn v jádru, každý ukazuje
   jiný princip (výška podle šířky, průhledný rodič, ustálení layoutu).
2. **Animace a časovače** (2.1) – infrastruktura, změna návratové hodnoty
   `dvui_window_end`.
3. **Paned** – praktický, vícesnímkový autofit, používá animace.
4. **Animate** – po animacích snadné.
5. **Reorder** (+ `drag_name`, zástupce ikony).
6. **FocusGroup a `tab_index`** (2.4), pak **Tabs**.
7. **Menu, FloatingMenu, Context**.
8. **Tree**.
9. **Docking** jako samostatný projekt.

Body 1–5 rozšiřují layout; body 6–9 jsou hlavně interakce a dávají smysl
spíš spolu s textem.
