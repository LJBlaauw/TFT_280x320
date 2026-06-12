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

## ETS via PIO-gestuurde sample-and-hold (aanvullende methode)

Aanvullend op de coherente swept-vectormeting: directe tijddomein-bemonstering
van Vout via een PIO-gestuurde sample-and-hold, gesynchroniseerd op een
schakelflank van de SMPS zelf. Vereist extra analoge front-end (switch +
hold-cap + buffer + comparator), in tegenstelling tot de hierboven beschreven
reconstructiemethode die geen extra hardware nodig heeft. Beide methoden zijn
complementair: ETS levert een directe tijddomein-waveform inclusief
DC-component, de swept-vectormethode levert het spectrum zonder extra
hardware.

### Principe

- PIO houdt een laagohmige analoge switch gesloten ("track") totdat een
  afgeteld interval na de trigger verstreken is, en opent hem dan ("hold")
  op de hold-condensator.
- OPA810 (FET-ingang, lage I_bias) buffert de hold-condensator naar de
  ADC-ingang.
- Trigger: high-speed comparator met hysterese op de schakelspike van de
  SMPS-uitgang (zie "Triggerbron" hieronder).
- Door per triggercyclus het afteltijdstip met een vaste stap Δt te
  verhogen, wordt de waveform punt-voor-punt opgebouwd over meerdere
  periodes (equivalent-time sampling).

### Componentkeuze

| Component | Keuze | Reden |
|---|---|---|
| Switch (sample) | ADG1413 (quad SPST, R_on ≈ 0,5–1 Ω, t_sw ≈ enkele ns) | Lage en stabiele R_on, snel genoeg voor ns-schakeling; extra kanalen in hetzelfde pakket bruikbaar als dummy (zie compensatie) |
| Switch (alternatief) | ADG333A (quad SPDT) | SPDT-sectie geeft van nature complementaire aansturing van twee throws — interessant als alternatieve dummy-implementatie, maar hogere R_on (~35–100 Ω), minder geschikt als hoofdswitch |
| Hold-condensator | 1 nF | Vergroot t.o.v. eerdere 47 pF-schatting: verlaagt relatieve impact van charge-injection én droop (zie hieronder) |
| Buffer | OPA810 | FET-ingang (lage I_bias → lage droop-bijdrage), >700 MHz bandbreedte → settling geen knelpunt |
| Comparator | high-speed, hysterese instelbaar | Trigger op SMPS-schakelspike |

### Tijdconstante en settling

```
τ = R_on × C_hold = 0,5 Ω × 1 nF = 0,5 ns
t_settle(12-bit) ≈ 8,3 × τ ≈ 4,2 ns
```

Ruim binnen één PIO-cyclus (6,67 ns @ 150 MHz sys_clk) — geen knelpunt.

### Triggerbron: schakelspike i.p.v. ripple-drempel

Een SMPS geeft op de uitgang bij elke schakelovergang een korte spike, met
een veel steilere flank dan de ripple zelf. Dit is een beter triggerpunt dan
een spanningsdrempel op de (relatief vlakke) ripple: de steile flank geeft
een veel kleinere tijd-jitter voor dezelfde spanningsruis op de
comparatoringang (Δt = V_noise / slope, en slope is hier ordes groter dan bij
de ripple-top).

### Holdoff

Het PIO-blok negeert nieuwe trigger-edges gedurende een ingestelde
holdoff-tijd (bijv. 0,8 × T_schakelperiode) na een geaccepteerde trigger.
Voorkomt hertriggeren op ruis of secundaire spikes binnen dezelfde periode.

### Charge-injection compensatie

**Probleem:** bij het openen van de switch koppelt de stuurflank via interne
overlap-capaciteiten van de switch een ladingspakket Q_inj naar de hold-cap:
ΔV = Q_inj / C_hold. Bij Q_inj ≈ 1 pC en C_hold = 1 nF is ΔV ≈ 1 mV
(≈ 4 LSB bij 12-bit/1 V FS) — al een stuk beter dan bij 47 pF (≈ 21 mV,
≈ 85 LSB), maar nog significant.

**Compensatietechniek — "dummy switch":**

Een tweede, ongebruikt kanaal van **hetzelfde ADG1413-pakket** dient als
compensatie-element:
- Beide pinnen (in/uit) van het dummy-kanaal worden samen verbonden met de
  hold-cap-node (geen signaalpad — alleen de parasitaire koppelcapaciteit
  telt).
- Het dummy-kanaal wordt aangestuurd met het **geïnverteerde** stuursignaal
  van het hoofdkanaal.
