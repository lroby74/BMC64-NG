# BMC64-NG — Differenze con BMX e il BMC64

### Le differenze, una per una

Aggiornata al 3 ottobre 2026. Per il BMC64 vale la versione 5.1.1 (la guida e i sorgenti che abbiamo), con le novità del 5.2 dove sono citate. Per BMX vale la versione 2026.08.19, l'ultima del suo registro delle modifiche.

---

## I tre sistemi

- **BMC64** è il progetto di Randy Rossi: un emulatore Commodore che gira da solo sul Raspberry, senza sistema operativo (Circle), con il VICE 3.3. Funziona su Raspberry Pi Zero, Zero 2 W, 2 e 3. Il Pi 4 e i successivi non sono supportati.
- **BMC64-NG** parte da BMC64, ma usa il VICE 3.10 e gira su Raspberry Pi 4, 400, 5, 500 e 500+. I Raspberry più vecchi non ci sono più.
- **BMX** è un altro fork di BMC64, anche lui con il VICE 3.10, per Raspberry Pi 4 e 5 (e le loro varianti). Ha tolto i Raspberry fino al 3.

Con il BMC64 BMC64-NG ha in comune il menu, il formato del file `machines.txt`, le immagini REU salvate e caricate dal menu e la scrittura dei dischi sulla scheda (Prefs > Flush disk writes). Con BMX ha in comune due cose sole: il VICE 3.10 e lo shader CRT.

---

## Rispetto al BMC64 di Randy

**Macchine e Raspberry**

- **Raspberry**: BMC64-NG gira su Pi 4, 400, 5, 500 e 500+. Il BMC64 su Pi Zero, Zero 2 W, 2 e 3, e non sul Pi 4 in poi.
- **Emulatore**: BMC64-NG ha il VICE 3.10, il BMC64 il VICE 3.3 del 2018.
- **C64 cycle exact**: in BMC64-NG il C64 è cycle exact (X64SC), con la versione Std più leggera a scelta nel menu Machine > Switch. Il BMC64 ha solo il C64 normale.
- **SuperCPU**: BMC64-NG ha il C64 con SuperCPU a 20 MHz. Il BMC64 no.
- **Modelli del C64**: dal menu Model... il C64 può essere un C64C, un C64 old, un SX-64 o un C64 GS (la console a sole cartucce). Il BMC64 no: il suo VICE li conosce, ma il menu non li offre.
- **Plus4Emu**: in BMC64-NG gira su tutti i Raspberry supportati. Nel BMC64 è solo per il Pi 3.

**Riavvio, spegnimento, tasti**

- **Spegnimento**: il BMC64 si spegne togliendo la corrente. BMC64-NG si spegne e si riavvia anche dal menu Reset e dalla tastiera (**ALT + ESC**, **ALT + SHIFT + ESC**, con conferma), con **Fn + F10** sul Pi 400 e col tasto di accensione del Pi 500 e 500+.
- **Scorciatoie**: BMC64-NG ha una trentina di scorciatoie con ALT (dischi, nastro, cartucce, REU, modello del drive, volume, lettore SID, scanline, warp, barra di stato). Il BMC64 ha le combinazioni C= e CTRL con F1, F3, F5, F7 e i pulsanti a cui si assegna una funzione, che BMC64-NG ha mantenuto.
- **Tastiere**: BMC64-NG ha le disposizioni americana, italiana, inglese e tedesca (menu Keyboard > Layout), anche per scrivere nei campi del menu. Il BMC64 ha la americana, la posizionale e la Maxi.

**Video**

- **Schermo principale**: Video > Main HDMI Output sceglie su quale HDMI va l'immagine e con lei l'audio. Il BMC64 no: ha un solo schermo.
- **C128 su due monitor**: Video > Active Display su Two HDMI manda il VIC-II su un monitor e il VDC sull'altro. Il BMC64 no: VIC-II e VDC vanno su una sola uscita, con Active Display e PIP.
- **Uscita composita**: BMC64-NG ha **AV Composite OUT** solo sul Pi 4 B, dal menu Video (jack da 3,5 mm, cavo AV a 4 poli). Il BMC64 ha i modi compositi 480p e 576p per i suoi Raspberry.
- **VGA**: BMC64-NG ha i modi VGA 1024x768 a 50 e 60 Hz per ogni macchina (HDMI con adattatore VGA). Il BMC64 ha un'altra strada (DPI con VGA666, o 1920x1080 con `raster_skip`).
- **Shader CRT**: BMC64-NG ha lo shader di BMX, con geometria, scanline, maschera dei fosfori, bagliore, riflessi e i preset `.crt`. Il BMC64 ha il filtro CRT di davej.
- **Conferma del modo video**: dopo un cambio di macchina o di modo ci sono 15 secondi per confermare con un tasto, altrimenti torna com'era (Safety timer (15s) in fondo al menu Switch). Il BMC64 no: chiede conferma solo per i reset, e per tornare indietro ha il CTRL + F7 tenuto 5 secondi.

