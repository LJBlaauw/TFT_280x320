# Power Supply Characterization System
## Dual-Function: Bode Plot Analyzer + Electronic Load
### RP2350 Dual-Core: Core 0 (MicroPython) + Core 1 (C)

---

> **Status:** Ontwerpnotities — target architectuur, nog niet volledig geïmplementeerd  
> **Werkende code:** `TFT_display_touch_driver/` (ILI9341 + XPT2046 testbench)  
> **Niet aanwezig in repo:** AD9102 driver, Goertzel, elektronische belasting, MicroPython Core 0  
> **Target board:** Pico 2 W / RP2350  
> **Pinreferentie:** [`docs/PIN_ASSIGNMENT.md`](docs/PIN_ASSIGNMENT.md)

---

## HARDWARE ARCHITECTURE

### Microcontroller
- **RP2350** (dual Cortex-M33, no external oscillator needed)
  - Core 0: MicroPython control logic
  - Core 1: Real-time C (display, touch, ADC)

### Signal Generation
- **AD9102 DDS Waveform Generator**
  - Interface: SPI0 (CS GP11 — zie [docs/PIN_ASSIGNMENT.md](docs/PIN_ASSIGNMENT.md))
  - Internal Signal RAM (user-loaded waveforms)
  - Loop mode (repeating waveforms)
  - Functions: Bode plot sweep + Electronic load waveforms

### ADC Sampling (RP2350 internal)
- **ADC Channels @ 500 kSa/s (standaard max, ADC-klok 48 MHz ÷ 96 cycli)**
  - ADC0 (GPIO26): Voedingsspanning AC-gekoppeld
    - **Mode A — Ruismeting:** continue sampling, auto-scaling, V_pp + V_rms
    - **Mode B — Staprespons:** gesynchroniseerd met DAC-stap, transient capture
  - ADC1 (GPIO27): Spanning aan voedingszijde van 10Ω injectieweerstand (AC gekoppeld) — Bode plot
  - ADC2 (GPIO28): Analoge MUX-uitgang (softwarematig geschakeld)
    - **MUX kanaal 0:** Uitgangsspanning voeding (DC gekoppeld) — hoofd-meetkanaal
    - **MUX kanaal 1:** NTC 10K temperatuursensor koelblok (spanningsmiddelpunt 10K/10K naar 3V3/GND)
- **Noot:** 1 MSa/s is mogelijk maar vereist overklokken van de ADC-klok naar 96 MHz
- **DMA:** Continuous acquisition without CPU blocking

### Display (Fasizi 280x320)
- **Controller:** ILI9341 (SPI0, CS GP10, DC GP13, RST GP12 — zie [docs/PIN_ASSIGNMENT.md](docs/PIN_ASSIGNMENT.md))
- **Touch:** XPT2046 (SPI0 gedeeld, CS GP9, 2 MHz — zie [docs/PIN_ASSIGNMENT.md](docs/PIN_ASSIGNMENT.md))
- **Resolution:** 280 × 320 pixels
- **Functions:**
  1. **Bode Plot Mode:** Graph display + Critical frequency markers
  2. **Electronic Load Mode:** Real-time oscilloscope (continuous waveform)
  3. **Settings UI:** Parameter input (frequencies, amplitude, etc.)

### Dual-Core Task Division *(target architectuur — gepland)*

| Core | Taal | Taak | Status |
|------|------|------|--------|
| **Core 0** | MicroPython | High-level control, logica, dataverwerking, moduselectie | gepland |
| **Core 1** | Pure C | Real-time: TFT display, touch, ADC, AD9102 SPI | display+touch aanwezig; rest gepland |

> **Let op:** Huidig werkende code draait volledig op Core 1 (C testbench). Core 0 MicroPython is nog niet geïntegreerd.

---

## FUNCTIONAL MODES

### MODE 1: BODE PLOT (Loop Gain Characterization)
**Goal:** Meet lus-gain G(f) = Vout(f) / Vfeedback(f) over het frequentiebereik

#### Foutsignaalinjectie
Normaal wordt een transformator gebruikt om een klein AC-foutsignaal in de feedbacklus te injecteren.
Hier injecteert de **AD9102 DDS** via opto-isolator een kleine AC-spanning over de 10Ω weerstand die in serie in de feedbacklus zit.

