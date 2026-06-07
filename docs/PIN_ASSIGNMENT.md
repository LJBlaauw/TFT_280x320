# PIN_ASSIGNMENT.md — Centrale Pinreferentie
## Power Supply Stability Tester — RP2350 / Pico 2 W

> **Enkelvoudige bron van waarheid.** Alle andere documenten en code verwijzen hiernaar.  
> Pinnen in `main_testbench.c` (regels 22–41) zijn de implementatie; dit document beschrijft de intentie.

## Boardstrategie

| Fase | Board | GPIO beschikbaar | Opmerking |
|------|-------|-----------------|-----------|
| Prototyping | RP2350W DIP-40 | 26 user GPIO | Huidige pin-nummers (GP8–GP28) werken direct |
| Eindontwerp | Seeed XIAO RP2350 (eigen KiCad footprint) | 19 GPIO + SWD pads | Expand pads + debug pads beschikbaar; pinremapping nodig |

> SWD op XIAO RP2350 zit op eigen debug-pads — **geen GPIO nodig** voor debugger.

---

## SPI-busindeling

| Bus | Eigenaar | Apparaten | Snelheid |
|-----|----------|-----------|----------|
| SPI0 (HW) | **Core 1 (C)** | Display + Touch + AD9102 | 10 MHz (display/AD9102), 2 MHz (touch) |
| PIO SPI | **Core 0 (MicroPython)** | SD kaart | 25 MHz |
| SPI1 (HW) | — | **vrij voor uitbreiding** | — |

> **CS is altijd handmatig.** Hardware SS-pin deassert na elke byte — onbruikbaar voor bursts.  
> Nooit twee CS tegelijk laag. Snelheid wisselen met `spi_set_baudrate()` vóór elke CS-assert.

---

## SPI0 — Display + Touch + AD9102 *(Core 1, gedeelde bus)*

| GPIO | Functie | Richting | Snelheid |
|------|---------|----------|----------|
| GP18 | SPI0 CLK (gedeeld) | uit | — |
| GP19 | SPI0 MOSI (gedeeld) | uit | — |
| GP16 | SPI0 MISO (gedeeld) | in | — |
| GP10 | Display CS (actief laag) | uit | 10 MHz |
| GP13 | Display DC | uit | — |
| GP12 | Display RST | uit | — |
| GP11 | AD9102 CS (actief laag) | uit | 10 MHz |
| GP9  | Touch CS (actief laag) | uit | 2 MHz |
| IRQ  | **niet aangesloten** (GP_UNUSED sentinel) | — | — |

## PIO SPI — SD kaart *(Core 0, elke vrije GPIO)*

| GPIO | Functie | Richting | Snelheid |
|------|---------|----------|----------|
| TBD  | PIO CLK | uit | 25 MHz |
| TBD  | PIO MOSI | uit | — |
| TBD  | PIO MISO | in | — |
| TBD  | SD CS (actief laag) | uit | — |

## SPI1 — Vrij voor uitbreiding (hardware beschikbaar)

## SPI0 uitbreiding — AD9102 DDS *(gepland)*

| GPIO | Functie | Opmerking |
|------|---------|-----------|
| GP11 | AD9102 CS | deelt SPI0-bus met display |
| TBD  | AD9102 FSYNC / RESET | nog niet bepaald |

## ADC-kanalen

| GPIO | ADC ch | Signaal | Koppeling | Status |
|------|--------|---------|-----------|--------|
| GP26 | ADC0 | Voedingsspanning | AC | gepland — ruismeting / staprespons |
| GP27 | ADC1 | Vfeedback (voedingszijde 10Ω injectieweerstand) | AC | gepland — Bode plot |
| GP28 | ADC2 | MUX-uitgang | — | gepland |

### ADC2 MUX-kanalen

| MUX ch | Signaal | Koppeling |
|--------|---------|-----------|
| 0 | Voedingsspanning (Vout) | DC |
| 1 | NTC koelblok (10K/10K spanningsmiddelpunt) | DC |

| GPIO | Functie |
|------|---------|
| TBD  | MUX_SEL (0 = Vout, 1 = NTC) |
| TBD  | FAN_PWM — hardware PWM naar ventilator (25 kHz aanbevolen) |

## Elektronische Belasting *(gepland)*

| Component | Verbinding | Opmerking |
|-----------|-----------|-----------|
| AD9102 uit | OPA810 in+ | V_DAC → opamp |
| OPA810 uit | IMZA40R036M2H gate | SiC FET aansturing |
| FET drain | voedingsuitgang | belastingsstroom |
| FET source | shunt 0.1Ω | stroom naar GND |
| shunt laag | GND | — |

**Stroomrelatie:** `I_load (A) = V_DAC (V)` — 1 V = 1 A

## Bode Plot Injectie *(gepland)*

| Component | Verbinding |
|-----------|-----------|
| AD9102 uit | HCNR201 opto-in |
| HCNR201 opto-uit | 10Ω knooppunt feedbacklus |
| MMBT3906 / MMBT3904 | driver-transistoren voor opto |

## Debug-poort — SWD

| Board | Interface | GPIO nodig |
|-------|-----------|-----------|
| RP2350W DIP-40 (prototype) | Externe Pico debugger via GP20/GP21 | GP20 = SWCLK, GP21 = SWDIO |
| XIAO RP2350 (eindontwerp) | Ingebouwde SWD debug-pads op footprint | Geen — eigen pads |

## NTC Temperatuursensor *(gepland)*

| Component | Waarde | Verbinding |
|-----------|--------|-----------|
| NTC | 10 kΩ, β ≈ 3950 K | tussen GND en ADC2/MUX1 |
| Pull-up | 10 kΩ | tussen 3V3 en ADC2/MUX1 |

---

## Snel overzicht — Prototype RP2350W DIP-40

```
SPI0 — Core 1 (display + touch + AD9102)    PIO SPI — Core 0 (SD kaart)
────────────────────────────────────────    ────────────────────────────
GP16 → MISO (gedeeld)                       TBD → PIO CLK
GP18 → CLK  (gedeeld)                       TBD → PIO MOSI
GP19 → MOSI (gedeeld)                       TBD → PIO MISO
GP10 → Display CS   (10 MHz)                TBD → SD CS (25 MHz)
GP11 → AD9102 CS    (10 MHz)
GP12 → Display RST                          SPI1 — vrij voor uitbreiding
GP13 → Display DC                           ────────────────────────────
GP9  → Touch CS     ( 2 MHz)                (hardware beschikbaar)

ADC / bewaking                              Debug (prototype)
────────────────────────────────────────    ────────────────────────────
GP26 → ADC0 (Vout AC)                       GP20 → SWCLK → debugger Pico
GP27 → ADC1 (Vfeedback AC)                  GP21 → SWDIO → debugger Pico
GP28 → ADC2 (MUX Vout/NTC)
TBD  → MUX_SEL
TBD  → FAN_PWM
```

> **GPIO-telling XIAO RP2350:** 17 gebruikt, 2 vrij, SPI1 hardware beschikbaar.  
> **XIAO eindontwerp:** SWD op eigen debug-pads — GP20/GP21 vrij.
