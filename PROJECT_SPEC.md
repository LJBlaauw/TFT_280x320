# Power Supply Stability Tester — Bode Plot & Electronic Load
## RP2350 · Core 1: Pure C · Core 0: MicroPython · Debugger: Pico via SWD

---

> **Status:** Ontwerpdocument  
> **Werkende code:** `TFT_display_touch_driver/` testbench — ILI9341 display + XPT2046 touch, eerder gebouwd; lokale toolchain niet in PATH van huidige sessie  
> **Nog niet geïmplementeerd:** AD9102 driver, Goertzel, elektronische belasting, beveiliging, Core 0 MicroPython  
> **Target board:** Pico 2 W / seeedstudio XIAO RP2350  
> **Pinreferentie:** [`docs/PIN_ASSIGNMENT.md`](docs/PIN_ASSIGNMENT.md) — enkelvoudige bron van waarheid

---

## Hardware Setup

- **Main MCU:** Raspberry Pi RP2350 (Seeed XIAO RP2350)
- **Debugger:** Separate Raspberry Pi Pico (via SWD)
- **Display:** 2.8" TFT LCD (280×320 px, ILI9341) — results visualisation
- **Touch:** XPT2046 — control interface
- **ADC:** RP2350 internal (3 channels, ADC2 shared via analogue MUX)

### Signal Generation — AD9102 DDS
1. **Bode plot mode:** Injecteer sinusgolfsignaal via opto-isolator over 10Ω in feedbacklus
2. **Electronic load mode:** Stuur OPA810-stuurcircuit aan (DC, sinus, blok, driehoek, zaagtand)

### Electronic Load — Stroompad
```
AD9102 → OPA810 opamp → gate IMZA40R036M2H (SiC FET) → drain naar voeding → source → shunt 0.1Ω → GND
```
- **Stroomformule:** `I_load = V_shunt / R_shunt = (0.1 × V_DAC) / 0.1Ω = V_DAC`
  - Opamp-circuit schraalt zodanig dat `V_shunt = 0.1 × V_DAC`
  - Resultaat: **1 V DAC-uitgang = 1 A belastingstroom**
- **FET:** Infineon IMZA40R036M2H (400 V SiC, 36 mΩ R_DS(on), PG‑TO247‑4)
- **Opamp:** OPA810 (hoge bandbreedte, rail-to-rail)
- **Shunt:** 0.1 Ω (stroom → spanning; niet gemeten via ADC, berekend via DAC-setpoint)

### Bode Plot Injectie — Opto-isolator
- **Opto:** HCNR201-300 (lineaire fotokoppelaarkoppeling, hoge snelheid)
- **Transistoren:** MMBT3906 & MMBT3904
- **Injectiepunt:** *over* de 10Ω weerstand die in serie in de feedbacklus zit
  - ADC1 meet aan de voedingszijde van de 10Ω (Vfeedback)
  - ADC0 meet de voedingsuitgang Vout
  - Gain = Vout / Vfeedback; Phase = ∠Vout − ∠Vfeedback

### Thermische Beveiliging
- **NTC:** 10 kΩ NTC op koelblok (10 kΩ naar GND, 10 kΩ naar 3V3 — spanningsmiddelpunt op ADC2 via MUX)
- **MUX:** Analoge multiplexer op ADC2-kanaal, wisselt tussen Vout-meting en NTC-meting
- **MUX-stuurpin:** GPIO (TBD — 1 digitale uitgang)

---

## Pin Configuration (RP2350)

