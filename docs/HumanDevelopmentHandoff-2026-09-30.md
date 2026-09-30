<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Předání vývoje sharp-runtime člověku

*Inventura vznikla 2026-09-30 na `feature/gamer-services-collections`, HEAD `88f6b11f`.
Níže uvedené H-01 až H-07 popisují původní nálezy na tomto snapshotu. Opravy vznikají výhradně
v odděleném worktree na `codex/human-handoff-20260930`; jejich ověřený výsledek je v závěrečné
sekci „Stav oprav“. Historické údaje jsou oddělené od současného výsledku.*

## 1. Stručný stav

sharp-runtime je rozsáhlá C++23 knihovna pro praktickou podmnožinu .NET `System.*`, zejména pro
CNA a porty XNA her. Aktuální [generovaný katalog](ComponentCatalog.md) má **44 fyzických
komponent a 108 přímých produkčních závislostí**. Kód je rozdělen na veřejné hlavičky,
implementace, komponentové testy a integrační testy. `Collections.Core` je převážně hlavičková
knihovna; změna šablony může zasáhnout mnoho překladových jednotek a konzumentů.

Pracovní větev `feature/gamer-services-collections` je v `next`; `next` ji převyšuje o jeden
integrační commit. `develop` je 58 commitů za `next`. Při lidském převzetí proto nejdříve
vybrat cílovou větev a revizi, zejména kvůli sesterskému checkoutu v CNA. V tomto průchodu
neproběhl merge, commit ani push.

Historický audit dopadl dobře **pro svůj tehdejší rozsah**: všech 364 nálezů je klasifikováno,
z toho 343 opraveno, 19 přijato jako výslovná odchylka a 2 byly falešně pozitivní. Zpráva
[AuditFindingsReconciliation.md](AuditFindingsReconciliation.md) a její strojová kontrola souhlasí.
Audit se však uzavíral 2026-08-22; zářijové změny a nové komponenty nejsou automaticky auditované
jen proto, že starý index ukazuje nulu otevřených nálezů.

**Dnešní plná brána není zelená.** Po odstranění místního problému se zápisem `ccache` build
selže na dvou `static_assert` v testu `Timer`; kontrola hranic komponent hlásí dva problémy,
kontrola shody lokální a CI matice jeden, přísný soupis hlaviček sedm. Testy používající místní
sockety v tomto sandboxu selhávají na `EPERM` už při `socket()`. Nejde z těchto výsledků vyvozovat
novou celkovou hodnotu „všechny testy prošly“.

Poslední doložená kompletní zelená sada na této větvi **před posledním commitem** je
**18 120/18 120 v 41 spustitelných souborech**, zaznamenaná 2026-09-28 v
[Migration-DictionaryEntryEnumeration.md](Migration-DictionaryEntryEnumeration.md) a `plan.md`.
I tehdy byly dva nálezy validátoru modulových hranic výslovně ponechány jako zděděný problém;
„zelená sada testů“ tedy neznamenala zelený `local_ci_check.sh`. Starší číslo **17 840/38** v
`README.md` a `AGENTS.md` patří k 2026-08-22 a není stav aktuální větve.

## 2. Měřítko repozitáře a dokumentace

Měření nad soubory vrácenými `git ls-files` na tomto HEAD; `MB` a `Mbit` jsou desetinné jednotky,
`MiB` je 2²⁰ bajtů. Řádky počítají fyzické konce řádků včetně komentářů a prázdných řádků.

| Oblast | Počet | Bajty | Přibližné řádky / poznámka |
|---|---:|---:|---|
| Celý sledovaný pracovní strom | 4 189 souborů | 41 528 975 | 663 925 řádků v souborech |
| Všechny sledované Markdown soubory | 2 100 | 12 890 100 = **12,89 MB = 103,12 Mbit = 12,29 MiB** | přibližně třetina objemu sledovaných dat |
| `audit/` Markdown | 1 755 | 4 559 926 = 36,48 Mbit | historické per-file důkazy a index |
| `docs/` Markdown | 292 | 5 738 171 = 45,91 Mbit | návrhy, migrace, poznámky |
| Markdown v kořeni | 8 | 2 567 838 = 20,54 Mbit | zejména `NEXT.md`, `plan.md`, `AGENTS.md` |
| `NEXT.md` samotný | 1 | 1 511 464 = 12,09 Mbit | není prakticky stručný cold-start návod |
| `AGENTS.md` samotný | 1 | 369 592 = 2,96 Mbit | obsahuje velmi dlouhý historický testový ledger |
| `plan.sqlite3` | 1 | 8 982 528 = 71,86 Mbit | binární evidence; **je sledovaná Gitem** |

