# BMC64-NG — Differenze con BMX e con il BMC64 originale

### Le differenze, una per una

Aggiornata all'11 novembre 2026, sul codice pubblicato dei tre: BMC64-NG V1.4 (11 novembre 2026), il BMC64 5.5.1 di Randy Rossi (10 novembre 2026) e BMX 2026.10.09.1, l'ultima pubblicata.

---

## I tre sistemi

- **BMC64** è il progetto di Randy Rossi: un emulatore Commodore che gira da solo sul Raspberry, senza sistema operativo (Circle), con il VICE 3.3. Funziona su Raspberry Pi Zero, Zero 2 W, 2 e 3 e, dalla versione 5.4.0 (21 ottobre 2026, ancora in beta), anche su Pi 4, 400, 5, 500 e 500+.
- **BMC64-NG** parte da BMC64, ma usa il VICE 3.10 e gira su Raspberry Pi 4, 400, 5, 500 e 500+. I Raspberry più vecchi non ci sono più.
- **BMX** è un altro fork di BMC64, anche lui con il VICE 3.10, per Raspberry Pi 4 e 5 (e le loro varianti). Ha tolto i Raspberry fino al 3.

Con il BMC64 BMC64-NG ha in comune il menu, il formato del file `machines.txt`, le immagini REU salvate e caricate dal menu, la scrittura dei dischi sulla scheda (Prefs > Flush disk writes), i nomi dei file fino a 255 caratteri, la rete su tutte le macchine col WiFi dal menu e, dalla 5.4.0 del BMC64 (in beta), i Raspberry Pi 4, 400, 5, 500 e 500+, con il C64 cycle exact (X64SC) e il Plus4Emu. Con BMX ha in comune due cose sole: il VICE 3.10 e lo shader CRT.

---

## BMC64-NG rispetto al BMC64 originale, quello di Randy

**Macchine e Raspberry**

- **Raspberry**: BMC64-NG gira su Pi 4, 400, 5, 500 e 500+. Il BMC64 su Pi Zero, Zero 2 W, 2 e 3 e, dalla 5.4.0, anche su Pi 4, 400, 5, 500 e 500+, in beta e con dei limiti (solo HDMI, a 720p o 1080p).
- **Emulatore**: BMC64-NG ha il VICE 3.10, il BMC64 il VICE 3.3 del 2018.
- **C64 cycle exact**: in BMC64-NG il C64 è cycle exact (X64SC), con la versione Std più leggera a scelta nel menu Machine > Switch. Il BMC64 sul Pi 4 e sul Pi 5 ha solo l'X64SC, e sui Raspberry fino al 3 solo il C64 normale.
- **SuperCPU**: BMC64-NG ha il C64 con SuperCPU a 20 MHz. Il BMC64 no.
- **Modelli del C64**: dal menu Model... il C64 può essere un C64C, un C64 old, un SX-64 o un C64 GS (la console a sole cartucce). Il BMC64 no: il suo VICE li conosce, ma il menu non li offre.

**Riavvio, spegnimento, tasti**

- **Spegnimento**: tutti e due si spengono e si riavviano dal menu, con conferma: BMC64-NG dal menu Reset, il BMC64 dal menu **Power** (**Reboot BMC64**, **Shut down**) e dalla Web UI. BMC64-NG risponde a **Fn + F10** sul Pi 400 e al tasto di accensione del Pi 500 e 500+; il BMC64 a **Fn + F10** del Pi 400, al pulsante del Pi 5 e al tasto del Pi 500. BMC64-NG si spegne e si riavvia anche dalla tastiera (**ALT + ESC**, **ALT + SHIFT + ESC**, con conferma) e dai tasti rapidi a scelta; il BMC64 dalla tastiera ha solo i reset della macchina.
- **Scorciatoie**: BMC64-NG ha una trentina di scorciatoie con ALT (dischi, nastro, cartucce, REU, modello del drive, volume, lettore SID, scanline, warp, barra di stato). Il BMC64 ha le combinazioni C= e CTRL con F1, F3, F5, F7 e i pulsanti a cui si assegna una funzione, che BMC64-NG ha mantenuto.
- **Tastiere**: BMC64-NG ha le disposizioni americana, italiana, inglese e tedesca (menu Keyboard > Layout), anche per scrivere nei campi del menu. Il BMC64 (menu **Keyboard** > **Keyboard Layout**, anche lui per i campi del menu) ha sul C64 l'americana, l'inglese, la tedesca, la francese, la norvegese e la latinoamericana; sul C128, VIC-20, Plus/4 e PET l'americana, e il C128 anche la tedesca. L'italiana no.
- **RESTORE su due tasti**: nelle mappature Positional di C64, C128, VIC-20 e SuperCPU RESTORE è **PrtScn** (le tastiere dei Pi 400 e 500) e anche **Page Up** (la tastiera vera del C64 sulla Keyrah V2). Il BMC64 ha solo Page Up.

**Video**