```
SPI0 (eigenaar: Core 1) — Display ILI9341 + Touch XPT2046 + AD9102 DDS:
  Gedeelde bus — CS bepaalt welk apparaat actief is; nooit twee CS tegelijk laag!
  Snelheid wisselen per apparaat: spi_set_baudrate(spi0, speed) vóór CS-assert.
  Opmerking: standalone testbench (TFT_display_touch_driver/) gebruikt nog SPI0 voor display
  en SPI1 voor touch — dat is de testbench-bedrading, niet de eindarchitectuur.
  - GP18: CLK  (gedeeld)
  - GP19: MOSI (gedeeld)
  - GP16: MISO (gedeeld)
  - GP10: Display CS  (actief laag, 10 MHz)
  - GP13: Display DC
  - GP12: Display RST
  - GP11: AD9102 CS   (actief laag, 10 MHz)
  - GP9:  Touch CS    (actief laag,  2 MHz)

PIO SPI (eigenaar: Core 0) — SD kaart:
  Gebruikt één PIO state machine; elke vrije GPIO bruikbaar.
  - TBD: CLK
  - TBD: MOSI
  - TBD: MISO
  - TBD: SD CS (actief laag, 25 MHz)

SPI1 — vrij voor uitbreiding (hardware beschikbaar)

Device snelheidstabel:
  Device      | Bus      | Snelheid | Opmerking
  ------------|----------|----------|----------------------------
  ILI9341     | SPI0     | 10 MHz   | display writes
  AD9102      | SPI0     | 10 MHz   | register writes (max 20 MHz)
  XPT2046     | SPI0     |  2 MHz   | ADC reads (max 2.5 MHz)
  SD kaart    | PIO SPI  | 25 MHz   | data transfer (niet op SPI0)

ADC-pinnen (meting):
  - ADC0 (GP26): Voedingsspanning AC — ruismeting / staprespons / Bode
  - ADC1 (GP27): Vfeedback — voedingszijde van 10Ω injectieweerstand (AC) — Bode plot
  - ADC2 (GP28): Analoge MUX-uitgang
      MUX 0: Vout DC (hoofdmeting voedingsspanning)
      MUX 1: NTC temperatuur koelblok

Analoge MUX besturing:
  - GPIO_MUX_SEL (TBD): 0 = Vout, 1 = NTC

Ventilator:
  - GPIO_FAN_PWM (TBD): hardware PWM, 25 kHz aanbevolen

Debug-poort:
  - Prototype RP2350W DIP-40:  GP20 = SWCLK, GP21 = SWDIO → externe Pico debugger
  - XIAO RP2350 eindontwerp:   eigen SWD debug-pads op KiCad footprint (geen GPIO)
```

---

## Project Goals

1. **Signaalgeneratie:** AD9102 sweept instelbare sinus (1 Hz–1 MHz) en genereert belastingsgolfvormen
   - Let op: 1 MHz is het generatiebereik van de AD9102, niet het direct meetbare Bode-bereik —
     dat wordt begrensd door de interne ADC (zie regel 417: Nyquist 125 kHz/kanaal @ 500 kSa/s
     standaard, 250 kHz/kanaal @ 1 MSa/s overklokt)
2. **ADC-sampling:** Multi-kanaal via DMA; oversampling instelbaar voor ruisonderdrukking
3. **Goertzel-analyse:** Magnitude en fase op doelfrequentie (geen FFT)
4. **Bode plot:** voor voedingen wordt de bodeplot gebruikt om de stabiliteitscriteria te controleren
   - Goertzel levert een complex getal per kanaal: `re + j·im`
   - Versterking: `G_dB = 20·log10(|Z_Vout| / |Z_Vfeedback|)`  waarbij `|Z| = sqrt(re²+im²)`
   - Fase: `φ = atan2(im_Vout, re_Vout) − atan2(im_Vfeedback, re_Vfeedback)`
   - Correctie inter-channel timing: `φ_corr = φ − 2π·f·Δt`  waarbij `Δt = 1/total_ADC_samplerate` (bij 500 kSa/s totaal: Δt = 2 µs; bij 1 MSa/s overklokt: Δt = 1 µs)
5. **Elektronische belasting:** Programmeerbare stroombelasting; scope-weergave van Vout
6. **Ruismeting (ADC0 mode A):** Peak-to-peak en RMS van voedingsruis; auto-scaling
7. **Staprespons (ADC0 mode B):** Transient meting gesynchroniseerd met DAC-stroomstap
8. **Bode plot (ADC0 + ADC1 mode C):** ADC0 = Vout (AC), ADC1 = Vfeedback (voedingszijde 10Ω injectieweerstand); beide via Goertzel → G_dB en φ per frequentiepunt
9. **Impedantiemeting:** `Z_out = ΔV_supply / I_calc` (ΔV van ADC2, I van DAC-setpoint)
10. **Thermische beveiliging (Core 1):** NTC-bewaking op koelblok; ventilator op PWM-uitgang; vermogensbeperking bij overschrijding limiet
11. **Data-export:** CSV-export naar SD kaart (PIO SPI, eigenaar Core 0) en/of USB/UART; SPI1 vrij voor uitbreiding

---

## File Structure