#### Input Parameters (via Touch UI):
- Start Frequency (Hz)
- Stop Frequency (Hz)
- Frequency Points (e.g., 10, 50, 100 points)
- Oversampling Factor (e.g., 10x, 100x per cycle)
- Settling Time per Frequency (ms)

#### Measurement Process:
1. AD9102 genereert sinus; injectie via opto-isolator *over* de 10Ω weerstand in de feedbacklus
   - ADC1 = Vfeedback (voedingszijde van 10Ω)
   - ADC0 = Vout (voedingsuitgang)
2. Wait settling time
3. Core 1 acquireert ADC0 (Vout AC) + ADC1 (Vfeedback) via DMA
4. Core 1 past **Goertzel Algoritme** toe op beide kanalen
   - Magnitude op doelfrequentie
   - Fase op doelfrequentie
5. Bereken lus-gain: G = |Vout| / |Vfeedback|
6. Bereken fase: φ = ∠Vout - ∠Vfeedback
7. Store result (freq, G_dB, phase_deg)
8. Loop next frequency

#### Display Output:
- **Bode Plot:** Eén gecombineerd plotvenster, log/log indeling (X = log(frequentie),
  linker Y-as = magnitude in dB, rechter Y-as = fase in graden)
  - Magnitude-curve en fase-curve in dezelfde grafiek, elk in een eigen kleur
    (bv. cyaan = magnitude, geel = fase) zodat ze visueel te onderscheiden zijn
- **Cursor:** via touch over het plotvenster te verschuiven; op het snijpunt met de
  curves toont het display de bijbehorende frequentie, magnitude (dB) en fase (°)
  naast de berekende stabiliteitswaarden (kruisfrequentie, fasemarge)
- **Critical Points:** Automatically marked
  - G = 1 (0 dB) kruisfrequentie → bijbehorende fase (fasemarge)
  - G = 0.5 (-6 dB) → bijbehorende fase
- **Export:** CSV (frequency, gain_dB, phase_deg)

---

### MODE 2: ELECTRONIC LOAD (Waveform Generation + Monitoring)
**Goal:** Programmeerbare stroombelasting met continue bewaking en thermische beveiliging

#### Stroompad & Berekening
```
AD9102 (V_DAC) → OPA810 → gate IMZA40R036M2H → source → shunt 0.1Ω → GND
                                                 ↑ drain aangesloten op voeding

Stroomformule:  I_load (A) = V_DAC (V)    [1 V DAC = 1 A belasting]
Uitgelegd:      V_shunt = 0.1 × V_DAC
                I_load  = V_shunt / R_shunt = (0.1 × V_DAC) / 0.1Ω = V_DAC
```
**Stroom wordt NIET gemeten via ADC — afgeleid van DAC-setpoint.**

#### Thermische Beveiliging — Core 1 (IMZA40R036M2H, PG‑TO247‑4)
**Let op — realtime-grenzen:** zodra de AD9102 een golfvorm zelfstandig uit Signal RAM
afspeelt, heeft de firmware geen controle per DAC-sample. `P_sample` wordt daarom *vooraf*
per golfvorm als SOA-precheck doorgerekend (niet als live "elke sample"-ingreep tijdens
afspelen); de lopende bewaking tijdens bedrijf draait op het tempo van de protection-
controlelus. Zie IMPLEMENTATION_PLAN.md Stap 6.3 voor de drielagen-aanpak (SOA-precheck /
firmware-noodstop met gemeten reactietijd / optionele hardware-gate-disable).

- **P_sample** = V_DS × I_load[n] — per (berekend) sample over de golfvorm, gebruikt
  voor de SOA-precheck (P_max_inst) vóórdat een profiel start, Core 1
  - V_DS ≈ V_supply (V_shunt = 0.1 × V_DAC ≤ 0.1 V bij 1 A, verwaarloosbaar)
- **P_peak** = V_DS × I_DAC_peak — piek vermogen op basis van DAC-amplitude
- **P_avg** = P_peak × duty_factor — duty_factor = mean(I_load)/I_peak (niet RMS²/peak²):
  DC=1.0, Sinus=0.5 (unipolaire sinus), Blokgolf=user duty, Driehoek/Zaagtand=0.5 (ramp 0→I_peak)