**Suono**

- **Motori del SID**: oltre a ReSid c'è **ReSIDfp** (Sound > SID Engine), con le sue regolazioni e il fruscio di fondo (Background Noise). Il BMC64 no: il suo SID Engine ha solo Fast e ReSid.
- **Campionamento**: SID Resampling di fabbrica su Resampling, il più fedele. Nel BMC64 sul Pi 3 si sceglie fra Fast, Interpolation e Fast Resampling e sul Pi Zero e 2 c'è solo Fast.
- **Filtri del reSID**: nel menu Sound la cartella reSID Filter Settings (Passband %, Gain %, Filter Bias) per il 6581 e per l'8580. Nella guida del BMC64 Passband e Gain devono restare quelli di fabbrica.
- **Stereo o mono** (Sound > Channels) con più SID.
- **SID Card su PET e Plus/4**, spenta di fabbrica. Il BMC64 no: il secondo SID c'è solo sul C64 e sul C128.
- **Lettore di file .SID**: ALT + S apre la cartella `/sids`; suona anche i file a due, tre, quattro e fino a otto SID (PSID v4E; coi brani a otto SID, come The Tuneful Eight, 24 barre e il reSID), con VU meter, tempo del brano, salti di 10 secondi, voci una per una e il lato di ogni SID (F1-F4, F1-F8 coi brani a otto SID). Nel BMC64 non c'è.
- **Un SID su un core a parte**: con più SID, il secondo gira su un altro core. Il BMC64 no: i suoi core in più calcolano solo le tabelle dei filtri all'avvio, poi restano fermi.
- **DAC USB**: Sound > Audio out sceglie fra Auto, Jack, HDMI e USB DAC dal menu. Il DAC si può collegare e scollegare a macchina accesa, e vanno anche i DAC a 48 kHz. Il BMC64 no: l'uscita audio (HDMI, jack o automatica) si sceglie solo in `machines.txt`, senza DAC USB.
- **Volume**: la barra a schermo, **ALT + "+"**, **ALT + "−"**, **ALT + M**.

**Clock**

- **Clock della CPU e della GPU dal menu** (CPU Clock e GPU Clock): la CPU -20%, -10%, di serie, +10%, +20%, la GPU di serie, +10%, +20%, per ogni modello; i MHz della GPU in cima al menu e nella barra di stato, accanto a quelli della CPU. Nel BMC64 il clock si cambia a mano in `config.txt` (le righe `arm_freq` e `over_voltage`, in commento nel file di esempio).

**Dischi, nastri, file**

- **Drive Commodore vero**: un 1541, 1571 o 1581 su adattatore XUM1541 o ZoomFloppy (ALT + 7). Il BMC64 no.
- **Floppy USB da PC** letti come FLP1 e FLP2. Il BMC64 no.
- **Ricerca nell'elenco dei file**: si scrivono le prime lettere, con i jolly (il punto interrogativo e l'asterisco), e se non trova cerca dentro il nome. Il BMC64 salta solo alla prima lettera scritta.
- **Immagini .REU**: **ALT + 0** carica un file dalla cartella `/REU`, **ALT + SHIFT + 0** lo stacca.
- **Le cartucce EasyFlash salvano** (Cartridge > Save EasyFlash Now, e da sole quando si stacca la cartuccia). Il BMC64 ha Save EasyFlash Now, ma non salva da solo quando si stacca la cartuccia.
- **Drive senza dischetto in Plus4Emu**: ALT + 2 e ALT + 3 mettono il 1541 o il 1551 e aprono l'elenco dei `.D64`.

**Rete**