```
TFT_280x320/
├── TFT_display_touch_driver/      [bestaat] testbench display + touch
│   ├── main_testbench.c           [bestaat]
│   ├── ili9341_display.h/c        [bestaat]
│   ├── xpt2046_touch.h/c          [bestaat]
│   ├── spi_interface.h/c          [bestaat]
│   ├── pico_sdk_import.cmake      [bestaat]
│   ├── README_TESTBENCH.md        [bestaat]
│   └── CMakeLists.txt             [bestaat]
│
├── docs/
│   └── PIN_ASSIGNMENT.md          [bestaat] centrale pinreferentie
│
├── main.c                         [gepland] Core 0 launcher (start MicroPython + Core 1)
├── core1_entry.c                  [gepland] Core 1 entry point (real-time C tasks)
│
├── drivers/                       [gepland]
│   ├── ad9102_dds.h/c             (AD9102 DDS via SPI0)
│   ├── adc_sampler.h/c            (ADC multi-kanaal, DMA, MUX-beheer)
│   └── adc_mux.h/c                (Analoge MUX ADC2: Vout DC / NTC)
│
├── signal_processing/             [gepland]
│   ├── goertzel.h/c               (magnitude + fase op doelfrequentie)
│   ├── waveform_gen.h/c           (golfvorm-tabellen: sinus, blok, driehoek)
│   ├── impedance_calc.h/c         (Z = ΔVout / I_calc)
│   ├── noise_measure.h/c          (ADC0 mode A — V_pp, V_rms, auto-scaling)
│   └── step_response.h/c          (ADC0 mode B — staprespons, gesync. trigger)
│
├── protection/                    [gepland]
│   └── thermal_protection.h/c     (NTC, vermogensbewaking, ventilator PWM)
│
├── ui/                            [gepland]
│   ├── ui_manager.h/c             (UI state machine, inter-core FIFO)
│   ├── bode_plot_ui.h/c           (Bode weergave + parameterinvoer)
│   ├── load_ui.h/c                (Elektronische belasting scope)
│   └── noise_ui.h/c               (Ruismeting / staprespons scherm)
│
├── CMakeLists.txt                 [placeholder] kopie testbench-build; niet buildbaar vanuit root (pico_sdk_import.cmake ontbreekt); wordt vervangen door hoofd-project build
├── PROJECT_SPEC.md                [bestaat]
└── PROJECT_SPEC_DETAILED.md       [bestaat]
```

---

## Key Functions

### AD9102 Control (`ad9102_dds.h/c`)
- `ad9102_init()` — Initialiseer SPI-interface
- `ad9102_set_frequency(float freq_hz)` — Stel uitgangsfrequentie in
- `ad9102_set_amplitude(float amplitude_v)` — Stel amplitude in
- `ad9102_load_waveform(const int16_t *samples, uint16_t n)` — Laad golfvorm in DDS RAM
- `ad9102_start()` / `ad9102_stop()` — In-/uitschakeluitgang
- `ad9102_set_dc(float v_out)` — DC-uitgang voor stroomstap (staprespons modus)

### ADC Kanaalconfiguratie (`adc_sampler.h/c` + `adc_mux.h/c`)

**ADC0 (GPIO26) — Voedingsspanning AC-gekoppeld**
- Meet wisselspanningscomponent van voedingsuitgang
- **Mode A — Ruismeting:**
  - Continue sampling (geen trigger)
  - Bereken V_pp, V_rms per meetvenster
  - Auto-scaling op scherm
- **Mode B — Staprespons:**
  - Trigger bij DAC-stap (GPIO-synchronisatie)
  - Pre-trigger buffer: ~20% van venster
  - Capture settling time van de voeding
- **Mode C - Bode plot**
   - AC-gekoppeld: wordt samen met ADC1 gebruikt om de Versterking en Fase te meten

**ADC1 (GPIO27) — Bode plot feedback**
- AC-gekoppeld; voedingszijde van de 10Ω injectieweerstand (niet te verwarren met 0.1Ω stroom-shunt)
- Uitsluitend actief tijdens Bode plot meting

**ADC2 (GPIO28) via analoge MUX**
- **MUX kanaal 0 (Vout):** Voedingsspanning DC-gekoppeld, hoofd-meetkanaal
  - `Vout_V = (ADC_raw / 4095) × Vref × voltage_divider_factor`