V modulech je 1 098 sledovaných souborů pod `include/` (143 508 řádků), 248 pod `src/`
(48 402 řádků) a 541 pod `tests/` (179 183 řádků). Přísný zdrojový inventář vidí 1 107
`.hpp` a 230 `.cpp` pod aktuálními zdrojovými cestami, celkem 193 238 řádků; tento soupis
zahrnuje jiné přesné přípony a pravidla než předchozí Git součty. Jde o velikost a náklady na
orientaci, nikoli o metriku kvality.

Pro člověka není rozumné číst 103 Mbit Markdownu lineárně. Jako první mapu stačí tento handoff,
`README.md` (hlavně build a architektura), [CMakeComponents.md](CMakeComponents.md),
[StandingApprovals.md](StandingApprovals.md), [AuditFindingsReconciliation.md](AuditFindingsReconciliation.md)
a příslušná migrační/návrhová zpráva ke konkrétnímu typu. Historické `NEXT.md`, `plan.md` a
`audit/` mají sloužit jako dohledatelný důkaz, ne jako každodenní pracovní fronta. Před použitím
jejich tvrzení „current“ je nutné ověřit datum a HEAD.

## 3. SQLite ticketů: co říká a co ne

`plan.sqlite3` má dvě tabulky, `ticket` a `task`. Dotazy `sqlite3 plan.sqlite3 ...` daly:

| Evidence | Stav k poslední aktualizaci databáze 2026-08-22 |
|---|---:|
| `ticket`: `done` | 2 409 |
| `ticket`: `blocked` | 3 |
| `ticket`: `wontfix` | 5 |
| `ticket`: `todo`, `doing`, `needs_user` | 0 |
| `task`: `ported` | 1 087 |
| `task`: `ignore` | 140 |
| `task`: legacy `ignored` | 14 980 |

Celkem je **2 417 ticketů**; nejvyšší číslo je **#2419**, protože dvě čísla v posloupnosti
chybějí. Celkem je **16 207 klasifikovaných task řádků**. `db_consistency_check.py` pro tuto
databázi prošel. Poslední `updated_at` ticketu je `2026-08-22 18:27:45`; databáze tedy **není
evidencí zářijového vývoje**, přestože je samotný soubor sledovaný Gitem. Výrok „0 todo“ znamená
uzavřenou srpnovou frontu, nikoli že současný HEAD nemá práci.

Tři blokované tickety mají odlišný charakter:

- **#1773:** migrace CNA/mobile-eggbert po změně `ICollection::CopyTo`; čeká na plánovaný
  downstream upgrade podle srpnového zápisu. Aktuální lokální CNA už ale používá sesterský
  checkout tohoto sharp-runtime; historický důvod blokace je potřeba znovu ověřit.
- **#2381:** CNA XNB `DateTimeReader` má zachovat `DateTimeKind`; jeho starší závislost na
  `develop` tento typ neznala. **Tento blocker je v nynějším lokálním CNA zastaralý:**
  `CNA_SHARP_RUNTIME_ROOT` v několika jeho CMake cache míří na sesterské
  `/rv/data/development/github.com/libcna/sharp-runtime`, zatímco
  `DecimalDateTimeContentTypeReaders.hpp` stále vrací `DateTime(ticks)` a v komentáři tvrdí,
  že `DateTimeKind` neexistuje. CNA na větvi `next` má navíc vlastní necommitnuté změny,
  které tento průchod nijak neměnil. Lidský správce by měl ticket znovu zařadit a opravit jej
  až po ověření zamýšlených sestav; obecná větev sharp-runtime `next` je stále o 58 commitů
  před `develop`, ale to samo už není důkaz, že lokální CNA čeká na merge.
- **#1962:** syrový ICMP fallback pro `Ping` na hostech s uzavřeným `ping_group_range`.
  Dosavadní prostředí nemělo `CAP_NET_RAW` pro jeho ověření. V tomto sandboxu je dnes zakázané
  dokonce vytváření běžných socketů, takže zde opravu nelze pravdivě odtestovat.

Pět `wontfix` nejsou skryté aktuální úlohy: záznamy mají důvody (mimo jiné zrušenou či
nahrazenou práci a vědomě odmítnutou optimalizaci `long double` hashování). Před případným
znovuotevřením číst jednotlivé `notes`, ne jen štítek stavu.