- **Rete su tutte le macchine**, anche sul Pi 5 e sul Pi 500, dal menu Network. Il BMC64 ha la rete solo su C64 e C128.
- **WiFi dal menu**: WiFi Settings > WiFi SSID cerca le reti e le elenca con banda, canale e segnale; WiFi Country Code si scrive a mano; la password si scrive in chiaro nel riquadro.
- **Indirizzo fisso**: IP Address Mode su DHCP o Static (Static IP, Netmask, Gateway, DNS). Il Raspberry si presenta al router come BMC64-NG. Il BMC64 no: solo l'indirizzo automatico (DHCP).
- **Ora legale**: Daylight Saving (Off, On, Auto (EU)) sotto Timezone. Il BMC64 prende l'ora dalla rete e ha un fuso orario in minuti nel file delle impostazioni (`timezone_offset_minutes`); senza ora legale.
- **SD Card Sharing...**: la microSD vista dal PC (`\\BMC64-NG` o `\\indirizzo`, utente bmc64, password bmc64, cambiabile con Sharing Password). Il BMC64 no.
- **NAS**: la voce NAS (NAS IP Address, NAS User Name, NAS Password, SMB Protocol, Test NAS Connection) apre un NAS come sul MiSTer, con la condivisione `Mister` e le sue cartelle. Il BMC64 no.
- **Il modem come la Ultimate II+** (solo C64 e C128): Modem (SwiftLink) spento di fabbrica, Modem Address di fabbrica `DE00`. Riceve anche chiamate, sulla porta 3000 (RING, **ATA**, **ATS0=1**). Il modem del BMC64 chiama soltanto; il suo indirizzo di fabbrica è `DE00` nel 5.1.1 e `D700` dal 5.2.

**Comandi e controlli**

- **Spinner come paddle** (Joyports > USB Spinner (Paddle), con Spinner Sensitivity) su C64, C128 e VIC-20. Il BMC64 no: come paddle usa solo gli assi analogici dei gamepad.
- **Il nastro senza nastro**: senza un nastro montato i comandi del registratore non fanno niente e in basso compare **NO TAPE**.
- **Barra di stato**: le spie vere di ogni modello di drive, la traccia (anche le mezze tracce), il contanastro, e temperatura e frequenza sulla seconda riga (**ALT + SHIFT + O**).

**Quando qualcosa va storto**

- **Il guardiano**: se l'emulatore resta fermo dieci secondi compare un riquadro nero grande, in inglese (BMC64-NG HAS STOPPED), con l'indirizzo per segnalare il difetto su GitHub.
- **File sulla scheda**: `BMC64-DIAG.TXT`, `PASSI.TXT` (e `PASSI-PRIMA.TXT`), `BLOCCO.TXT` (e `BLOCCO-PRIMA.TXT`), `USB-LOG.TXT`. Il BMC64 ha il suo registro, acceso dal menu principale e dal file di configurazione.

---

## Rispetto a BMX

**Raspberry e macchine**

- **Raspberry**: BMC64-NG è provato su Pi 4, 400, 5, 500 e 500+. BMX prepara la scheda anche per il CM4 e, sul Pi 4, può costruire una sua versione vecchia a 32 bit.
- **Plus/4**: BMC64-NG ha due motori, il VICE e Plus4Emu. BMX solo il VICE.
- **Macchine**: BMC64-NG ha C64 (cycle exact e Std), SuperCPU, C128, VIC-20, Plus/4 e PET; BMX ha le stesse, con C64 cycle exact e SuperCPU segnati come sperimentali.
- **Cambio di macchina**: BMC64-NG dal menu Machine > Switch coi profili di `machines.txt`, con 15 secondi per confermare; BMX dal menu Machine con Apply & Reboot, coi profili di `machines.ini` (non legge `machines.txt`), senza conferma a tempo.

**Riavvio, spegnimento, tasti**

- **Riavvio e spegnimento**: BMC64-NG anche dalla tastiera (**ALT + ESC**, **ALT + SHIFT + ESC**, **Fn + F10** sul Pi 400, il tasto di accensione sul Pi 500 e 500+). BMX solo dal menu System.
- **Scorciatoie**: tutte quelle con ALT sono di BMC64-NG. BMX ha solo F12 e le otto combinazioni C= e CTRL + tasto funzione, che ha anche BMC64-NG.
- **Tastiere**: BMC64-NG americana, italiana, inglese e tedesca; BMX americana e tedesca.

**Clock**