- **Schermo principale**: Video > Main HDMI Output sceglie su quale HDMI va l'immagine e con lei l'audio. Il BMC64 no: ha un solo schermo.
- **C128 su due monitor**: Video > Active Display su Two HDMI manda il VIC-II su un monitor e il VDC sull'altro. Il BMC64 no: VIC-II e VDC vanno su una sola uscita, con Active Display e PIP.
- **Uscita composita**: BMC64-NG ha **PAL Composite 576p@50Hz** e **NTSC Composite 480p@60Hz** nel menu Switch di ogni macchina, solo sul Pi 4 B (jack da 3,5 mm, cavo AV a 4 poli), coi bordi tenuti a parte da quelli della HDMI. Il BMC64 ha gli stessi due modi compositi solo sui Raspberry fino al 3: sul Pi 4 e sul Pi 5 ha solo la HDMI.
- **Risoluzione**: tutti e due hanno 720p e 1080p per ogni macchina nel menu Switch. BMC64-NG la cambia anche da Video > HDMI Resolution; il BMC64 sul Pi 4 e sul Pi 5 la sceglie solo con la voce dello Switch.
- **VGA**: BMC64-NG ha i modi VGA 1024x768 a 50 e 60 Hz per ogni macchina (HDMI con adattatore VGA). Il BMC64 ha un'altra strada: il DPI con VGA666 (solo sui Raspberry fino al 3) o 1920x1080 con `raster_skip`.
- **Monitor VGA e HDMI insieme**: Video > VGA Monitor On dice su quale HDMI sta il monitor VGA, e lo Switch manda lo schermo sul VGA con le voci VGA e sull'altro monitor con le altre, con un solo riavvio. Il BMC64 no: ha un solo schermo.
- **Shader CRT**: BMC64-NG ha lo shader di BMX, con geometria, scanline, maschera dei fosfori, bagliore, riflessi e i preset `.crt`. Il BMC64 ha lo shader CRT di davej: sul Pi 4 e sul Pi 5 (crt-pi, disegnato dalla GPU) solo con **Enable**, **Curvature** e **Reset**; scanline, maschera, bagliore e gamma si regolano solo sui Raspberry fino al 3.
- **Conferma del modo video**: dopo un cambio di macchina o di modo ci sono 15 secondi per confermare con un tasto, altrimenti torna com'era (Safety timer (15s) in fondo al menu Switch). Il BMC64 no: chiede solo se riavviare, e per tornare indietro ha il CTRL + F7 tenuto 5 secondi dopo l'avvio.
- **Fotografie dello schermo**: Prefs > Screenshots e **SUPER + PRTSCN**, in PNG o PPM, nella cartella `/screenshots`. Il BMC64 no.

**Suono**