Pozor na dokumentační rozpor: `README.md` nazývá `plan.sqlite3` „git-ignored local database“.
`.gitignore` ji skutečně jmenuje, ale `git ls-files plan.sqlite3` ji vrací. Ignore neodstraní už
sledovaný soubor. Člověk by měl vědomě rozhodnout, zda bude tato databáze dále verzovanou
autoritou, nebo zda se převede na exportovaný textový index a lokální SQLite; tiché přestání
evidence by ztratilo historické zdůvodnění.

## 4. Audit: výsledek a jeho platnost

Původní [AUDIT_SCOPE.md](../audit/AUDIT_SCOPE.md) vymezil snapshot z 2026-07-25: 1 748
způsobilých sledovaných prvostranných textových souborů, ke každému zrcadlová zpráva v `audit/`.
[AUDIT_FINAL_REPORT.md](../audit/AUDIT_FINAL_REPORT.md) uvádí **364 nálezů**: 91 high,
262 medium, 11 low. Je to uzávěrka sběru důkazů, nikoli tvrzení, že tehdy bylo 364 oprav.
Konečná srpnová reconciliace je **343 remediated, 19 accepted-deviation, 2 false-positive,
0 confirmed**. Dnes prošly `reconcile_audit_findings.py` i `validate_audit_findings.py`.

Od auditu přibyly mimo jiné `Resources`, `ServiceModel`, `Xml.Serialization`, velké XML změny a
nová politika sdílení `EventHandler`. Per-file zrcadlení auditu nebylo pro tyto pozdější změny
rozšířeno. Konzervativní kontrola nynějších textových souborů našla přibližně 666 cest bez
historické zrcadlové zprávy; její filtr není totožný s filtrem původního auditu, proto se číslo
nesmí vydávat za 666 potvrzených neauditovaných vad. Znamená jen, že kontrola **indexu 364
historických nálezů** není novým kompletním auditem zářijového kódu.
Přesnější první rozsah pro navazující práci: `git diff --name-only 54578590..HEAD`
na původním snapshotu vrací **184 změněných `.cpp`/`.hpp` cest**, z toho **178 v `modules/`**,
od posledního commitu 2026-08-22. Ani tento seznam sám o sobě není hotový audit;
ticket **#2420** požaduje manifest podle skutečných pravidel původního auditu a výsledek
kontroly pro každou způsobilou cestu.

Přijaté odchylky je nutné respektovat při lidském přepisování: nejde o CLR/GC/reflexi,
veřejné indexy/počty textu počítají bajty UTF-8 místo .NET UTF-16 jednotek, plná kultura a
normalizace nemají ICU databázi, `TimeZoneInfo` nečte struktury TZif pravidel a kryptografie
neobsahuje TLS/symetrické šifrování. Přesné hranice jsou v `AGENTS.md`, veřejných hlavičkách a
auditních rozhodnutích. „Zlepšení parity“ bez měření konzumentů může porušit vědomý kontrakt.

## 5. Aktuální nálezy k zapsání do nové lidské fronty

| ID | Naléhavost | První lidský krok |
|---|---|---|
| H-01 | Blokuje build; veřejné ABI | rozhodnout a dokumentovat změnu layoutu, přestavět konzumenty |
| H-02 | Blokuje hranice komponent | opravit deklarace viditelnosti a znovu ověřit graf |
| H-03 | Blokuje lokální/CI shodu | doplnit `Resources` do CI matice |
| H-04 | Blokuje zdrojový inventář | klasifikovat sedm WCF typů bez falešných tasků |
| H-05 | Aktivní dokumentace klame | aktualizovat datované hodnoty a dvouúlohové příkazy |
| H-06 | Riziko budoucího přepisu | navázat audit a benchmark na skutečný konzumentský profil |
| H-07 | Stará ticketová blokace | znovu změřit CNA a překlasifikovat downstream úlohy |

### H-01 — build a veřejné ABI po `EventHandler::Share` (blokuje každou zelenou bránu)

Poslední commit `88f6b11f` nahradil přímé úložiště události `shared_ptr<State>` v
[EventHandler.hpp](../modules/core/include/System/EventHandler.hpp). Jednoúlohový kompilovaný
probe na tomto HEAD změřil `sizeof(EventHandler<EventArgs>) = 16`; stejný probe s hlavičkou z
`HEAD^` naměřil **64**. `sizeof(System::Timers::Timer)` je nyní **64**, ale
[TimerExceptionBoundaryTests.cpp](../modules/timers/tests/System/Timers/TimerExceptionBoundaryTests.cpp)
na řádcích 258–259 záměrně pinují starých **112** (původní 104 + vptr). Po `CCACHE_DISABLE=1
cmake --build build --parallel 2` build selže přesně na těchto dvou `static_assert`. Úplnější
`ninja -C build -j2 -k0` sestavil ostatní dosažitelné cíle a neukázal další kompilátorovou chybu.