- **MUX kanaal 1 (NTC):** Temperatuursensor koelblok
  - `T_C = beta / ln(R_ntc / R_ref × exp(beta / T_ref)) - 273.15`
  - 10K NTC (β ≈ 3950 K), spanningsmiddelpunt met 10K naar 3V3

```c
// API
void adc_mux_select(adc_mux_ch_t ch);   // CH_VOUT of CH_NTC
void adc_mux_read_vout(float *v_out);
void adc_mux_read_ntc_temp(float *temp_c);
```
> **MUX settling:** Na schakeling 5–10 ADC-samples weggooien voor de eerste geldige meting.  
> Analoge MUX heeft typisch 1–5 µs settling time; bij 1 MSa/s = 1–5 samples per µs.

### Goertzel Analyse (`goertzel.h/c`)
```c
goertzel_t *goertzel_init(float sample_rate, float target_freq, uint32_t num_samples);
void  goertzel_process(goertzel_t *g, float sample);
float goertzel_magnitude(goertzel_t *g);
float goertzel_phase_deg(goertzel_t *g);
void  goertzel_reset(goertzel_t *g);
```
- O(N) per doelfrequentie (vs O(N log N) voor FFT)
- Geen tabel of extra geheugen nodig voor enkele frequentie

### Impedantieberekening (`impedance_calc.h/c`)
```
Stroomberekening (niet gemeten — afgeleid van DAC-setpoint):
    I_load (A) = V_DAC_setpoint (V)    [1 V = 1 A]

Uitgangsimpedantie:
    Z_out (Ω) = ΔV_supply / I_load

    Meetpad (kies één):
      AC-ripple (sinuslast): ADC0 (AC-gekoppeld) → Goertzel → amplitude + fase
          ΔV = Goertzel-amplitude op ADC0;  I_load = V_DAC_peak
      DC-dip (load-step):    ADC2/MUX0 (DC-gekoppeld) → V_voor − V_na
          ΔV = DC-dip;  I_load = V_DAC stapgrootte;  fase niet meetbaar

Fase (alleen bij AC-ripple via Goertzel):
    φ (°) = fase_Vout − fase_I   [fase_Vout via Goertzel op ADC0; fase_I = bekende AD9102-referentiefase]
```
```c
void impedance_calc(float delta_v_supply, float i_calc_a,
                    float phase_v_deg,   float phase_i_deg,
                    float *z_ohm,        float *z_phase_deg);
```

### Thermische Beveiliging (`thermal_protection.h/c`)
**Let op — realtime-grenzen:** zodra de AD9102 zelfstandig een golfvorm uit Signal RAM
afspeelt, heeft de firmware geen per-DAC-sample controle over de uitgang. `P_sample`
wordt daarom *vooraf* (per golfvorm, als SOA-pre-check) berekend en gecontroleerd —
niet als live "elke sample-cyclus"-ingreep tijdens het afspelen. De daadwerkelijke
bewaking tijdens bedrijf loopt op het tempo van de protection-controlelus (zie
IMPLEMENTATION_PLAN.md Stap 6.3 voor de drielagen-aanpak: SOA-precheck / firmware-
noodstop met gemeten reactietijd / optionele hardware-gate-disable).

**Berekeningen (gebruikt voor SOA-precheck en lopende bewaking):**
- Per-sample vermogen (SOA-precheck, vooraf over de hele golfvorm): `P_sample = V_DS × I_load[n]`
  - V_DS ≈ V_supply (benadering: V_shunt = 0.1 × V_DAC ≤ 0.1 V bij 1 A, verwaarloosbaar t.o.v. V_supply)
- Piek vermogen: `P_peak = V_DS × I_DAC_peak`  (amplitude van het DAC-signaal)
- Gemiddeld vermogen: `P_avg = P_peak × duty_factor`
  - duty_factor = **mean(I_load[n]) / I_DAC_peak** — gemiddeld stroomratio over één periode (niet RMS²/peak²)
  - Geldt voor unipolaire belasting (stroom ≥ 0):
    | Golfvorm | duty_factor | Toelichting |
    |----------|-------------|-------------|
    | DC       | 1.0 | mean = I_peak |
    | Sinus    | 0.5 | unipolaire sinus: offset = amplitude; mean = amplitude |
    | Blokgolf | user duty (0–1) | mean = I_peak × duty |
    | Driehoek / Zaagtand | 0.5 | ramp 0→I_peak; mean = I_peak/2 (niet RMS²/peak² = 1/3) |
