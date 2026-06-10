Belangrijkste Bevindingen

project_spectrum_analyzer_ontwerp.md (line 1) is geen nette spec maar een ruwe chat-/tool-log.
Er staan Bash, IN, OUT, zoekresultaten en oude tussenconclusies in. Daardoor bevat het ook inmiddels verlaten opties zoals Band C tot 30 MHz en eerdere gedachten over SA612/dioderingmenger. Als bron van waarheid is dit riskant. project_spectrum_analyzer_arch.md (line 10) is veel bruikbaarder als samenvatting.

project_spectrum_analyzer_arch.md (line 106) overclaimt: “Coherentie is puur firmware”.
Dat klopt alleen als het SMPS-signaal óf extern gesynchroniseerd/getriggerd is, óf extreem stabiel genoeg blijft gedurende de hele sweep. Een vrijlopende SMPS is niet automatisch coherent met de RP2350-klok. De tekst erkent dit later zelf op regel 100 (line 100), dus dit is intern tegenstrijdig. Technisch beter: “firmware kan de LO numeriek benaderen, maar reconstructie vereist sync/trigger of fase-driftcorrectie.”

project_spectrum_analyzer_arch.md (line 57) noemt “exacte golfvormreconstructie”.
Dat is te sterk. Door 10 MHz bandlimiet, ontbrekende DC-component en kalibratiefouten wordt het een bandbegrensde reconstructie, geen exacte golfvorm. Voor jouw doel is dat nog steeds bruikbaar, maar de spec moet dat eerlijk zeggen.

project_spectrum_analyzer_arch.md (line 64): arctan(Q_n / I_n) moet atan2(Q_n, I_n) zijn.
Anders raak je kwadrantinformatie kwijt en krijg je problemen bij I_n = 0. Dit is een echte implementatiefout.

project_spectrum_analyzer_arch.md (line 32): overgang A→B op 250 kHz klopt alleen voor enkelkanaals ADC op 500 kSa/s.
Bij twee ADC-kanalen in round-robin is het 250 kSa/s per kanaal, dus Nyquist 125 kHz. De RP2350 ADC is volgens Raspberry Pi 12-bit, 500 kS/s totaal; RP2350-varianten/GPIO-aantallen kloppen verder met de officiële docs: Raspberry Pi RP2350 documentation.

project_spectrum_analyzer_arch.md (line 84): 2 ms sweeptijd is optimistisch.
Die rekent alleen RC-settling. In de praktijk komen ADC-acquisitietijd, voldoende coherente perioden, SNR/averaging, LO-settling en software-overhead erbij. Voor 500 kHz/20 harmonischen kan het nog plausibel snel zijn; voor lage schakelfrequenties wordt het venster veel langer.

project_spectrum_analyzer_arch.md (line 43) is architectonisch nog vaag.
De tekst mengt Vout, Vfeedback, inverterende opamp en I/Q-architectuur. Voor haalbaarheid moet het schema expliciet maken: welke vier sample-condensatoren horen bij 0/90/180/270 graden, hoe I en Q differentieel gevormd worden, en welke signalen naar ADC0/ADC1 gaan.

TS3A5017-keuze is technisch zinvol, maar let op spanningsbereik.
TI specificeert de TS3A5017 als 2.3–3.6 V, dual 4:1 mux, 11 Ω typische Ron, 165 MHz bandbreedte en signalen alleen binnen 0..V+: TI TS3A5017. Dus bij 3.3 V voeding moet de analoge frontend netjes biasen/begrenzen binnen het toegestane bereik.

Haalbaarheid

Band A/B tot 10 MHz is technisch haalbaar als spectrum-analyzer/lock-in-achtige meter. De grootste risico’s zitten niet in de RP2350 of TS3A5017 zelf, maar in fasecoherentie, analoge I/Q-matching, klokresolutie, kalibratie en de precieze analoge frontend.

Mijn advies: maak project_spectrum_analyzer_arch.md de enige actuele spec, en reduceer project_spectrum_analyzer_ontwerp.md tot archief of verwijder de ruwe tool-log eruit. De technische correcties die ik als eerste zou doen: atan2, “exact” vervangen door “bandbegrensd”, overgangsfrequentie mode-afhankelijk maken, en de coherentie-eis veel explicieter formuleren.


project_spectrum_analyzer_ontwerp.md
Document · MD

project_spectrum_analyzer_arch.md
Document · MD
11:22 AM
Ik ben alleen geintereseerd in de rimpelspanning.  ik kan eventueel een trigger maken door het AC signaal door een schmitt trigger met hysteres aan te bieden. wat is een fase-driftcorrectie en hoe werkt dit?
11:36 AM
Ja, als je alleen de rimpelspanning wilt reconstrueren, wordt het probleem netter: DC mag wegvallen, en een Schmitt-trigger op het AC-rimpelsignaal kan inderdaad dienen als fase-referentie. Dat is precies de ontbrekende schakel voor tijddomeinreconstructie.

Fase-driftcorrectie betekent: tijdens een swept meting corrigeer je dat de fase van het SMPS-signaal langzaam verschuift tussen harmonische-metingen.

Stel je meet harmonischen na elkaar:

n=1   op tijd t0
n=2   op tijd t0 + 100 us
n=3   op tijd t0 + 200 us
...
n=20  op tijd t0 + 1.9 ms
Als de schakelfrequentie niet exact constant is, of niet exact coherent is met jouw RP2350-klok, dan is het signaal bij elke stap een klein beetje verder “weggedraaid”. Dan lijkt de gemeten fase van harmonische 10 bijvoorbeeld niet alleen de echte golfvormfase te zijn, maar ook extra fase door tijdsverloop.