To není pouze „zastaralý test“. Veřejná šablona změnila layout a zde je přímo uloženým členem
`Timer`; další typy s tímto členem jsou v CNA. `XObject`, `ObservableCollection` a
`ReadOnlyObservableCollection` **nejsou** přímí uživatelé této šablony (jejich eventy používají
jiný typ handleru), což upřesnil následný průchod zdrojem. Staré a nové hlavičky/binárky se
nesmějí míchat. Před úpravou pinu člověk musí rozhodnout, zda je tato cena záměrná, změřit
layout dotčených veřejných typů, zapsat migrační poznámku a přestavět konzumenty. Následně
znovu spustit plný build a testy. Stávající pravidla pro změnu veřejného layoutu jsou v
[StandingApprovals.md](StandingApprovals.md); poslední commit obsahuje jen dvě změněné cesty,
nikoli migrační zprávu.

### H-02 — dvě chybné hrany komponent (blokují `validate_module_boundaries.py`)

`python3 scripts/validate_module_boundaries.py` vrátil přesně dvě chyby:

1. [Xml.Serialization/CMakeLists.txt](../modules/xml-serialization/CMakeLists.txt) deklaruje
   veřejně `Collections.Core IO Xml`, ale veřejný
   [XmlSerializer.hpp](../modules/xml-serialization/include/System/Xml/Serialization/XmlSerializer.hpp)
   includuje `System/InvalidOperationException.hpp` z `Core.Base`. Spotřebitel tedy závisí na
   veřejném `Core.Base`, který není deklarován.
2. [ServiceModel/CMakeLists.txt](../modules/service-model/CMakeLists.txt) označuje `Net.Http`
   jako `PUBLIC_DEPENDENCIES`, ačkoli nalezené `Net.Http` includes jsou pouze v
   [SoapChannel.cpp](../modules/service-model/src/System/ServiceModel/SoapChannel.cpp).
   Je to nadbytečný veřejný dosah; závislost patří podle současného grafu do `PRIVATE_DEPENDENCIES`.

Oba problémy jsou doložené i jako zděděné v zářijové zprávě SAMPLE-104. Opravit metadata,
regenerovat katalog a znovu spustit validátor a izolované konzumenty. U `Xml.Serialization`
zvážit i všechny ostatní hlavičky, aby veřejný seznam vyjadřoval celý kontrakt.

### H-03 — lokální a GitHub matice se rozešly

`python3 scripts/validate_selective_component_matrix.py` vrací: workflow matrix is missing
local entries `[('Resources', 'resources.cpp')]`. Lokální
[check_selective_components.sh](../scripts/check_selective_components.sh) má jedenáct kladných
položek včetně `Resources`; [components.yml](../.github/workflows/components.yml) má deset.
CI tedy nekontroluje stejnou izolaci. Doplnit řádek v CI matici a nechat validační test projít.

### H-04 — sedm `ServiceModel` typů mimo plánovací index

`python3 scripts/source_header_inventory.py --csv build-tmp/human_handoff_inventory.csv`
skončil s kódem 1: 1 337 zdrojových souborů, 193 238 řádků, 0 chybějících SPDX, ale sedm
veřejných deklarací bez `task` řádku: `BasicHttpBinding`, `CommunicationException`,
`EndpointAddress`, `FaultException`, `ServiceHost`, `Channels.SoapChannel` a
`Channels.SoapMessageEncoder`. V opačném směru není žádný `ported` řádek bez hlavičky.
Pravděpodobná souvislost je nový SOAP/WCF modul po poslední aktualizaci SQLite; nepřidávat
automaticky falešné řádky z jiného .NET korpusu. Rozhodnout, zda jsou tyto typy v rozsahu
`task` indexu, a buď je klasifikovat ve správném zdroji, nebo odůvodnit přesné výjimky ve
`source_header_inventory_exemptions.json`.

### H-05 — zavádějící aktivní návody a zastaralé měřené hodnoty

