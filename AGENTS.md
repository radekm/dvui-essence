# AGENTS.md – pravidla pro práci v repozitáři dvui-essence

Tato pravidla platí pro všechny (lidi i AI agenty), kdo v repozitáři pracují.
Před začátkem práce si přečti také [PLAN.md](PLAN.md).

## Komunikace a jazyk

- S vlastníkem repozitáře se komunikuje **česky**.
- Zdrojový kód (identifikátory, názvy souborů, commit zprávy) se píše **anglicky**.
- Komentáře ve zdrojovém kódu se píší **česky**.
- Dokumentace (README, PLAN, AGENTS, návody) se píše **česky**.
- Když si nejsi něčím jistý, **zeptej se** vlastníka, než začneš něco
  rozsáhlého dělat nebo než uděláš nevratné rozhodnutí.

## Struktura repozitáře

- Výchozí a pracovní větev je `master`.
- Repozitář obsahuje více samostatných projektů, každý ve vlastním adresáři
  (`dvui-layout/`, `dvui-animation/`, později `dvui-text/`, `dvui-text-selection/`, `dvui-text-edit/`, …).
- Každý projekt má vlastní `meson.build` a lze ho sestavit samostatně
  (ze svého adresáře).
- `shared/` obsahuje kód společný všem projektům (popis v
  [shared/README.md](shared/README.md)): základní typy (`dvui/base.h` –
  id, `DVUI_SRC`, geometrie, barvy, vstupní výčty, `dvui_Backend`
  a deklarace funkcí okna volaných backendem), geometrii a barvy, id,
  arénu, hash mapu, datové úložiště (`dvui_DataStore`), backendy SDL 3 GPU
  a testovací a shadery. `shared/` nemá vlastní
  `meson.build`; projekty jeho soubory uvádějí cestou `../shared/…`.
- Každý projekt má **vlastní** (zkopírovaný a upravený) zbytek: `dvui.h`,
  okno (`window.c`), WidgetData, vlastnosti widgetů (seznam značek
  `dvui_Prop`, X-makra), téma, layout a widgety. Funkce okna, které volá
  backend (`dvui_window_add_event_*`, `dvui_window_modifiers_set`,
  `dvui_scroll_speed`), implementuje každý projekt ve svém okně.
- **Zásada pro sdílený kód: zobecňuj jen málo.** Do `shared/` patří jen to,
  co jde sdílet beze změny nebo s malou úpravou. Když by sdílení vyžadovalo
  výraznější změnu (parametrizaci, háčky, podmínky pro jednotlivé
  projekty), kód raději zduplikuj do projektu a uprav. Když nevíš,
  zeptej se vlastníka. Změna ve `shared/` musí zachovat build, testy
  a chování všech projektů, které ho používají.
- `dvui-original/` je **neměnná** kopie původního DVUI (obyčejné soubory, ne
  submodul). Slouží jako reference. Nikdy ji neupravuj; při aktualizaci na novější
  verzi DVUI se nahradí celá a v PLAN.md se zapíše nový commit.

## Jazyk a sestavení

- Nový kód se píše v **C23** (`c_std=c23`, Meson ≥ 1.4). Minimální překladače
  jsou **GCC ≥ 15 a Clang ≥ 19** – první verze s `#embed`. Používej jen tu
  část C23, kterou obě podporují (bool/true/false jako klíčová slova,
  `nullptr`, `static_assert`, `[[nodiscard]]`, `typeof`, `alignas`, `#embed`).
- Binární data (shadery) se do programu vkládají přes `#embed`, ne
  generovaným kódem ani vlastními nástroji v buildu.
- Build systém je **Meson**.
- Cílové platformy: **Linux, Windows (Clang nebo MinGW-GCC), macOS**. Nepoužívej
  rozšíření specifická pro jeden překladač, pokud nejsou schovaná za `#if`.
  `__COUNTER__` je povolen (podporují ho GCC, Clang i MSVC).

## Závislosti

- Všechny závislosti musí být uložené přímo v repozitáři (vendored).
- **Jedinou výjimkou je SDL 3** – ta se do repozitáře neukládá, hledá se v systému
  přes `dependency('sdl3')`.
- **Každou novou závislost musí vlastník explicitně povolit.** Bez povolení žádnou
  knihovnu ani nástroj nepřidávej (ani do build kroku).
- Seznam povolených závislostí:
  | Závislost | Kde | Poznámka |
  |---|---|---|
  | SDL 3 | systém | jen backend a demo; jádro projektů na SDL nezávisí |
- Předkompilované shadery pro SDL3 GPU jsou převzaté z DVUI
  (`dvui-original/src/backends/sdl3gpu/compiled/`), nejsou to nové závislosti.
  Leží v `shared/shaders/` a `shared/src/backends/sdl3gpu.c` je vkládá přes
  `#embed`.

## Věrnost DVUI

- Kód layoutu musí zůstat **věrný DVUI**: stejné datové struktury, stejné
  algoritmy, stejné pořadí kroků (init → register → rectFor/minSizeForChild →
  deinit → minSizeSetAndRefresh → minSizeReportToParent), stejné klíče v datovém
  úložišti (`"_data"`, `"_scroll_info"`, `"__col_widths"`, …).
- Pojmenování: Zig `BoxWidget.init` → C `dvui_box_widget_init`, `dvui.placeIn`
  → `dvui_place_in`, typy `dvui_BoxWidget`, `dvui_Rect`, výčty `DVUI_EXPAND_BOTH`.
- U každé portované funkce uveď v komentáři, odkud pochází
  (např. `// DVUI: src/widgets/BoxWidget.zig – rectFor`).
- Každou vědomou odchylku od DVUI zapiš podrobně do `docs/ODCHYLKY.md`
  a stručně do sekce „Odchylky od DVUI“ v `README.md` příslušného
  projektu. Neodchyluj se bez důvodu.

## Dokumentace patří k projektu

- Každý projekt má dokumentaci u sebe: `<projekt>/README.md`
  a `<projekt>/docs/`. Sdílený kód má `shared/README.md`.
- Kořenový `README.md` obsahuje jen přehled projektů a odkazy na jejich
  dokumentaci; stav a plán jsou v `PLAN.md`.
- Dokumentaci jednoho projektu nedávej do jiného. Když se projekt změní
  kvůli `shared/`, aktualizuj jeho vlastní dokumentaci (rozhraní, stav,
  odchylky) a odkaž na `shared/README.md`.

## Styl kódu

- Hvězdička ukazatele patří k typu: `Typ* p`, `const char* name`,
  `void** items`, `(dvui_Window*)ptr`, `dvui_WidgetData* dvui_widget_data(…)`.
  Nepiš `Typ *p`.
- V jedné deklaraci deklaruj jen jeden ukazatel (`Typ* a; Typ* b;`, ne `Typ* a, *b;`).

## Kvalita

- Před commitem musí projít `meson test` daného projektu (při změně
  `shared/` všech projektů).
- Layout se testuje bez GPU přes testovací backend (`backends/testing`), stejně
  jako to dělá DVUI.
- Kód se překládá bez varování s `warning_level=3`.
- Commit zprávy anglicky, krátce a věcně.