- NTC-temperatuur: gemeten via ADC2 MUX kanaal 1 (elke ~100 ms)

**IMZA40R036M2H limieten (Infineon SiC, PG‑TO247‑4):**
- V_DS max: 400 V
- I_D max (T_C=25°C): afhankelijk van koeling — configureerbaar
- Thermische limiet: instelbaar op basis van koelblok + heatsink

**Ventilator PWM-regeling:**
- Hardware PWM op GPIO_FAN_PWM, aanbevolen 25 kHz (boven hoorbaar bereik)
- Lineaire regeling op basis van NTC-temperatuur:
  ```
  T < t_fan_start  →  ventilator uit  (PWM = 0%)
  t_fan_start ≤ T < t_fan_full  →  lineair: PWM = fan_min + (T - t_fan_start) / (t_fan_full - t_fan_start) × (100 - fan_min)
  T ≥ t_fan_full   →  ventilator max (PWM = 100%)
  ```
- `fan_min_pwm`: minimale duty cycle bij starttemperatuur (bijv. 30%) zodat ventilator zeker aanloopt

**Actie bij overschrijding:**
```
SOA-precheck: golfvorm met punt(en) > P_max_inst  →  start wordt geweigerd (komt nooit tot uitvoer)
P_avg  > P_max_avg   →  reduceer amplitude naar veilige waarde
T_ntc  > T_warning   →  waarschuwing op scherm (geel)
T_ntc  > T_limit     →  vermogen beperkt naar safe_fraction; rode melding; ventilator 100%
(buiten-verwachting overschrijding tijdens bedrijf → firmware-noodstop binnen gemeten/
 gedocumenteerde reactietijd van de controlelus, evt. aangevuld met hardware-gate-disable;
 zie IMPLEMENTATION_PLAN.md Stap 6.3)
```
```c
typedef struct {
    float p_max_inst_w;    // Max ogenblikkelijk vermogen (SOA FET)
    float p_max_avg_w;     // Max gemiddeld vermogen (thermisch)
    float t_warning_c;     // Waarschuwingstemperatuur (bijv. 60°C)
    float t_limit_c;       // Harde grens (bijv. 80°C) → vermogens-dip
    float safe_fraction;   // Fractie max vermogen bij thermische limiet (bijv. 0.5)
    float t_fan_start_c;   // Ventilator start (bijv. 40°C)
    float t_fan_full_c;    // Ventilator max (bijv. 70°C)
    uint8_t fan_min_pwm;   // Min duty cycle bij start, % (bijv. 30)
} protection_config_t;

bool    protection_check(float v_ds, float i_load, float duty_cycle,
                         float temp_c, const protection_config_t *cfg);
void    protection_emergency_stop(void);
float   protection_get_power_limit(float temp_c, const protection_config_t *cfg);
uint8_t protection_fan_duty(float temp_c, const protection_config_t *cfg);
```

### Bode Plot (`bode_plot.h/c`)
- `bode_init(float f_start, float f_stop, uint16_t n_points)`
- `bode_add_point(float freq, float gain_db, float phase_deg)`
- `bode_render_to_tft()` — Teken magnitude + faseplot op TFT
- `bode_find_crossover(float *f_cross, float *phase_margin)` — G=1 punt
- `bode_export_csv()` — Exporteer resultaten

### TFT Display (`ili9341_display.h/c` — **aanwezig, eerder gebouwd**)
- Driverbestanden aanwezig in `TFT_display_touch_driver/`; testbench eerder succesvol gebouwd (toolchain niet in PATH van huidige sessie)

### Touch UI (`xpt2046_touch.h/c` — **aanwezig, eerder gebouwd**)
- Driverbestanden aanwezig in `TFT_display_touch_driver/`; testbench eerder succesvol gebouwd (toolchain niet in PATH van huidige sessie)

---

## Algoritme: Bode Plot (Goertzel)