- **NTC** op koelblok via ADC2 MUX kanaal 1 (gepoll elke ~100 ms)
- Drempelacties:
  | Conditie | Actie |
  |---|---|
  | T > T_warning (bijv. 65°C) | Gele waarschuwing op scherm |
  | T > T_limit (bijv. 80°C) | Amplitude reduceren naar safe_fraction |
  | SOA-precheck: golfvorm bevat punt(en) met P > P_max_inst | Start geweigerd (komt nooit tot uitvoer) |
  | P_inst > P_max_inst tijdens bedrijf (onverwacht) | Firmware-noodstop: DAC-uitgang → veilige toestand binnen gemeten/gedocumenteerde reactietijd van de controlelus (evt. + hardware-gate-disable) |
  | P_avg > P_max_avg | Amplitude reduceren |

#### Waveform Types:
1. **DC:** Constant voltage
2. **Sine Wave:** sin(t) + offset
3. **Square Wave:** ±amplitude + offset, with duty cycle control
4. **Triangle Wave:** linear ramp up/down + offset
5. **Sawtooth Wave:** linear ramp + offset

#### Input Parameters (via Touch UI):
- Waveform Type (selector)
- Frequency (Hz)
- Amplitude (V)
- Offset (V)
- Duty Cycle (0-100%, square wave only)

#### AD9102 Operation (Electronic Load):
1. Core 0 stuurt golfvorm-parameters via FIFO (CMD_LOAD_WAVEFORM | idx; load_waveform_cmd_t)
2. Core 1 genereert golfvorm-tabel (sinus, blok, driehoek, enz.) en laadt AD9102 Signal RAM via SPI0
3. Core 1 stelt AD9102 in op loop-modus (continue afspelen)
4. Frequentie gestuurd via AD9102-register (Core 1)

#### Display (Real-Time Scope):
- **Screen:** 280×320 pixels acting as oscilloscope
- **Channels Displayed:**
  - Ch1: ADC2/MUX0 — voedingsspanning Vout (DC, auto-scaling)
  - Ch2: I_calc — berekende belastingstroom van DAC-setpoint (niet gemeten)
- **Sub-weergave (optioneel):** ADC0 AC-spanning (ripple/ruis op voeding)
- **Update Rate:** ~10–30 fps (real-time)
- **Triggering:** Auto-trigger of manueel
- **Measurements:** V_rms, V_pp, V_gemiddeld; I_calc, P_inst, P_avg; T_koelblok

---

## GPIO PIN ASSIGNMENT (Flexible - Breadboard)

### SPI Buses
**SPI 0 (Display + AD9102):**
- CLK: GP18 (configurable)
- MOSI: GP19 (configurable)
- MISO: GP16 (configurable)
- Display CS: GP10
- AD9102 CS: GP11
- Display DC: GP13
- Display RST: GP12

**SPI 0 (Touch CS — gedeeld met display, eindarchitectuur):**
- Touch CS: GP9 (actief laag, 2 MHz — `spi_set_baudrate` vóór elke touch-read)
- Opmerking: standalone testbench gebruikt SPI1 voor touch (GP14/GP15/GP8); eindproject gebruikt gedeelde SPI0-bus. Zie [`docs/PIN_ASSIGNMENT.md`](docs/PIN_ASSIGNMENT.md).

### ADC Pins
- ADC0: GP26 (fixed) — Vout AC-gekoppeld (ruismeting / staprespons)
- ADC1: GP27 (fixed) — Vfeedback AC-gekoppeld (Bode plot)
- ADC2: GP28 (fixed) — MUX-uitgang: kanaal 0 = Vout DC, kanaal 1 = NTC

### Analoge MUX
- MUX_SEL: GPIO (TBD) — 0 = Vout DC, 1 = NTC koelblok

### Debug Port (SWD)
- SWCLK: GP20 (or other, configurable)
- SWDIO: GP21 (or other, configurable)

---

## SOFTWARE ARCHITECTURE

### File Structure

