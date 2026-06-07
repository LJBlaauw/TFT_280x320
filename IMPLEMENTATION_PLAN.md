# IMPLEMENTATIE STAPPENPLAN — Power Supply Characterization System

> **Doel van dit document:** een lineaire reeks van kleine, onafhankelijk testbare stappen
> die samen het systeem uit [PROJECT_SPEC.md](PROJECT_SPEC.md) /
> [PROJECT_SPEC_DETAILED.md](PROJECT_SPEC_DETAILED.md) opbouwen.
>
> **Werkwijze:**
> 1. Werk de stappen **in volgorde** af — elke stap bouwt op de vorige.
> 2. Een stap is pas "klaar" als het **acceptatiecriterium** aantoonbaar is gehaald
>    (build succesvol, meting op scoop/multimeter, output op UART, etc.).
> 3. Vink de stap af in de statusregel, vul datum + korte bevinding in, en ga pas dán door.
>    Zo blijft dit document de levende voortgangsregistratie van het project.
>
> **Statuscodes:** `⬜ Te doen` · `🔄 Bezig` · `✅ Afgerond` · `⛔ Geblokkeerd`

---

## Statusoverzicht (bijwerken na elke afgeronde stap)

| Fase | Omschrijving | Status |
|---|---|---|
| 0 | Fundament: project-skelet & build | ⬜ Te doen |
| 1 | AD9102 DDS driver | ⬜ Te doen |
| 2 | ADC sampling (DMA, multi-kanaal, MUX) | ⬜ Te doen |
| 3 | Signaalverwerking (Goertzel, golfvorm-tabellen) | ⬜ Te doen |
| 4 | Bode Plot functionaliteit + UI | ⬜ Te doen |
| 4B-A | Ruismeting (ADC0 modus A) — vóór Fase 5, geen stroomstap nodig | ⬜ Te doen |
| 5 (deel 1) | Elektronische belasting: veiligheidsbaseline + eerste belastingstest (Stap 5.1-5.2) | ⬜ Te doen |
| 4B-B | Staprespons (ADC0 modus B) — bewust ná Stap 5.1-5.2 (genereert een echte stroomstap via dezelfde belastingsketen) | ⬜ Te doen |
| 5 (deel 2) | Elektronische belasting + scope-UI (Stap 5.3-5.8: statische/dynamische belasting, scope, MUX-arbitrage, impedantie, parameter-UI) | ⬜ Te doen |
| 6 | Thermische beveiliging + ventilator | ⬜ Te doen |
| 7 | Inter-core communicatie + Core 0 MicroPython | ⬜ Te doen |
| 8 | Data-export (CSV via UART/USB → SD-kaart) | ⬜ Te doen |
| 9 | Calibratie & systeemintegratie | ⬜ Te doen |
| 10 | Eindvalidatie compleet systeem | ⬜ Te doen |

---

## FASE 0 — Fundament: project-skelet & build

**Doel van de fase:** een hoofdproject opzetten dat los van de testbench buildt en flasht,
zodat alle volgende fases hierop kunnen voortbouwen zonder de werkende testbench te breken.