`README.md` na začátku a v části CI uvádí 41 komponent/96 hran a 17 840 testů/38 souborů;
`docs/CMakeComponents.md` uvádí 41/91. Generovaný katalog nyní říká 44/108, zářijová
zpráva SAMPLE-104 18 120/41 před H-01. `README.md` označuje `NEXT.md` za stručný handoff,
ale má 1,51 MB. Návod v `docs/CMakeComponents.md` stále nabízí `--parallel 3` na řádcích
51, 112 a 125; `README.md` má `--parallel 4` u i686 ukázky na řádku 243. Obě hodnoty odporují
trvalému stropu **dvou** souběžných build úloh v `AGENTS.md`. Tyto stránky je potřeba
aktualizovat tak, aby jasně označily historický baseline a současný neověřený HEAD; nezvyšovat
číslo testů pouhým odhadem.

### H-06 — důkazní mezera po srpnovém auditu a chybějící měření výkonu

Zářijové moduly a zářijová změna veřejného `EventHandler` mají nové důkazy v testech a
některých návrhových souborech, ale neprošly stejně širokým auditním průchodem jako červencový
snapshot. Projekt má jeden samostatný benchmark, `bench/StringBenchmark.cpp`, a žádný současný
samostatný collection benchmark v `bench/`. Je to **riziko pro budoucí optimalizaci**, ne
potvrzená výkonnostní chyba. Zejména nová
[Dictionary.hpp](../modules/collections/include/System/Collections/Generic/Dictionary.hpp)
publikuje `MapType` a mutable `ToMap`, současně drží sloty a index pro pořadí enumerace;
SAMPLE-104 změřil růst `Dictionary<int,int>` 64 → 176 bajtů. Před přepisem zavést měření
skutečných operací a paměti pro reprezentativní CNA data, vedle sémantických testů.

### H-07 — blokované downstream tickety mají zastaralý předpoklad o checkoutu

V #1773 a #2381 je jako důvod čekání uveden ostrý rozdíl mezi `next` a `develop` v závislosti
CNA. Dnes `cna/CMakeLists.txt` ve výchozím stavu bere `../sharp-runtime` a jeho existující
`cmake-build-debug`, `build-probe` a další cache mají `CNA_SHARP_RUNTIME_ROOT` opravdu nastavený
na místní sesterský checkout. Pro #2381 navíc platí přímý rozpor: CNA dosud zahazuje horní dva
bity XNB `DateTime`, ačkoli tento sharp-runtime již má konstruktor s `DateTimeKind`.
`cna` nelze bez dalšího prohlásit za zelené, protože zde nebyl buildnut a pracovní strom má
vlastní změny. Nejde o povolení automaticky editovat jiný repozitář; je to důvod nenechat
ticket ve stavu `blocked` bez nového lidského měření. #1773 vyžaduje samostatný průchod call
sites a mobile-eggbert, protože jednoduché textové hledání staré `CopyTo(void*, …)` cesty
nic nenašlo, ale tím se migrace sama nedokazuje.

### Oddělené omezení tohoto běhu, ne nález v kódu

- Konfigurovaný `build/` používá `ccache` s cache v `/rv/cnaccache`, která je pro tuto relaci
  jen pro čtení. První build skončil na `ccache: ... Read-only file system`; opakování s
  `CCACHE_DISABLE=1` pokračovalo ke skutečnému H-01. Je nutné opravit prostředí nebo při
  ověřování výslovně vypnout cache, nikoli měnit kód knihovny.
- Sandbox odmítá i `socket(AF_INET, SOCK_STREAM, 0)` s `EPERM`; stejně odmítá ICMP datagram a
  raw socket. `run_component_tests.sh build` prošel dřívějšími suite včetně `Collections.Core`
  **2 792/2 792** a `Core.Base` **6 238/6 238**, pak skončil na `Net.Http`
  **177 passed, 24 failed**, přičemž jeden samostatně zopakovaný případ hlásil
  `Socket::Socket: socket() failed`. Samostatné zbývající testovací soubory bez socketů,
  včetně `Resources` 16/16, `Xml` 563/563, `Xml.Linq` 349/349,
  `Xml.Serialization` 58/58 a `SharpRuntimeIntegrationTests` 957/957, prošly.
  Socketové suite selhávají na stejném omezení. Starý binární `SharpRuntimeTests_Timers`
  se nemá počítat: H-01 zabránilo jeho novému sestavení.
- Historická Doxygen 1.9.8 brána dovoluje nejvýše **2 675 varování**. Není to nula;
  současný počet jsem neměřil, protože celý build už má nezávislou blokující chybu a generování
  dokumentace zapisuje velký strom.

## 6. Převzetí lidským C++ vývojem

### První stabilizační série