```
1. Initialiseer AD9102 en ADC0 en ADC1
2. Voor elke testfrequentie (logaritmisch):
   a. AD9102 genereert sinus als foutsignaal via opto-isolator
   b. Wacht settling time (instelbaar, 100–500 ms)
   c. Sample ADC1 (Vfeedback) + ADC0 (Vout) via DMA
   d. Goertzel op beide kanalen → magnitude + fase
   e. G = |Vout| / |Vfeedback|  [lus-gain]
   f. φ = ∠Vout − ∠Vfeedback   [fasemarge]
   g. Sla op (freq, G_dB, phase_deg)
   h. Update TFT met nieuw datapunt
3. Teken volledig Bode plot
   - Markeer G=1 (0 dB) kruisfrequentie + fasemarge
   - Markeer G=0.5 (−6 dB)
4. Optioneel: export naar CSV
```

## Algoritme: Elektronische Belasting

```
1. Gebruiker kiest golfvorm + parameters (amplitude, freq, duty cycle, offset)
2. Bereken I_max = V_DAC_max (A);  controleer tegen P_max_avg / V_supply
   - **SOA-precheck (vooraf, eenmalig per golfvorm):** loop `P_sample = V_supply_est × I_load[n]`
     door over de volledige golfvorm-tabel; als enig punt P_max_inst overschrijdt, wordt de
     start geweigerd (zie ook Stap 6.3 in IMPLEMENTATION_PLAN.md) — dit gebeurt vóór stap 3,
     niet doorlopend tijdens het afspelen
3. Stuur golfvorm-parameters naar Core 1 (FIFO: CMD_LOAD_WAVEFORM | idx) → Core 1 genereert tabel en laadt AD9102 RAM
4. Core 1 start AD9102 in loop-modus
5. Bewakingslus (loopt op het tempo van de protection-controlelus, niet per DAC-sample):
   a. Lees ADC2/MUX0 → V_supply (DC)
   b. Bereken P_peak = V_supply × I_DAC_peak;  P_avg = P_peak × duty_factor (zie thermische tabel)
   c. Controleer thermische beveiliging (elke 100 ms NTC via MUX1)
   d. Toon scope: V_supply (ADC2) + I_calc (DAC-setpoint)
   e. Lineaire PWM-regeling ventilator op basis van NTC-temperatuur (zie "Ventilator
      PWM-regeling" hierboven — geen PI-regelaar: lineair is voldoende en eenvoudiger te
      valideren voor dit project)
   f. Bij T_warning: gele waarschuwing
   g. Bij T_limit:   reduceer amplitude → veilig vermogen
   h. Bij onverwachte P_max-overschrijding tijdens bedrijf: firmware-noodstop binnen
      gemeten/gedocumenteerde reactietijd van de controlelus (zie Stap 6.3 in
      IMPLEMENTATION_PLAN.md)
```

## Algoritme: Ruismeting — ADC0 Mode A

```
1. Selecteer "Noise" modus
2. Continue sampling ADC0 (AC-gekoppeld) @ instelbare samplerate
3. Per meetvenster (bijv. 4096 samples):
   - V_pp  = max(samples) − min(samples)
   - V_rms = sqrt(mean(samples²))
4. Auto-scaling: pas Y-as aan op V_pp
5. Toon op scherm: rolling waveform + V_pp, V_rms, freq-spectrum (optioneel)
```

## Algoritme: Staprespons — ADC0 Mode B

```
1. Selecteer "Step" modus; stel stapgrootte I_step in (V_DAC)
2. ARM trigger: ADC0 klaar voor pre-trigger buffer
3. AD9102 genereert stroomstap (DC laag → DC hoog)
4. GPIO-sync: bij stap → start ADC0 capture (pre + post trigger)
5. Sla capture op (bijv. 2048 samples)
6. Bepaal settling time: tijd tot |V_out − V_steady| < drempel
7. Toon transient op scherm met tijdas + settling time annotatie
```

## Algoritme: Bode Plot — ADC0 Mode C
   zie Algoritme: Bode Plot (Goertzel) hierboven

---

## Impedantieberekening

```
Elektronische belasting genereert bekende stroom I_calc = V_DAC (1 V = 1 A).
ADC2 meet de variatie in voedingsspanning ΔV_supply.

Z_out = ΔV_supply / I_calc

Meting: load-mode, niet Bode-mode.
    ΔV_supply = Goertzel-amplitude (piek) bij sinuslast,
                of DC-dip (V_dip) bij load-step — gemeten via ADC2/MUX0
    I_calc    = V_DAC_setpoint (peak-amplitude, geen ADC-meting)
    Let op: V_pp ≠ Goertzel-amplitude; gebruik consistent piek-waarden aan beide zijden.

Voorbeeld (sinuslast, Goertzel):
    V_DAC = 2 V  →  I_load = 2 A (piek-amplitude)
    ΔV_supply = 0.1 V (Goertzel-amplitude op ADC2/MUX0)
    Z_out = 0.1 V / 2 A = 50 mΩ
```