Legenda: `[exists]` = aanwezig in repo · `[planned]` = gepland · `[TBD]` = nog niet uitgewerkt

```
TFT_280x320/
├── TFT_display_touch_driver/           [exists]  — werkende testbench
│   ├── main_testbench.c                [exists]
│   ├── ili9341_display.h/c             [exists]
│   ├── xpt2046_touch.h/c              [exists]
│   ├── spi_interface.h/c              [exists]
│   ├── CMakeLists.txt                 [exists]
│   ├── pico_sdk_import.cmake          [exists]
│   └── README_TESTBENCH.md            [exists]
│
├── docs/
│   ├── PIN_ASSIGNMENT.md              [exists]  — enkelvoudige pinreferentie
│   ├── AD9102_PROTOCOL.md             [planned]
│   └── CALIBRATION.md                 [planned]
│
├── main.c                             [planned] — Core 0 MicroPython launcher
├── core0_micropython.py               [planned]
│   ├── bode_plot_controller.py        [planned]
│   ├── load_controller.py             [planned]
│   └── data_export.py                 [planned]
│
├── core1_entry.c                      [planned] — Core 1 entry point
├── core1_entry.h                      [planned]
│
├── drivers/
│   ├── ad9102_dds.h/c                 [planned] — AD9102 DDS via SPI
│   ├── adc_sampler.h/c                [planned] — ADC DMA, multi-kanaal
│   ├── adc_mux.h/c                    [planned] — ADC2 MUX: Vout ↔ NTC
│   └── spi_interface.h/c              [exists]  — in TFT_display_touch_driver/
│
├── signal_processing/
│   ├── goertzel.h/c                   [planned] — magnitude + fase
│   ├── waveform_gen.h/c               [planned] — sinus, blok, driehoek
│   ├── impedance_calc.h/c             [planned] — Z = ΔVout / I_calc
│   ├── noise_measure.h/c              [planned] — V_pp, V_rms (ADC0 mode A)
│   └── step_response.h/c              [planned] — staprespons (ADC0 mode B)
│
├── protection/
│   └── thermal_protection.h/c         [planned] — NTC + vermogensbewaking
│
├── ui/
│   ├── ui_manager.h/c                 [planned]
│   ├── bode_plot_ui.h/c               [planned]
│   ├── load_ui.h/c                    [planned]
│   └── noise_ui.h/c                   [planned]
│
├── CMakeLists.txt                     [placeholder] — niet buildbaar vanuit root; zie TFT_display_touch_driver/ voor werkende build
└── CMakeLists_mpy.txt                 [TBD]     — MicroPython build
```

### Core Communication (Core 0 ↔ Core 1)
- **Queue:** Fifo for messages (parameter updates, requests)
- **Shared Memory:** Double-buffered ADC sample buffer
- **Ownership protocol + memory barrier:** gedeelde SRAM-buffers: Core 1 schrijft + `__dmb()` + FIFO-index; Core 0 leest na ontvangst index — geen gelijktijdig schrijven naar dezelfde entry

#### Message Types (zie PROJECT_SPEC.md voor volledige commandodefinities):
- `CMD_BODE_START` — start sweep (parameters in gedeelde struct, index via FIFO)
- `CMD_LOAD_WAVEFORM | idx` — laad golfvorm (load_waveform_cmd_t buf[idx], zie PROJECT_SPEC.md)
- `CMD_EMERGENCY_STOP` — noodstop (beide richtingen)
- `CMD_EXPORT_DATA` — Core 0 vraagt Core 1 om exportbuffer klaar te zetten voor SD-schrijven

---

## CORE 1 REAL-TIME TASKS (C Code)

### Task 1: ADC Sampling + Goertzel (DMA + Interrupt)
```
while(1):
    - Configure DMA for active measurement channels:
        Bode-modus:  ADC0 (Vout AC) + ADC1 (Vfeedback AC) @ 250 kSa/s per kanaal (500 kSa/s totaal standaard)
        Load/scope:  ADC2/MUX0 (Vout DC) @ 500 kSa/s
        Ruis/stap:   ADC0 (Vout AC) @ 500 kSa/s
    - Fill dual buffer (Buffer A, Buffer B)
    - Trigger interrupt when buffer full → switch buffer
    - In Bode-modus: apply Goertzel on both channels → stuur EVT_BODE_POINT + index via FIFO
    - Anders: notify Core 0 "new samples ready" + buffer index
```