- **Clock della CPU**: BMC64-NG dal menu CPU Clock, da -20% a +20% sul clock di serie di ogni modello, quindi anche più basso. BMX solo verso l'alto (Pi 4 da 1500 a 2400 MHz, Pi 5 da 2400 a 3200 MHz, a passi di 25 MHz).
- **Clock della GPU**: BMC64-NG dal menu principale (GPU Clock), Core e V3D insieme, di serie, +10% o +20%, coi MHz della GPU in cima al menu e nella barra di stato. BMX ha Core Clock e V3D Clock separati nella cartella Expert, anche lui solo verso l'alto (Pi 4 fino a 800 MHz, Pi 5 fino a 1200).

**Video**

- **Schermo principale**: in BMC64-NG Video > Main HDMI Output sceglie l'HDMI 0 o 1 per tutte le macchine. In BMX lo decide il firmware.
- **C128 su due monitor**: BMC64-NG sì (Two HDMI, con scambio). BMX mostra i due schermi su un monitor solo.
- **VGA**: BMC64-NG ha i profili 1024x768 a 50 e 60 Hz. BMX no.
- **Uscita composita**: BMC64-NG ha **AV Composite OUT** sul Pi 4 B, dal menu Video, e si torna alla HDMI da lì o con C= + F7 tenuti 5 secondi. BMX ha due modi compositi (576p e 480p) segnati come sperimentali.
- **Conferma del modo video**: 15 secondi per confermare, poi torna com'era (Safety timer (15s) la toglie). BMX non ce l'ha.
- **Modi video**: BMC64-NG ha 720p (1080p per il PET), 768x525 e 768x545 a frequenza esatta e VGA 1024x768. BMX ha 480p, 576p, 720p, 1080p e i suoi modi a frequenza esatta.
- **VIC-20 NTSC** col suo KERNAL americano: BMC64-NG sì. BMX ha il tempo NTSC, ma non sceglie un KERNAL NTSC.

**Suono**

- **Uscite**: BMC64-NG HDMI, DAC USB e jack da 3,5 mm del Pi 4. BMX HDMI e DAC USB.
- **SID Card su PET e Plus/4**: solo BMC64-NG ($8F00 o $E900 sul PET, $FD40 o $FE80 sul Plus/4).
- **Lettore di file .SID** (ALT + S) fino a otto SID: solo BMC64-NG. BMX non apre i `.SID`.
- **ReSIDfp** come terzo motore del SID (Sound > SID Engine), con Background Noise: BMC64-NG. BMX no: nel suo menu il motore ha solo FastSID e reSID.
- **Filtri del reSID**: BMC64-NG la cartella reSID Filter Settings valida per tutti i SID. BMX mostra i valori a schermo e regola il secondo SID per conto suo.
- **Un SID su un core a parte**: BMC64-NG con due, tre e quattro SID, e coi brani a otto SID i quattro pari; BMX solo con due.
- **Stereo o mono** (Sound > Channels): BMC64-NG. In BMX lo stereo va col secondo SID.

**File, dischi e cartucce**

- **Drive Commodore vero** (XUM1541, ZoomFloppy, ALT + 7): solo BMC64-NG.
- **Floppy USB** (FLP1 e FLP2) e i drive CMD FD-2000 e FD-4000: BMC64-NG.
- **Ricerca nell'elenco dei file** con i jolly (il punto interrogativo e l'asterisco): BMC64-NG.
- **Immagini .REU**: BMC64-NG con **ALT + 0** dalla cartella `/REU` e dal menu. BMX solo dal menu della REU.
- **Programmi .prg**: in BMC64-NG partono anche senza drive. BMX usa un dischetto temporaneo e vuole un drive.
- **File .SID**: solo BMC64-NG.
- **Salvataggio dei dischi**: Prefs > Flush disk writes in BMC64-NG, e le cartucce EasyFlash salvano.

**Rete**

- **Rete**: BMC64-NG ha Wi-Fi o Ethernet su tutte le macchine, anche sul Pi 5 e 500; WiFi dal menu con elenco delle reti; indirizzo automatico o fisso; ora legale; ora presa dalla rete. BMX ha Wi-Fi o Ethernet con indirizzo fisso e ricerca delle reti.
- **SD Card Sharing...**: la microSD vista dal PC via SMB (`\\BMC64-NG`), con il Raspberry nella cartella Rete di Windows. BMX no.
- **NAS come sul MiSTer**: solo BMC64-NG. BMX no.
- **Modem come la Ultimate II+** (SwiftLink, chiama e riceve): BMC64-NG, solo C64 e C128. BMX ha un modem Hayes con rubrica e suoni, RS-232 via rete, UP9600 e SwiftLink.
- **Comandi via web e aggiornamento da Internet**: solo BMX. In BMC64-NG niente controllo via web, per scelta.