1. Zafixovat jeden lidsky ověřený HEAD a prostředí. Na stroji, kde fungují místní sockety a
   zapisovatelná cache, zaznamenat GCC/Clang/CMake, zlib, tzdata, architekturu a verze CNA a
   mobile-eggbert. Zachovat limit dvou build úloh a opakovaně používat `build/`.
2. Rozhodnout H-01 jako veřejnou layout změnu, doplnit přesné layout a migrační důkazy a
   přestavět konzumenty. Pak opravit H-02 až H-04 a dosáhnout zelené úplné lokální/CI brány.
   Až po tom stanovit nový datovaný počet testů a komponent. Během této série se vyhnout
   nahodilým sémantickým přepisům.
3. Zkrátit **aktivní** orientační dokumenty na jednu stránku stavu, jednu stránku architektury,
   reprodukovatelný build a živou frontu. Historické záznamy a audit ponechat dohledatelné.
   Opravit H-05 a rozlišovat „naposledy zelené“ od „současný HEAD“.

### Trvalá práce po malých svislých řezech

Každou změnu má napsat, zrevidovat a commitnout člověk. AI může číst kód, vysvětlit algoritmus,
vyhledat odpovídající .NET větev, navrhnout testovací matici, zpochybnit tvrzení a interpretovat
profil/ASan/UBSan výstup. AI nemá být autorem patchů ani tichým správcem ticketů. Toto je návrh
budoucího pracovního pravidla, ne dodatečná změna současných `AGENTS.md` instrukcí.

Pro jeden typ držet jeden krátký záznam: **konkrétní problém a volající konzument → původní
chování a .NET reference → zachované C++ odchylky → návrh → sémantický test → měření výkonu →
ABI/source dopad → build a relevantní sanitizer → CNA/mobile-eggbert dopad → rozhodnutí**.
Nejdříve pozorovat současné chování, potom měnit implementaci. Schválení změn veřejného
rozhraní řídit podle [StandingApprovals.md](StandingApprovals.md), které obsahuje skutečné
dřívější volby uživatele. Negativní compile fixtures a modulový validátor chrání vlastnosti,
které obyčejný gtest nemůže zachytit.

### Pilot: kolekce, které chcete časem přepsat či optimalizovat

Vhodný první pilot je **jedna** konkrétní operace kolekce z reálného CNA profilu, nikoli
hromadný přepis celého namespace. Čtecí hledání v místním CNA našlo jen **jednu** explicitní
instanci `System::Collections::Generic::Dictionary<…>` v `PhoneApplicationService`, žádnou
explicitní instanci runtime `List<…>` a několik použití `OrderedDictionary<…>` v content
pipeline a XNB. Část C# `List<T>` tam záměrně mapuje na `std::vector<T>`; mobile-eggbert nemá
vlastní přímé zápisy těchto tří plně kvalifikovaných typů. To je **statický inventář spellingů**,
nikoli profil CPU ani důkaz absence nepřímých použití. Pokud chcete začít tam, kde jsou
spotřebitelé, `OrderedDictionary` nebo content pipeline mají nyní silnější doloženou cestu
než obecná optimalizace `Dictionary`; konečný výběr má určit měření.

`Dictionary` právě získal
slotové pořadí odpovídající .NET Framework a jeho mutable `ToMap()` je veřejný únik do STL;
perf změna nesmí odstranit enumeraci, opakované využití uvolněných slotů, fail-fast chování,
float/NaN hash kontrakt ani C++ interop. `List<T>` má sledovaný indexer proxy a výslovně
nesledované mutable iterátory; sjednotit je mechanicky by změnilo zdrojovou kompatibilitu.
Začít malým benchmarkem s normálními klíči, float/NaN, mutacemi, enumerací a rozměry dat,
které skutečně používá CNA. Vždy měřit před/po na stejném compileru a optimalizačním profilu;
mikrobenchmark nesmí nahradit test chování.

Potom postupovat od čitelnosti a vlastnictví paměti k výkonu. U šablon a veřejných členů
hlídat layout, `sizeof`, přetížení, iterator invalidation a ODR. Po každém řezu přestavět
dotčené konzumenty a spustit nejprve úzké testy, potom úplnou bránu. Při odstranění přijaté
odchylky nejdříve rozhodnout, zda stojí za nový kontrakt; neoznačovat ji zpětně za chybu AI.

### Co předat do lidského vlastnictví jako první

1. **Build a komponenty:** CMake graf, validator, CI matice, pravidla pro dvě úlohy.
2. **Veřejné typy a ABI:** eventy/delegáti, kolekce, `DateTime` a Unicode jednotky.
3. **Reálné konzumenty:** CNA, mobile-eggbert a vzorové hry; je třeba zjistit, které části
   podmnožiny jsou skutečně používané.