### Task 2: Display Update (30 fps)
```
every 33ms:
    - Read buffer from Core 0
    - For Bode mode: render graph at current point
    - For Load mode: render waveform as scope
    - Call tft_render_frame()
```

### Task 3: Touch Polling (100 Hz)
```
every 10ms:
    - Read XPT2046
    - Detect button press/release
    - Call ui_handle_touch(x, y)
    - Queue message to Core 0 if needed
```

### Task 4: AD9102 SPI Control
```
On Core 0 request:
    - Load waveform to AD9102 RAM
    - Set frequency register
    - Enable/disable output
```

---

## CORE 0 HIGH-LEVEL LOGIC (MicroPython)

### Bode Plot Workflow
```
1. UI: User sets freq start/stop, points, oversample
2. Loop for each frequency:
   a. Request Core 1: "Set AD9102 to freq_f" (foutsignaal injectie)
   b. Wait settling time
   c. Request Core 1 via FIFO: "Sample ADC0+ADC1 at freq_f, apply Goertzel"
   d. Receive processed Bode point from Core 1 (EVT_BODE_POINT + buffer index)
   e. Read bode_buf[idx]: freq_hz, gain_db, phase_deg  (Goertzel + G/φ al door Core 1 berekend)
   f. Store (freq, gain_db, phase_deg)
   g. Plot point on display
3. After all frequencies:
   - Render complete Bode plot
   - Auto-detect: G=1 kruisfrequentie + fase, G=0.5 + fase
   - Display on TFT
   - Offer export to CSV
```

### Electronic Load Workflow
```
1. UI: Gebruiker kiest golfvorm, stelt parameters in
2. Beveiligingscheck vóór start:
   - I_max = V_DAC_max / 1   (1V = 1A)
   - P_max_check = V_supply_est × I_max × duty_cycle  ≤  P_max_avg?
   - Indien niet: beperk amplitude of weiger start
3. Stuur golfvorm-parameters naar Core 1 via FIFO (CMD_LOAD_WAVEFORM | idx)
4. Core 1 genereert tabel en laadt AD9102 RAM via SPI0:
   - Sinus:    sin(2π×k/N) × amplitude + offset
   - Blok:     ±amplitude × duty_cycle + offset
   - Driehoek: lineaire ramp + offset
5. Bewakingslus (continu):
   a. Lees ADC2/MUX0 → V_supply (DC)
   b. I_calc = V_DAC_setpoint  (1V = 1A; V_shunt = 0.1 × V_DAC)
   c. P_inst = V_DS × I_calc  (V_DS ≈ V_supply)
   d. P_avg  = P_inst × duty_factor  (DC=1.0, sine=0.5, square=duty, triangle=0.5)
   e. Poll NTC (elke 100 ms via MUX1) → T_koelblok
   f. Controleer thermal_protection_check()
      - T > T_warning → gele waarschuwing scherm
      - T > T_limit   → reduceer amplitude + rode melding
      - P > P_max     → noodstop (DAC = 0)
   g. Update scope-weergave (V_supply + I_calc golfvorm)
   h. Check touch input voor parameterwijzigingen
```

---

## SIGNAL PROCESSING: GOERTZEL ALGORITHM

### Why Goertzel (not FFT)?
- **Efficiency:** Only compute at target frequency (1 bin, not N bins)
- **Accuracy:** High resolution at single frequency
- **Speed:** O(N) vs O(N log N) for this use case
- **Memory:** O(1) vs O(N) for FFT

### Implementation
```c
goertzel_t *goertzel = goertzel_init(sample_rate, target_freq, num_samples);
for(int i = 0; i < num_samples; i++) {
    goertzel_process(goertzel, sample[i]);
}
float magnitude = goertzel_magnitude(goertzel);
float phase = goertzel_phase_deg(goertzel);
```

---

## DISPLAY RENDERING