- **Motori del SID**: oltre a ReSid c'è **ReSIDfp** (Sound > SID Engine), con le sue regolazioni, i profili del 6581 per compositore (Composer Profile) e il fruscio di fondo (Background Noise). Il BMC64 no: il suo SID Engine ha solo Fast e ReSid.
- **Filtri del reSID**: nel menu Sound la cartella reSID Filter Settings (Passband %, Gain %, Filter Bias) per il 6581 e per l'8580. Nella guida del BMC64 Passband e Gain devono restare quelli di fabbrica.
- **Stereo o mono** (Sound > Channels) con più SID. Il BMC64 no: con due SID esce sempre in stereo.
- **8580 + Digi Boost** (Sound > SID Model, SID2 Model e SID Card Model): l'8580 con la modifica che fa sentire i campioni digitalizzati. Il BMC64 no: ha solo 6581 e 8580.
- **SID Card su PET e Plus/4**, spenta di fabbrica. Il BMC64 no: il secondo SID c'è solo sul C64 e sul C128.
- **Autostart sul PET**, solo coi `.prg`. Il BMC64 no: sul PET l'Autostart non c'è.
- **Chiave PetSpeed del PET**: Machine > PetSpeed dongle, sulla porta del nastro. Il BMC64 no.
- **Lettore di file .SID**: ALT + S apre la cartella `/sids`; suona anche i file a due, tre, quattro e fino a otto SID (PSID v4E; coi brani a otto SID, come The Tuneful Eight, 24 barre e il reSID per partire; S passa fra reSID e reSIDfp dal vivo), con VU meter, tempo del brano, salti di 10 secondi (all'indietro subito, da una fotografia della macchina in memoria), voci una per una e il lato di ogni SID (F1-F4, F1-F8 coi brani a otto SID). Nel BMC64 non c'è.
- **I SID su più core**: con 2 o 3 SID, e nel lettore fino a 8, quelli oltre il primo li calcolano il core 2 e, quando lo schermo lo lascia libero, il core 3. Il BMC64 ha al massimo due SID: il secondo lo calcola il core 2, e il core 3 calcola solo le tabelle dei filtri all'avvio, poi resta fermo.
- **DAC USB**: Sound > Audio out sceglie fra Auto, Jack, HDMI e USB DAC dal menu. Il DAC si può collegare e scollegare a macchina accesa, e vanno anche i DAC a 48 kHz. Il BMC64 no: l'uscita audio (HDMI, jack o automatica) si sceglie solo nei file, con `audio_out` in `cmdline.txt` o in `machines.txt`, senza DAC USB.
- **Volume**: la barra a schermo, **ALT + "+"**, **ALT + "−"**, **ALT + M**. Il BMC64 ha solo la voce **Volume** nel menu Sound.

**Clock**

- **Clock della CPU e della GPU dal menu** (CPU Clock e GPU Clock): la CPU -20%, -10%, di serie, +10%, +20%, la GPU di serie, +10%, +20%, su ogni Raspberry dal Pi 4 in poi; i MHz della GPU in cima al menu e nella barra di stato, accanto a quelli della CPU. Nel BMC64 il clock si cambia a mano in `config.txt` (le righe `arm_freq` e `over_voltage`, in commento nel file di esempio).
- **Overclocking...** (menu principale): tensione, limite di temperatura, Core Clock e V3D Clock, e mai `force_turbo` da lì. Il BMC64 no.

**Dischi, nastri, file**

- **Drive Commodore vero**: un 1541, 1571 o 1581 su adattatore XUM1541 o ZoomFloppy (ALT + 7). Il BMC64 no.
- **XUM1541 fatta con un Arduino Pro Micro**: potrebbe non essere compatibile con BMC64-NG: il drive Commodore vero funziona solo alla prima accensione a freddo, e dopo un cambio di macchina o un riavvio del Raspberry smette di rispondere. BMC64-NG è provato con una ZoomFloppy.
- **Directory of Drive on XUM1541** (Drives): la directory del drive vero letta dai settori del dischetto, con tutte le voci, anche quelle che il LOAD"$" del BASIC non mostra. Il BMC64 no.
- **I drive che parlano fra loro**: con due o più drive accesi, prima di toccare il cavo seriale un drive porta gli altri alla sua ora, e due drive possono parlarsi senza passare dal computer. Il BMC64 no.
- **Floppy USB da PC** letti come FLP1 e FLP2. Il BMC64 no.
- **Ricerca nell'elenco dei file**: si scrivono le prime lettere e la lista si stringe già mentre si scrive, con i jolly (il punto interrogativo e l'asterisco), e se non trova cerca dentro il nome. Il BMC64 salta solo alla prima lettera scritta.
- **Nomi lunghi nel menu**: un nome che non sta nella riga scorre quando ci passa sopra il cursore, così si legge tutto (fermo un attimo, poi scorre, poi ricomincia). Il BMC64 lo taglia.
- **Immagini .REU**: **ALT + 0** carica un file dalla cartella `/REU`, **ALT + SHIFT + 0** lo stacca.
- **Le cartucce EasyFlash salvano** (Cartridge > Save EasyFlash Now, e da sole quando si stacca la cartuccia). Il BMC64 ha Save EasyFlash Now, ma non salva da solo quando si stacca la cartuccia.
- **Magic Desk Plus**: la cartuccia con SRAM ed EEPROM, che salva in due file accanto al `.crt` (`.srm` e `.eep`). Il BMC64 no.
- **Drive senza dischetto in Plus4Emu**: ALT + 2 e ALT + 3 mettono il 1541 o il 1551 e aprono l'elenco dei `.D64`.
- **Immagini .NIB** (le copie a basso livello di nibtools, anche dei dischi protetti): si montano e si avviano come gli altri dischetti, convertite in `.g64` col codice del nibconv di nibtools. Il BMC64 no.

**Rete**