4. **Nezávislý oracle:** lokální .NET zdroj je historicky preview .NET 11, zatímco některé hry
   cílí XNA/.NET Framework 4. U sporného chování vybrat správný cílový kontrakt a zaznamenat
   jej v testu, ne předpokládat jednu univerzální „.NET parity“.

## 7. Reprodukce a meze tohoto ověření

Použité čtecí kontroly: `sqlite3 plan.sqlite3`, `git ls-files`,
`python3 scripts/db_consistency_check.py`, `python3 scripts/reconcile_audit_findings.py`,
`python3 scripts/validate_audit_findings.py`, `python3 scripts/generate_component_catalog.py
--check`, `python3 scripts/validate_module_boundaries.py`,
`python3 scripts/validate_selective_component_matrix.py` a
`python3 scripts/source_header_inventory.py`. První čtyři databázové/auditní/katalogové
kontroly prošly; poslední tři hlásí H-02 až H-04. Dva unit test skripty validátorů prošly
(17/17 a 4/4). Unicode tabulky odpovídají lokálnímu UCD 16.0 snapshotu.

Použité build adresáře: existující `build/`, `build-probe/` pro dva krátké smazané layout
proby a `build-tmp/` pro logy; nejvýše **2** souběžné kompilace. Místní `ccache` vyžadoval
`CCACHE_DISABLE=1`. Nebyl spuštěn nový plný Clang, Doxygen, sanitizer ani desetiminutový
selektivní build, protože H-01/H-02/H-03/H-04 už plnou bránu nezávisle blokují. Změny ve
zdrojovém kódu ani v databázi při původní inventuře nebyly provedeny.

## 8. Stav oprav v odděleném worktree

Tato sekce popisuje navazující práci na `codex/human-handoff-20260930` založené z `next`.
Původní checkout `feature/gamer-services-collections` zůstává nedotčený; výše uvedená
měření H-01 až H-07 jsou historický snímek před opravami.

| Nález | Stav v handoff větvi |
|---|---|
| H-01 | Rozložení `EventHandler` a `Timer` je znovu testováno; [migrační zpráva](Migration-EventHandlerSharedStateLayout.md) dokumentuje veřejný ABI zlom. Mobile-eggbert/CNA se proti novému worktree celé přestavěly a slinkovaly. |
| H-02 | Opraveny deklarace veřejné a privátní závislosti `Xml.Serialization` a `ServiceModel`; regenerován katalog 44/109. Validátor hran prošel. |
| H-03 | `Resources` doplněno do GitHub matice; validátor shody 11/11 prošel. |
| H-04 | Sedm WCF deklarací má přesné, zdůvodněné výjimky v soupisu; inventář zdrojů prošel bez chyb. |
| H-05 | Aktivní návody, počet komponent, povolené build adresáře a dvouúlohové příkazy opraveny; `NEXT.md` a `plan.md` ukazují na tento datovaný přehled. |
| H-06 | Přidán volitelný mikrobenchmark skutečně používaného generického `OrderedDictionary<string,string>`. Úplný audit změn po srpnu zůstává výslovně otevřený jako ticket **#2420**; historické „0 otevřených nálezů“ se na něj nevztahuje. |
| H-07 | Poznámky #1773 a #2381 v `plan.sqlite3` aktualizovány podle současného lokálního CNA. Stav `blocked` zůstává pro práci a ověření v downstream repozitářích; jejich zdroje tento worktree nemění. |
| H-08, nalezeno při opravě | Holý GitHub full job nemá `SHARP_RUNTIME_SOAP_ENDPOINT`, takže dvě živé zkoušky vždy přeskočí a přísný runner správně selže. Lokální helper nyní umí spustit **celou** bránu s nezměněným původním serverem; provisionování právně použitelného fixture pro GitHub zůstává ticket **#2421**. Testy se nefiltrují a pravidlo nulového počtu přeskočení se nemění. |

Reprodukovatelný výchozí benchmark (Release, bez výkonového prahu v testech):

```bash
cmake -S . -B build-probe -G Ninja \
  -DSHARP_RUNTIME_COMPONENTS=All \
  -DSHARP_RUNTIME_BUILD_TESTS=OFF \
  -DSHARP_RUNTIME_BUILD_BENCHMARKS=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-probe --target SharpRuntimeCollectionsBench --parallel 2
./build-probe/SharpRuntimeCollectionsBench
```

