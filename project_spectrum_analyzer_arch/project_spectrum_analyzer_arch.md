---
name: project-spectrum-analyzer-arch
description: "Architecture decisions for SMPS ripple spectrum measurement extension (Tayloe QSD, Band A/B design, component choices)"
metadata: 
  node_type: memory
  type: project
  originSessionId: 29984f3a-87fd-4a32-b48a-25008f6c618d
---

## Hardwarestrategie

Prototyping: **RP2350W 40-pin DIP** (Pico 2 W) — 26 user GPIO, ruim voldoende voor alle signalen inclusief Tayloe-uitbreiding. Geen pincompromissen tijdens ontwikkeling.
Eindontwerp: keuze tussen Seeed XIAO RP2350 (compact, beperkte GPIO) of losse RP2350A/B chip (maximale GPIO, volledig C) — wordt bepaald nadat prototyping is afgerond.

## Meetbereik

Besloten om de spectrumanalyser-uitbreiding te beperken tot twee banden:

**Band A (10 kHz – 250 kHz):** directe ADC-bemonstering + Goertzel, geen externe menger.
**Band B (250 kHz – 10 MHz):** Tayloe QSD met 74HC4052 of TS3A5017 aangestuurd door PIO (4-fase volgorde).

**Why:** Band C (10–30 MHz, dubbele conversie met passieve dioderingmenger + vaste PIO-LO van 20/30 MHz) is bewust weggelaten om het PCB-ontwerp simpel te houden en geen sub-band-filtering/switching nodig te hebben.

**How to apply:** Bij toekomstige discussies over meetbereik, schakeltopologie of componentkeuze: het systeem is bewust beperkt tot 10 MHz bovengrens. Geen Band C implementeren tenzij gebruiker dit expliciet heroverweegt.

## Componentkeuze Band B

- **Switch-IC:** 74HC4052 of TS3A5017 (dual 4-kanaals mux, gedeelde A/B adreslijnen → 2 GPIO + 1 enable = 3 GPIO totaal voor dubbele menger)
- **Hold-condensator:** 10 nF, NP0, 0.5% tolerantie (serie E12/E24, goed beschikbaar in 0805/1206)
- **Series-weerstand:** 1 kΩ (domineert over R_on van 74HC4052, maakt RC-tijdconstante voorspelbaar)
- **IF-bandbreedte:** ~16 kHz (1/(2π × 1kΩ × 10nF))
- **Overgangsfrequentie A→B:** 250 kHz (aansluitend op ADC Nyquist bij 500 kSa/s enkelvoudig kanaal)

## LO-generatie Band B

PIO genereert 4-fase schakelpatroon (4 SM-instructies per LO-cyclus):
- f_LO = sys_clk / (div × 4), sys_clk = 150 MHz
- Bereik: 250 kHz tot ~10 MHz (div: 37.5 tot ~3.75)
- Frequentieresolutie bij 10 MHz: Δf ≈ N×f²/(256×sys_clk) = 4×(10e6)²/(256×150e6) ≈ 10 kHz

## I/Q architectuur

- 74HC4052 dual-sectie: sectie X → Vout (normaal), sectie Y → Vfeedback (180° verschoven via inverterende opamp)
- 2 opamps als ingangsbuffers: één inverterende + één niet-inverterende (amplitude-matching kritisch)
- Gedeelde A/B adreslijnen sturen beide secties synchroon aan
- Enable/inhibit lijn ontkoppelt het mengercircuit wanneer niet in gebruik
- Resterende fase/amplitude-mismatch: digitaal kalibreerbaar (zelfde aanpak als ADC-kanaalskalibratie)

## Meetkapabiliteit

- Spectrale karakterisering van SMPS-schakelpieken en harmonischen tot 10 MHz
- Flanken tot ~50 ns karakteriseerbaar via spectraal harmonisch bereik (BW ≈ 0.35/τ_flank ≈ 7 MHz)
- Tijddomeinreconstructie boven de ADC-samplerate: zie sectie hieronder

## Tijddomeinreconstructie via coherente swept vectormeting

Doel: exacte golfvormreconstructie van periodieke SMPS-signalen ver boven de ADC-samplerate (1 MSa/s), door inverse DFT over gemeten harmonischen met fase.

### Methode

1. **Meet f_sw nauwkeurig** via Band A Goertzel (fundamentele schakelfrequentie)
2. **Sweep LO coherent**: PIO stelt LO in op exact n × f_sw voor n = 1 … N_max (max harmonische binnen Band B)
3. **Meet I_n en Q_n** per stap via Tayloe-uitgang (ADC0/ADC1)
4. **Extraheer per harmonische**: A_n = √(I_n² + Q_n²), φ_n = atan2(Q_n / I_n)
5. **Reconstrueer tijddomein** (puur rekenkundig, willekeurige tijdresolutie):

```
x(t) = Σ A_n × cos(n × 2π × f_sw × t + φ_n),  n = 1 … N_max
```

### Tijdresolutie reconstructie

```
Δt = 1 / (2 × f_max) = 1 / (2 × 10 MHz) = 50 ns
```

Voorbeeld 500 kHz zaagtand, τ_flank = 100 ns: flank is 2× de resolutie → goed reconstrueerbaar.
Bijzonderheid: bij τ_flank = 100 ns valt de 20e harmonische (10 MHz) precies op de eerste nulstelling
van de sinc-envelop → Band B-grens is optimaal afgestemd op dit signaal.

### Sweeptijd

```
20 harmonischen × ~100 µs settling (5 × RC = 5 × 10 µs) ≈ 2 ms totale sweeptijd
```

### Vereisten voor correcte reconstructie

| Vereiste | Gevolg bij niet-voldoen |
|---|---|
| f_sw stabiel tijdens sweep | Fasedrift tussen stappen → golfvormvervorming |
| PIO-LO = exact n × f_sw | Systematische fasefouten per harmonische |
| Goertzel-venster = geheel aantal LO-perioden | Spectrale lekstraling → fasefouten |
| I/Q amplitude- en fase-matching | Vervorming reconstructie — kalibreerbaar |
| SMPS in steady-state | Niet-stationair signaal → incoherente meting |

### Wat gaat verloren

- **DC-component**: AC-koppeling ADC0 → gemiddelde/offset niet reconstrueerbaar
- **Fase-coherentie**: vereist stabiele f_sw; vrij-lopend signaal zonder sync werkt niet
- **Harmonischen boven Band B**: afgekapt bij 10 MHz; voor signalen met τ_flank > 35 ns is dit acceptabel (BW_flank < 10 MHz)

### Implementatie-aanpak (firmware)

- Geen extra hardware nodig t.o.v. de geplande Band A/B architectuur
- Coherentie is puur firmware: PIO-LO als exact veelvoud van gemeten f_sw instellen
- Reconstructie-berekening op Core 0 (of MicroPython/C post-processing)