- **Indirizzo fisso**: IP Address Mode su DHCP o Static (Static IP, Netmask, Gateway, DNS). Il Raspberry si presenta al router come BMC64-NG. Il BMC64 no: solo l'indirizzo automatico (DHCP).
- **Ora legale**: Daylight Saving (Off, On, Auto (EU)) sotto Timezone. Il BMC64 prende l'ora dalla rete e ha il fuso orario nel menu Network (**Timezone (reboot)**, da UTC-12:00 a UTC+14:00), senza ora legale.
- **Data e ora**: in cima al menu principale, prese dalla rete col fuso e l'ora legale del menu Network; con la stessa ora si vedono i file da SD Card Sharing. Il BMC64 prende l'ora dalla rete ma non la mostra.
- **NTP dal menu**: Network > NTP (Server 1, Server 2, Sync interval, Save & Apply, Sync now). Il BMC64 no: prende l'ora sempre da `pool.ntp.org`, ogni 15 minuti; nel menu ha solo il fuso (**Timezone (reboot)**) e, in **Prefs** > **Boot Options**, **Delay autostart for time sync**.
- **La data dei file senza rete**: finché l'ora non arriva vale la data più recente fra i file della radice della scheda. Il BMC64 scrive la data zero, che Windows mostra come 1601.
- **SD Card Sharing...**: la microSD vista dal PC (`\\BMC64-NG-400`, col nome del modello, o `\\indirizzo`, utente bmc64, password bmc64, cambiabile con Sharing Password). Il BMC64 no: la sua scheda dal PC si vede solo nel browser, con la Web UI.
- **NAS**: la voce NAS (NAS IP Address, NAS User Name, NAS Password, SMB Protocol, NAS Path, Test NAS Connection) apre un NAS come sul MiSTer; NAS Path è la condivisione e la cartella da cui parte il navigatore, una per macchina, vuoto di fabbrica come gli altri campi; la password si scrive in chiaro e dopo il riavvio si vede con gli asterischi. Il BMC64 no.
- **Il modem come la Ultimate II+** (C64, C128, Plus/4 e VIC-20): Modem (SwiftLink) spento di fabbrica, Modem Address di fabbrica `DE00`. Riceve anche chiamate, sulla porta 3000 (RING, **ATA**, **ATS0=1**). Il modem del BMC64 chiama soltanto; anche lui è spento di fabbrica (**Modem Address** su **Off**) e sul C64 e sul C128 si accende scegliendo `D700`, `DE00`, `DF00` o `DF80`. Il modem sul Plus/4 (l'ACIA di serie, `$FD00`) e sul VIC-20 (la porta utente) c'è anche nel BMC64. Sul C64 e sul C128 la linea sta sul DSR, come sulla Ultimate II+; nel BMC64 sta sul DCD.
- **WiC64** (C64 e C64 Std, anche HTTPS): Network > WiC64. Il BMC64 no.

**Comandi e controlli**

- **L'adattatore dei joystick sulla porta utente**: in BMC64-NG lo Starbyte si sceglie (nel BMC64 è nell'elenco ma il menu non ci arriva) e dopo un'istantanea il menu ritrova il tipo giusto (il BMC64 guarda solo il primo).
- **Spinner come paddle** (Joyports > USB Spinner (Paddle), con Spinner Sensitivity) su C64, C128 e VIC-20. Il BMC64 no: come paddle usa solo gli assi analogici dei gamepad.
- **I pulsanti del gamepad**: Mapping Mode, Assignments by Button, Assignments by Function e Clear a Button. Il BMC64 ha solo le scelte per pulsante.
- **Mouse 1350 e adattatore V8**: Mouse > Type **1350 (Joystick)** e Userport Joystick Adapter > **V8**, con le porte dalla 5 alla 10. Il BMC64 no.
- **Autofire**: il menu Autofire, subito sotto Joyports, con Port 1 e Port 2 (sul VIC-20 una porta) su Off, Slow o Fast. Il BMC64 no.
- **Il menu a blocchi con ALT**: nel menu **ALT** + **↑** e **↓** saltano su e giú a blocchi come **PgUp** e **PgDn** (tenuti premuti, continuano), con ogni tastiera: anche sul 500+, dove FN non arriva al computer. Il BMC64 salta a blocchi con **PgUp** e **PgDn** o con **F3** e **F5**; **ALT** con le frecce non fa niente.
- **Il mouse nel menu**: la freccia sceglie la riga, il tasto sinistro fa INVIO e il destro torna indietro, la rotella scorre la lista o cambia i numeri della voce sotto la freccia; si spegne in **Prefs** (Mouse in Menu). Il BMC64 nel menu il mouse non lo usa.
- **Il nastro senza nastro**: senza un nastro montato i comandi del registratore non fanno niente e in basso compare **NO TAPE**.
- **Barra di stato**: le spie vere di ogni modello di drive, la traccia (anche le mezze tracce), il contanastro, e temperatura e frequenza sulla seconda riga (**ALT + SHIFT + O**).

**Quando qualcosa va storto**