Výstup je CSV s nanosekundami na operaci pro 16, 256 a 4096 položek. Nejde o
porovnání se STL typem s jinou sémantikou; opakovat na stejném stroji, compileru a
datech CNA před a po konkrétní lidské změně.

Výchozí měření z pěti po sobě jdoucích běhů: GCC 14.2.0, CMake 3.31.6,
`Release`, Ryzen 7 PRO 7840U, `sizeof(OrderedDictionary<string,string>) = 88`.
Tabulka ukazuje medián na jednu operaci; samotná surová CSV jsou v lokálním
`build-tmp/handoff-benchmark-{1..5}.csv` a nejsou součástí zdrojového commitu.

| Operace | 16 položek | 256 položek | 4 096 položek |
|---|---:|---:|---:|
| `Add`, včetně podílu založení kontejneru | 79 ns | 127 ns | 125 ns |
| `ContainsKey`, zásah | 17 ns | 14 ns | 16 ns |
| `ContainsKey`, chybějící předem připravený klíč | 9 ns | 9 ns | 12 ns |
| `operator[]`, zásah | 18 ns | 12 ns | 16 ns |
| Průchod v pořadí, součet délek klíče a hodnoty | 0,7 ns | 0,6 ns | 0,7 ns |
| `Remove` vždy od začátku, předem postavený kontejner | 0,37 µs | 9,4 µs | 230 µs |

Rozptyl při 4 096 položkách byl pro `Remove` **163–326 µs**; nejsou to stabilní
výkonnostní garance. Procházení je velmi malé tělo nad souvislou pamětí a měří
spíše spodní mez této konkrétní iterace. Růst mazání odpovídá současnému
`rebuildIndex()` po každém odebraném klíči, ale bez profilu skutečných volání
v CNA to není automatická priorita optimalizace.

`plan.sqlite3` má po tomto zápisu 2 419 ticketů: 2 409 `done`, 3 `blocked`,
5 `wontfix` a 2 `todo` (#2420, #2421). Databázová konzistence prošla.

Čerstvý GCC build i všech **18 123 testů v 41 executables** prošly bez chyb, varování
a přeskočení. Testovací běh použil nezměněný Yacht SOAP server na dočasném soukromém portu;
prosté `local_ci_check.sh` bez této služby končí dvěma přeskočenými živými testy. Clang 19.1.7
prošel 230 produkčními překladovými jednotkami s `-Werror` a nulou varování.
Doxygen 1.9.8 skončil na **2 674 varováních**, tedy pod nezvýšeným limitem 2 675.
Jednotný příkaz pro celou lokální bránu s tímto serverem je:

```bash
python3 scripts/run_component_tests_with_soap_fixture.py \
  /rv/tmp/samples/SAMPLE-071-Yacht_4_0/xna4-build/bin build --local-ci
```

Úplný opakovaný průchod **prošel**: všechny validátory, 55 negativních fixture a
284 odmítaných míst, GCC build bez varování, 18 123/18 123 testů, Doxygen pod
limitem a **11/11 izolovaných komponent** včetně jejich testů a negativních
include fixture. Konzumentský target `WindowsPhoneSpeedyBlupi` se přestavěl
a slinkoval přes CNA v **702 krocích** proti tomuto sharp-runtime worktree.
Následná kontrola na čistém CNA HEAD `39231e4ed` nevyžadovala další kompilaci;
mobile-eggbert byl na `cb9ca51`. V konzumentském logu bylo 20 varování jen z
vendored Draco a systémového STL při jeho překladu, žádná chyba; zdroje CNA,
mobile-eggbert i původního sharp-runtime checkoutu zůstaly bez změn způsobených
tímto během.

Použité build adresáře v odděleném worktree: `build/` pro úplnou bránu,
`build-probe/` pro Release benchmark a dočasnou kopii SOAP fixture,
`build-consumer/` pro mobile-eggbert/CNA a `build-tmp/` pro lokální logy i
dočasné selektivní stromy. **Maximum byly dvě současné kompilace** ve všech
CMake voláních. Zvláštní zacházení vyžadovaly dvě živé SOAP zkoušky: helper
`run_component_tests_with_soap_fixture.py` byl spuštěn s argumentem
`--local-ci`, nastavil `SHARP_RUNTIME_SOAP_ENDPOINT` a po testech službu
vypnul. Source inventory zapisoval CSV do `build-tmp/`. Žádný build strom
nevznikl pod `/tmp` ani v cizích zdrojových repozitářích.