### Stap 0.1 — Root build-structuur opzetten
- **Wat te bouwen:** `pico_sdk_import.cmake` naar root kopiëren, root-`CMakeLists.txt` herschrijven
  zodat die zelfstandig buildbaar is (nu `[placeholder]`, zie PROJECT_SPEC.md regel 246/166).
  Maak een minimale `main.c` die alleen `stdio_init_all()` doet en elke seconde "alive" print.
  **Let op:** de huidige root-`CMakeLists.txt` is nog een kopie van de testbench-configuratie
  en bevat `pico_set_binary_type(testbench no_flash)` ([CMakeLists.txt:58](CMakeLists.txt#L58))
  — dat bouwt een RAM-only image die niet vanaf flash opstart. Het hoofdproject moet expliciet
  een **normale flash-binary** worden (`pico_set_binary_type(<target> default)`/`copy_to_ram`
  wordt weggelaten of bewust op `flash` gezet), tenzij later bewust voor een RAM-run wordt
  gekozen — documenteer in dat geval waarom.
- **Hoe te testen:** `cmake -G Ninja -DPICO_BOARD=pico2_w .. && ninja` vanuit `build/` in de root;
  flash `.uf2`; lees UART (`picocom -b 115200 /dev/ttyACM0`) en zie de "alive"-regel verschijnen;
  **power-cycle het bord** (zonder herflashen) en controleer dat de "alive"-regel na het
  opstarten opnieuw verschijnt — dat bewijst dat de binary vanaf flash opstart en niet alleen
  in RAM leefde tijdens de actieve debug-/flash-sessie.
- **Acceptatiecriterium:** build slaagt zonder errors/warnings, UART toont periodieke output op
  echte hardware, én de binary start na een normale power-cycle (geen herflash) zelfstandig op
  vanaf flash — het binary-type is expliciet gekozen en gemotiveerd, niet overgenomen uit de
  testbench-kopie.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 0.2 — Display + touch verplaatsen naar gedeelde drivers
- **Wat te bouwen:** `ili9341_display.h/c`, `xpt2046_touch.h/c`, `spi_interface.h/c` overnemen
  (of als library linken) in het hoofdproject volgens de **eind-architectuur pinout**
  (gedeelde SPI0, Touch CS = GP9 — zie [docs/PIN_ASSIGNMENT.md](docs/PIN_ASSIGNMENT.md)).
  **Let op — dit is een echte bus-migratie, geen kopieerklus:** de werkende testbench
  draait display op SPI0 en touch op een *aparte* SPI1-bus (zie
  [README_TESTBENCH.md:19](TFT_display_touch_driver/README_TESTBENCH.md#L19) en
  [README_TESTBENCH.md:29](TFT_display_touch_driver/README_TESTBENCH.md#L29)), terwijl het
  eindproject display, touch én de AD9102 op **dezelfde SPI0-bus** combineert. Dat introduceert
  busdeling-risico's (verschillende klokfrequenties/SPI-modi per device, MISO-tristate,
  CS-glitches) die in de testbench nooit zijn opgetreden — behandel dit als een **aparte
  bus-integratietest**, niet als een pin-hernummering.
- **Hoe te testen:** herhaal de testbench-vulscherm-/touch-test (zie `README_TESTBENCH.md`)
  vanuit het hoofdproject met de gedeelde SPI0-bus, en voeg minimaal de volgende
  busintegratie-checks toe: (1) wissel de SPI-snelheid/-modus per transactie tussen
  display, touch en (zodra aanwezig) AD9102, en controleer dat elk device correct reageert;
  (2) meet met een scoop dat alle CS-lijnen hoog (inactief) staan wanneer de bus idle is —
  geen device mag per ongeluk geselecteerd blijven; (3) controleer dat MISO van niet-actieve
  devices daadwerkelijk in tristate gaat (geen busconflict/contentie); (4) wissel
  herhaaldelijk (honderden keren) tussen touch- en displaytransacties in een lus en
  controleer op corrupte/verschoven beeldpixels of foutieve touch-coördinaten als teken van
  busvervuiling.
- **Acceptatiecriterium:** display + touch werken identiek aan de testbench, maar nu met de
  eind-architectuur pin-indeling op de gedeelde SPI0-bus; daarnaast zijn de vier
  busintegratie-checks hierboven (snelheid/modus-wissel, CS-idle-hoog, MISO-tristate,
  herhaald wisselen zonder busvervuiling) uitgevoerd en gedocumenteerd vóórdat de AD9102 in
  Fase 1 op dezelfde bus wordt aangesloten.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 0.3 — Core 1 entry-point skelet
- **Wat te bouwen:** `core1_entry.h/c` met een lege taakstructuur (`while(1) { tight_loop_contents(); }`)
  en een aanroep vanaf Core 0 via `multicore_launch_core1()`.
- **Hoe te testen:** Core 0 stuurt een testwaarde via FIFO naar Core 1, Core 1 print die terug
  via UART (toont dat beide cores draaien en communiceren).
- **Acceptatiecriterium:** UART toont een bericht dat round-trip via Core 1 is gegaan.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 0.4 — AD9102: pinnen + reference-clock-bron definitief vastleggen (clock/reset/trigger/FSYNC/output-enable)
- **Wat te bouwen:** twee aparte beslissingen, niet één:
  1. **Reference-clock-bron (hardwarekeuze, geen gewone GPIO-toewijzing):** de AD9102 heeft
     een stabiele MCLK/referentieklok nodig met expliciete frequentie- en jitter-eisen
     (zie AD9102-datasheet voor toegestane bereik en jitter-grenzen bij de gewenste
     DDS-resolutie/sample-rate). Kies en motiveer één van: (a) een externe
     oscillator/kristal-module op een vaste GPIO/clock-pin, (b) een door de RP2350
     gegenereerde clock-output (bv. `clk_gpout`/PWM/PIO als clock-bron) — en controleer of
     de daarmee haalbare frequentie/jitter binnen de AD9102-specificatie valt, of (c) een
     gedeelde systeemklok indien het ontwerp dat toelaat. Documenteer de keuze inclusief
     berekende/verwachte jitter en de marge t.o.v. de AD9102-eis.
  2. **Overige pinnen (gewone GPIO-toewijzing):** beslis en leg vast welke GPIO's de
     AD9102 RESET, TRIGGER, FSYNC en (indien van toepassing) output-enable aansturen —
     deze staan nu nog als TBD in [docs/PIN_ASSIGNMENT.md](docs/PIN_ASSIGNMENT.md) regel 56.
  Werk PIN_ASSIGNMENT.md bij met de definitieve toewijzing inclusief de gemotiveerde
  clock-bronkeuze (waarom deze bron/GPIO's, welke alternatieven zijn afgevallen en waarom).
- **Hoe te testen:** (1) meet met een scoop/frequentieteller dat de reference-clock op de
  AD9102-pin de gespecificeerde frequentie haalt binnen de jitter-grenzen uit de datasheet;
  (2) controleer met een doorgangstest/multimeter dat elke overige toegewezen pin
  daadwerkelijk verbonden is met het juiste AD9102-pad, en dat er geen pinconflicten zijn met
  display/touch/SPI/ADC (zie volledige pinout in PIN_ASSIGNMENT.md).
- **Acceptatiecriterium:** (1) de reference-clock-bron is expliciet gekozen, gemotiveerd en
  gemeten binnen de AD9102-frequentie-/jitter-specificatie; (2) alle overige AD9102-pinnen
  die nodig zijn voor Fase 1 (reset, trigger, FSYNC, output-enable) staan vastgelegd in
  PIN_ASSIGNMENT.md, zijn fysiek bedraad en conflicteren niet met andere subsystemen.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 0.5 — Minimale safety-baseline (vóór elke vermogen-/belastingscode)
- **Wat te bouwen:** de absolute ondergrens aan veiligheid die *vóór* alle volgende fases moet
  bestaan: (1) DAC-/AD9102-uitgang gegarandeerd laag/uit direct na boot en bij elke reset
  (veilige default, geen ongedefinieerd opstartgedrag), (2) een software-noodstopcommando
  dat de uitgang ongeacht de huidige modus onmiddellijk naar de veilige toestand zet,
  (3) een watchdog/failsafe die het systeem in de veilige toestand brengt bij een hang/crash.
- **Hoe te testen:** meet de uitgang direct na power-up/reset (moet laag/uit zijn vóór enige
  configuratie); roep het noodstopcommando aan tijdens een actieve uitgang en meet de
  reactietijd; forceer een hang (bv. oneindige lus) en controleer dat de watchdog de uitgang
  alsnog naar de veilige toestand brengt.
- **Acceptatiecriterium:** de uitgang is in elke denkbare opstart-/faalsituatie (boot, reset,
  crash, hang, expliciete noodstop) aantoonbaar in de veilige (lage/uitgeschakelde) toestand —
  dit moet werken vóórdat er één golfvorm of belastingstest wordt uitgevoerd.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 0.6 — Architectuur-spike + go/no-go: MicroPython (Core 0) náást C (Core 1)
> **Dit is een go/no-go-beslismoment, geen gewone bouwstap.** De uitkomst bepaalt welke
> architectuur de rest van het plan (met name Fase 4/5-UI en Fase 7) volgt.
>
> **Expliciete taakverdeling (geldt voor zowel GO als de meeste NO-GO-alternatieven), om de
> ambiguïteit "wie bouwt de UI" weg te nemen:**
> - **Core 1 (C, real-time):** bouwt **render- en touch-primitives**, niet de definitieve
>   schermflow — d.w.z. plot-/scope-rendering, widgets (numerieke invoer met
>   eenheid/decimaal-punt, mode-keuzeschermen, touch-cursor), en de meet-/regellogica
>   (Goertzel, Bode-sweep, belasting, bewaking). Deze primitives zijn in Fase 4/5
>   *bruikbaar en zelfstandig testbaar*, maar zijn de **bouwstenen** van de UI, niet het
>   eindproduct: ze moeten op commando van buitenaf (FIFO/gedeeld geheugen) aanstuurbaar zijn.
> - **Core 0 (MicroPython):** orkestreert de uiteindelijke schermflow/navigatie/menu-logica
>   en roept de Core 1-primitives aan — dit is de "UI/logica"-laag uit PROJECT_SPEC.md.
>   Fase 7 verbindt de twee lagen definitief; Fase 4/5 lopen dus vooruit met *testbare
>   bouwstenen*, niet met een UI die later wordt weggegooid.
>
> - **GO** — dual-core MicroPython+C werkt stabiel: ga verder zoals hierboven beschreven
>   (Fase 4/5 bouwen de Core 1 render-/touch-/meet-primitives en valideren ze met een
>   tijdelijke aanroep-harness; Fase 7 vervangt die harness door de definitieve
>   MicroPython-orkestratie op Core 0).
> - **NO-GO** — instabiel/niet haalbaar binnen deze toolchain: kies expliciet en documenteer
>   één van de alternatieven vóórdat Fase 4 begint, bijvoorbeeld (a) **C-only UI** op beide
>   "lagen" (MicroPython laten vervallen, Fase 7 herontwerpen rond pure C inter-core-logica —
>   in dat geval wordt de Core 1-laag uit Fase 4/5 alsnog de definitieve UI, en vervalt de
>   "tijdelijke harness"-aanname), (b) **andere MicroPython-integratie** (bv. MicroPython op
>   slechts één core met C-drivers als native modules, of een ander RTOS/threading-model), of
>   (c) **Fase 7 herontwerp** met een andere taakverdeling tussen de cores. Leg de gekozen
>   richting — én de gevolgen daarvan voor de Core 1/Core 0-taakverdeling hierboven — hier
>   vast vóórdat er verder UI-werk wordt gestart, dat voorkomt dat Fase 4/5 een UI bouwen die
>   later vanwege een architectuurwissel moet worden weggegooid.
- **Wat te bouwen:** een minimale, wegwerpbare proof-of-concept die aantoont dat de
  uiteindelijke dual-core architectuur (Core 0 = MicroPython UI-orkestratie/logica, Core 1 = C
  real-time render-/touch-/meet-primitives incl. display/touch/ADC/AD9102) **in deze build
  daadwerkelijk werkt**, vóórdat Fase 4/5 er primitives bovenop bouwen die later mogelijk
  weggegooid moeten worden: start MicroPython op Core 0 en de Stap 0.3 C-skelet-taak op
  Core 1, wissel een paar testberichten uit via FIFO en/of een gedeelde ringbuffer (zie
  PROJECT_SPEC.md "Bus-eigenaarschap"), en laat beide cores tegelijk iets zichtbaars doen
  (bv. Core 1 tekent op het display terwijl Core 0 een MicroPython-script draait dat die
  tekenactie aanstuurt via FIFO — dit is meteen een eerste rookproef van de
  primitive-aanroep-aanpak hierboven).
- **Hoe te testen:** laat de spike een afgesproken tijd (bv. 10 minuten) ononderbroken draaien
  met beide cores actief en FIFO-verkeer, en controleer op crashes, geheugenproblemen
  (MicroPython-heap vs. C-stack/-heap-conflicten), of vertraging van de real-time C-taken
  door MicroPython-activiteit op de andere core.
- **Acceptatiecriterium:** MicroPython op Core 0 en C op Core 1 draaien tegelijk, stabiel en
  zonder onderlinge interferentie binnen deze toolchain/build; FIFO/gedeeld-geheugen-verkeer
  komt foutloos aan. Bij problemen: documenteer ze hier vóórdat Fase 4/5 beginnen met het
  bouwen van UI — dat voorkomt dubbel werk of een UI die later moet worden weggegooid.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 0.7 — Doc-sync: PROJECT_SPEC.md / PROJECT_SPEC_DETAILED.md bijwerken vóór implementatie
- **Wat te bouwen:** geen code — een documentatie-controle-/synchronisatieronde die
  PROJECT_SPEC.md en PROJECT_SPEC_DETAILED.md aligneert met de ontwerpbeslissingen die in
  dit plan al zijn vastgelegd, zodat er niet tegen twee verschillende waarheden wordt
  gebouwd. Controleer in elk geval:
  1. **Noodstop/SOA-taal:** "per-sample"/"binnen één sample-cyclus"-formuleringen zijn
     vervangen door de drielagen-aanpak uit Stap 6.3 (SOA-precheck per golfvorm /
     firmware-noodstop met gemeten reactietijd / optionele hardware-gate-disable) — zie de
     bijwerkingen in PROJECT_SPEC.md (Thermische Beveiliging, Bewakingslus,
     Real-Time-Requirements-tabel) en PROJECT_SPEC_DETAILED.md (Thermische Beveiliging).
  2. **Ventilatorregeling:** spec en gedetailleerde spec noemen consequent **lineaire
     PWM-regeling** (geen PI-regelaar) — zie PROJECT_SPEC.md "Ventilator PWM-regeling" en
     de Bewakingslus-stap.
  3. **ADC-samplerate/Bode-bereik:** spec en gedetailleerde spec gebruiken de gecorrigeerde
     waarden (500 kSa/s totaal standaard / 250 kSa/s per kanaal, Nyquist 125 kHz; optioneel
     1 MSa/s overklokt → 500 kSa/s per kanaal, Nyquist 250 kHz) consistent met de
     FASE 4-introtekst en Stap 2.5 in dit plan.
  4. **Core 0/Core 1-taakverdeling:** PROJECT_SPEC_DETAILED.md (regel 3-20, 54-57) bevat nog
     oudere workflowtekst die de verdeling minder scherp formuleert dan de verduidelijking in
     Stap 0.6 (Core 1 = render-/touch-/meet-**primitives**, Core 0 = UI-orkestratie/menu-logica
     die deze primitives aanstuurt — geen "Core 1 bouwt de definitieve UI"-framing). Lijn deze
     tekst en de load/protection-workflowbeschrijvingen (regel ~340-363) uit met die
     verduidelijking, zodat lezers van de spec en het plan dezelfde rolverdeling zien.
  5. **Overige nieuwe ontwerpkeuzes uit dit plan** die nog niet in de specs staan
     (bv. AD9102-clockbron-eis uit Stap 0.4, ADC2-MUX-arbitrageregel uit Stap 5.6,
     output-range/clamp-regels uit Stap 1.7, storage/config-aanpak uit Stap 0.8) worden
     teruggeschreven naar de specs zodra ze hier zijn uitgewerkt.
  **Voer deze stap echt vroeg uit** (direct na 0.6, vóór Fase 1 start) — niet als
  opruimklus achteraf: hoe later de specs en het plan uiteenlopen, hoe groter de kans dat
  implementatie tegen een verouderde aanname aanloopt.
- **Hoe te testen:** doorloop PROJECT_SPEC.md en PROJECT_SPEC_DETAILED.md regel voor regel
  naast dit plan en markeer elke tegenstrijdigheid; pas de specs aan of pas — als de spec
  toch leidend blijkt — in plaats daarvan dit plan aan, zodat er na deze stap precies één
  consistente waarheid resteert.
- **Acceptatiecriterium:** geen enkele tegenstrijdigheid meer tussen PROJECT_SPEC.md,
  PROJECT_SPEC_DETAILED.md en IMPLEMENTATION_PLAN.md op de bovenstaande punten; nieuwe
  ontwerpkeuzes die tijdens implementatie ontstaan worden voortaan direct in zowel het plan
  als de relevante spec(s) doorgevoerd (geen "fix het later wel in de specs").
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 0.8 — Storage/config-persistentie (kalibratie, veiligheidslimieten, UI-defaults)
- **Wat te bouwen:** een opslagstrategie + minimale implementatie voor configuratie die
  reboots moet overleven: kalibratiewaarden (bv. shunt-weerstand, ADC-offset/gain-correctie),
  veiligheidslimieten (`p_max_inst_w`, `p_max_avg_w`, `t_warning`, `t_limit`, `fan_min_pwm`,
  enz. — zie de `protection_config_t`-achtige structuren in PROJECT_SPEC.md/DETAILED) en
  UI-standaardwaarden (laatst gebruikte Bode-sweepinstellingen, golfvorm-parameters).
  **Concrete opslagkeuze (geen open vraag):** **flash-config (bv. littlefs of een eigen
  sectorindeling in een gereserveerd flash-gebied) is de baseline en enige vereiste voor
  veiligheidslimieten** — dit pad is onafhankelijk van een nog onbewezen SD-kaart-traject
  (Fase 8 staat later in het plan en levert pas in Stap 8.2/8.3 een werkende SD-kaart op).
  Veiligheidslimieten/kalibratie mogen dus **nooit** afhankelijk worden gemaakt van SD-storage.
  Een SD-gebaseerde configuratie (bv. om instellingen makkelijker tussen apparaten te
  kopiëren) mag later **optioneel** worden toegevoegd zodra Fase 8 een bewezen SD-pad
  oplevert, als aanvulling op — niet vervanging van — de flash-baseline. Gebruik een
  eenvoudig versieformaat zodat toekomstige structuurwijzigingen niet stilzwijgend oude
  configuratie corrumperen.
- **Hoe te testen:** wijzig kalibratie-/limiet-/UI-waarden via de UI of een testscript, doe
  een power-cycle/reset, en controleer dat exact dezelfde waarden worden teruggelezen; test
  ook het gedrag bij ontbrekende/corrupte configuratie (moet terugvallen op veilige
  fabrieksdefaults, nooit op ongedefinieerde/gevaarlijke waarden — zie Stap 0.5).
- **Acceptatiecriterium:** kalibratie, veiligheidslimieten en UI-defaults overleven een
  power-cycle aantoonbaar ongewijzigd; bij ontbrekende of corrupte opslag start het systeem
  altijd op met veilige, gedocumenteerde fabrieksdefaults (nooit met noodstop-/SOA-limieten
  op nul, ongedefinieerd of "laatst bekend maar mogelijk corrupt").
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

---

## FASE 1 — AD9102 DDS driver (`drivers/ad9102_dds.h/c`)

**Doel van de fase:** betrouwbare SPI-aansturing van de AD9102, stap voor stap van
register-toegang naar volledige golfvorm-output. Dit is een hardware-kritieke driver — elke
stap moet op de scoop/multimeter geverifieerd worden voordat je verder gaat.

### Stap 1.1 — SPI-transactie & register read/write
- **Wat te bouwen:** `ad9102_init()`, low-level `ad9102_write_reg()` / `ad9102_read_reg()`
  met handmatige CS (GP11, 10 MHz, zie devicesnelheidstabel in PROJECT_SPEC.md).
- **Hoe te testen:** schrijf een bekende waarde naar een gewoon schrijfbaar control-register en
  lees die terug; controleer daarnaast de **power-on default-waarden** van een paar standaard
  control-registers tegen de AD9102-datasheet (gebruik geen apart "chip-ID/revisie-register" als
  testcriterium — de datasheet documenteert geen los identificatieregister voor dit doel).
  Test optioneel ook een SRAM read/write (schrijf een patroon, lees het terug).
- **Acceptatiecriterium:** read-back van een beschreven control-register komt overeen met de
  geschreven waarde, de power-on defaults van de geteste control-registers komen overeen met de
  datasheet, én (indien getest) een SRAM read-back is byte-voor-byte gelijk aan het geschreven patroon.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 1.2 — Klok/reference, reset, trigger en FSYNC initialiseren
- **Wat te bouwen:** `ad9102_clock_init()`, `ad9102_reset()`, en de trigger-/FSYNC-aansturing
  op de pinnen die in Stap 0.4 zijn vastgelegd. Breng het device via deze pinnen naar een
  bekende, gedocumenteerde beginstatus voordat er enige golfvorm wordt geconfigureerd.
- **Hoe te testen:** controleer met de scoop/logic analyzer dat de reference-clock aanwezig is
  met de verwachte frequentie/vorm, dat een RESET-pulse het device aantoonbaar terugzet
  (bv. via een register read-back van de power-on default uit Stap 1.1), en dat
  trigger/FSYNC-signalen de verwachte timing en polariteit hebben.
- **Acceptatiecriterium:** clock/reset/trigger/FSYNC gedragen zich exact zoals in de
  AD9102-datasheet gespecificeerd; na reset bevindt het device zich aantoonbaar in de
  gedocumenteerde default-status.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 1.3 — DC-uitgang
- **Wat te bouwen:** `ad9102_set_dc(float v_out)`.
- **Hoe te testen:** stel een paar DC-niveaus in (bv. 0V, 1V, 2V) en meet de uitgang met een multimeter.
- **Acceptatiecriterium:** gemeten spanning komt binnen verwachte tolerantie overeen met setpoint
  (noteer eventuele schaal-/offsetfout — input voor latere calibratie, Fase 9).
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 1.4 — DDS-sinuspad: frequentie-instelling (Bode-injectiesignaal)
- **Wat te bouwen:** `ad9102_set_frequency(float freq_hz)` via het **DDS tuning word (DDS_TW)**
  — dit is een ander signaalpad dan de Signal RAM/pattern-modus van Stap 1.5; gebruik dit pad
  voor het Bode-injectiesignaal (zuivere instelbare sinus, geen voorgeladen golfvormtabel).
- **Hoe te testen:** zet een paar frequenties (bv. 100 Hz, 1 kHz, 10 kHz) en meet de werkelijke
  frequentie op de scoop.
- **Acceptatiecriterium:** gemeten frequentie wijkt < 1% af van setpoint over het geteste bereik,
  en de uitgang is een zuivere sinus zonder de bufferwissel-artefacten van het SRAM/pattern-pad.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 1.5 — SRAM/pattern-pad: golfvorm laden in Signal RAM + loop-modus
- **Wat te bouwen:** `ad9102_load_waveform(samples, n)`, `ad9102_start()/stop()`,
  loop-mode-/address-counter-/pattern-timing-configuratie — dit is het pad voor **arbitraire
  belastingsgolfvormen** (Fase 5), nadrukkelijk gescheiden van het DDS-sinuspad uit Stap 1.4.
  Begin met een vooraf berekende sinustabel (vaste freq, geen dynamische generatie nodig in deze stap).
- **Hoe te testen:** laad de sinustabel, start loop-modus, bekijk uitgang op de oscilloscoop.
- **Acceptatiecriterium:** scoop toont een continue, stabiele golfvorm zonder glitches bij
  bufferwissel/patroon-herhaling.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 1.6 — Amplitude-instelling
- **Wat te bouwen:** `ad9102_set_amplitude(float amplitude_v)`, werkend voor zowel het
  DDS-sinuspad (Stap 1.4) als het SRAM/pattern-pad (Stap 1.5) — noteer expliciet als één van
  beide paden een ander mechanisme/bereik vereist.
- **Hoe te testen:** stel een paar amplitudes in op beide paden en meet piek-piekwaarde op de scoop.
- **Acceptatiecriterium:** gemeten amplitude komt overeen met setpoint (binnen tolerantie; afwijkingen
  noteren voor calibratie) op zowel het DDS- als het SRAM/pattern-pad.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 1.7 — Output-range, offset en clamp-regels (DAC-code ↔ 0 A, geen negatieve laststroom)
- **Wat te bouwen:** een expliciete, getoetste mapping tussen DAC-code/setpoint-spanning en
  fysieke uitgangswaarde, met name voor de electronic-load-keten waar geldt
  `I_load (A) = V_DAC (V)` (zie PROJECT_SPEC_DETAILED.md, Stroomformule). Leg vast:
  1. **Welke DAC-code/offset-instelling overeenkomt met I_load = 0 A** (de "nul"-referentie
     van de keten) en hoe deze wordt geprogrammeerd/gekalibreerd op de AD9102
     (DDS-offsetregister resp. SRAM-pattern-offset, afhankelijk van het gekozen pad).
  2. **Clamp-regels per golfvorm-type** die garanderen dat de resulterende laststroom nooit
     negatief wordt (bv. een sinus moet met een DC-offset ≥ amplitude worden gegenereerd —
     dezelfde unipolaire-aanname als de `duty_factor`-tabel in PROJECT_SPEC_DETAILED.md
     veronderstelt, niet een bipolair signaal rond 0 A): bereken en valideer voor elk
     ondersteund golfvorm-type (sinus, blok, driehoek, zaagtand) dat amplitude + offset
     binnen het toegestane DAC-bereik blijft én I_load[n] ≥ 0 over de hele periode.
  3. **Hoe overschrijdingen worden afgehandeld vóór ze de hardware bereiken** — bv. de
     parameter-validatie weigert/clipt instellingen die een negatieve stroom of een
     DAC-codebereik-overschrijding zouden veroorzaken, vóórdat `ad9102_set_amplitude`/
     `ad9102_load_waveform` wordt aangeroepen (sluit aan bij de SOA-precheck uit Stap 6.3).
- **Hoe te testen:** programmeer de "0 A"-referentie en meet met de scoop/multimeter dat de
  laststroom inderdaad ~0 A is bij setpoint 0; doorloop voor elk golfvorm-type een aantal
  amplitude/offset-combinaties (incl. grenswaarden) en controleer dat (a) de gegenereerde
  golfvorm nooit onder I_load = 0 A zakt, en (b) combinaties die dat zouden veroorzaken al
  bij de parameter-validatie worden geweigerd/geclipt — niet pas zichtbaar als fout gedrag
  op de uitgang.
- **Acceptatiecriterium:** de DAC-code-naar-0 A-mapping is gemeten en gedocumenteerd; voor
  elk ondersteund golfvorm-type is aantoonbaar dat I_load nooit negatief kan worden — hetzij
  doordat de toegestane parameterruimte dit per ontwerp uitsluit, hetzij doordat de
  validatie ongeldige combinaties vóóraf weigert/clipt.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

---

## FASE 2 — ADC sampling (`drivers/adc_sampler.h/c`, `drivers/adc_mux.h/c`)

**Doel van de fase:** van een enkele polling-read naar volledige DMA-gestuurde
multikanaals-acquisitie met dubbele buffer, plus de analoge MUX voor ADC2.

### Stap 2.1 — Eén kanaal, polling
- **Wat te bouwen:** initialiseer ADC0 (GP26), lees raw waarden in een lus, print via UART
  (of teken als getal op het scherm).
- **Hoe te testen:** sluit een bekende DC-spanning (bv. uit een spanningsdeler op 3V3) aan op
  GP26 en vergelijk de berekende spanning met een multimeter-meting.
- **Acceptatiecriterium:** berekende spanning (`ADC_raw / 4095 × Vref`) komt overeen met de
  multimeter-meting binnen ADC-tolerantie.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 2.2 — DMA-gestuurde sampling, één kanaal
- **Wat te bouwen:** configureer DMA voor continue overdracht ADC → buffer met interrupt bij vol.
- **Hoe te testen:** sample een bekend AC-signaal (bv. AD9102-sinus uit Fase 1) en bereken
  V_pp/V_rms uit de buffer; vergelijk met scoop-meting van hetzelfde signaal.
- **Acceptatiecriterium:** berekende V_pp/V_rms komt overeen met scoop-meting binnen tolerantie,
  en CPU blijft vrij tussen DMA-interrupts (geen blocking reads meer).
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 2.3 — Dubbele buffer (double-buffering)
- **Wat te bouwen:** Buffer A/B-omschakeling met round-robin ADC0+ADC1 @ 500 kSa/s totaal
  (zie Klok & Timing in PROJECT_SPEC_DETAILED.md).
- **Hoe te testen:** laat de acquisitie lang genoeg lopen om meerdere bufferwissels te zien;
  log via UART "buffer A vol" / "buffer B vol" en controleer dat er geen samples gemist worden
  (continuïteit tussen buffers, bv. via een telmarker in het signaal).
- **Acceptatiecriterium:** continue acquisitie zonder gaten of dubbele samples bij bufferwissel,
  refill-tijd komt overeen met de berekende ~16 ms/buffer.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 2.4 — Analoge MUX voor ADC2 (Vout / NTC)
- **Wat te bouwen:** `adc_mux_select()`, `adc_mux_read_vout()`, `adc_mux_read_ntc_temp()`,
  inclusief settling-skip van 5–10 samples na omschakeling.
- **Hoe te testen:** schakel om tussen kanaal 0 (Vout) en 1 (NTC); vergelijk Vout-meting met
  multimeter en NTC-temperatuur met een referentie-thermometer (of bekende weerstand i.p.v. NTC).
- **Acceptatiecriterium:** beide MUX-kanalen geven plausibele, stabiele waarden direct na de
  settling-periode; geen "vervuiling" van het ene kanaal in het andere.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 2.5 — High-speed ADC (overklok 1 MSa/s) + bandbreedte-validatie
- **Wat te bouwen:** optionele overklok-modus van de ADC-klok naar 96 MHz → 1 MSa/s totaal
  (= 500 kSa/s per kanaal in round-robin, Δt = 1 µs i.p.v. 2 µs — zie PROJECT_SPEC_DETAILED.md
  "Klok & Timing"); pas de Δt-tijdcorrectie uit PROJECT_SPEC.md regel 115 aan op de gekozen modus.
- **Hoe te testen:** herhaal de dubbele-buffer-test van Stap 2.3 in de overklok-modus en
  vergelijk: (a) ruisniveau/jitter t.o.v. de standaard 500 kSa/s-modus op hetzelfde signaal,
  (b) de daadwerkelijke Nyquist-grens door een sinus net onder en net boven 250 kHz aan te
  bieden en te controleren op aliasing in de gesamplede data, (c) of de analoge front-end
  (opamp/filter-bandbreedte) deze samplerate zinvol ondersteunt of zelf al het beperkende
  element is.
- **Acceptatiecriterium:** overklok-modus levert continue, gapless acquisitie op 1 MSa/s
  totaal (500 kSa/s/kanaal); de gemeten Nyquist-grens (~250 kHz/kanaal) en het
  ruis-/jitterniveau zijn gedocumenteerd, inclusief de conclusie of de analoge front-end
  deze bandbreedte daadwerkelijk bruikbaar maakt voor Bode-metingen — gebruik dit als
  onderbouwing voor de praktische Bode-bereikgrenzen in de FASE 4-intro, Stap 4.2
  (sweep-bereik) en Stap 4.6 (kruisfrequentie-detectie/markering van het experimentele gebied).
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

---

## FASE 3 — Signaalverwerking (`signal_processing/*.h/c`)

**Doel van de fase:** de reken-algoritmes eerst **los van hardware** valideren (host-side
unit tests met bekende invoerdata), dan pas live op de RP2350 toetsen. Dit voorkomt dat
hardware-ruis een algoritmefout maskeert of omgekeerd.

### Stap 3.1 — Goertzel: host-side unit test
- **Wat te bouwen:** `goertzel.h/c` met `goertzel_init/process/magnitude/phase_deg/reset`.
  Schrijf een test die een sinus van bekende amplitude/fase/frequentie genereert (in software,
  geen hardware) en de Goertzel-uitkomst vergelijkt met de bekende waarden.
- **Hoe te testen:** draai de test op de host (of in een minimale RP2350-testbuild met UART-output)
  met meerdere frequenties/fases/SNR-niveaus (incl. ruis toevoegen aan het testsignaal).
- **Acceptatiecriterium:** magnitude- en fase-uitkomst wijken < 1% resp. < 1° af van de bekende
  ingevoerde waarden bij een schoon signaal, en blijven bruikbaar bij realistische ruisniveaus.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 3.2 — Goertzel op echte ADC-data
- **Wat te bouwen:** koppel Fase 2.2/2.3 (ADC-acquisitie) aan Goertzel: laat de AD9102 een
  sinus van bekende frequentie genereren, sample met ADC0, bereken magnitude/fase.
- **Let op — fasereferentie:** ADC0 alléén levert geen *absolute* fase; fase heeft altijd een
  referentie nodig. Kies en documenteer in deze stap expliciet welke referentie je gebruikt,
  bijvoorbeeld:
  - de AD9102 trigger-/FSYNC-puls (Stap 1.2) als bekend fase-nulpunt van de gegenereerde sinus, of
  - een tweede ADC-kanaal (ADC1) dat hetzelfde signaal sampelt, zodat je een *relatief*
    faseverschil meet (dit is feitelijk al een voorproefje van Stap 3.3's round-robin-opzet), of
  - de scoop-trigger op hetzelfde referentiesignaal als waarmee de scoop zelf de fase bepaalt.
  Beperk deze stap eventueel bewust tot **alleen amplitude/magnitude**-validatie en verschuif de
  fase-validatie naar Stap 3.3 (waar de inter-channel-referentie sowieso nodig is) — vermeld
  expliciet welke keuze is gemaakt en waarom.
- **Hoe te testen:** vergelijk Goertzel-magnitude (en, indien een referentie is gekozen, fase)
  met een onafhankelijke scoop-meting van hetzelfde signaal, getriggerd op dezelfde referentie.
- **Acceptatiecriterium:** Goertzel-magnitude (en, indien gevalideerd, fase t.o.v. de gekozen
  referentie) komt overeen met de scoop-referentiemeting binnen een vooraf vastgestelde
  tolerantie (bv. 5%) — afwijkingen documenteren als calibratie-input. De gekozen
  fasereferentie is expliciet vastgelegd in de bevinding van deze stap.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 3.3 — Inter-channel timing-correctie
- **Wat te bouwen:** `φ_corr = φ − 2π·f·Δt` toepassen op de round-robin ADC0/ADC1-fase-offset
  (Δt = 1/samplerate, zie PROJECT_SPEC.md regel 115).
- **Hoe te testen:** voer dezelfde sinus tegelijk in op ADC0 én ADC1 (elektrisch verbonden,
  dus theoretische faseverschil = 0°) en controleer dat de gecorrigeerde faseverschil ≈ 0° is
  over het hele frequentiebereik.
- **Acceptatiecriterium:** een vaste tolerantie van ±1° over het *volledige* bereik tot 250 kHz
  is onrealistisch streng (1° bij 250 kHz komt overeen met ~11 ns timing-nauwkeurigheid).
  Specificeer daarom een **frequentie-afhankelijke tolerantie**, bijvoorbeeld:
  - tot ~100 kHz: gecorrigeerd faseverschil binnen ±1–2°
  - 100–250 kHz: karakteriseer en documenteer de werkelijk haalbare restfout (mag groter zijn);
    gebruik dit als basis voor het praktische Bode-meetbereik (zie Stap 4.2 / FASE 4-intro).
  Een meetpunt is geslaagd als het binnen de voor *die frequentieband* gespecificeerde tolerantie valt.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 3.4 — Goertzel performance-/timingtest op RP2350
- **Wat te bouwen:** meet en optimaliseer de uitvoeringstijd van `goertzel_process()` voor het
  **worst-case samplevenster** (hoogste samplerate × langste meetvenster die in de praktijk
  voorkomen, zie "Meetvenster per freq" in PROJECT_SPEC.md "Real-Time Requirements"); overweeg
  fixed-point i.p.v. float als de RP2350-FPU-prestatie een knelpunt blijkt voor real-time
  verwerking naast DMA-acquisitie, display-rendering en touch-polling.
- **Hoe te testen:** meet de uitvoeringstijd met de RP2350-cyclusteller voor het grootste
  realistische samplevenster (incl. de overklok-modus uit Stap 2.5); vergelijk float- en
  (indien geïmplementeerd) fixed-point-uitvoering qua snelheid én nauwkeurigheid (t.o.v.
  Stap 3.1's referentiewaarden).
- **Acceptatiecriterium:** Goertzel-verwerking van het worst-case venster past ruim binnen het
  beschikbare tijdsbudget tussen opeenvolgende metingen/buffers, zonder gemiste deadlines voor
  acquisitie, display of touch; eventuele fixed-point-implementatie blijft binnen de
  nauwkeurigheidstolerantie van Stap 3.1.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 3.5 — Golfvorm-tabellen (`waveform_gen.h/c`)
- **Wat te bouwen:** generator-functies voor sinus, blokgolf (met duty cycle), driehoek, zaagtand,
  als lookup-tabellen voor de AD9102 Signal RAM.
- **Hoe te testen:** laad elke gegenereerde tabel via Stap 1.5 (SRAM/pattern-pad) in de AD9102 en bekijk de uitgang
  op de scoop; vergelijk vorm/symmetrie/duty-cycle visueel en met scoop-metingen.
- **Acceptatiecriterium:** elke golfvormsoort toont de verwachte vorm op de scoop, blokgolf-duty
  cycle komt overeen met ingestelde waarde binnen meetnauwkeurigheid van de scoop.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

---

## FASE 4 — Bode Plot functionaliteit + UI

**Doel van de fase:** de losse bouwstenen (AD9102, ADC, Goertzel) combineren tot een werkende
meting op één frequentie, dan opschalen naar een volledige sweep met visualisatie.

> **Praktisch Bode-meetbereik (expliciet begrensd):** de AD9102 kan tot 1 MHz genereren
> (PROJECT_SPEC.md regel 108), maar het *direct via de interne ADC meetbare* Bode-bereik
> wordt begrensd door de Nyquist-grens (zie Stap 2.5 / PROJECT_SPEC_DETAILED.md "Klok & Timing")
> én de haalbare fase-nauwkeurigheid (Stap 3.3). Hanteer daarom als werkdefinitie:
> - **Standaard Bode-bereik:** tot ~100 kHz, met de tolerantie uit Stap 3.3 (±1–2°)
> - **Experimenteel/uitgebreid bereik:** 100–250 kHz, expliciet gekarakteriseerd en
>   gedocumenteerd (grotere fasefout toegestaan, maar gerapporteerd i.p.v. genegeerd)
> - **Boven ~250 kHz:** niet haalbaar met deze ADC-opzet; alleen relevant als AD9102-
>   generatiebereik (signaalgeneratie/belastingsgolfvormen), niet als Bode-meting
>
> Stap 4.6 (kruisfrequentie-detectie) en de UI (Stap 4.4/4.5/4.8) moeten dit onderscheid
> zichtbaar maken — bv. door het experimentele gebied visueel te markeren in de plot
> (Stap 4.5/PROJECT_SPEC_DETAILED.md "Bode Plot Screen") en/of door de sweep-instellingen
> te begrenzen/waarschuwen boven 100 kHz.

### Stap 4.1 — Eén Bode-meetpunt end-to-end
- **Wat te bouwen:** sluit de keten: AD9102 genereert sinus → wacht settling time → sample
  ADC0+ADC1 → Goertzel → bereken G_dB en φ voor één vaste testfrequentie.
- **Hoe te testen:** meet aan een bekende referentie-impedantie/filter (bv. een eenvoudig
  RC-netwerk met bekende overdracht) en vergelijk de berekende G_dB/φ met de theoretische
  waarde voor die testfrequentie.
- **Acceptatiecriterium:** gemeten G_dB en φ komen overeen met de berekende theoretische waarde
  van het referentienetwerk binnen een vooraf vastgestelde tolerantie.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 4.2 — Frequentie-sweep (zonder UI, data naar UART/CSV)
- **Wat te bouwen:** `bode_init/add_point`, loop over een logaritmische frequentiereeks,
  voer Stap 4.1 herhaald uit en verzamel de resultaten in `bode_buf`.
- **Hoe te testen:** sweep over hetzelfde referentienetwerk als 4.1 en vergelijk de volledige
  curve (magnitude + fase) met de theoretische Bode-plot van dat netwerk (bv. geplot in Python/Excel).
- **Acceptatiecriterium:** gemeten sweep-curve volgt de theoretische curve qua vorm en
  kantelpunt(en) binnen tolerantie over het hele frequentiebereik.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 4.3 — GUI-ontwerp: generieke invoercomponenten (toetsenbord, eenheden, decimalen)
- **Wat te bouwen (ontwerp/wireframes, nog geen volledige implementatie):**
  - Numeriek invoerwidget (touch-toetsenbord): cijfers 0-9, decimaal punt, backspace/clear,
    +/- voor offset-achtige waarden
  - Eenheid-selector naast het invoerveld (bv. Hz / kHz / MHz voor frequenties,
    ms / s voor tijden, mV / V voor amplitudes) die de ingevoerde waarde naar SI omrekent
  - Validatie- en foutweergave: bereikcontrole (min/max), relatie-checks tussen velden
    (bv. start-frequentie < stop-frequentie, aantal punten > 0)
  - Generieke layout die herbruikbaar is over alle meetmodi (Bode, Ruismeting,
    Staprespons, Elektronische belasting) zodat niet per scherm een eigen invoerwidget
    ontworpen hoeft te worden
- **Hoe te testen:** loop op papier/in een klikbaar prototype een aantal representatieve
  invoergevallen door (bv. "2.5 kHz", "100 Hz", "1 MHz", "0.001 s") en controleer of het
  widget elk geval correct naar SI-eenheden omrekent en ongeldige invoer (bv. start > stop,
  lege waarde, te groot bereik) herkent en duidelijk terugmeldt.
- **Acceptatiecriterium:** het invoerwidget-ontwerp dekt alle in de Bode- en
  belasting-workflows benodigde grootheden en eenheden, rekent correct om naar SI, en
  blokkeert/markeert ongeldige combinaties vóórdat een meting start.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 4.4 — GUI-ontwerp: meetmodus-selectie & Bode-parameterscherm
- **Wat te bouwen (ontwerp/wireframes, voortbouwend op Stap 4.3):**
  - Meetmodus-selectiescherm: keuzemenu tussen de meetfuncties (Bode plot, Ruismeting,
    Staprespons, Elektronische belasting) als startpunt van de UI-navigatie
  - Bode-parameterscherm dat de Stap 4.3-widgets hergebruikt voor: start-frequentie,
    stop-frequentie, aantal meetpunten, oversampling-factor, settling time
    (zie "Input Parameters" in PROJECT_SPEC_DETAILED.md)
  - Bode-plotscherm: schermindeling met het gecombineerde log/log magnitude+fase-venster,
    touch-cursor en infobalk (zie PROJECT_SPEC_DETAILED.md "Bode Plot Screen")
  - Navigatieflow tussen de drie schermen (mode-keuze → parameters → start/resultaat)
- **Hoe te testen:** loop de volledige Bode-flow door op de wireframes (mode kiezen →
  parameters invoeren incl. eenheden/decimalen → sweep starten → resultaten + cursor
  bekijken) en controleer of elk scherm binnen 280×320 past zonder overlap of afsnijding.
- **Acceptatiecriterium:** elk scherm en invoerveld uit de Bode-workflow (mode-keuze,
  start/stop, punten, oversampling, settling time, plot + cursor-infobalk) is in een
  wireframe vastgelegd, past op het 280×320 scherm en de navigatie tussen schermen is
  eenduidig vastgelegd.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 4.5 — Bode-plot rendering op TFT
- **Wat te bouwen:** `bode_render_to_tft()` — gecombineerd log/log magnitude+fase-venster
  (twee kleuren) zoals ontworpen in Stap 4.4 en geschetst in PROJECT_SPEC_DETAILED.md
  ("Display Rendering" → "Bode Plot Screen").
- **Hoe te testen:** voer de sweep van 4.2 uit en vergelijk de getekende plot visueel met de
  data/referentiecurve; controleer assen (log-frequentie, dB, graden), kleurcodering en
  leesbaarheid op het 280×320 scherm.
- **Acceptatiecriterium:** plot toont magnitude én fase correct in één venster op de juiste
  assen/schaal/kleur, leesbaar en zonder rendering-glitches tijdens live updates per meetpunt.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 4.6 — Kruisfrequentie-detectie + markering
- **Wat te bouwen:** `bode_find_crossover()` — G=1 (0 dB) en G=0.5 (−6 dB) punten + bijbehorende fase.
- **Hoe te testen:** vergelijk de gedetecteerde kruisfrequentie/fasemarge met handmatige aflezing
  uit de gemeten curve van 4.2/4.5.
- **Acceptatiecriterium:** automatisch gedetecteerde kruisfrequentie en fasemarge komen overeen
  met handmatige analyse van dezelfde dataset (binnen één meetpunt-resolutie).
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 4.7 — Touch-cursor op het Bode-plotscherm
- **Wat te bouwen:** cursor (verticale lijn) die via touch over het plotvenster te
  verschuiven is; bij elke positie wordt de bijbehorende frequentie bepaald (interpolatie
  tussen meetpunten) en worden magnitude, fase en de stabiliteitswaarden (kruisfrequentie/
  fasemarge uit Stap 4.6) in de infobalk getoond.
- **Hoe te testen:** schuif de cursor over bekende meetpunten en controleer of de getoonde
  frequentie/magnitude/fase overeenkomen met de onderliggende dataset; test ook posities
  tussen meetpunten (interpolatie) en de randen van het plotvenster.
- **Acceptatiecriterium:** cursor volgt de touch-positie vloeiend, toont correcte
  geïnterpoleerde waarden op elk punt binnen het plotbereik, en blijft binnen de
  schermgrenzen (geen out-of-range crashes).
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 4.8 — Parameter-UI implementatie (Bode)
- **Wat te bouwen:** implementeer het in Stap 4.4 ontworpen meetmodus- en
  parameterscherm (incl. de Stap 4.3-invoerwidgets met eenheid-selectie en decimaal punt)
  voor start/stop-frequentie, aantal punten, oversampling en settling time.
- **Hoe te testen:** wijzig parameters via de UI (incl. verschillende eenheden, bv. "1.5 kHz"
  vs "1500 Hz") en start een sweep; controleer dat de daadwerkelijk uitgevoerde sweep
  (frequentiebereik, aantal punten, duur) overeenkomt met de ingevoerde waarden.
- **Acceptatiecriterium:** sweep gedraagt zich exact volgens de via touch ingevoerde
  parameters, ongeacht gekozen eenheid/decimale notatie; UI-invoer is robuust tegen
  randgevallen (bv. start > stop, 0 punten, lege/ongeldige invoer).
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

---

## FASE 4B-A — Ruismeting (`noise_measure.h/c`)

**Doel van de fase:** de ADC0-meetmodus "Mode A — Ruismeting" uit PROJECT_SPEC.md
(regel 117/362) is een projectdoel en geplande module (PROJECT_SPEC.md regel 154-155), maar
stond tot nu toe niet als implementatiestap in dit plan. Zonder expliciete stap blijft het
een "vergeten" doel — deze fase voorkomt dat. Deze fase hergebruikt de ADC-acquisitie uit
Fase 2 en de GUI-bouwstenen uit Fase 4.3/4.4.

> **Waarom een aparte fase, los van Staprespons:** Ruismeting genereert geen reële stroomstap
> via de AD9102/belastingsketen — in tegenstelling tot Staprespons (zie **FASE 4B-B**, die
> bewust ná Stap 5.2 in het plan staat omdat die wél een echte stroomstap door de
> belastingsketen stuurt). Door de twee modi in aparte fases met een eigen plek in de
> volgorde te zetten, is het onmogelijk om "lineair door het plan te lopen" en toch een
> verboden volgorde te raken: Ruismeting kan veilig vóór Fase 5 worden gebouwd en getest,
> Staprespons staat fysiek ná de belastings-veiligheidsbaseline.

### Stap 4B-A.1 — Ruismeting (ADC0 Mode A): meting + auto-scaling
- **Wat te bouwen:** `noise_measure.h/c` volgens "Algoritme: Ruismeting — ADC0 Mode A"
  (PROJECT_SPEC.md regel 362): continue ADC0-sampling, per meetvenster V_pp/V_rms berekenen,
  auto-scaling van de Y-as op basis van V_pp.
- **Hoe te testen:** voer een bekend ruis-/AC-signaal in op ADC0 (bv. AD9102-sinus met
  ingestelde amplitude, of een referentie-ruisbron) en vergelijk de berekende V_pp/V_rms met
  een scoop-/multimeter-referentiemeting van hetzelfde signaal.
- **Acceptatiecriterium:** berekende V_pp/V_rms komen overeen met de referentiemeting binnen
  vooraf vastgestelde tolerantie, en de auto-scaling houdt het signaal leesbaar in beeld over
  een realistisch bereik van signaalniveaus.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 4B-A.2 — Ruismeting: GUI (rolling waveform + V_pp/V_rms)
- **Wat te bouwen:** schermontwerp + implementatie voor de Ruismeting-modus: rolling
  waveform-weergave plus V_pp/V_rms-uitlezing (en optioneel een eenvoudig frequentiespectrum,
  zie PROJECT_SPEC.md regel 366-367), gebruikmakend van de generieke invoer-/navigatiecomponenten
  uit Stap 4.3/4.4 voor parameters als samplerate/meetvenster.
- **Hoe te testen:** laat een bekend signaal continu lopen en controleer of de weergave
  vloeiend ververst, leesbaar blijft bij verschillende signaalniveaus (auto-scaling), en de
  getoonde V_pp/V_rms overeenkomen met Stap 4B-A.1.
- **Acceptatiecriterium:** scherm toont een stabiele, leesbare rolling waveform met correcte
  V_pp/V_rms-waarden, ververst vloeiend (vergelijkbaar fps-doel als scope-UI in Fase 5) en
  zonder rendering-glitches tijdens live updates.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

---

## FASE 5 — Elektronische belasting + scope-UI

> **Volgorde-waarschuwing:** deze fase stuurt de elektronische belasting daadwerkelijk aan —
> dat is het meest risicovolle deel van het project (vermogen, warmte, mogelijke schade aan de
> voeding-onder-test of het belastingscircuit). Stap 5.1 en 5.2 **moeten** zijn afgerond vóórdat
> er één statisch of dynamisch profiel op een echte voeding wordt uitgevoerd — niet pas ná
> Fase 6 (zoals in een eerdere versie van dit plan).

### Stap 5.1 — Veiligheidsbaseline vóór belastingstests
> **Harde afhankelijkheid:** deze stap mag niet zonder Stap 1.7 (output-range/offset/
> clamp-regels) worden afgerond — zonder een geverifieerde DAC-code↔0 A-mapping en
> clamp-regels per golfvorm-type kan geen enkele `P_max_check`/SOA-precheck betrouwbaar
> garanderen dat I_load ≥ 0 blijft. Rond Stap 1.7 dus af (of neem de clamp-validatie hier
> expliciet over) vóórdat enige load-golfvorm wordt gestart.
- **Wat te bouwen:** bovenop de minimale baseline uit Stap 0.5 (veilige DAC-default, noodstop,
  watchdog), specifiek voor de belastingsketen: `I_max`/`P_max_check`-berekening vóór het
  starten van een belastingsprofiel (zie "Electronic Load Workflow" stap 2 in
  PROJECT_SPEC_DETAILED.md) die start weigert/limiteert bij overschrijding, **plus** een
  *minimale* thermische bewaking — een vereenvoudigde, vroeg beschikbare versie van Stap 6.1
  (NTC-temperatuur lezen) en Stap 6.3 (harde temperatuur-afkapgrens → noodstop), zodat een
  belastingstest nooit zonder enige thermische beveiliging draait. De volledige, uitgewerkte
  thermische beveiliging + ventilatorregeling (Fase 6) mag parallel/later worden afgerond,
  maar deze minimale afkapgrens moet er vóór Stap 5.2 al zijn.
- **Hoe te testen:** probeer bewust een te hoog vermogens-/stroom-setpoint in te stellen via de
  UI en controleer dat het systeem start weigert of automatisch limiteert met duidelijke melding;
  forceer (gesimuleerd) een te hoge NTC-temperatuur en controleer dat de minimale afkapgrens de
  belasting daadwerkelijk en op tijd uitschakelt.
- **Acceptatiecriterium:** systeem start nooit een profiel dat de geconfigureerde P_max_avg
  zou overschrijden, en schakelt de belasting aantoonbaar uit zodra de minimale
  temperatuur-afkapgrens wordt overschreden — beide vóórdat er ook maar één test op een
  echte voeding plaatsvindt.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 5.2 — Eerste belastingstest: dummy-load / lage spanning / stroomlimiet
- **Wat te bouwen:** niets nieuws qua firmware — dit is de eerste keer dat de belastingsketen
  (AD9102 → OPA810 → FET → shunt) daadwerkelijk stroom trekt. Doe dit **niet** direct op de
  uiteindelijke voeding-onder-test: gebruik een dummy-load/labvoeding met ingestelde
  stroomlimiet en een laag spanningsniveau, zodat een fout in de keten of de
  veiligheidsbaseline (Stap 5.1) geen schade veroorzaakt.
- **Hoe te testen:** stuur een klein, vast DAC-setpoint naar de belastingsketen en meet de
  werkelijke stroom (externe stroommeter/shuntmeting) en temperatuurstijging bij oplopende,
  voorzichtig stapsgewijs verhoogde setpoints; controleer telkens dat Stap 5.1 (P_max-check,
  noodstop, thermische afkap) daadwerkelijk ingrijpt wanneer de limieten bewust worden overschreden.
- **Acceptatiecriterium:** de belastingsketen reageert voorspelbaar en veilig op setpoints en
  op opzettelijke limietoverschrijdingen, zonder schade aan keten of bron, vóórdat er wordt
  overgegaan op de echte voeding-onder-test (Stap 5.3 e.v.).
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

---

## FASE 4B-B — Staprespons (`step_response.h/c`)

**Doel van de fase:** de ADC0-meetmodus "Mode B — Staprespons" uit PROJECT_SPEC.md
(regel 118/374) is een projectdoel en geplande module (PROJECT_SPEC.md regel 154-155), maar
stond tot nu toe niet als implementatiestap in dit plan. Deze fase staat **bewust hier**,
fysiek ná Stap 5.1 (veiligheidsbaseline) en Stap 5.2 (eerste belastingstest op
dummy-load/lage spanning/stroomlimiet) — in tegenstelling tot Ruismeting (**FASE 4B-A**,
vóór Fase 5) genereert Staprespons namelijk een echte stroomstap via dezelfde
AD9102/belastingsketen als Fase 5. Door de fase hier te plaatsen in plaats van er alleen
voor te waarschuwen, is een verboden volgorde (stroomstap vóór veiligheidsbaseline/eerste
belastingstest) door de planningsvolgorde zelf uitgesloten — wie het plan van boven naar
beneden doorloopt, kan deze stap niet meer per ongeluk te vroeg uitvoeren.

### Stap 4B-B.1 — Staprespons (ADC0 Mode B): trigger + pre/post-trigger capture
- **Wat te bouwen:** `step_response.h/c` volgens "Algoritme: Staprespons — ADC0 Mode B"
  (PROJECT_SPEC.md regel 374): instelbare stapgrootte (`I_step`/`V_DAC`), ARM-trigger met
  pre-trigger buffer, GPIO-gesynchroniseerde start van de ADC-capture bij de DAC-stap, en
  bepaling van de settling time (tijd tot `|V_out − V_steady| < drempel`).
- **Hoe te testen:** genereer een bekende stroomstap **op de dummy-load/labvoeding uit Stap 5.2**
  (met de Stap 5.1-veiligheidsbaseline — P_max-check, noodstop, thermische afkap — actief) en
  vergelijk de gecapturede transiënt en de berekende settling time met een onafhankelijke
  scoop-meting (trigger op hetzelfde GPIO-syncsignaal) van dezelfde stap. Pas ná succesvolle
  afronding van Fase 5 (echte voeding-onder-test) herhalen op de voeding-onder-test, indien
  relevant voor de uiteindelijke karakterisatie.
- **Acceptatiecriterium:** gecapturede pre/post-trigger-data en berekende settling time komen
  overeen met de scoop-referentiemeting binnen vooraf vastgestelde tolerantie, voor
  verschillende stapgroottes — én de stap is uitgevoerd met de Stap 5.1-veiligheidsbaseline
  actief en (vóór Fase 5 volledig is afgerond) uitsluitend op dummy-load/labvoeding.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 4B-B.2 — Staprespons: GUI (transiënt + settling-time annotatie)
- **Wat te bouwen:** schermontwerp + implementatie voor de Staprespons-modus: transiënt-plot
  met tijdas, settling-time-annotatie en stapgrootte-parameter, gebruikmakend van de generieke
  invoer-/navigatiecomponenten uit Stap 4.3/4.4.
- **Hoe te testen:** doorloop de workflow (stapgrootte instellen → ARM → trigger → resultaat)
  en controleer of het scherm de transiënt, tijdas en settling-time-annotatie correct en
  leesbaar toont, in lijn met Stap 4B-B.1.
- **Acceptatiecriterium:** scherm toont de transiënt met correcte tijdas-schaal en een
  duidelijk afleesbare settling-time-annotatie die overeenkomt met de berekende waarde uit
  Stap 4B-B.1, leesbaar binnen het 280×320 schermformaat.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

---

> *(vervolg van FASE 5 — Elektronische belasting + scope-UI; FASE 4B-B hierboven staat
> bewust tussen Stap 5.2 en 5.3 in, zie de toelichting in die fase-intro.)*

### Stap 5.3 — Statische belasting (DC) + stroomberekening
- **Wat te bouwen:** stuur een vaste DAC-waarde naar de belastingsketen (AD9102 → OPA810 → FET
  → shunt) en bereken `I_load = V_DAC`.
- **Hoe te testen:** meet de werkelijke belastingsstroom met een externe stroommeter/shuntmeting
  bij een paar setpoints en vergelijk met de berekende waarde.
- **Acceptatiecriterium:** gemeten stroom komt overeen met `I_load = V_DAC` binnen tolerantie van
  het opamp-/FET-circuit (afwijkingen documenteren — input voor calibratie).
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 5.4 — Dynamische golfvormen op de belasting
> **Harde afhankelijkheid:** net als Stap 5.1 mag ook deze stap niet zonder Stap 1.7
> (output-range/offset/clamp-regels) worden uitgevoerd — elke hier geladen golfvorm
> (sinus/blok/driehoek/zaagtand) moet al door de Stap 1.7-clamp-validatie zijn gegaan,
> zodat I_load[n] ≥ 0 over de hele periode gegarandeerd is vóórdat deze daadwerkelijk op
> de belastingsketen wordt afgespeeld.
- **Wat te bouwen:** koppel `waveform_gen` (Stap 3.5) aan de belastingsketen via
  `CMD_LOAD_WAVEFORM`; ondersteun sinus/blok/driehoek/zaagtand met instelbare freq/amplitude/offset/duty.
- **Hoe te testen:** stel elke golfvormsoort in en meet de resulterende belastingsstroom (extern)
  en/of de spanningsdip op de voeding; vergelijk vorm en timing met de ingestelde parameters.
- **Acceptatiecriterium:** belastingsstroom volgt de ingestelde golfvorm qua vorm, frequentie,
  amplitude en duty cycle binnen meetnauwkeurigheid.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 5.5 — Real-time scope-weergave
- **Wat te bouwen:** render Vout (ADC2/MUX0) en I_calc als oscilloscoopbeeld (zie schets
  "Electronic Load Screen" in PROJECT_SPEC_DETAILED.md), inclusief V_rms/V_pp/I_calc/P-metingen.
- **Hoe te testen:** laat een golfvorm lopen en vergelijk de live scope-weergave + cijfers op
  het TFT-scherm met een externe scoop op hetzelfde meetpunt.
- **Acceptatiecriterium:** scope-weergave toont de juiste golfvorm met stabiele 10–30 fps
  refresh, en de getoonde meetwaarden komen overeen met de externe scoop binnen tolerantie.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 5.6 — ADC2-MUX-arbitrage in load-modus (Vout-scope vs. NTC-bewaking)
- **Wat te bouwen:** een scheduler/arbitrageregel voor ADC2/MUX die de twee gelijktijdige
  behoeften in load-modus verzoent: (a) Stap 5.5 wil ADC2/MUX0 (Vout) continu bemonsteren voor
  een gapless scope-buffer, terwijl (b) Stap 5.1/Fase 6 periodiek naar ADC2/MUX1 (NTC) moet
  omschakelen voor thermische bewaking. Specificeer expliciet: hoe vaak/hoe lang wordt
  Vout-sampling onderbroken voor een NTC-meting, hoeveel samples worden weggegooid voor
  MUX-settling (zie Stap 2.4: 5–10 samples), hoe wordt de resulterende "gap" in de
  scope-buffer opgevangen (interpolatie/markering/skip), en — cruciaal — wat er gebeurt met
  de safety-keten als de MUX vastloopt of niet binnen de verwachte tijd terugschakelt
  (bv. fail-safe: aannemen dat temperatuur onbekend is → conservatief gedrag/noodstop).
- **Hoe te testen:** laat een lange scope-sessie (Stap 5.5) draaien met periodieke
  NTC-pollingen (Stap 6.1) actief en controleer: (1) of de scope-weergave geen zichtbare
  gaten/glitches vertoont op de momenten van MUX-omschakeling, (2) of de NTC-temperatuur
  desondanks met de gespecificeerde regelmaat wordt ververst, (3) door de MUX-omschakeling
  bewust te vertragen/te blokkeren (gesimuleerde storing): of de safety-keten daadwerkelijk
  in de fail-safe-toestand terechtkomt in plaats van door te draaien op verouderde data.
- **Acceptatiecriterium:** Vout-scope blijft visueel continu en bruikbaar ondanks periodieke
  NTC-omschakeling, NTC-temperatuur wordt op de gespecificeerde cadans ververst zonder
  "vervuiling" tussen kanalen, en een vastgelopen/trage MUX leidt aantoonbaar tot fail-safe
  gedrag (geen ongemerkt doordraaien op verouderde temperatuurdata).
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 5.7 — Impedantieberekening
- **Wat te bouwen:** `impedance_calc.h/c` — `Z_out = ΔV_supply / I_calc` voor zowel
  AC-ripple (Goertzel) als DC-dip (load-step) methodes.
- **Hoe te testen:** meet aan een voeding met een bekende/gespecificeerde uitgangsimpedantie
  (of een bekende serieweerstand als surrogaat) en vergelijk de berekende Z_out met de
  verwachte waarde.
- **Acceptatiecriterium:** berekende impedantie komt overeen met de bekende referentiewaarde
  binnen tolerantie, voor zowel de sinuslast- als load-step-methode.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 5.8 — Parameter-UI (belasting)
- **Wat te bouwen:** touch-UI voor golfvorm-selectie, frequentie, amplitude, offset, duty cycle,
  start/stop (zie schets "Electronic Load Screen").
- **Hoe te testen:** wijzig elke parameter via de UI tijdens een lopende meting en controleer
  dat de scope-weergave en gemeten waarden direct het nieuwe setpoint volgen.
- **Acceptatiecriterium:** alle parameters zijn live aanpasbaar via touch en het systeem
  reageert correct en zonder glitches/crashes.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

---

## FASE 6 — Thermische beveiliging + ventilator (`protection/thermal_protection.h/c`)

### Stap 6.1 — NTC-temperatuurmeting
- **Wat te bouwen:** koppel `adc_mux_read_ntc_temp()` (Fase 2.4) aan de β-formule uit
  PROJECT_SPEC_DETAILED.md ("Calibration & Scaling").
- **Hoe te testen:** vergelijk de berekende temperatuur met een referentie-thermometer op het
  koelblok bij verschillende temperaturen (bv. koud, kamertemperatuur, warm na belasting).
- **Acceptatiecriterium:** berekende temperatuur wijkt < 2°C af van de referentiemeting over
  het verwachte werkbereik.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 6.2 — Vermogensbewaking (P_sample / P_peak / P_avg)
- **Wat te bouwen:** per-sample en gemiddeld-vermogenberekening incl. `duty_factor`-tabel
  per golfvormtype (zie tabel in PROJECT_SPEC_DETAILED.md).
- **Hoe te testen:** laat bekende belastingsprofielen lopen en vergelijk de berekende
  P_inst/P_avg met handmatige berekening op basis van gemeten V_supply en ingesteld setpoint.
- **Acceptatiecriterium:** berekende vermogens komen overeen met handmatige referentieberekening
  binnen tolerantie, voor elk ondersteund golfvormtype.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 6.3 — Drempelacties (waarschuwing / reductie / noodstop)
- **Wat te bouwen:** `protection_check()`, `protection_emergency_stop()`,
  `protection_get_power_limit()` met de actietabel uit PROJECT_SPEC_DETAILED.md.
  **Let op:** "noodstop binnen één sample-cyclus" is **niet realistisch** zodra de AD9102
  zelfstandig een golfvorm uit Signal RAM afspeelt — de firmware krijgt dan niet per
  DAC-sample controle over de uitgang. Bouw daarom drie complementaire lagen:
  1. **Vooraf berekende SOA-check per golfvorm:** vóór het starten van een profiel wordt
     gecontroleerd dat *geen enkel* punt van de golfvorm (piek, RMS, duty-gewogen gemiddelde)
     de SOA/`P_max`-grenzen kan overschrijden — dit voorkomt dat een gevaarlijke golfvorm
     ooit start (zie ook Stap 5.1).
  2. **Firmware-noodstop (snelle control-loop):** `protection_emergency_stop()` reageert
     binnen de tijdspanne van de protection-controlelus (niet per DAC-sample) op
     temperatuur-/vermogensoverschrijdingen die tijdens het lopen ontstaan (bv. trage
     thermische drift) en zet de AD9102/uitgang naar de veilige toestand.
  3. **Hardwarematige gate-disable/comparator (indien P_max_inst-overschrijding sneller dan
     de firmware-reactietijd kan optreden):** een analoge comparator/gate die de FET/uitgang
     onafhankelijk van de firmware uitschakelt zodra een hard ingestelde drempel wordt
     overschreden — documenteer expliciet of deze hardwarelaag nodig is, en zo ja, leg de
     benodigde componenten/aansturing vast (eventueel als losse hardware-stap).
- **Hoe te testen:** (1) probeer bewust een golfvorm te starten die de SOA zou overschrijden
  en controleer dat dit al bij de pre-check geweigerd wordt; (2) simuleer/forceer elk
  drempelniveau via de firmware-controlelus (bv. tijdelijk lage limieten) en meet de
  daadwerkelijke reactietijd van waarschuwing/reductie/noodstop; (3) indien aanwezig: test de
  hardwarematige gate-disable onafhankelijk van de firmware (bv. door de firmware-respons
  tijdelijk te vertragen/uit te schakelen) en meet die reactietijd apart.
- **Acceptatiecriterium:** (1) een SOA-overschrijdende golfvorm kan nooit starten; (2) elke
  drempelconditie triggert de gespecificeerde firmware-actie binnen de **gemeten en
  gedocumenteerde** reactietijd van de controlelus (niet "binnen één sample-cyclus"); (3) als
  een hardwarelaag aanwezig is, schakelt die de uitgang aantoonbaar uit, onafhankelijk van en
  sneller dan de firmware-respons.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 6.4 — Ventilator-PWM
- **Wat te bouwen:** `protection_fan_duty()` met lineaire regeling op basis van NTC-temperatuur
  (25 kHz hardware-PWM, zie PROJECT_SPEC_DETAILED.md).
- **Hoe te testen:** varieer (gesimuleerde of werkelijke) temperatuur en meet de PWM-duty cycle
  op de FAN_PWM-pin (scoop/logic analyzer); controleer `fan_min_pwm` bij t_fan_start en 100% bij t_fan_full.
- **Acceptatiecriterium:** PWM-duty cycle volgt de gespecificeerde lineaire curve binnen
  meetnauwkeurigheid, inclusief de ondergrens `fan_min_pwm`.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

---

## FASE 7 — Inter-core communicatie + Core 0 MicroPython

### Stap 7.1 — FIFO-commandoprotocol
- **Wat te bouwen:** implementeer de commando-/event-codes (`CMD_BODE_START`,
  `CMD_LOAD_WAVEFORM`, `CMD_EMERGENCY_STOP`, `EVT_BODE_POINT`, `EVT_TOUCH`,
  `EVT_TEMP_WARNING`, etc., zie PROJECT_SPEC.md "Inter-core FIFO").
- **Hoe te testen:** stuur elk commando-/event-type over en weer en log op UART dat het
  juiste type + payload aan de andere kant aankomt.
- **Acceptatiecriterium:** alle gedefinieerde commando's/events worden correct verzonden,
  ontvangen en gedecodeerd zonder verlies, in beide richtingen.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 7.2 — Gedeelde buffers met ownership-protocol
- **Wat te bouwen:** `bode_buf`/`load_cmd_buf` met `volatile` + `__dmb()`-memory barriers
  zoals beschreven in PROJECT_SPEC.md ("Bus-eigenaarschap en shared-buffer ownership").
- **Hoe te testen:** stress-test met snelle achtereenvolgende schrijf-/leesacties vanaf beide
  cores; controleer op race conditions (bv. door een sequence-counter in elke buffer-entry
  mee te sturen en te verifiëren dat Core 0 nooit een "halve" of inconsistente entry leest).
- **Acceptatiecriterium:** geen enkele inconsistente/gecorrumpeerde buffer-read, ook niet
  onder belasting, over een lange testrun (bv. > 10.000 transacties).
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 7.3 — MicroPython op Core 0: basis
- **Wat te bouwen:** `core0_micropython.py`-skelet dat opstart, met Core 1 communiceert via
  de FIFO uit 7.1, en een minimale UI-state machine heeft (modusselectie).
- **Hoe te testen:** start het systeem, wissel van modus via de UI en controleer dat Core 0
  het juiste commando naar Core 1 stuurt en Core 1 het verwachte gedrag toont.
- **Acceptatiecriterium:** modus-wisseling via MicroPython-UI resulteert betrouwbaar in het
  juiste gedrag op Core 1 (display + metingen schakelen mee).
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 7.4 — Volledige workflows via MicroPython
- **Wat te bouwen:** `bode_plot_controller.py` en `load_controller.py` die de complete
  workflows uit PROJECT_SPEC_DETAILED.md aansturen (sweep-loop, belastingsbewakingslus).
- **Hoe te testen:** draai een complete Bode-sweep én een complete belastingssessie end-to-end,
  aangestuurd vanuit MicroPython, en vergelijk het resultaat met de eerdere C-only tests
  (Fase 4.2 / Fase 5.4-5.5).
- **Acceptatiecriterium:** resultaten van de MicroPython-aangestuurde workflows komen overeen
  met de eerder geverifieerde C-only resultaten (geen regressie door de extra laag).
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

---

## FASE 8 — Data-export (`data_export.py`, CSV via UART/USB → SD-kaart)

> **Volgorde-keuze:** SD-kaart via PIO-SPI vanuit MicroPython is een eigen subsysteem met
> aanzienlijk haalbaarheidsrisico (pinnen nog TBD, PIO + FAT/SD-driver in MicroPython). Bouw
> daarom eerst een werkende CSV-exportbaseline over UART/USB (geen filesystem nodig) — dat
> levert direct bruikbare export op én isoleert het SD-kaart-risico als losse, latere stap
> (eventueel als proof-of-concept die je mag laten vallen zonder de exportfunctie te verliezen).

### Stap 8.1 — CSV-export via UART/USB (baseline)
- **Wat te bouwen:** `bode_export_csv()` + `CMD_EXPORT_DATA`-afhandeling die de dataset
  (frequency, gain_dB, phase_deg) als CSV over UART/USB-serial naar een PC streamt — geen
  SD-kaart of filesystem vereist.
- **Hoe te testen:** exporteer de dataset van een sweep (Fase 4.2), vang de UART/USB-output op
  een PC op (bv. met `picocom`/een Python-script dat naar een bestand schrijft) en vergelijk
  de inhoud met de live-gemeten waarden op het scherm.
- **Acceptatiecriterium:** de over UART/USB ontvangen CSV bevat alle meetpunten, correct
  geformatteerd en numeriek gelijk aan de tijdens de meting getoonde/opgeslagen waarden — dit
  is de werkende exportbaseline, onafhankelijk van of de SD-kaart-stap (8.2) ooit slaagt.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 8.2 — SD-kaart proof-of-concept: PIO-SPI basis read/write
- **Wat te bouwen:** PIO-SPI-interface (Core 0) voor SD-kaart, basis bestand schrijven/lezen.
  Behandel dit expliciet als **proof-of-concept**: de definitieve pinnen staan nog TBD in
  PIN_ASSIGNMENT.md, en MicroPython + PIO + FAT/SD-driver is een aparte risicovolle laag
  bovenop de in 8.1 al werkende exportfunctionaliteit.
- **Hoe te testen:** schrijf een testbestand met bekende inhoud, lees het terug (op het
  systeem zelf en/of door de kaart in een PC te lezen) en vergelijk byte-voor-byte.
- **Acceptatiecriterium:** geschreven en teruggelezen data is identiek; bestand is leesbaar
  op een externe PC. Lukt dit niet binnen een vooraf afgesproken tijdsbox: documenteer de
  blokkade en val terug op de UART/USB-baseline uit 8.1 — de exportfunctie blijft dan
  beschikbaar zonder SD-kaart.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 8.3 — CSV-export naar SD-kaart
- **Wat te bouwen:** koppel de in 8.1 gebouwde CSV-generatie aan de in 8.2 bewezen
  SD-kaart-toegang, zodat exports wegschrijven naar bestand op de SD-kaart i.p.v. (of naast)
  UART/USB.
- **Hoe te testen:** exporteer dezelfde sweep-dataset als in 8.1 naar de SD-kaart, lees het
  bestand terug op een PC en vergelijk de inhoud met zowel de live-gemeten waarden als de
  UART/USB-export uit 8.1 (moeten numeriek gelijk zijn).
- **Acceptatiecriterium:** SD-kaart-CSV bevat dezelfde meetpunten, correct geformatteerd en
  numeriek gelijk aan zowel de schermweergave als de UART/USB-referentie-export uit 8.1.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

---

## FASE 9 — Calibratie & systeemintegratie

### Stap 9.1 — ADC-kanaal calibratie (gain/offset)
- **Wat te bouwen:** bepaal en verwerk `ac_gain_factor`, `fb_gain_factor`,
  `voltage_divider_factor` (zie "Calibration & Scaling" in PROJECT_SPEC_DETAILED.md) op basis
  van referentiemetingen per kanaal.
- **Hoe te testen:** voer bekende referentiespanningen in op elk kanaal, vergelijk de
  gecalibreerde uitlezing met de referentiewaarde over het volledige meetbereik.
- **Acceptatiecriterium:** gecalibreerde metingen liggen binnen de doelnauwkeurigheid
  (vooraf vastgesteld %) over het hele bereik, op alle drie de ADC-kanalen.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 9.2 — Goertzel fase-calibratie
- **Wat te bouwen:** kalibratiesweep bij bekende frequentie om de systematische
  fase-offset tussen kanalen vast te stellen en te corrigeren (PROJECT_SPEC_DETAILED.md
  "Goertzel Phase Correction").
- **Hoe te testen:** herhaal de identiek-kanalen-test uit Stap 3.3 ná calibratie en
  controleer dat de restfout verder is afgenomen.
- **Acceptatiecriterium:** resterende systematische fase-offset tussen kanalen ligt
  ruim binnen de tolerantie die nodig is voor betrouwbare fasemarge-bepaling (bv. < 0,5°).
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 9.3 — Volledige systeemintegratie-test
- **Wat te bouwen:** niets nieuws — dit is de eerste keer dat alle subsystemen
  (Bode, belasting, thermisch, UI, export) gelijktijdig actief zijn/elkaar afwisselen
  zonder geheugen- of timingproblemen.
- **Hoe te testen:** draai een lange-duur sessie waarin alle modi na elkaar (en waar
  relevant gelijktijdig, bv. thermische bewaking tijdens belasting) gebruikt worden;
  monitor op crashes, geheugenlekken, gemiste deadlines (display fps, ADC-sampling, touch).
- **Acceptatiecriterium:** systeem blijft > X uur stabiel draaien (vooraf afspreken,
  bv. 4 uur) zonder crash, met alle real-time deadlines (30 fps display, 100 Hz touch,
  continue ADC-acquisitie) gehaald.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

---

## FASE 10 — Eindvalidatie compleet systeem

### Stap 10.1 — Bode-meting op echte voeding
- **Wat te bouwen:** niets nieuws — eerste meting op de daadwerkelijke te karakteriseren
  voeding (in plaats van een referentienetwerk).
- **Hoe te testen:** voer een volledige sweep uit op de echte voeding, vergelijk de
  resulterende Bode-plot en fasemarge met verwachtingen/datasheet van de voeding (indien
  beschikbaar) of met een onafhankelijke meting (bv. met lab-instrumentarium, indien voorhanden).
- **Acceptatiecriterium:** resultaten zijn plausibel en consistent met de verwachte
  stabiliteitskarakteristiek van de voeding; herhaalde metingen geven reproduceerbare resultaten.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 10.2 — Elektronische belasting op echte voeding
- **Wat te bouwen:** niets nieuws — eerste belastingstest op de echte voeding met
  thermische bewaking actief.
- **Hoe te testen:** draai meerdere belastingsprofielen op de echte voeding, observeer
  scope-weergave, P_avg/T-bewaking en fan-gedrag tijdens een langere sessie.
- **Acceptatiecriterium:** systeem regelt belasting, bewaakt vermogen/temperatuur en
  stuurt de ventilator correct, zonder ingrijpen van de gebruiker, gedurende de hele sessie.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

### Stap 10.3 — Documentatie-afronding
- **Wat te bouwen:** werk `AD9102_PROTOCOL.md` en `CALIBRATION.md` (beide nu `[planned]`,
  zie bestandsstructuur in PROJECT_SPEC_DETAILED.md) bij met de daadwerkelijk geïmplementeerde
  protocollen en calibratiewaarden/-procedures.
- **Hoe te testen:** laat iemand anders (of jezelf na enige tijd) het systeem opnieuw
  calibreren puur op basis van de documentatie.
- **Acceptatiecriterium:** calibratieprocedure is reproduceerbaar puur op basis van de
  documentatie, zonder terug te hoeven vallen op code-archeologie.
- **Status:** ⬜ | **Datum:** ____ | **Bevinding:** ____

---

## Wijzigingslog van dit plan

> Voeg hier een regel toe wanneer het plan zelf wijzigt (stappen toegevoegd/aangepast/geschrapt),
> zodat duidelijk blijft waarom de huidige vorm is zoals die is.

| Datum | Wijziging | Door |
|---|---|---|
| 2026-06-07 | Eerste versie van het stappenplan opgesteld | — |
| 2026-06-07 | Review-bevindingen verwerkt: stray export-artefacten verwijderd; AD9102-pin-/safety-baseline-stappen (0.4-0.6) en architectuur-spike toegevoegd; AD9102-stappen gesplitst in clock/DDS-pad/SRAM-pad; ADC high-speed-teststap (2.5) toegevoegd; fase-tolerantie en Bode-bereik frequentie-afhankelijk gemaakt; Goertzel-performancetest (3.4) toegevoegd; FASE 4B (Ruismeting/Staprespons) toegevoegd; FASE 5 herordend (veiligheidsbaseline + dummy-load vóór echte belastingstests); noodstop-stap (6.3) opgesplitst in SOA-precheck/firmware/hardware-laag; FASE 8 herordend (CSV via UART/USB als baseline vóór SD-kaart-PoC) | — |
| 2026-06-07 | Vervolg-review verwerkt: Stap 4B.3 volgorde-afhankelijkheid van Fase 5.1-5.2 expliciet gemaakt; nieuwe Stap 5.6 (ADC2-MUX-arbitrage scope vs. NTC) toegevoegd; Stap 3.2 fasereferentie-opties uitgewerkt (trigger/FSYNC/2e ADC-kanaal/scoop) en beperkt tot amplitude-validatie; Stap 0.6 expliciet go/no-go gemaakt; vage 'Stap 4.x'-verwijzing in Stap 2.5 geconcretiseerd; Stap 0.4 gesplitst in clockbron-hardwarekeuze (frequentie/jitter-eis) + overige pinnen; nieuwe Stap 0.7 (doc-sync specs↔plan) en Stap 0.8 (storage/config-persistentie) toegevoegd; nieuwe Stap 1.7 (output-range/offset/clamp, voorkomt negatieve laststroom) toegevoegd; PROJECT_SPEC.md/PROJECT_SPEC_DETAILED.md noodstop-/SOA-taal gesynchroniseerd met de drielagen-aanpak uit Stap 6.3 (geen "per-sample"/"binnen één sample-cyclus" meer) en ventilatorregeling consequent op lineaire PWM gezet (PI-regelaar uit spec verwijderd) | — |
| 2026-06-07 | Tweede vervolg-review verwerkt: Stap 0.6 maakt nu de Core 0/Core 1-UI-taakverdeling expliciet (Core 1 = render-/touch-/meet-primitives, Core 0 = UI-orkestratie — geen "definitieve C-UI op Core 1" meer); Stap 0.2 uitgebreid met expliciete bus-integratiechecks voor de gedeelde-SPI0-migratie (snelheid/modus-wissel, CS-idle-hoog, MISO-tristate, herhaald wisselen zonder busvervuiling, n.a.v. README_TESTBENCH die SPI1 voor touch gebruikt); FASE 4B opgesplitst in **FASE 4B-A — Ruismeting** (vóór Fase 5, geen stroomstap) en **FASE 4B-B — Staprespons** (fysiek geplaatst ná Stap 5.2, i.p.v. alleen een volgorde-waarschuwing) zodat een verboden volgorde door de planningsvolgorde zelf wordt uitgesloten; Statusoverzicht-tabel bijgewerkt voor de gesplitste fases; Stap 0.8 maakt de opslagkeuze concreet (flash-config is baseline en vereist voor veiligheidslimieten, SD-config optioneel later via Fase 8); Stap 0.1 uitgebreid met expliciete controle/correctie van het `no_flash`-binary-type uit de testbench-CMakeLists en een power-cycle-test; Stap 1.7 als harde afhankelijkheid toegevoegd aan Stap 5.1 en Stap 5.4; Stap 0.7 (doc-sync) uitgebreid met een Core-taakverdelingscontrole en een "voer dit vroeg uit"-aanwijzing; README_TESTBENCH.md gecorrigeerd: XPT2046 is een *resistive* (niet capacitive) touch-controller | — |