- **PASSI.TXT**: tiene le ultime cose che la macchina ha cominciato e finito (l'avvio, le voci del menu, i tasti rapidi); se il Raspberry si blocca, dopo la riaccensione `PASSI-PRIMA.TXT` dice dove si era fermato. In questa versione il guardiano (il riquadro nero BMC64-NG HAS STOPPED) è spento.
- **File sulla scheda**: `BMC64-DIAG.TXT`, `PASSI.TXT` (e `PASSI-PRIMA.TXT`), `USB-LOG.TXT`, `NET-LOG.TXT`, `SMB-LOG.TXT`. Il BMC64 ha il suo registro, acceso dal menu principale e dal file di configurazione.

---

## BMC64-NG rispetto a BMX

**Raspberry e macchine**

- **Raspberry**: BMC64-NG è provato su Pi 4, 400, 5, 500 e 500+. BMX prepara la scheda anche per il CM4 e, sul Pi 4, può costruire una sua versione vecchia a 32 bit.
- **Plus/4**: BMC64-NG ha due motori, il VICE e Plus4Emu. BMX solo il VICE.
- **Cambio di macchina**: BMC64-NG dal menu Machine > Switch coi profili di `machines.txt`, con 15 secondi per confermare; BMX dal menu Machine con Apply & Reboot, coi profili di `machines.ini` (non legge `machines.txt`), senza conferma a tempo.

**Riavvio, spegnimento, tasti**

- **Riavvio e spegnimento**: BMC64-NG anche dalla tastiera (**ALT + ESC**, **ALT + SHIFT + ESC**, **Fn + F10** sul Pi 400, il tasto di accensione sul Pi 500 e 500+). BMX solo dal menu System.
- **Riavvio e spegnimento dai tasti rapidi a scelta**: in BMC64-NG fra le scelte di **C= + F1…F7** e **CTRL + F1…F7** ci sono anche **Reboot BMC64** e **Power Off BMC64**, per la tastiera vera del C64 sulla Keyrah, che non ha ALT. In BMX fra quelle scelte non ci sono.
- **Scorciatoie**: tutte quelle con ALT sono di BMC64-NG. Col tasto Super BMX lancia una funzione insieme a un altro tasto (W warp, S barra di stato, T nastro, C cartuccia, R reset e altre); in BMC64-NG il tasto Super da solo apre e chiude il menu, come F12, e **SUPER + PRTSCN** fa la fotografia dello schermo.
- **Tastiere**: BMC64-NG americana, italiana, inglese e tedesca; BMX americana e tedesca.
- **RESTORE su due tasti**: BMC64-NG **PrtScn** (Pi 400 e 500) e **Page Up** (Keyrah V2); BMX **Page Up**, e **F12** se si spegne F12 opens menu.
- **Il menu riaperto**: BMC64-NG torna sull'ultima voce toccata anche quando si esce con **ESC**; BMX dopo **ESC** si riapre su **Quick Access...**.

**Clock**

- **Clock della CPU**: BMC64-NG dal menu CPU Clock, da -20% a +20% sul clock di serie di ogni Raspberry dal Pi 4 in poi, quindi anche più basso. BMX solo verso l'alto (Pi 4 da 1500 a 2400 MHz, Pi 5 da 2400 a 3200 MHz, a passi di 25 MHz).
- **Clock della GPU**: BMC64-NG ha nel menu principale **GPU Clock**, che alza insieme Core e V3D (di serie, +10% o +20%), coi MHz della GPU in cima al menu e nella barra di stato. BMX non ha una voce così: li alza uno per uno, con Core Clock e V3D Clock nella cartella Expert (Pi 4 fino a 800 MHz, Pi 5 fino a 1200).
- **Ventola**: BMC64-NG dal menu principale (Fan), Auto con la curva di Raspberry Pi OS oppure da 0% a 100% a passi di 10%: sul Pi 5 la ventola sulla sua presa, sul Pi 4 quella col filo di comando sul piedino 8 (GPIO 14). BMX scrive `gpiofanpin=45` sul Pi 5 e la ventola la regola Circle con la temperatura, senza voce nel menu; sul Pi 4 niente.

**Video**

- **Schermo principale**: in BMC64-NG Video > Main HDMI Output sceglie l'HDMI 0 o 1 per tutte le macchine. In BMX lo dice `hdmi_output` in `cmdline.txt` (0, 1 o auto).
- **I primi Pi 5 (chip C1)**: in BMC64-NG lo scalatore, con gli shader CRT, dà l'immagine anche sui primi Pi 5 col chip BCM2712 C1, con la PixelValve e la trasparenza dei piani programmate come fa Linux su quel chip. Anche BMX, dalla versione 2026.09.18, ha una sua cura per il chip C.
- **Lo shader CRT a 1080p sul Pi 4 e sul 400**: in BMC64-NG la GPU disegna al massimo un quadro 720p e lo scalatore lo porta a 1080p, così a 1080p fa lo stesso lavoro che a 720p. BMX sul Pi 4 disegna sempre grande quanto lo schermo.
- **Lo shader CRT con tutti gli effetti, dal Pi 4 in poi**: se la GPU non fa in tempo a disegnare un quadro, BMC64-NG ne disegna uno più piccolo e lo scalatore lo porta alla grandezza dello schermo, così la macchina resta al 100%. BMX disegna sempre il quadro intero, anche quando la GPU non fa in tempo.
- **Lo shader CRT con l'immagine spostata**: in BMC64-NG, se **H Center** o **V Center** portano l'immagine in parte fuori dallo schermo, lo shader lavora sulla parte che si vede; in BMX lo shader si spegne e l'immagine resta senza effetti.
- **Il PET col CRTC senza sincronismo**: in BMC64-NG, se un programma lascia il CRTC senza sincronismo verticale, lo schermo e il menu vanno avanti; BMX e il BMC64 hanno il CRTC del VICE com'e', che in quel caso non chiude piu' il quadro.
- **L'Autostart del PET come il LOAD vero**: in BMC64-NG l'avvio di un `.prg` non tocca `$C7`/`$C8`, come il LOAD vero del PET; BMX e il BMC64 hanno l'avvio del VICE com'è, che ci scrive l'indirizzo del programma.
- **Il PET col quadro che si accorcia**: in BMC64-NG, quando il CRTC riprogrammato accorcia il quadro, lo strato del PET resta alto quanto la sua tela fissa e lo schermo resta al chip video, a piena velocità; in BMX lo strato segue il quadro del VICE e lo stesso controllo dello schermo passa ogni quadro al processore (letto nel codice, non provato).
- **Il PET col quadro al suo posto**: in BMC64-NG la tela del PET resta alta 272 righe qualunque cosa chieda il CRTC e il quadro resta centrato nello schermo che si vede; BMX ha il CRTC del VICE 3.10 com'è, che allunga la tela e centra il quadro lì dentro, mentre lo schermo mostra sempre le stesse righe (letto nel codice, non provato); il BMC64 ha il VICE 3.3, che la tela non la tocca.
- **La microSD che si riprende**: in BMC64-NG il lettore della scheda, se un trasferimento si ferma a metà, rimette a posto le linee, ferma o reinizializza la scheda e rifà il comando, come fa Linux; BMX e il BMC64 hanno il lettore di Circle com'è, che riprova senza rimettere a posto niente e poi dà errore (letto nel codice, non provato).
- **C128 su due monitor**: BMC64-NG sì (Two HDMI, con scambio). BMX mostra i due schermi su un monitor solo.
- **VGA**: BMC64-NG ha i profili 1024x768 a 50 e 60 Hz. BMX no.
- **Monitor VGA e HDMI insieme**: in BMC64-NG Video > VGA Monitor On fa passare lo schermo dal monitor HDMI a quello VGA, sull'altra porta, con la sola voce dello Switch. BMX no: non ha i modi VGA, e la porta la dice `hdmi_output` in `cmdline.txt`.
- **Uscita composita**: BMC64-NG ha **PAL Composite 576p@50Hz** e **NTSC Composite 480p@60Hz** nel menu Switch del Pi 4 B, coi bordi tenuti a parte da quelli della HDMI, e si torna alla HDMI con una voce HDMI dello Switch o con C= + F7 tenuti 5 secondi. BMX ha due modi compositi (576p e 480p) segnati come sperimentali.
- **Conferma del modo video**: 15 secondi per confermare, poi torna com'era (Safety timer (15s) la toglie). BMX non ce l'ha.
- **Modi video**: BMC64-NG ha 720p e 1080p a 50 e 60 Hz per ogni macchina (Video > HDMI Resolution, o lo Switch) e VGA 1024x768, e una scheda nuova parte a 1080p. BMX ha 480p e 576p in HDMI, 720p, 1080p e i suoi modi a frequenza esatta, e di fabbrica parte a 720p (il PET a 1080p).
- **La risoluzione in cima al menu**: in BMC64-NG la prima riga del menu principale finisce con la risoluzione che esce, per esempio *C64 PAL 50Hz HDMI 1080p*. In BMX la riga dice solo la macchina e il video (*C64 PAL 50Hz HDMI*).
- **VIC-20 NTSC** col suo KERNAL americano: BMC64-NG sì. BMX ha il tempo NTSC, ma non sceglie un KERNAL NTSC.
- **Fotografie dello schermo**: in BMC64-NG la macchina si ferma il tempo di scrivere sulla scheda, e mentre si gioca non costano niente. BMX scrive col core 3, e ha anche Stop emulator.

**Suono**

- **Uscite**: BMC64-NG HDMI, DAC USB e jack da 3,5 mm del Pi 4. BMX HDMI e DAC USB.
- **SID Card su PET e Plus/4**: solo BMC64-NG ($8F00 o $E900 sul PET, $FD40 o $FE80 sul Plus/4).
- **Lettore di file .SID** (ALT + S) fino a otto SID: solo BMC64-NG. BMX non apre i `.SID`.
- **ReSIDfp** come terzo motore del SID (Sound > SID Engine), con Background Noise e i profili del 6581 per compositore: BMC64-NG. BMX no: nel suo menu il motore ha solo FastSID e reSID.
- **Filtri del reSID**: in BMC64-NG la cartella reSID Filter Settings del menu Sound vale per tutti i SID. In BMX ce n'è una per il primo SID e una per il secondo, regolati ognuno per conto suo.
- **I SID su più core**: BMC64-NG col core 2 e, quando lo schermo lo lascia libero, col core 3, con 2 o 3 SID e nel lettore fino a 8; BMX solo il secondo SID sul core 2, con due SID.
- **Stereo o mono** (Sound > Channels): BMC64-NG. In BMX lo stereo va col secondo SID.

**File, dischi e cartucce**

- **Drive Commodore vero** (XUM1541, ZoomFloppy, ALT + 7 o **Real drive** in **Change Model...**, anche senza le ROM dei drive), con **Reset Drive on XUM1541** che fa ripartire i drive veri: solo BMC64-NG.
- **La directory del drive vero letta dai settori** (Drives > Directory of Drive on XUM1541): solo BMC64-NG.
- **I drive che parlano fra loro** sul cavo seriale, senza passare dal computer: solo BMC64-NG.
- **Floppy USB** (FLP1 e FLP2) e i drive CMD FD-2000 e FD-4000: BMC64-NG.
- **Default disk** (Drives): tutti e due. BMX di serie attacca il suo `utils.d64`, con programmi di altri; BMC64-NG di serie nessuno.
- **Ricerca nell'elenco dei file** già mentre si scrive, con i jolly (il punto interrogativo e l'asterisco): BMC64-NG.
- **Immagini .REU**: BMC64-NG con **ALT + 0** dalla cartella `/REU` e dal menu. BMX solo dal menu della REU.
- **Programmi .prg**: con **PRG loading** su **Via disk image**, di serie in tutti e due, in BMC64-NG partono anche senza drive, dritti in RAM. BMX in quel caso usa un dischetto temporaneo e vuole un drive; con **Direct to RAM** no.
- **File .SID**: solo BMC64-NG.
- **Immagini .NIB**: solo BMC64-NG (convertite in `.g64` col codice del nibconv di nibtools).
- **Salvataggio dei dischi**: Prefs > Flush disk writes in BMC64-NG. BMX non ha una voce così.
- **Magic Desk Plus**, con la sua memoria nei file `.srm` e `.eep` accanto al `.crt`: BMC64-NG.