- Doordat beide kanalen op dezelfde die zitten, zijn de
  stuur-naar-kanaal-koppelcapaciteiten goed gematcht. De tegengestelde
  stuurflank van het dummy-kanaal injecteert een ladingspakket met
  tegengesteld teken op dezelfde node → grotendeels annulering van Q_inj.

**Implementatie met PIO:**

Twee PIO-uitgangspinnen worden in **één instructie** gezet
(`set(pins, waarde)` of `out(pins, waarde)` met een 2-bits patroon). Beide
pinnen wisselen dan op exact dezelfde sys_clk-flank, zonder programmeerbare
vertraging tussen de twee stuursignalen — dit is de aanbevolen aanpak.

Twee praktische aandachtspunten:
- **Polariteit**: met een 2-bits PIO-patroon `{hoofd, dummy}` = `{1,0}` /
  `{0,1}` krijg je per definitie complementaire aansturing — exact wat
  nodig is voor de ADG1413 (actief-hoog enable).
- **Amplitude-matching**: het dummy-kanaal hangt met *beide* pinnen aan de
  hold-node (i.p.v. één pin zoals het hoofdkanaal), en koppelt daardoor
  nominaal ~2× zoveel lading voor dezelfde stuurflank. Compenseer dit met
  een kleine serie-capaciteit (orde honderden fF, te trimmen op prototype)
  tussen het dummy-kanaal en de hold-node, zodat de effectieve
  koppelcapaciteit overeenkomt met die van het hoofdkanaal.

**Restfout:** de resterende (na compensatie) charge-injection is grotendeels
**vast en signaal-onafhankelijk** (gekoppeld via de stuurflank, niet via
V_in), en dus kalibreerbaar als offset — in tegenstelling tot de
ongecompenseerde 21 mV/85 LSB fout bij 47 pF, die door de grotere relatieve
omvang eerder signaalafhankelijke vervorming geeft.

### Droop tijdens ADC-conversie

```
droop ≈ I_leak / C_hold × t_hold
```

Met C_hold = 1 nF (i.p.v. 47 pF) is de droop-rate ~21× lager dan bij de
oorspronkelijke schatting. Bij I_leak in de orde van 1 nA
(switch-lekstroom + OPA810 I_bias ≈ 3 pA, dus switch-gedomineerd) en
t_hold ≈ 2 µs (RP2350-ADC-conversietijd):

```
droop ≈ 1 nA / 1 nF × 2 µs ≈ 2 mV ≈ 8 LSB (bij 1 V FS, 12-bit)
```

Net als de charge-injection-restfout is dit een **vaste, deterministische
fout** (hold-tijd is PIO-gestuurd en dus constant) → kalibreerbaar als
offset. Te verifiëren op het prototype.

### Tijdresolutie

```
Δt_PIO = 1 / sys_clk = 1 / 150 MHz ≈ 6,67 ns
f_equiv ≈ 1 / Δt_PIO ≈ 150 MSa/s  →  Nyquist ≈ 75 MHz
```

Ruim boven de 10 MHz Band B-grens — de tijdresolutie van deze methode is dus
geen beperkende factor; de comparator-jitter op de triggerflank en de
hierboven genoemde amplitudefouten zijn dat wel.

### Randvoorwaarden

| Voorwaarde | Gevolg bij niet-voldoen |
|---|---|
| f_sw stabiel tijdens de sweep | Equivalent-time-as loopt scheef |
| Steile, repeterende schakelspike als triggerbron | Hoge trigger-jitter, vage flanken in reconstructie |
| Holdoff actief in PIO | Hertriggeren op ruis/secundaire spikes |
| C_hold groot genoeg (1 nF) t.o.v. R_on en lekstromen | Charge-injection en droop domineren de 12-bit-nauwkeurigheid |
| Dummy-switch compensatie + offsetcalibratie | Restfout van enkele LSB i.p.v. tientallen LSB |
| SMPS in steady-state tijdens sweep | Niet-stationair signaal → incoherente meting |

### Relatie tot bestaande methoden

- **Aanvullend, niet vervangend**: deze methode vereist extra analoge
  hardware (switch + hold-cap + buffer + comparator) t.o.v. de
  tijddomeinreconstructie via coherente swept vectormeting, die zonder
  extra hardware werkt op de bestaande Band A/B-architectuur.
- **Voordeel t.o.v. swept-vectormethode**: directe DC-component (geen
  AC-koppelverlies), geen noodzaak om N_max harmonischen vooraf te kennen —
  geschikt om niet-sinusvormige transiënten direct te visualiseren.
- **Te overwegen als**: validatie/kalibratiemiddel voor de
  swept-vectorreconstructie, of als losstaande tijddomein-meetmodus voor
  signalen die slecht door een beperkt aantal harmonischen te beschrijven
  zijn.