### Bode Plot Screen
Magnitude (dB, cyaan) en fase (°, geel) in **één** log/log plotvenster
(X-as = log(frequentie), linker Y-as = dB, rechter Y-as = graden).
Een touch-cursor (verticale lijn) is over het venster te verschuiven; op het
snijpunt met de curves toont de infobalk frequentie, magnitude en fase, naast
de berekende stabiliteitswaarden (kruisfrequentie / fasemarge).
```
┌─────────────────────────────────┐
│ BODE PLOT: Z(f)  10Hz → 100kHz   │  (title + status)
├─────────────────────────────────┤
│dB  ┌───────────┊──────────┐ deg │
│ 20 │●●          ┊          │ 90 │
│    │   ●●●      ┊●●●       │    │  ── magnitude (cyaan, ●)
│  0 │      ●●●●●●┊   ●●●    │  0 │  ┄┄ fase (geel, ┄)
│    │ ┄┄┄┄┄┄┄┄┄┄┄┊┄┄┄┄┄┄●●●●│    │
│-20 │            ┊           │-90 │
│    └───┬────┬───┊──┬────┬──┘    │
│       10Hz 100Hz┊ 1k  10k 100k  │
│                 ┊                │
│                 └─ cursor (touch-drag, verschuifbaar over X-as)
├─────────────────────────────────┤
│ cursor: f=1.0kHz  G=-3dB  φ=-91° │  (waarden op cursor-snijpunt)
│ f_critical=1.4kHz  PM=46° ✓      │  (berekende stabiliteitswaarden)
│ [START] [SETTINGS] [EXPORT]      │  (buttons)
└─────────────────────────────────┘
```

### Electronic Load Screen (Scope)
```
┌─────────────────────────────────┐
│  ELECTRONIC LOAD  [SIN 1kHz]     │  (golfvorm + freq)
├─────────────────────────────────┤
│ Vout ┌──────────────────────┐    │
│  10V │    /\    /\    /\    │    │  (ADC2/MUX0 — gemeten)
│      │   /  \  /  \  /  \   │    │
│   0V │__/____\/____\/____\__│    │
│      └──────────────────────┘    │
│ I_set┌──────────────────────┐    │
│   2A │    /\    /\    /\    │    │  (berekend v. DAC-setpoint, niet gemeten)
│   0A │__/____\/____\/____\__│    │
│      └──────────────────────┘    │
├─────────────────────────────────┤
│ Vout: RMS=7.07V  Peak=10V        │  (metingen)
│ Iset: RMS=1.41A  Peak=2A         │
│ P:    10W avg    T: 42°C ✓       │  (vermogen + temp NTC)
├─────────────────────────────────┤
│ [WAVEFORM] [FREQ] [AMPLITUDE]    │  (knoppen)
│ [OFFSET]   [DUTY] [START/STOP]   │
└─────────────────────────────────┘
```

---

## CLOCK & TIMING

### ADC @ 500 kSa/s (standaard)
- **ADC Clock:** 48 MHz ÷ 96 cycli/conversie
- **Sample Rate:** 500,000 samples/sec totaal (round-robin over actieve kanalen)
- **Per kanaal (ADC0 + ADC1, Bode-modus):** 250 kSa/s → Nyquist tot 125 kHz ✓
- **Goertzel Δt correctie:** Δt = 1 / 500 kSa/s = 2 µs (tijdsverschuiving ADC0 → ADC1 in round-robin)
- **Buffer Size:** Dual 8 kB buffers (8000 samples totaal = 4000 per kanaal bij round-robin)
- **Buffer Refill Time:** 8000 / 500 000 = 16 ms per buffer
- **Optioneel (overklokt):** 1 MSa/s bij ADC-klok 96 MHz → Δt = 1 µs, Nyquist 250 kHz/kanaal
- **MUX settling (ADC2):** na MUX-schakeling 5–10 samples weggooien (analoge settling ~1–5 µs)

### Bode Plot Acquisition
- **Settling Time:** 100-500 ms per frequency (user-configurable)
- **Sample Window:** ~100 cycles at target freq (e.g., 10 cycles = 100 µs @ 1 MHz)
- **Total Time for 50 points (1Hz→10kHz):** ~5-30 minutes