**Rete**

- **Rete**: tutti e due hanno Wi-Fi o Ethernet, l'elenco delle reti, l'indirizzo automatico o fisso e l'ora dalla rete con l'ora legale. BMC64-NG la ha su tutte le macchine, anche sul Pi 5 e 500, con data e ora in cima al menu. BMX prende l'ora dalla rete dalla 2026.10.09 (Network > NTP, spenta di serie), col fuso UTC, uno spostamento fisso o l'Europa centrale con l'ora legale, e la mostra nella cartella NTP.
- **La rete dei Pi 5 rev 1.7**: questi Pi 5 hanno un altro chip per l'Ethernet (MaxLinear MXL86110 invece del Broadcom BCM54213PE). BMC64-NG li riconosce; nei sorgenti di BMX il codice per quel chip non c'è.
- **La data dei file senza rete**: in BMC64-NG vale la data più recente fra i file della radice della scheda. BMX scrive la data zero, che Windows mostra come 1601.
- **SD Card Sharing...**: la microSD vista dal PC via SMB (`\\BMC64-NG-400`, col nome del modello), con il Raspberry nella cartella Rete di Windows. BMX ha File sharing (SMB), dalla 2026.09.18, col nome del server e della condivisione a scelta.
- **NAS come sul MiSTer**: BMC64-NG, con la cartella di ogni macchina e `NAS-NON-SALVATI` se il NAS non risponde. BMX ha Network Drive (SMB), dalla 2026.09.18, con la ricerca dei server in rete.
- **Modem come la Ultimate II+** (SwiftLink, chiama e riceve): BMC64-NG, su C64, C128, Plus/4 e VIC-20. BMX ha un modem Hayes con rubrica e suoni, RS-232 via rete, UP9600 e SwiftLink.
- **Comandi via web e aggiornamento da Internet**: solo BMX. In BMC64-NG niente controllo via web, per scelta.