**Comandi e controlli**

- **Spinner come paddle** (Joyports > USB Spinner (Paddle)): BMC64-NG su C64, C128 e VIC-20. BMX non ha niente sullo spinner (cercato nei suoi sorgenti).
- **Il nastro senza nastro**: BMC64-NG mostra **NO TAPE** e non fa niente. In BMX i comandi del nastro arrivano al VICE senza controllo.
- **Il suono del nastro** (Tape sound emulation) non cancella la musica del SID in BMC64-NG; nel file del VICE di BMX il difetto c'è ancora (nostro confronto del 19 settembre).
- **Correzioni del VICE posteriori alla 3.10**: BMC64-NG ne ha quattro prese da BMX; BMX ne ha di più (vedi in fondo).

**Quando qualcosa va storto**

- **BMC64-NG** scrive sulla scheda `BMC64-DIAG.TXT`, `PASSI.TXT`, `BLOCCO.TXT` e `USB-LOG.TXT`, e ha il **guardiano** con il riquadro nero BMC64-NG HAS STOPPED. BMX ha un pannello di diagnostica a schermo, senza file e senza guardiano.

---

## Cosa hanno loro e noi no (o non ancora)

**BMX**

- **Overclock spinto**: CPU fino a 2400 MHz sul Pi 4 e 3200 MHz sul Pi 5, **Voltage Offset**, **Temperature Limit** e, nella cartella Expert, **Core Clock** e **V3D Clock** (clock della GPU). Il CPU Clock di BMC64-NG si ferma a +20%. Core e V3D li regola anche BMC64-NG, col suo GPU Clock (di serie, +10%, +20%).
- **480p e 576p** come modi video per tutte le macchine.
- **Scheda con due partizioni** (SYS: e USER:), un disco scelto da montare a ogni accensione e un disco di utilità (con ccgms) nel drive 9.
- **Menu**: i cinque posti di accesso rapido (Quick Access), il menu col mouse, il menu ingrandibile, le modifiche di sistema in attesa mostrate prima del riavvio (Pending system changes).
- **Editor delle mappe della tastiera** e monitor a schermo di tastiera, mouse e GPIO.
- **Modem Hayes** con rubrica e suoni, RS-232 via rete, UP9600 e Userport.
- **Comandi via web** (spenti di fabbrica) e **aggiornamento da GitHub**.
- **Pannello di diagnostica a schermo** con fotogrammi, memoria e carico dei core.
- **Palette caricate dalla scheda** (`palettes`) e **contenuto dei dischi e dei nastri** visibile nell'elenco dei file.
- **Correzioni del VICE posteriori alla 3.10** in più delle nostre quattro, come il DMA della REU della SuperCPU e il glitch del POT del mouse 1351 (confronto del 19 settembre, punto 4.3).
- **Controller 8BitDo dichiarati** (Ultimate 2, Ultimate C, dongle V1 e V2, tastiere): in BMC64-NG non provati.

**BMC64 di Randy**

- **Raspberry Pi Zero, Zero 2 W, 2 e 3**: BMC64-NG non gira su nessuno dei tre.
- **Web UI e aggiornamento** (BMC64 5.2): un'interfaccia web sulla rete locale con un browser dei file della scheda, un editor dei programmi BASIC e un aggiornatore con `bmc64-update.zip`. BMC64-NG non li ha: il proprietario ha escluso la Web UI.
- **Uscita DPI** (VGA666, RGB sul pettine) con i suoi modi in `machines.txt`: in BMC64-NG il codice c'è, ma i modi in `machines.txt` no.
- **Scelta della partizione della scheda** (`disk_partition=` in `cmdline.txt`): c'è anche in BMC64-NG, ma senza la correzione del BMC64 5.2.0 (#369).
- **Immagini grandi di CMD HD e IDE64** (oltre 32 MB) lette dalla scheda a mano a mano: nel BMC64 dal 5.2; in BMC64-NG, al 16 settembre, ancora da fare.