### Display Refresh
- **Bode Mode:** Update plot every new point (~1-5 sec)
- **Load Mode:** Continuous scope (30 fps)

---

## CALIBRATION & SCALING

### ADC Raw → Voltage
```
ADC0 (Vout AC):       V_ac        = (ADC_raw / 4095) × Vref × ac_gain_factor
ADC1 (Vfeedback AC):  Vfeedback_V = (ADC_raw / 4095) × Vref × fb_gain_factor
ADC2/MUX0 (Vout DC):  Vout_V      = (ADC_raw / 4095) × Vref × voltage_divider_factor
ADC2/MUX1 (NTC):      T_C         = beta / ln(R_ntc/R_ref × e^(beta/T_ref)) - 273.15
                                     R_ntc = R_ref × (Vref/V_ntc - 1)  [bovendeler 10K naar 3V3]
                                     R_ref = 10kΩ, beta = 3950 K, T_ref = 298.15 K
```

### Stroom & Impedantie
```
I_load (A) = V_DAC_setpoint (V)          [1 V = 1 A, kalibratievrij door opamp-circuit]
Z_out  (Ω) = ΔV_supply / I_load          [uitgangsimpedantie; ΔV = Goertzel-amplitude (sinuslast) of DC-dip (load-step)]
```

*Schalingsfactoren ADC: TBD vanuit hardware-ontwerp*

### Goertzel Phase Correction
- Phase offset between ADC channels (cross-coupling) may need correction
- Calibration sweep at known frequency to establish offset

---

## BUILD & DEBUG

### Huidige werkende build — testbench (TFT_display_touch_driver/)

**Vereiste:** Pico VS Code extension (installeert SDK, toolchain, cmake, ninja onder `~/.pico-sdk/`)  
Of handmatig: zie [README_TESTBENCH.md](TFT_display_touch_driver/README_TESTBENCH.md)

```bash
export PATH="$HOME/.pico-sdk/cmake/v3.31.5/bin:$HOME/.pico-sdk/ninja/v1.12.1:$HOME/.pico-sdk/toolchain/14_2_Rel1/bin:$PATH"

cd TFT_display_touch_driver
mkdir -p build && cd build

# Configureren — Ninja, debug build, Pico 2 W / RP2350
cmake -G Ninja -DPICO_BOARD=pico2_w ..

# Bouwen
ninja

# Output:
#   build/testbench.uf2   → via USB flashen (sleep naar Pico schijf in bootloader-modus)
#   build/testbench.elf   → via debug probe laden (SWD / OpenOCD)
```

### Flashen naar Pico 2 W
1. Houd BOOT ingedrukt, druk op RESET → Pico verschijnt als USB-schijf
2. Sleep `testbench.uf2` naar de schijf

### Debug via debug probe (SWD)
- Verbind SWCLK (GP20) en SWDIO (GP21) met externe Pico debugger
- Gebruik VS Code "Pico Debug (Cortex-Debug)" launch configuratie (`.vscode/launch.json`)
- `testbench.elf` wordt automatisch geladen via OpenOCD

### Debug output
- **UART:** `stdio` op UART0 (GP0/GP1) — `picocom -b 115200 /dev/ttyACM0`

---

## NEXT STEPS

1. ~~**Finalize GPIO pin assignment**~~ — gedaan, zie [docs/PIN_ASSIGNMENT.md](docs/PIN_ASSIGNMENT.md)
2. ~~**Choose SPI clock speed**~~ — gedaan, zie device snelheidstabel in PROJECT_SPEC.md
3. ~~**Build ILI9341 + XPT2046 drivers**~~ — gedaan, TFT_display_touch_driver/ eerder gebouwd en geverifieerd
4. **Define ADC calibration process** (referentiemetingen, gain/offset per kanaal)
5. **Create waveform lookup tables** (sinus, blokgolf, driehoek — voor AD9102 RAM)
6. **Implement Goertzel in C** (signal_processing/goertzel.h/c)
7. **Implement AD9102 SPI driver** (drivers/ad9102_dds.h/c)
8. **Integrate MicroPython on Core 0** (core0_micropython.py)
9. **Implement thermal protection + fan PWM** (protection/thermal_protection.h/c)