**Comandi e controlli**

- **L'adattatore dei joystick sulla porta utente** (Joyports > Userport Joystick Adapter): in BMC64-NG i tipi che si scelgono sono quelli che il VICE 3.10 ha per la macchina, sul Plus/4 Synergy e WOJ. BMX tiene l'elenco del VICE 3.3: sul Plus/4 restano solo tipi che il 3.10 rifiuta, e l'adattatore non si accende; sul C64 resta lo Hummer, che nel 3.10 è solo del C64DTV.
- **Spinner come paddle** (Joyports > USB Spinner (Paddle)): BMC64-NG su C64, C128 e VIC-20. BMX non ha niente sullo spinner (cercato nei suoi sorgenti).
- **Mouse 1350, adattatore V8 e chiave PetSpeed del PET**: BMC64-NG.
- **Autofire** (Off, Slow, Fast, porta per porta): BMC64-NG. BMX nel menu non ce l'ha (cercato nei suoi sorgenti: c'è solo dentro il VICE).
- **Il menu a blocchi con ALT**: nel menu **ALT** + **↑** e **↓** saltano su e giú a blocchi come **PgUp** e **PgDn** (tenuti premuti, continuano), con ogni tastiera, anche sul 500+. In BMX **ALT** nel menu non fa niente, e **↑** e **↓** vanno di una riga.
- **Il nastro senza nastro**: BMC64-NG mostra **NO TAPE** e non fa niente. In BMX i comandi del nastro arrivano al VICE senza controllo.
- **Il suono del nastro** (Tape sound emulation) non cancella la musica del SID in BMC64-NG; nel file del VICE di BMX il difetto c'è ancora (riletto l'11 novembre nel suo 2026.10.09.1, in datasette-sound.c).
- **Correzioni del VICE posteriori alla 3.10**: BMX ha il rifacimento del drive idle, col selettore nel menu Drives dalla 2026.10.04; BMC64-NG no, per non rallentare. BMC64-NG ha, dal 4 ottobre 2026, le correzioni di fedeltà del VICE di sviluppo: il registro seriale delle CIA, la lettura in più di RTS, l'editor 80 colonne originale del PET, il registro $FFF0 dei PET 8096/8296, il blocco del VDC del C128 con R0=0, il kernal C64 personalizzato del C128 (non viene più rimesso quello di serie), i bit di $01 del C128, due registri del TED, il joystick e la colonna 7 del VIC-20, il glitch del POT solo quando cambia un POT collegato, la ROM del drive cambiata dal menu che arriva subito al drive acceso, i comandi T-W del drive virtuale, SFX Sound Expander, DigiMAX, tapecart e la scheda HRE del PET. Dal 10 ottobre 2026 anche la pulizia della tastiera che dimentica la coda dei tasti (r46275 del 28 settembre).
- **Il VICE di sviluppo fino al 9 novembre 2026**: BMC64-NG ha anche le correzioni fino alla r46396, le forme d'onda combinate e il fruscio di fondo nuovi del reSIDfp (r46370) e lo sprite del C64C (bug 2164): uno sprite che passa da multicolore ad alta risoluzione a metà riga, sui chip 8565 e 8562.