---

## Real-Time Requirements

| Parameter | Waarde |
|---|---|
| ADC samplerate | 500 kSa/s totaal standaard (250 kSa/s per kanaal, Nyquist 125 kHz); optioneel 1 MSa/s overklokt (500 kSa/s per kanaal, Nyquist 250 kHz) |
| Settling time per freq (Bode) | 100–500 ms (instelbaar) |
| Meetvenster per freq | ~10–100 perioden van testfrequentie |
| Totale Bode-meettijd (50 punten) | 5–30 minuten |
| Display-refresh (scope) | 10–30 fps |
| NTC poll-interval | ~100 ms |
| Ventilator PWM update | ~500 ms (temperatuur traag) |
| Vermogensbewaking (SOA-precheck per golfvorm + lopende controlelus) | precheck vooraf; lopende bewaking op tempo protection-controlelus (gemeten/gedocumenteerd, niet per DAC-sample — zie IMPLEMENTATION_PLAN.md Stap 6.3) |

---

## Calibratie

```
ADC0 (Vout AC):    V_ac   = (ADC_raw / 4095) × Vref × ac_gain_factor
ADC1 (Vfeedback):  V_fb   = (ADC_raw / 4095) × Vref × fb_gain_factor
ADC2 (Vout DC):    Vout_V = (ADC_raw / 4095) × Vref × voltage_divider_factor
ADC2 (NTC):        T_C    = beta_formula(ADC_raw, R_ref=10kΩ, beta=3950K)

Stroom (berekend, geen ADC):
    I_load (A) = V_DAC_setpoint (V)   [schaling 1:1 door opamp-circuit]

Impedantie:
    Z_out (Ω) = ΔV_ADC2 / V_DAC

Goertzel fase-offset:
    Kalibreer bij bekende frequentie om systematische fase-verschuiving te corrigeren
```

---

## RP2350 Specifieke Features

- **Dual Core:** Core 0 (MicroPython) = UI/logica/export; Core 1 (C) = real-time ADC/SPI/display
- **DMA:** ADC→RAM zonder CPU-overhead
- **Hardware FPU (Cortex-M33):** Snelle Goertzel- en drijvende-komma-berekeningen
- **SPI0 (Core 1):** Display + Touch + AD9102 — gedeelde bus, CS per apparaat, snelheid wisselen
- **PIO SPI (Core 0):** SD kaart — 1 state machine, elke GPIO, 25 MHz
- **SPI1:** vrij voor uitbreiding
- **Inter-core FIFO:** hardware 32-bit FIFO (één per richting) — geen SPI-busmutex nodig; gedeelde SRAM via ownership protocol + memory barrier
- **ADC MUX:** Softwarematige multiplexing ADC2 (Vout ↔ NTC)

---

## Multi-core Architectuur

### CS-gedrag op gedeelde SPI-bus

CS is **altijd handmatig** (software GPIO). De hardware SS-pin van RP2350 deassert na elke byte — onbruikbaar voor multi-byte transacties. Het vaste patroon:

```c
spi_set_baudrate(spi0, 2000000);     // snelheid voor dit apparaat
gpio_put(TOUCH_CS, 0);               // assert CS (actief laag)
spi_write_blocking(spi0, cmd, len);  // volledige transactie
gpio_put(TOUCH_CS, 1);               // deassert CS
spi_set_baudrate(spi0, 10000000);    // terug naar display-snelheid
```

**Regel:** nooit twee CS tegelijk laag. Elk apparaat tristate zijn MISO zodra CS hoog is.

### Bus-eigenaarschap en shared-buffer ownership

| Bus | Eigenaar | Toegang |
|-----|----------|---------|
| SPI0 | Core 1 (C) | Display, Touch, AD9102 — uitsluitend vanuit Core 1 |
| PIO SPI | Core 0 (MicroPython) | SD kaart — uitsluitend vanuit Core 0 |
| SPI1 | — | vrij |