Zonder correctie krijg je dan een vervormde reconstructie.

Met Trigger

Als je uit het AC-rimpelsignaal een Schmitt-trigger maakt, krijg je per periode een digitaal nulpunt:

rimpel AC  -> comparator / Schmitt trigger -> digitale sync-puls
Dan kun je bij elke harmonischemeting meten:

tijd sinds laatste trigger = Δt_trigger
De gemeten fase corrigeer je dan terug naar hetzelfde tijdnulpunt:

φ_corrected_n = φ_measured_n - 2π × n × f_sw × Δt_trigger
Of in graden:

φ_corrected_n = φ_measured_n - 360° × n × f_sw × Δt_trigger
Daarna gebruik je φ_corrected_n in de inverse DFT/reconstructie.

Belangrijk: de factor n staat erin omdat de 20e harmonische 20 keer zo snel in fase draait als de fundamentele. Een tijdsfoutje van 10 ns is bij 500 kHz bijna niets voor n=1, maar bij 10 MHz/n=20 al duidelijk merkbaar.

Praktisch Werkt Het Zo

Meet f_sw uit de triggerperiode, bijvoorbeeld met een timer capture op de RP2350.
Start of timestamp elke Tayloe/Goertzel-meting t.o.v. de laatste triggerflank.
Meet per harmonische I_n en Q_n.
Bepaal amplitude en fase:
A_n = sqrt(I_n² + Q_n²)
φ_n = atan2(Q_n, I_n)
Corrigeer fase naar dezelfde triggerreferentie:
φ_n_corr = φ_n - 2π × n × f_sw × Δt
Reconstrueer:
x(t) = Σ A_n × cos(n × 2π × f_sw × t + φ_n_corr)
Waarom Schmitt-Trigger Goed Past

Voor rimpelspanning is een Schmitt-trigger bruikbaar omdat je meestal een periodieke component hebt op de schakelfrequentie. Je hoeft geen DC te kennen; je hebt alleen een stabiele fase-marker nodig. Hysterese helpt tegen ruis rond de nuldoorgang.

Maar: trigger-jitter wordt direct fase-jitter. Dus het triggerpad moet schoon zijn: bandbegrensd, juiste hysterese, snelle comparator/Schmitt, en liefst capture via hardware-timer/PIO in plaats van polling.

Kort gezegd: fase-driftcorrectie maakt van “harmonischen gemeten op verschillende momenten” weer “harmonischen alsof ze allemaal ten opzichte van dezelfde triggerflank zijn gemeten”. Dat is precies wat je nodig hebt om de rimpelgolfvorm betrouwbaar te reconstrueren.

11:36 AM
de PIO wordt al gebruikt om de klok voor de mixer te maken, dus het triggersignaal kan direct geintegreerd worden.
11:39 AM
Ja, precies. Dat maakt de architectuur veel sterker.

Als de PIO toch al de mixer-LO maakt, kun je het triggersignaal gebruiken om de PIO-fase te her-synchroniseren of minstens te timestampen. Dan wordt de trigger geen losse softwaremeting achteraf, maar onderdeel van de meetklok zelf.

De beste aanpak:

AC-rimpel -> Schmitt trigger -> PIO input
PIO -> 4-fase LO voor Tayloe mixer
PIO/DMA -> samplevenster start op vaste fase na trigger
Daarmee kun je per harmonischemeting zeggen:

LO-fase = bekend t.o.v. triggerflank
ADC-venster = bekend t.o.v. triggerflank
Dan is de fase van I/Q direct bruikbaar voor reconstructie, met veel minder driftcorrectie achteraf.

Er zijn twee nuttige modi:

Trigger-locked single shot per harmonische
Wacht op trigger, reset/start PIO-fasedeler, meet harmonische n, stop of latch resultaat. Daarna volgende harmonische.
Dit geeft de schoonste fase-referentie.

Free-running LO met trigger timestamps
PIO loopt door, maar registreert bij elke trigger de actuele LO-fase/tellerstand. Firmware corrigeert de gemeten fase met die offset.
Dit is soepeler als je de LO niet telkens wilt onderbreken.

Voor jouw doel, rimpelgolfvorm reconstrueren, zou ik de eerste kiezen: per harmonische de PIO-LO vanaf dezelfde triggerfase starten. Dan meet je elke harmonische alsof hij exact op hetzelfde punt in de rimpelperiode begint.

Belangrijke nuance: de LO-frequentie moet nog steeds n × f_sw zijn. Dus je meet eerst f_sw uit opeenvolgende triggers, stelt de PIO-deler in voor harmonische n, wacht op de volgende triggerflank, reset de PIO-fase, en integreert dan een geheel aantal perioden.

Conceptueel:

voor n = 1..N:
    meet/update f_sw uit triggerperiode
    stel PIO LO = n * f_sw
    wacht op triggerflank
    reset PIO phase accumulator / state machine
    meet I/Q over K perioden
    φ_n = atan2(Q, I)
Dan is φ_n al gerefereerd aan dezelfde triggerflank. Eventuele resterende correctie is alleen nog vaste vertraging:

φ_n_corr = φ_n - 2π × n × f_sw × t_delay
waar t_delay de vaste vertraging is van Schmitt-trigger, PIO-start, analoge mixer, ADC-sampling en filters. Die kun je kalibreren met een bekend testsignaal.

Dus ja: trigger direct in PIO integreren is waarschijnlijk de elegantste oplossing. Dan wordt fase-driftcorrectie grotendeels vervangen door fase-gesynchroniseerde acquisitie.