**Quando qualcosa va storto**

- **BMC64-NG** scrive sulla scheda `BMC64-DIAG.TXT`, `PASSI.TXT`, `USB-LOG.TXT`, `NET-LOG.TXT` e `SMB-LOG.TXT` (in questa versione il guardiano è spento). BMX ha un pannello di diagnostica a schermo, senza file.

---

## Cosa hanno loro e noi no (o non ancora)

**BMX**

- **Overclock spinto**: CPU fino a 2400 MHz sul Pi 4 e 3200 MHz sul Pi 5. Il CPU Clock di BMC64-NG si ferma a +20%; **Voltage Offset**, **Temperature Limit**, **Core Clock** e **V3D Clock** ci sono anche in BMC64-NG, nella voce Overclocking...
- **480p e 576p in HDMI** (720x480 a 60 Hz e 720x576 a 50 Hz) come modi video per tutte le macchine.
- **Scheda con due partizioni** (SYS: e USER:) e un disco di utilità (con ccgms e wic64) nel drive 8.
- **Menu**: le modifiche di sistema in attesa mostrate prima del riavvio (Pending system changes).
- **Monitor a schermo** di tastiera, mouse e GPIO.
- **Modem Hayes** con rubrica e suoni, RS-232 via rete, UP9600 e Userport.
- **Comandi via web** (spenti di fabbrica) e **aggiornamento da Internet**, dal sito di BMX.
- **Pannello di diagnostica a schermo** con fotogrammi, memoria e carico dei core.
- **Palette caricate dalla scheda** (`palettes`) e **contenuto dei dischi e dei nastri** visibile nell'elenco dei file.
- **Controller 8BitDo dichiarati** (Ultimate 2, Ultimate C, dongle V1 e V2, tastiere): in BMC64-NG il codice c'è, ma non sono stati provati.
- **Le novità di BMX dopo agosto**: elenco intelligente dei file, gestione automatica dei drive all'autostart, una mappa di tastiera per dispositivo, una pagina di informazioni sull'hardware.

**BMC64 di Randy**

- **Raspberry Pi Zero, Zero 2 W, 2 e 3**: BMC64-NG non gira su nessuno di questi.
- **Web UI e aggiornamento** (BMC64 5.2, su tutte le macchine dalla 5.3): un'interfaccia web sulla rete locale, spenta di fabbrica (Network > Web UI Settings), con un browser dei file della scheda, un editor dei programmi BASIC e dei file di configurazione, la pagina dei profili e un aggiornatore con `bmc64-update.zip`. BMC64-NG non li ha: il proprietario ha escluso la Web UI.
- **Uscita DPI** (VGA666, RGB sul pettine) con i suoi modi in `machines.txt`: il BMC64 la ha solo sui Raspberry fino al 3, non sul Pi 4 e sul Pi 5. In BMC64-NG il codice c'è, ma i modi in `machines.txt` no.
- **Scelta della partizione della scheda** (`disk_partition=` in `cmdline.txt`): c'è anche in BMC64-NG, ma senza la correzione del BMC64 5.2.0 (#369).
- **Le novità del BMC64 fino al 5.5.1** (10 novembre 2026): i profili (con **Profile Summary**), **Prefs** > **Boot Options** (**Warp after boot**, **Delay autostart for time sync**), **Auto-attach current disks at boot**, l'indirizzo IP a schermo quando la rete si collega (**Show IP Address on Connect**), il GPIO machine signal (`enable_gpio_machine_signal`), la mappa **Pimmodore** per una tastiera vera del C64 con la Keymmodore-64, e le tastiere francese, norvegese e latinoamericana.