Omdat elke bus één eigenaar heeft zijn er geen bus-race conditions. **Voor gedeelde SRAM-buffers gelden wel ownership-afspraken:** Core 1 schrijft een entry, signaleert via FIFO; daarna leest Core 0 — nooit tegelijk schrijven naar dezelfde index. Gebruik `volatile` en een memory barrier (`__dmb()`) om write-reordering te voorkomen.

### Inter-core FIFO

Core 0 communiceert met Core 1 via de hardware FIFO (32-bit, één per richting):

```c
// ── Commando-codes (hoge byte = type, lage bytes = parameter) ──
#define CMD_DISPLAY_UPDATE  0x01000000  // Core 0 → Core 1: herteken scherm
#define CMD_BODE_START      0x10000000  // Core 0 → Core 1: start sweep (params in shared struct)
#define CMD_LOAD_WAVEFORM   0x20000000  // Core 0 → Core 1: laad golfvorm (lage 8 bit = struct-index)
#define CMD_EXPORT_DATA     0x30000000  // Core 0 → Core 1: zet exportbuffer klaar voor SD-schrijven
#define CMD_EMERGENCY_STOP  0xFF000000  // beide richtingen: noodstop

// ── Resultaten Core 1 → Core 0 ──
#define EVT_TOUCH           0x81000000  // lage 16 bit = x<<8 | y (ruwe coördinaat)
#define EVT_BODE_POINT      0x90000000  // lage 8 bit = buffer-index (geen pointer!)
#define EVT_TEMP_WARNING    0xA0000000  // lage 8 bit = temperatuur in °C
```

Golfvorm-parameters passen niet in 32 bit — gebruik een gedeelde struct + index:
```c
typedef struct {
    uint8_t  waveform_type;   // 0=DC 1=sine 2=square 3=triangle 4=sawtooth
    float    frequency_hz;
    float    amplitude_v;
    float    offset_v;
    float    duty_cycle;      // 0.0–1.0; alleen geldig voor square wave
} load_waveform_cmd_t;

volatile load_waveform_cmd_t load_cmd_buf[4];  // kleine ring, index 0–3
// Core 0 vult load_cmd_buf[idx], stuurt CMD_LOAD_WAVEFORM | idx via FIFO
```

Voor grotere data (Bode-punten, ADC-buffers) gebruik je een **vaste gedeelde ringbuffer met index** — geen pointers in de FIFO (RP2350-adressen passen niet betrouwbaar in 24 bit):

```c
// Gedeelde buffer — beide cores kunnen lezen, Core 1 schrijft
#define BODE_BUF_SIZE 256
volatile shared_bode_point_t bode_buf[BODE_BUF_SIZE];

// Core 1: schrijf naar buffer, stuur index via FIFO
uint8_t idx = next_write_idx();
bode_buf[idx].freq_hz   = current_freq;
bode_buf[idx].gain_db   = gain_db;
bode_buf[idx].phase_deg = phase;
__dmb();  // memory barrier: zorg dat schrijf zichtbaar is voor Core 0
multicore_fifo_push_blocking(EVT_BODE_POINT | idx);

// Core 0: ontvang index, lees buffer
uint32_t msg = multicore_fifo_pop_blocking();
if ((msg & 0xFF000000) == EVT_BODE_POINT) {
    uint8_t idx = msg & 0xFF;
    process_bode_point(&bode_buf[idx]);  // Core 1 schrijft nu niet naar idx
}
```

### Taakverdeling

```
Core 0 (MicroPython)                Core 1 (C — real-time)
────────────────────────────────    ──────────────────────────────────
UI state machine                    ADC DMA sampling
Menu / parameterinvoer              Goertzel berekening
Meting starten  ──FIFO──►          AD9102 aansturen (SPI0)
                ◄──FIFO──  Touch   Touch pollen (SPI0, 10 ms)
                ◄──FIFO──  Data    Display bijwerken (SPI0)
SD CSV export (PIO SPI)             Thermische bewaking + ventilator PWM
                ◄──FIFO──  Alarm   Noodstop doorsturen
```

---

## Debug Setup

- **SWD Interface:** Externe Pico debugger via GP20/GP21
- **Breakpoints:** Debug signaalverwerking, Goertzel, beschermingslogica
- **Printf over UART:** Real-time debug-uitvoer (GP0/GP1)
- **Build type:** Debug (−Og −g) als standaard in CMakeLists.txt
