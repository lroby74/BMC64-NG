# BMC64-NG

### Un Commodore vero dentro un Raspberry Pi

---

## Che cos'è

BMC64-NG trasforma un Raspberry Pi delle generazioni piú recenti (dal 4 in poi) in un computer Commodore. 
Non è un programma che gira dentro Linux: **è l'unica cosa che gira nel Raspberry**.
Non c'è un sistema operativo sotto, non c'è un desktop, non c'è niente da
avviare. Accendi, e dopo qualche secondo ** hai davanti lo schermo blu del C64 **.

Quando hai finito, spegni togliendo la corrente. Come si faceva nel 1982.
Ma se vuoi fare le cose in modo piú realistico e fedele, premi il o i tasti giusti (dipende dal modello di PI)
e il Raspberry PI si spegnerá comodamente senza staccare la spina.
---

## Perché è nata questa versione specifica

Il progetto originale, **BMC64**, è di Randy Rossi, che ha avuto l'idea e l'ha
fatta funzionare ma purtroppo ha dei limiti:
1) È nato per i Raspberry fino al PI 3 e varianti, che si troveranno con sempre maggiore difficoltá sul mercato e comunque
offrono prestazioni limitate rispetto ai modelli usciti in seguito. Solo dalla versione 5.4 (ottobre 2026) gira anche su PI 4 e PI 5, ancora in prova (beta).
2) A causa di quanto detto al punto 1, non puó essere aggiornato alla versione piú recente del Vice 3.10, ed é tutt'ora legato alla 3.3 del 2018.
3) Il C64 in modalità Cycle Exact, che dona maggiore fedeltà all'emulazione, lo fa solo su PI 4 e PI 5 dalla 5.4: sul PI 3 (e sui modelli ancora più 
limitati) non c'è la potenza per farlo girare.
4) Ci sono un sacco di feature e di comoditá che mancano sul BMC64 originale e che qui é stato possibile inserire, rendendo di fatto 
l'esperienza utente molto piú piacevole e immersiva.

BMC64-NG é tutto questo: é emulazione aggiornata, supporto ai Raspberry moderni, e parecchie cose nuove.

---

## Che cosa lo distingue dalla versione precedente BMC64

**L'emulazione è quella vera.** Sotto c'è **VICE 3.10**, l'emulatore
Commodore più accurato che esista, quello che gli sviluppatori di demo usano
come riferimento. Non una riscrittura semplificata: il VICE, intero.

**La fedeltà viene prima della comodità.** Se il Commodore vero faceva una
cosa strana, qui la fa anche BMC64-NG. Un difetto che aveva anche la macchina
originale non è un difetto da riparare: è come deve essere.

**Il ciclo esatto.** Sul C64 l'emulazione é *cycle exact*, quella
che riproduce il comportamento del VIC-II ciclo per ciclo — serve per le demo
e per i giochi che spingono il chip grafico oltre le sue specifiche. 
Anche su un Pi 4 a frequenza di serie regge senza problemi.

**Anche il drive vero.** Con un adattatore XUM1541 puoi collegare un
**Commodore 1541 fisico, un 1571 o un 1581 (anche piú di uno in cascata se impostati su diversi canali)** e leggere i tuoi dischetti originali.
E' anche possibile usare un PI1541 che emula alla perfezione un 1541 e un 1581

**Nota:** una XUM1541 fatta con un Arduino Pro Micro potrebbe non essere compatibile: con quella il drive Commodore vero funziona solo alla prima accensione a freddo, e dopo un cambio di macchina o un riavvio del Raspberry smette di rispondere. BMC64-NG è provato con una ZoomFloppy.

---

## Le macchine

| macchina | note | 
|---|---|
| **Commodore 64** | nella versione *cycle exact* (massima precisione) |
| **Commodore 64 Std** | la versione normale del VICE, non cycle exact: piú leggera. Nel menu **Switch** le voci del C64 si chiamano **X64SC** (cycle exact) e **Std** |
| **Commodore 64 con SuperCPU** | l'acceleratore della CMD a 20 MHz, con la ROM libera del VICE |
| **Commodore 64 GS** | la console a sole cartucce, dal menu **Model...** del C64. Come sulla console vera funzionano solo i tasti "dell'interruttore": F12, ALT + R, ALT + SHIFT + R, ALT + ESC, ALT + SHIFT + ESC, piú ALT + C e ALT + SHIFT + C per cambiare cartuccia e ALT + O e ALT + SHIFT + O per la barra di stato. Gli altri dicono NOT ON C64 GS |
| **Commodore 128** | con entrambi gli schermi: il VIC-II a 40 colonne e il VDC a 80 |
| **Commodore VIC 20** | 
| **Commodore Plus/4** | con due motori diversi: quello del VICE e **Plus4Emu**, reputato piú fedele |
| **Commodore PET** | 

Dal menu **Model...** il Commodore 64 può essere anche un **C64C**, un **C64 old** o un **SX-64**, oltre al C64 GS. La SuperCPU va al 100% anche sul Pi 4 e sul Pi 400.

Sul **PET** e sul **Plus/4** la voce del modello dice quello che sta girando, per esempio **Model: 8032** o **Model: Plus/4 (PAL)**, e cambia appena se ne sceglie un altro (**Model: Custom** se le impostazioni non fanno nessun modello).

---

## I modelli su cui è stato testato e certificato funzionante

- Raspberry Pi 4 Model B
- Raspberry Pi 400
- Raspberry Pi 5
- Raspberry Pi 500
- Raspberry Pi 500+

Serve una microSD e una tastiera USB (solo per il 4 e il 5). Il resto è opzionale.

---

## Le funzioni

### Dischi, nastri, cartucce

- **Dischetti** fino a **quattro unità** contemporaneamente (8, 9, 10, 11).
  Quindici formati: `.d64` `.d67` `.d71` `.d80` `.d81` `.d82` `.d1m` `.d2m`
  `.d4m` `.g64` `.g71` `.g41` `.p64` `.x64` `.dhd`.
- **I `.nib`**, le copie a basso livello fatte con nibtools, anche dei dischi protetti: si scelgono come gli altri dischetti, da **Attach Disk...** o da **Autostart Prg/Disk...**. BMC64-NG li converte in `.g64` col codice del nibconv di nibtools, nella cartella `/nib-g64` della scheda, e monta quello; il `.nib` non si tocca. Il `.g64` si rifà a ogni montaggio del `.nib`: per tenere quello che un gioco salva sul dischetto, si monta il `.g64` della cartella `/nib-g64`. Plus4Emu non li legge: non legge nemmeno i `.g64`.
- **Modelli di drive** scelti uno per uno: 1541, 1541-II, 1570, 1571, 1581,
  o nessuno. Ognuna delle quattro unità può essere un modello diverso.
  Piú il 1551 sul C16 / Plus4
- **I rumori del dischetto che entra e che esce**: con **Drive sound emulation** acceso (menu **Prefs**), montare e smontare un'immagine fa il rumore del dischetto che entra e che esce dal drive vero, insieme a quelli del motore e della testina. Vengono dalla Ultimate-II+. Il 1581 ha i suoi (motore, testina, dischetto dentro e fuori), presi anche loro dalla Ultimate-II+: è un 3,5 pollici e si sente molto meno.
- **Plus4Emu, il drive senza dischetto**: un 1541 o un 1551 senza dischetto non trova niente, come quello vero. ALT + 2 (1541) e ALT + 3 (1551) fanno come ALT + 4 col 1581: mettono il drive e aprono subito l'elenco dei `.D64` da attaccare. I file della cartella della scheda si leggono con **IEC FileSystem** acceso (nel menu, Drive 8 o Drive 9), e la scelta resta salvata. In Plus4Emu non ci sono i suoni del drive e del registratore: il suo motore non li ha. Nel Plus/4 del VICE ci sono.
- **Emulazione vera del 1541**: il drive ha il suo processore e il suo
  programma, esattamente come l'originale. Serve per i caricatori veloci e per
  le protezioni dei giochi d'epoca.
- **I drive che parlano fra loro**: con due o più drive accesi, i programmi che fanno parlare due drive sul cavo seriale senza passare dal computer ora funzionano: prima di toccare il cavo un drive porta gli altri alla sua ora, come fa il computer coi drive. Con un drive solo non cambia niente; con due o più drive accesi il Raspberry fa un po' più di calcolo.
- **Drive fisico** via adattatore XUM1541: colleghi un 1541 vero o altro drive Commodore e leggi i
  tuoi dischetti.
- **La XUM1541 fatta con un Arduino Pro Micro** potrebbe non essere compatibile: con quella il drive Commodore vero funziona solo alla prima accensione a freddo, e dopo un cambio di macchina o un riavvio del Raspberry smette di rispondere. BMC64-NG è provato con una ZoomFloppy.
- **Il drive vero si sceglie anche senza le ROM dei drive**: nel menu **Drives**, **Change Model...** di un'unità ha la voce **Real drive** (grigia se l'adattatore XUM1541 non c'è), come **ALT + 7**. I modelli emulati (1541, 1571, 1581...) si possono scegliere solo se nella cartella `DRIVES` della scheda ci sono le loro ROM. Fino al 6 novembre 2026, senza nessuna ROM dei drive, la voce **Real drive** mancava.
- **I programmi col loro caricatore veloce, sul drive fisico**: molti giochi e i copiatori portano nel drive un caricatore loro, che parla col computer su fili che l'adattatore XUM1541 non porta: via adattatore non possono funzionare, ed è lo stesso col VICE sul PC. La macchina però non si ferma: il drive finisce il file da solo (sulla barra di stato **FAST STOP**), il programma resta ad aspettare e il Reset rimette tutto a posto, col dischetto del Pi1541 ancora montato. Quello che si carica dal BASIC senza un caricatore suo va, col caricatore veloce di BMC64-NG.
- **Drive on XUM1541: Pi1541 o drive Commodore**: nel menu **Drives** scegli cosa c'è sull'adattatore XUM1541. Con **Commodore**, per un 1541 vero o un altro drive Commodore, il drive riceve il reset della linea come su un Commodore vero: a ogni accensione e cambio di macchina e a ogni reset. Così un drive rimasto bloccato dalla macchina di prima riparte da solo, senza spegnerlo. Con **Pi1541 + Commodore** tieni sullo stesso cavo un drive Commodore e un Pi1541, per esempio il 1541 sull'unità 8 e il Pi1541 sulla 9 (ognuno col suo numero, e tutte e due le unità su drive fisico): i reset fanno come col Pi1541, e all'accensione la linea si tira solo se un drive è rimasto bloccato. Con **Pi1541**, la scelta di fabbrica, i reset non toccano la linea, perché il Pi1541 smonterebbe il .D64. Un Pi1541 che al reset riparte come il drive vero, senza smontare il .D64, va con **Commodore**, anche insieme a un drive Commodore. La scelta è una sola per tutte le macchine e resta da sola, senza **Save settings**.
- **Directory of Drive on XUM1541**: nel menu **Drives** mostra la directory del dischetto di ogni drive sull'adattatore XUM1541 (un Pi1541 o un drive Commodore vero; con più drive, una dopo l'altra), letta direttamente dai settori del dischetto, come fanno i programmi di copia. Fa vedere tutte le voci, anche quelle che il **LOAD"$"** del BASIC non mostra: alcuni dischetti hanno nei nomi dei caratteri che fermano il **LIST** a metà (il Super Kit 1541, per esempio), e il C64 vero fa lo stesso. I caratteri di controllo nei nomi si vedono in negativo, come sul C64. Le righe non fanno partire niente: **Sinistra** o **ESC** tornano indietro. Legge i dischetti del 1541, del 1570, del 1571 e del 1581; se il drive è occupato o non risponde, la pagina lo dice.
- **Il caricamento veloce con due drive sul cavo**: il caricatore veloce di BMC64-NG usa un filo che sentono tutti i drive accesi, quindi va solo con un drive da solo sul cavo. Con un altro drive acceso il file si carica alla velocità normale, e la barra di stato lo dice: **FAST OFF: DRIVE 9 ON BUS TOO** (col numero dell'altro drive). Spegni l'altro drive e la velocità torna.
- **Reset Drive on XUM1541**: nel menu **Drives** rimette in piedi l'adattatore e fa ripartire ogni drive che c'è sopra, e la pagina dice drive per drive com'è andata. Il Pi1541 riparte col comando UJ e l'immagine resta montata. Il drive Commodore riceve il reset della linea, tenuta finché il drive la prende (fino a 3 secondi); se non riparte, si manda UJ. **restarted (bus reset)** vuol dire che il reset della linea gli è arrivato, e il LED si è acceso per un attimo; **restarted with UJ** che la linea non gli arriva (il cavo, o l'ingresso del reset del drive) ed è ripartito col comando, senza accendere il LED, come fa un 1541 vero con UJ; **does not answer** che va spento e riacceso; **not on the bus** che a quel numero non c'è nessuno.
- **Reset Drive on XUM1541, quanto ci mette il drive**: col drive Commodore la linea del reset resta tenuta finché il drive la prende (quando la prende tiene i fili DATA e CLOCK), fino a 3 secondi, e la pagina dice dopo quanto: **Bus reset taken, N ms**. **Bus reset not taken, 3 s** vuol dire che in 3 secondi nessun drive l'ha presa: il filo del reset non gli arriva (il cavo, o l'ingresso del reset del drive) e si prova col comando UJ; se il drive non risponde nemmeno a quello, va spento e riacceso. **Bus reset held N ms** vuol dire che il bus era già tenuto da un drive fermo: la linea si tiene un secondo, perché non si può vedere quando la prende. Dopo un reset o un comando UJ, BMC64-NG aspetta che il drive abbia finito di ripartire (lascia libero il filo DATA) prima di chiamarlo, perché un 1541 vero chiamato mentre riparte si ferma; e un drive fermo che tiene il bus costa 2 secondi, non 15 a ogni comando.
- **Real Drive: Try the fast loader**: nel menu **Drives** carica due volte il primo file del dischetto nel drive sull'adattatore XUM1541, col caricatore veloce di BMC64-NG e nel modo di sempre, e dice quanto va ciascuno e se i byte sono gli stessi. Va solo con un 1541 da solo sul cavo: con un altro drive acceso, o con un 1570, un 1571 o un 1581, la pagina lo dice. Se il drive non risponde, la pagina dice **Drive 8 does not answer.** (col numero dell'unità) e consiglia **Reset Drive on XUM1541** o di spegnere e riaccendere il drive.
- **Creazione di dischetti vuoti** dal menu.
- **Il dischetto a ogni accensione**: nel menu **Drives**, **Default disk**: **Disk image** sceglie un'immagine e **Drive** l'unità (8, 9, 10 o 11; con **None** non si usa). A ogni accensione quell'immagine si attacca da sola, se l'unità è vuota, e se l'unità è spenta si accende da sola. Di serie non c'è nessuna immagine e non succede niente. Si salva con **Save settings**. Con **PRG loading** su **Via disk image**, un `.prg` lanciato con l'autostart mette il suo dischetto sull'unità 8 al posto di questo; con **Direct to RAM** no.
- **I dischetti scritti arrivano da soli sulla scheda**: in **Prefs**, **Flush disk writes** sceglie quando quello che si scrive su un dischetto finisce nel file sulla scheda: **On write** (di fabbrica) poco dopo che il drive ha smesso di scrivere, **On detach** solo quando si stacca il dischetto o il drive passa a un'altra traccia, **On write (log)** come On write, e lo scrive nel registro. Se un gioco scatta subito dopo un salvataggio, prova **On detach**. Con Plus4Emu la voce non c'è. Fra le scelte dei tasti rapidi c'è anche **Flush Disks**, che lo fa subito.
- **Due lettori di floppy USB**: nell'elenco dei file compaiono come **FLP1** e **FLP2**. I file e le immagini dei dischi si mettono sul floppy dal PC (dischetti FAT), e da BMC64-NG si usano come quelli della scheda.
- **Nastri** `.tap` e `.t64`, con il contanastro, i comandi del registratore e
  l'avanzamento veloce.
- **Il suono del nastro**: in **Prefs**, **Tape sound emulation** (spento di fabbrica) fa sentire il nastro che carica, come col registratore vero, e **Tape sound emulation volume** ne regola il volume. Si sente anche insieme alla musica del SID.
- **Cartucce** `.crt` e `.bin`.
- **La cartuccia a ogni accensione**: sul C64 e sul C128, per ritrovare una cartuccia a ogni accensione bisogna renderla predefinita dal menu **Cartridge** con la voce **Set current cart default**, che salva da sola. Vale anche al contrario: se la stacchi e non la vuoi piú all'accensione, dopo averla staccata scegli di nuovo **Set current cart default**.
- **Le cartucce EasyFlash salvano**: quello che il gioco scrive nella flash torna nel suo file .crt con la voce **Save EasyFlash Now** del menu **Cartridge**, da solo quando si stacca la cartuccia o se ne mette un'altra, e anche quando si spegne, si riavvia o si cambia macchina, se il gioco ci ha scritto.
- **Le cartucce Magic Desk Plus** (C64 e C128): quello che il programma scrive nella SRAM e nella EEPROM della cartuccia resta in due file accanto al `.crt`, con lo stesso nome e le estensioni `.srm` e `.eep`, e alla volta dopo si ritrova. Nel menu **Cartridge**, **Save Magic Desk Plus Now** li scrive subito; **Auto-save Magic Desk Plus**, acceso di fabbrica perché la cartuccia vera tiene la memoria anche da spenta, li scrive da solo quando si stacca la cartuccia o se ne mette un'altra, e anche quando si spegne, si riavvia o si cambia macchina.
- **Programmi** `.prg` caricati direttamente.
- **Autostart**: scegli il file e parte da solo, senza scrivere niente.
- **L'Autostart del PET**: anche sul PET c'è **Autostart Prg...**, solo coi `.prg`: la lista mostra soltanto quelli, e il programma scelto si carica e parte da solo.
- **L'Autostart del PET come il LOAD vero**: un `.prg` avviato da **Autostart Prg...** trova la memoria come dopo un LOAD vero dal drive: l'avvio non tocca più i due byte `$C7`/`$C8`, che dopo l'accensione puntano alla tabella del CRTC dell'editor e che il LOAD vero lascia stare. Così la demo *No Pets Allowed* sul 4032, che rimette lo schermo normale leggendo quella tabella, va avanti. Fino al 9 novembre 2026 l'avvio ci scriveva l'indirizzo del programma, e quella demo, con A o con B, dopo quindici secondi perdeva lo schermo.
- **La chiave PetSpeed del PET**: nel menu **Machine** del PET la voce **PetSpeed dongle** mette sulla porta del nastro la chiave che il compilatore PetSpeed vuole per partire. Come sul ferro la chiave occupa la porta: finché c'è, il registratore non c'è. Si salva con **Save settings**.
- **Il PET col CRTC senza sincronismo**: se un programma lascia il CRTC del PET senza sincronismo verticale, la macchina non si ferma: come il monitor vero, che senza sincronismo continua a girare, lo schermo si ridisegna da solo e il menu risponde. Fino all'8 novembre 2026 in quel caso la macchina sembrava bloccata, con un rumore continuo, e andava spenta.
- **Il PET a piena velocità quando il quadro si accorcia**: se un programma riprogramma il CRTC e il quadro diventa più corto (la demo *No Pets Allowed* sul 4032 con la SID, due minuti e ventitré secondi dopo il tasto), l'immagine resta della stessa grandezza, le righe che mancano restano nere e l'emulazione va a piena velocità, anche dopo il reset. Fino al 9 novembre 2026 in quel caso ogni quadro lo componeva il processore invece del chip video, e l'emulazione scendeva al 20-25% fino al riavvio.
- **Il PET col quadro sempre al suo posto**: se un programma riprogramma il CRTC (la demo *No Pets Allowed* sul 4032), l'immagine resta al centro dello schermo dall'inizio alla fine, e dopo un reset il BASIC torna dov'era all'accensione. Fino al 10 novembre 2026, una trentina di secondi dopo il tasto, l'immagine scendeva di 56 righe, tagliata in basso (alla fine della demo saliva invece di una trentina), e restava spostata anche dopo il reset, fino al riavvio o a un altro giro della demo.
- **Il lettore della microSD che si riprende**: se un trasferimento dalla scheda si ferma a metà, BMC64-NG rimette a posto il lettore, riprende la scheda e rifà la lettura o la scrittura da capo, come fa Linux. Vale per tutto quello che usa la scheda, **SD Card Sharing** compresa. Quando succede, `SMB-LOG.TXT` sulla scheda dice quante volte il lettore si è ripreso da solo e dove si era fermato.
- **Il dischetto dell'autostart**: con l'emulazione vera del drive, un `.prg` lanciato con l'autostart il VICE lo mette in un dischetto che scrive nella radice della scheda, `autostart-` e il nome della macchina, per esempio `autostart-PLUS4.d64`. Si può cancellare: alla volta dopo si rifà.
- **PRG loading**: la voce **PRG loading** del menu principale, sotto **Autostart Warp** (sul PET sotto **Autostart Prg...**), dice come parte un `.prg` con l'autostart. **Via disk image** (di serie) fa come sopra: il dischetto dell'autostart se c'è il drive 8, e senza drive dritto in RAM. **Direct to RAM** lo mette sempre dritto in RAM, anche col drive acceso. Si salva con **Save settings**.
- **I nomi di file lunghi**: dischi, nastri, cartucce e programmi con nomi fino a 255 caratteri, il massimo della scheda, si attaccano e partono dal menu come gli altri. Prima un nome molto lungo bloccava la macchina. Nell'elenco un nome che non sta nella riga scorre quando ci passi sopra col cursore, così lo leggi tutto.
- **Ricerca nei file**, come sulla Kung Fu Flash 2: dentro una cartella scrivi le prime lettere del nome (per esempio `dra`): già mentre scrivi, senza INVIO, restano solo i file e le cartelle che cominciano cosí, maiuscole o minuscole, e INVIO porta sul primo. Si possono usare i jolly della KFF2: il punto interrogativo vale un carattere qualunque, e `*lair` trova i nomi che contengono "lair". DEL cancella; cambiando cartella la ricerca sparisce.
- **Se nessun nome comincia cosí**, la stessa ricerca si rifà dentro al nome: scrivendo `12` fra i file di Dragon's Lair esce `DrL-12-850.reu`, e scrivendo `II` esce `Turrican II.tap`.
- **I file del Mac**: i file `._qualcosa` che un Mac lascia sulla scheda accanto agli altri non compaiono negli elenchi del menu, come nel BMC64 di Randy.
- **Immagini .REU**: i giochi e le demo che usano la REU la caricano da un file. I file vanno nella cartella `/REU`: **ALT + 0** ne carica uno, **ALT + SHIFT + 0** lo stacca.
- **Il contenuto della REU nel suo file**: nel menu **Ram Expansion Unit (REU)**, cartella **Ram Image (optional)**, la voce **Auto-save image**, spenta di fabbrica. Accesa, il file .REU si riscrive col contenuto della REU quando si spegne o si riavvia, dal menu o coi tasti, e quando si cambia macchina. Staccando la corrente non si salva. Per le immagini dei giochi, come Dragon's Lair, conviene lasciarla spenta.
- **Se il file non si può scrivere** (per esempio perché è di sola lettura), allo spegnimento il Raspberry resta acceso e lo dice (NOT SAVED); chiedendo di nuovo di spegnere, si spegne lo stesso.
- **I dischetti prima di spegnere o riavviare**: quando si spegne o si riavvia, dal menu o coi tasti, e quando si cambia macchina, i dischetti e il nastro si staccano prima del reset: quello che il drive aveva appena scritto, e che il VICE teneva ancora in memoria, finisce sulla scheda.

### Video

- **Scanline** regolabili da 0 a 100, per l'aspetto del monitor a tubo.
- **Rapporto d'immagine** 4:3 o 16:9, commutabile al volo.
- **L'immagine spostata non rallenta**: con **H Center** e **V Center**, o allargandola, l'immagine può uscire un po' dai bordi: la parte fuori si taglia e basta. Prima, appena usciva anche di una riga, l'emulazione poteva scendere al 50%.
- **Il VIC-20 NTSC col suo KERNAL**: nei modi NTSC del VIC-20 parte da solo il KERNAL americano (901486-06), che mette lo schermo al centro come su un VIC-20 NTSC vero; nei modi PAL quello europeo (901486-07). Una ROM diversa scelta dal menu resta la tua.
- **Il Plus/4 NTSC col suo KERNAL**: nei modi NTSC del Plus/4 del VICE (e del C16) parte da solo il KERNAL americano (318005-05), come su un Plus/4 NTSC vero; nei modi PAL quello europeo (318004-05). Una ROM diversa scelta dal menu resta la tua.
- **Regolazione dell'immagine**: luminosità, contrasto, colore, gamma, e la
  geometria (larghezza, altezza, centratura).
- **I tuoi preset dello shader CRT**: nel menu **CRT Shader**, **Save as preset...** salva le regolazioni di adesso in un file `.crt` della cartella `/crt`, col nome che scrivi (se c'è già, il menu chiede se sovrascriverlo). Se scegli un altro preset con delle regolazioni cambiate e non salvate, il menu chiede cosa farne: **Save as preset...**, **Discard changes** o **Cancel**. **Save settings** ricorda il preset, o le regolazioni cambiate a mano, e all'avvio si riparte da lì. Un file di impostazioni della V1.3 non lo sa ancora: all'avvio vale il preset **Default** finché non salvi una volta.
- **Il doppio schermo del C128**: il VIC-II a 40 colonne e il VDC a 80 sono
  due uscite separate, ognuna con le sue regolazioni. Si commuta con un tasto.
  E se le colleghi due monitor, avrai il doppio output 40 e 80 colonne proprio come allora.
- **Il doppio schermo anche in 16:9**: con due monitor ognuno dei due schermi del C128 riempie tutto il televisore, in 4:3 come in 16:9.
- **I due monitor dal menu**: nel menu **Video** del C128, **Active Display** su **Two HDMI** manda il VIC-II su una HDMI e il VDC sull'altra (la prima volta il menu chiede di riavviare). Prima metti **Second Display Present** su **On**: finché è su **Off**, girando **Active Display** con le frecce, con INVIO o col tasto **Cycle Display**, **Two HDMI** si salta e non chiede niente. I due schermi li disegna il core 3, così il core dell'emulazione resta tutto alla macchina.
- **L'uscita HDMI principale**: nel menu **Video** la voce **Main HDMI Output** sceglie su quale delle due porte HDMI va lo schermo principale, e con lui l'audio HDMI. Con INVIO il menu chiede di riavviare, e la scelta vale da lí in poi. Sul C128 con due monitor scambia i due schermi. Se all'accensione la porta scelta non ha il monitor o non risponde, lo schermo va sull'altra.
- **La risoluzione HDMI, 720p o 1080p**: nel menu **Video** la voce **HDMI Resolution** sceglie fra **720p** e **1080p** per la macchina che sta girando, PAL o NTSC come adesso. Con INVIO il menu chiede di riavviare. Nel menu **Switch** ogni macchina ha le sue voci **720p** e **1080p** una accanto all'altra. La voce non c'è col composito e coi modi VGA.
- **La scheda nuova parte a 1080p**: una scheda fatta con l'archivio intero della V1.4 parte a 1080p, e **HDMI Resolution** riporta a 720p. Chi aggiorna con l'archivio Update tiene quello che ha.
- **Lo shader CRT a 1080p sul Pi 4 e sul 400**: la GPU disegna al massimo un quadro 720p e lo scalatore lo porta a 1080p con un filtro morbido, così a 1080p la GPU fa lo stesso lavoro che a 720p. Fino al 6 novembre 2026, a 1080p con gli effetti dello shader accesi, la GPU non finiva un quadro in tempo e la macchina andava a metà velocità. A 720p e sul Pi 5 non cambia niente.
- **Lo shader CRT con tutti gli effetti, dal Pi 4 in poi**: se la GPU non fa in tempo a disegnare un quadro (succede con molti effetti accesi insieme: sul Pi 4 e sul 400 già a 720p, sul Pi 5 e sul 500 a 1080p), BMC64-NG disegna un quadro più piccolo, quanto basta perché ci stia, e lo scalatore lo porta alla grandezza dello schermo. Così la macchina resta al 100%, con l'immagine un po' più morbida; spegnendo effetti, la GPU torna a poco a poco al quadro intero. Con le regolazioni di serie dello shader non cambia niente. Fino al 7 novembre 2026, con tutti gli effetti accesi, la macchina andava a metà velocità.
- **Sul Pi 5 e sul 500** il quadro più piccolo dello shader CRT è solo più stretto: l'altezza resta intera, e lo scalatore lo allarga solo in orizzontale.
- **Lo shader CRT con l'immagine spostata**: se **H Center** o **V Center** portano una parte dell'immagine fuori dallo schermo (per esempio sul VIC-20, per nascondere un bordo), lo shader CRT resta acceso e lavora sulla parte che si vede, tagliata come senza shader. Fino all'8 novembre 2026 in quel caso lo shader si spegneva da solo e l'immagine restava senza effetti.
- **L'uscita composita del Pi 4 B**: nel menu **Switch** ogni macchina ha anche **PAL Composite 576p@50Hz** e **NTSC Composite 480p@60Hz**. Dopo il riavvio immagine e suono escono dal jack da 3,5 mm (cavo AV a 4 poli) col segnale della macchina vera, 50 o 60 Hz non interlacciato; la HDMI si spegne e lo shader CRT non c'è. I bordi partono come sul BMC64 originale, cioè come un C64 vero su un monitor a tubo; se li ritocchi dal menu **Video** e salvi, il composito tiene i suoi, separati da quelli della HDMI. La velocità segue il televisore: il suo passo si misura all'accensione. Si torna alla HDMI con una voce HDMI dello **Switch**, oppure tenendo premuti il tasto Commodore (di fabbrica CTRL di sinistra) e F7 per 5 secondi e lasciando F7. Il Pi 400, il Pi 5 e il Pi 500 il jack composito non ce l'hanno, e da loro le voci non compaiono.
- **Le barre e la centratura sul composito**: col composito le due barre di stato sono disegnate per le sue righe (288 in PAL, 240 in NTSC), coi caratteri interi. In PAL **H Center** a 0 mette l'immagine al centro di un monitor a tubo, e spostarla di qualche punto non rallenta più l'emulazione.
- **Niente 16:9 e 4:3 sul composito**: col composito le voci **Aspect 4:3** e **Aspect 16:9** del menu **Video** non ci sono, e il tasto rapido del 16:9 e 4:3 (ALT + "\", e nel lettore SID anche "\" senza ALT) dice NOT ON COMPOSITE: le proporzioni le dà il composito, e toccarle sballava l'immagine. Sulla HDMI tutto come prima.
- **Monitor VGA**: con un adattatore da HDMI a VGA va anche un monitor VGA da PC. Nel menu **Switch** ogni macchina ha i modi **VGA 1024x768** a 50 e 60 Hz (circa 40 e 48 kHz: un monitor che arriva solo a 31 kHz non li aggancia).
- **I primi Pi 5 (chip C1)**: nella V1.3, sui primi Pi 5 (col chip BCM2712 C1) lo schermo disegnato dalla GPU, quello degli shader CRT, restava nero. Dalla V1.4 BMC64-NG li guida come i Pi 5 col chip D0, il Pi 500 e il 500+: immagine e shader CRT su tutti. Nel file BMC64-DIAG.TXT della scheda le righe *pi5kms chip C* dicono che il Pi 5 ha il chip C1. Se lo schermo restasse nero, `pi5kms=0` in cmdline.txt riporta lo schermo del firmware, senza shader.
- **Il monitor VGA accanto a quello HDMI**: se tieni un monitor HDMI su una porta e quello VGA, con l'adattatore, sull'altra, nel menu **Video** la voce **VGA Monitor On** dice su quale porta sta il VGA: **HDMI 0** o **HDMI 1** (di fabbrica **None**). Da lí le voci dello **Switch** con VGA nel nome mandano lo schermo sul monitor VGA e le altre sul monitor HDMI, con un solo riavvio: non serve piú passare anche da **Main HDMI Output**. La voce vale per tutte le macchine, si scrive subito sulla scheda e conta dalla prossima scelta dello **Switch**. Anche il ritorno col tasto Commodore e F7 va sul monitor HDMI, e se entro 15 secondi non premi un tasto torna tutto com'era, porta compresa. Con **None** lo **Switch** lascia lo schermo sulla porta dov'è. Sul C128 con due monitor la voce scelta nello **Switch** decide dove va il VIC-II, e il VDC va sull'altro.
- **Fotografia dello schermo** salvata sulla scheda.
- **Le fotografie dello schermo**: **SUPER + PRTSCN** (il tasto col logo di Windows o col lampone, insieme a STAMP), oppure **Take screenshot** in **Prefs** > **Screenshots**, salva una foto nella cartella `/screenshots` della scheda: `bmc64ng-0001.png`, poi 0002 e avanti. **Capture point** dice quale immagine si prende: **Emulator** lo schermo del Commodore così com'è, **Scaled** già ingrandito, **CRT**, di fabbrica, con lo shader CRT, **Screen** quello che arriva al monitor. **Size** sceglie la grandezza, **Format** PNG o PPM, e **Save settings** li tiene. La macchina si ferma il tempo di scrivere sulla scheda, poi riparte da dov'era.

### Suono

- **Il SID vero**, emulato con reSID: sia il 6581 dei primi C64 sia l'8580 dei
  successivi.
- **Il digi boost dell'8580**: nel menu **Sound**, **SID Model**, **SID2 Model** e **SID Card Model** hanno anche **8580 + Digi Boost**: l'8580 con la modifica che sul ferro fa sentire i campioni digitalizzati, che sull'8580 normale si sentono appena. Vale col reSID e col reSIDfp. Si salva con **Save settings**.
- **La SID Card sul Plus/4 e sul PET**: queste macchine il SID non ce l'hanno, e come allora lo si aggiunge con una cartuccia. Nel menu **Sound** c'è la voce **SID Card**, spenta di fabbrica, con il modello (6581 o 8580), l'indirizzo (`$FD40` o `$FE80` sul Plus/4, `$8F00` o `$E900` sul PET), il filtro e il motore (**SID Card Engine**: ReSid di fabbrica, o ReSIDfp con la sua cartella **reSIDfp Settings**).
- **Uscita a scelta**: HDMI, jack delle cuffie, o un **DAC USB** esterno per
  chi vuole la qualità migliore.
- **Lettore di file .SID**: una cartella `/sids` sulla scheda e il menu ti fa
  sfogliare e suonare i brani della SID collection, compresi quelli creati per due e tre SID.
- **Nel lettore SID** ALT + , e ALT + . passano al brano precedente e al successivo; ALT + P mette in pausa senza staccare il brano, e ripremendolo riprende.
- **Nel lettore SID anche senza ALT**: , e . passano al brano precedente e al successivo, P mette in pausa, M silenzia. Il volume si alza e si abbassa con le frecce su e giú, oppure con ALT + + e ALT + −: senza ALT, 0, − e + sono le voci del quarto SID.
- **Avanti e indietro nel brano**: nel lettore SID le frecce a destra e a sinistra, senza ALT, saltano 10 secondi avanti o indietro, e in basso compare +10 SEC o -10 SEC; premendole piú volte i salti si sommano (tre volte a destra sono 30 secondi). Il lettore ci arriva di corsa e in silenzio, e da lí il brano riprende a suonare. Mentre il brano suona, il lettore tiene in memoria una fotografia della macchina a ogni secondo: all'indietro riparte subito dalla fotografia di quel punto, senza ricominciare il brano, e la musica rifà la stessa identica strada; anche in avanti, se il punto l'ha già suonato, riparte dalla sua fotografia invece di correre. Nei primi 10 secondi il brano ricomincia e basta. In pausa le frecce non fanno niente.
- **Le voci una per una**: nel lettore SID i tasti da 1 a 9 spengono e riaccendono una voce alla volta, da 1 a 3 quelle del primo SID, da 4 a 6 del secondo, da 7 a 9 del terzo; ALT + 1, ALT + 2 e ALT + 3 spengono e riaccendono un SID intero. Uscendo dal lettore le voci si riaccendono tutte.
- **V, T e N** nel lettore SID mostrano e nascondono i VU meter, il tempo del brano e le righe in alto col nome del brano.
- **Gli altri tasti con ALT** nel lettore SID non fanno niente, e in basso compare NOT IN SID PLAYER: restano quelli del lettore, il volume, ALT + S, ALT + SHIFT + S, ALT + ESC e ALT + SHIFT + ESC.
- **4:3 e 16:9 nel lettore SID**: il tasto "\" passa fra 4:3 e 16:9 anche senza ALT, e ALT + "\" funziona come fuori dal lettore. Con una tastiera italiana, senza ALT, vanno sia il tasto con \ in alto a sinistra, accanto all'1, sia il tasto < > accanto allo SHIFT sinistro. Fuori dal lettore, "\" senza ALT resta un tasto del C64.
- **Stereo con piú SID**: con due SID il primo suona a sinistra e il secondo a destra; con tre, il primo a sinistra, il terzo a destra e il secondo al centro.
- **I brani a 4 SID**: il lettore suona anche i file a 4 SID (quelli di SID-Wizard, formato PSID v4E): all'inizio il primo e il secondo SID a sinistra, il terzo e il quarto a destra, e i VU meter diventano 12 barre, larghe almeno come con tre SID e un po' di piú se c'è posto, che vanno dal primo 0 del tempo, a sinistra, alla fine della scritta a destra (PAL 50Hz); sotto ogni gruppo il numero del SID, il modello e il lato (per esempio 1 8580 L). I tasti da 1 a 9, poi 0, − e + spengono e riaccendono le 12 voci (10, 11 e 12 sono quelle del quarto SID); ALT + 1, 2, 3 e 4 il SID intero.
- **I SID su più core**: il core dell'emulatore non calcola più da solo tutti i SID: con 2 o 3 SID, e nel lettore fino a 8, il core 2 lo aiuta sempre e il core 3 quando lo schermo non lo occupa. Con 3 SID ognuno ha il suo core; nel lettore, coi brani da 4 a 8 SID, i SID si dividono a gruppi di tre (con 8 SID: l'emulatore l'1, il 4 e il 7, il core 2 il 2, il 5 e l'8, il core 3 il 3 e il 6); quando il core 3 è occupato, a coppie (coi 4 SID il core 2 fa il secondo e il quarto). Il suono è identico: cambia solo chi fa il calcolo, e conta soprattutto col reSIDfp, che ne chiede di più. Con sid_core3=0 in cmdline.txt il core 3 resta solo allo schermo.
- **Il lato di ogni SID, con F1-F4**: nel lettore SID F1, F2, F3 e F4 spostano il primo, il secondo, il terzo e il quarto SID a sinistra (L), al centro (C) o a destra (R): a ogni pressione il lato dopo, L, C, R e di nuovo L. Vale in tutti e quattro i lettori, da 1 a 4 SID. Si parte da: un SID al centro; due SID L e R; tre L, C e R; quattro L, L, R e R. Il lato si vede sotto le colonne, accanto al modello, e un avviso lo dice quando cambia. I lati scelti restano per tutti i brani con lo stesso numero di SID finché resti nel lettore; uscendo tornano quelli di partenza. Con **Channels** su **Mono** (menu **Sound**) i lati non ci sono, e F1-F4 dicono CHANNELS: MONO.
- **reSID o reSIDfp dal vivo, col tasto S**: nel lettore SID il tasto **S** (senza ALT) passa fra reSID e reSIDfp mentre il brano suona, e il nome del motore (ENGINE: reSID o ENGINE: reSIDfp) compare per un secondo nella riga della barra del volume. La scelta resta per gli altri brani finché resti nel lettore; uscendo torna il **SID Engine** del menu **Sound**. Vale anche coi brani a 8 SID, che partono col reSID. Fuori dal lettore S resta un tasto del C64.
- **Un SID su un core a parte**: con due o tre SID il secondo lo calcola un altro dei quattro core del Raspberry, insieme agli altri; nel lettore SID, in stereo, anche coi brani a quattro SID. Il suono è lo stesso, e il core dell'emulazione ha piú respiro.
- **I brani a 8 SID**: il lettore suona anche i file con piú di quattro SID, fino a otto (formato PSID v4E), per esempio The Tuneful Eight, quattro brani a due SID scritti per suonare insieme su otto SID. I VU meter diventano 24 barre larghe la metà; sotto ogni gruppo il numero del SID e il lato, e sotto ancora il modello. All'inizio i SID dispari sono a sinistra e i pari a destra, come sulla Ultimate 64. **F1**…**F8** cambiano il lato di ogni SID come F1-F4 negli altri lettori, **ALT + 1**…**ALT + 8** spengono e riaccendono un SID intero; le voci una per una non ci sono (i tasti da 1 a 9 lo ricordano: ALT+1-8: SID ON/OFF). Con questi brani il lettore parte col reSID, qualunque sia il **SID Engine** scelto, e sul Pi 4 e sul 400 col modo **Fast**: con otto SID il **Resampling** non ci sta, nemmeno usando più core. Il tasto **S** li passa al reSIDfp e ritorno. I SID si dividono fra i core come negli altri brani. Uscendo dal lettore, o col brano dopo, tornano il motore e il modo di prima; gli altri brani suonano come prima.
- **Il modello di ogni SID**: coi brani a più SID ogni SID suona col modello scritto nel file, 6581 o 8580, fino all'ottavo; se il file non lo dice, col modello del primo SID. Prima dal quarto in poi suonavano col modello del terzo.
- **Il lettore SID sul Pi 4: i limiti**: il reSIDfp chiede molto più calcolo del reSID. Sul Pi 4 col clock di serie i brani fino a 4 SID col reSIDfp reggono; coi brani a 8 SID il reSIDfp vuole l'overclock: alza il **CPU Clock** del menu principale a **+20%**, oppure resta al clock di serie col reSID, che chiede meno calcolo (tasto **S** o **SID Engine** del menu **Sound**).
- **Il campionamento del SID**: nel menu **Sound** la voce **SID Resampling** è di fabbrica su **Resampling**, il modo piú fedele: il suono del SID arriva alle cuffie senza i disturbi che gli altri modi aggiungono sugli acuti. **Fast Resampling** suona uguale, ma usa piú memoria e ci mette di piú a prepararsi; **Interpolation** e **Fast** costano meno al Raspberry e sono meno fedeli.
- **I filtri del reSID**: nel menu **Sound** la cartella **reSID Filter Settings** ha le regolazioni del 6581 e dell'8580: **Passband %** (di fabbrica 90), **Gain %** (97) e **Filter Bias (mV)**. Valgono per tutti i SID di quel modello. Passband e Gain contano solo con **SID Resampling** su **Resampling** o **Fast Resampling**; il suono cambia quando si chiude il menu, e **Save settings** li tiene.
- **reSIDfp, il terzo motore del SID**: nel menu **Sound** la voce **SID Engine** ha anche **ReSIDfp**, accanto a **ReSid**, che resta quello di fabbrica. È il motore del SID più recente del VICE, e il più fedele al chip vero: le forme d'onda combinate (quelle che il SID produce quando una voce ne usa due insieme) sono prese da misure su chip veri, e il filtro del 6581 ha la sua distorsione. È allineato all'ultima versione dei suoi autori (libresidfp, 13 ottobre 2026), che ha rifatto le forme d'onda combinate del 6581 più vicine ai chip veri e il fruscio di fondo. La cartella **reSIDfp Settings** ha le sue regolazioni: **Combined Waveforms** (Weak, Average, Strong), **Background Noise** (qui sotto), e per il **6581** **Composer Profile...** (qui sotto), **Filter Curve**, **Filter Range** e **Old Caps (2200pF)**, per l'**8580** **Filter Curve**. Il suono cambia quando chiudi il menu; **Save settings** tiene tutto. Chiede un po' più di calcolo del reSID, e il secondo SID resta sul suo core. C'è anche per la SID Card del Plus/4 e del PET. Non c'è sul C64 DTV, che ha un SID suo.
- **Il fruscio di fondo di reSIDfp**: un C64 vero non è mai del tutto silenzioso, e nelle sue registrazioni c'è sempre un leggero fruscio di fondo. reSIDfp lo aggiunge al suono per somigliare al chip vero: la voce **Background Noise** in **reSIDfp Settings** lo regola da **OFF** (con la macchina ferma, silenzio come col reSID) a **200%**, a passi del 10%. Di fabbrica è a **100%**, il livello scelto dagli autori di reSIDfp per somigliare alle registrazioni dei C64 veri, lo stesso del VICE (dalla V1.4 è il loro livello nuovo, otto volte più basso di prima); **200%** è il doppio. Se ti disturba, mettilo su OFF. Si sente soprattutto quando non suona niente, per esempio fermi al BASIC. Ogni SID ha il suo fruscio, indipendente dagli altri: con due, tre o quattro SID si sommano, come farebbero quelli di altrettanti chip veri.
- **I profili del 6581 per compositore**: il filtro del 6581 cambiava molto da un chip all'altro, anche fra chip usciti dalla stessa fabbrica lo stesso giorno, e ogni musicista scriveva sul suo. In **reSIDfp Settings** > **6581** la voce **Composer Profile...** apre l'elenco di 58 compositori (Rob Hubbard, Martin Galway, Jeroen Tel, Ben Daglish...): scegline uno e il filtro del 6581 prende il **Filter Range** del chip su cui scriveva, con **Filter Curve** e **Combined Waveforms** di fabbrica. L'elenco viene da sidplayfp, come nel VICE di sviluppo. Il profilo in uso ha accanto un asterisco fra parentesi, finché non cambi a mano una di quelle tre voci; **None** rimette i valori di fabbrica. **Save settings** lo tiene. Vale solo per il 6581 e solo col motore ReSIDfp.
- **Stereo o mono**: nel menu **Sound**, subito sotto **Audio out**, la voce **Channels** sceglie come escono i SID quando sono piú di uno (il **Dual SID**, o un brano a 2 o 3 SID nel lettore). **Stereo**, di fabbrica: ogni SID dal suo lato (con 3 SID il primo a sinistra, il terzo a destra e il secondo su tutti e due). **Mono**: tutti i SID insieme su tutti e due i lati. **Save settings** la tiene. C'è su C64, C128 e SuperCPU; le altre macchine hanno un SID solo, o nessuno, e il suono è già mono.
- **Il tempo del brano**: nel lettore SID, a sinistra della terza riga, i minuti e i secondi del brano. È il tempo della musica: in pausa si ferma, e cambiando brano riparte da zero. A destra c'è ogni quanto suona il brano: PAL 50Hz, NTSC 60Hz, oppure CIA col numero che il brano ha messo nel timer.
- **Il volume nel lettore SID**: per un secondo dopo ogni tasto del volume compare una barra orizzontale a segmenti, fra le scritte sotto le colonne (per esempio 1 8580 R) e la riga del brano (DEFAULT TUNE), con la percentuale a destra, e MUTE in rosso quando il volume è a zero.
- **Dentro il lettore SID** la barra di stato non compare, per non coprire le colonne, e ALT + W non fa niente: a velocità doppia un brano non si ascolta.
- **Il DAC USB anche a computer acceso**: con **Audio out** su **USB DAC** il suono passa al DAC appena lo colleghi e torna sull'HDMI quando lo stacchi, e vanno anche i DAC che lavorano solo a 48 kHz, con tutte le macchine, Plus4Emu compreso. Non serve piú collegarlo prima di accendere.
- **Se il DAC USB sparisce per un attimo** (qualche DAC economico si stacca e si riattacca da solo), il suono aspetta tre secondi prima di tornare sull'HDMI: se il DAC torna, riprende da lí.
- **I DAC e le cuffie USB a più canali**: il suono mono esce sui due canali anteriori anche dai dispositivi a 4, 6 o 8 canali, come le cuffie 7.1. Prima usciva solo dall'anteriore sinistro.
- **Volume** regolabile da tastiera, e il silenziatore.
- **Il 100% è il suono com'è**, senza amplificazioni che lo storcano. Arrivato al 100% ALT + + non fa piú niente, e a zero ALT + − nemmeno: la barra di stato dice VOLUME 100% MAX, oppure MUTE.

### Comandi e controlli

- **Joystick USB** e gamepad, fino a quattro porte, dieci con l'adattatore V8 sulla porta utente.
- **I pulsanti del gamepad**: nel menu **Joyports**, **Configure USB Gamepad 1...** (e 2, 3, 4): **Mapping Mode** su **Automatic**, di fabbrica, usa il profilo giusto per i pad che BMC64-NG conosce (Xbox 360, M30) e uno generico per gli altri, e lo rimette a ogni collegamento. Su **Custom** le assegnazioni sono le tue e restano: **Assignments by Button...** dà una funzione a ogni pulsante, **Assignments by Function...** un pulsante a ogni funzione (si sceglie la funzione e si preme il pulsante sul gamepad), **Clear a Button...** toglie un'assegnazione. **Reset to Default Bindings** torna ad **Automatic**. Si salva con **Save settings**.
- **L'8BitDo M30 2.4G preso per un controller Switch Pro**: il ricevitore può restare nel modo Switch, e allora i pulsanti sono sbagliati. Col pad collegato tieni premuti Home (il cuore) e la croce in giù per 5 secondi, finché i LED lampeggiano: il ricevitore torna al suo modo a 6 pulsanti e lo tiene.
- **I POT dei gamepad**: nella stessa pagina, **POT X Up Value** e **POT Y Up Value** sono di fabbrica 255, **POT X Down Value** e **POT Y Down Value** 0, e il computer legge davvero questi numeri: quelli Up a riposo, quelli Down col pulsante premuto. Con **Hat+POTX/Y Paddles** gli assi analogici del gamepad arrivano al SID come paddle.
- **Joystick sui GPIO**: se hai un adattatore per joystick d'epoca collegato ai
  pin del Raspberry, funziona.
- **Keyset**: usare la tastiera come joystick, con i tasti che scegli tu. **PotX** e **PotY** sono il secondo e il terzo pulsante, per i giochi che li usano, come sui gamepad USB; se non ti servono, dagli lo stesso tasto del Fire.
- **Mouse**: nel menu **Joyports**, **Port 1** o **Port 2** su **USB Mouse**. Che mouse è lo dice il menu **Mouse** > **Type** (di serie il 1351 del Commodore; poi NEOS, Amiga, Atari CX-22, Atari ST, SmartMouse, Micromys e il 1350 del Commodore, che il computer legge come un joystick), e **Sensitivity (%)** quanto si muove, da 10 a 200 (di serie 100). Il menu **Mouse** c'è su C64, SuperCPU, C128, VIC-20 e Plus/4; si salva con **Save settings**.
- **Due joystick in più sulla porta utente** (C64, C128, VIC-20, PET e Plus/4 col VICE): nel menu **Joyports**, **Userport Joystick Adapter**, con **Enable** e **Adapter Type**. I tipi sono quelli che il VICE 3.10 ha per quella macchina: CGA, PET, OEM, HIT, Kingsoft, Starbyte e WOJ sul C64 (anche con la SuperCPU) e sul C128; CGA, PET, OEM e WOJ sul VIC-20 e sul PET; Synergy e WOJ sul Plus/4. Gli altri restano grigi. Il tipo salvato con le impostazioni si ritrova giusto all'avvio.
- **L'adattatore V8, otto joystick sulla porta utente** (C64, SuperCPU, C128, VIC-20 e PET): nello stesso menu **Adapter Type** su **V8**. Lo leggono i programmi scritti per lui. **Port 3** e **Port 4** sono i primi due joystick, la cartella **V8 Ports 5-10** ha gli altri sei. Si salva con **Save settings**.
- **Lo spinner come paddle**: su C64, C128 e VIC-20, nel menu **Joyports**, **Port 1** o **Port 2** su **USB Spinner (Paddle)**: la rotella di un mouse USB fa da paddle, come sul MiSTer. Va bene anche uno spinner che per il Raspberry è un mouse con la rotella, come quello del TAITO EGRET II mini. Ogni scatto della rotella sposta la paddle di un passo, da 0 a 255, e uno qualunque dei tasti del mouse è il tasto della paddle, anche oltre il quinto: sul pannello del TAITO sparano anche i due pulsanti grandi. Il verso è quello del pannello del TAITO (provato con Omega Race e Le Mans). **Spinner Sensitivity**, nello stesso menu, dice quanto si muove la paddle a ogni scatto: da 25% a 400%, e 100% è un passo per scatto. Lo spinner del TAITO per il PC è un mouse: la trackball muove il puntatore e lo spinner è la rotella su e giù. Anche un mouse USB che non dichiara la classe "boot" adesso va al driver del mouse, con la rotella. Il mouse 1351 e lo spinner non stanno insieme: scegliendone uno, l'altro torna su **None**; e se l'altra porta cambia, lo spinner non rallenta più la macchina. Come sul Commodore vero, sul C64 e sul C128 il tasto della paddle tenuto giù sulla porta 1 blocca la tastiera: quella linea è anche una riga della tastiera.
- **Autofire**: nel menu principale, subito sotto **Joyports**, il menu **Autofire** ha **Port 1** e **Port 2** (sul VIC-20 la porta è una sola; sul PET e sul Plus4Emu il menu non c'è). **Off**, la scelta di fabbrica, lascia il fuoco com'è; con **Slow** e **Fast**, finché tieni premuto il fuoco, la porta spara da sola circa 7 o 14 colpi al secondo. Vale per il joystick che c'è su quella porta, qualunque sia: gamepad USB, tastiera o GPIO. Si salva con **Save settings**.
- **Mappe di tastiera** posizionale e simbolica con layout US, IT, UK e DE.
- **La tastiera dal menu**: nel menu **Keyboard** la voce **Mapping** sceglie fra **Positional** e **Symbolic**, e **Layout** fra US, Italiana, British e Deutsch (le ultime tre con **Mapping** su **Symbolic**). Le tastiere del Pi 400 e del Pi 500 vanno come una tastiera USB. Con **Symbolic** il **+** e gli altri tasti battuti con lo SHIFT non escono più, ogni tanto, come caratteri grafici.
- **La tua mappa della tastiera** (da BMX): nel menu **Keyboard** si mette **Mapping** su **Custom** e si apre **Mapping Editor...**: una riga per ogni tasto della macchina (C64, C128, VIC-20, SuperCPU, PET e Plus/4 col VICE). Sulla riga, INVIO e poi il tasto del PC da dargli; SHIFT+INVIO ne aggiunge un altro, DEL toglie quello che si vede e SHIFT+DEL tutti, Sinistra e Destra scorrono quelli della stessa riga. Se quel tasto del PC era già di un altro tasto, il menu chiede se spostarlo, partendo da CANCEL. **Save & Apply** la salva e la usa subito, **Restore Defaults...** torna come la **Positional**. Si scrive nel file **user_pos.vkm** della cartella della macchina, che all'inizio è uguale alla **Positional**.
- **La tastiera vera del C64 con la Keyrah V2**: con la mappatura **Positional** e il deviatore della Keyrah in basso (il modo per gli emulatori) vanno tutti i tasti, anche **RESTORE**: la Keyrah lo manda come **Page Up**, che nelle mappature Positional di C64, C128, VIC-20 e SuperCPU è il secondo tasto RESTORE, accanto a **PrtScn** dei Pi 400 e 500, che resta com'era. Nel menu **Keyboard** la riga **Restore key** li dice tutti e due. Le due porte joystick DB9 della Keyrah vanno coi keyset **NUMPAD 17930** e **NUMPAD 64825**, oppure si rimappano in **Custom Keyset 1** e **2** per usarle al posto di quelli NUMPAD.
- **Riavvio e spegnimento dalla tastiera vera del C64**: nel menu **Keyboard** le voci **C= + F1 Hotkey**, **C= + F3 Hotkey**, **C= + F5 Hotkey** e **C= + F7 Hotkey** e le quattro **CTRL + F1…F7 Hotkey** hanno due scelte in più, **Reboot BMC64** e **Power Off BMC64**: riavviano e spengono il Raspberry anche dalla tastiera del C64 sulla Keyrah, che non ha ALT né il tasto di accensione. Il tasto rapido fa comparire in basso la stessa domanda di ALT + ESC e ALT + SHIFT + ESC (REBOOT? oppure POWER OFF?): lo stesso tasto rapido una seconda volta, entro cinque secondi, conferma; **ESC** (RUN/STOP sulla tastiera del C64) annulla. Se è aperta la domanda dell'altro, il tasto la cambia nella sua e i cinque secondi ripartono. Vanno con ogni tastiera e su tutte le macchine, anche sul C64 GS, e la scelta si salva con **Save settings**.
- **I monta, lo screenshot e il riquadro del SID dai tasti rapidi**: le stesse voci **C= + F1…F7 Hotkey** e **CTRL + F1…F7 Hotkey** del menu **Keyboard** hanno anche le otto scelte di BMX. **Attach Tape**, **Attach Cart**, **Attach Drive 8**, **Attach Drive 9**, **Attach Drive 10** e **Attach Drive 11** aprono l'elenco dei file a macchina che gira, come **ALT + T**, **ALT + C**, **ALT + 8** e **ALT + 9**: INVIO monta, ESC (RUN/STOP sulla tastiera del C64) chiude. **Screenshot** fa la fotografia dello schermo, come **SUPER + PRTSCN**. **SID OSD** apre un riquadro con le voci del SID del menu **Sound** (il modello, il filtro, le regolazioni del reSID o del reSIDfp per quel modello, **Dual SID**, l'indirizzo e il modello del secondo SID; sul Plus/4 e sul PET la SID Card): si cambiano con le frecce mentre la musica suona, lo stesso tasto rapido o ESC lo chiudono, e **Save settings** le tiene. Dove la macchina non ha la cosa la scelta è grigia: Attach Tape sulla SuperCPU, Attach Cart sul PET, SID OSD sul VIC-20 e su Plus4Emu. Sul C64 GS vanno la cartuccia e lo screenshot; nel lettore di SID i monta e il riquadro non si aprono.
- **Le periferiche USB all'accensione, sul Pi 5 e sul Pi 500**: una periferica USB che all'accensione non si fa riconoscere viene riprovata da sola, fino a tre volte, come fa Linux.
- **Le periferiche USB dopo un riavvio, sul Pi 4 e sul Pi 400**: una periferica che dopo un riavvio o un cambio di macchina non si fa riconoscere viene riprovata da sola, fino a quattro volte. Dal Pi 4 in poi, all'avvio la USB aspetta le periferiche quanto il BMC64 5.4.5, un po' meno di prima. Se il guasto resta, il file USB-LOG.TXT dice cosa è successo.
- **USB Off at Restart**, sul Pi 4 e sul Pi 400: se una periferica USB dopo un riavvio o un cambio di macchina non risponde finché non la si stacca e riattacca, la voce **USB Off at Restart** del menu principale, sopra **Save settings**, spegne tutte le prese USB prima di ogni riavvio fatto dal menu, cambio di macchina compreso, per il tempo scelto, da **1.0 s** a **5.0 s**: è come staccare e riattaccare le periferiche. Di fabbrica è **Off**. Le frecce cambiano la scelta; con INVIO il menu chiede di riavviare, e già quel riavvio spegne le prese. Le prese si spengono tutte insieme.
- **Il tasto < > delle tastiere ISO**: nelle mappature **Positional** di C64, C128, VIC-20, SuperCPU e Plus/4 il tasto accanto allo SHIFT di sinistra (quello di < e > sulle tastiere italiane ed europee) scrive la sterlina **£**, come nel BMC64 di Randy; prima non faceva niente. Nel lettore SID resta il tasto del 4:3 e 16:9.
- **Il Caps Lock della tastiera USB**: fa lo SHIFT LOCK anche con la mappatura **Positional** di C64, SuperCPU, Plus/4 e Plus4Emu, e sul Plus/4 anche con la **Symbolic**, come nel VICE; prima lì non faceva niente. Va anche insieme alla tastiera vera del C64 sulla Keyrah: lo SHIFT LOCK del C64 e il Caps della tastiera USB valgono tutti e due, come due tasti SHIFT. Sul Plus/4 adesso fa lo SHIFT anche lo **SHIFT sinistro** (della tastiera USB e della Keyrah, e quindi anche lo SHIFT LOCK del C64): prima lo faceva solo il destro. Sul C128 e sul PET il Caps fa lo SHIFT LOCK anche lui, con tutte le mappature, come nel VICE: prima sul C128 con la **Positional** era il tasto **CAPS LOCK** del C128 (come nel BMC64 di Randy), e sul PET non faceva niente. Il **CAPS LOCK** del C128 resta su **ALT+F4**.
- **Le tastiere Logitech col ricevitore Bolt** (per esempio la MX Keys Mini): i tasti escono giusti e F12 apre il menu. Prima, col ricevitore che fa anche da mouse, la K scriveva A, la L scriveva E e F12 non faceva niente. È la correzione di Circle, la stessa del BMC64 di Randy.
- **I mouse USB che dichiarano i tasti prima del puntatore** (per esempio il Microsoft 045e:0737): adesso i tasti vanno e il puntatore si muove giusto. È la correzione di Circle del 4 novembre 2026. In più un mouse con un descrittore insolito non ferma più il Raspberry.
- **Una periferica USB staccata mentre il Raspberry le mandava dati** (per esempio il LED del Caps Lock a una tastiera) non ferma più il Raspberry, dal Pi 4 in poi: è la correzione di Circle del 4 novembre 2026.
- **Gli otto tasti in più del C128** (ESC, TAB, ALT, CAPS LOCK, HELP,
  LINE FEED, 40/80, NO SCROLL), che su una tastiera da PC non esistono.
- **Il tastierino del C128**: sul C128 il tastierino numerico della tastiera del PC fa da tastierino numerico del C128.

### Il resto

- **Istantanee**: salvi lo stato esatto della macchina e lo riprendi dopo.
- **Il Load subito dopo il Save**: aprendo **Load Snapshot** dopo un salvataggio, il cursore è già sul file appena salvato, come nel BMC64 di Randy.
- **REU**: l'espansione di memoria Commodore, da 512 KB a 16 MB.
- **Warp**: la macchina va al massimo che il Raspberry consente, per saltare i
  caricamenti lunghi.
- **Rete e WiFi**: configurabili dal menu.
- **La rete su tutte le macchine**, anche sul Pi 5 e sul Pi 500: col cavo o in WiFi, dal menu **Network** (**Network Device**: Off, Ethernet o WiFi). Sul Pi 5 e sul Pi 500 il WiFi usa i tre file `brcmfmac43455-sdio.raspberrypi,5-model-b` (`.bin`, `.txt` e `.clm_blob`) della cartella `/firmware` della scheda: ci sono già.
- **La rete via cavo sui Pi 5 della revisione 1.7**: questi Pi 5 hanno un altro chip per l'Ethernet (MaxLinear MXL86110 invece del Broadcom BCM54213PE): BMC64-NG li riconosce tutti e due e prepara ognuno a modo suo. In `NET-LOG.TXT` la riga `PHY ID` dice quale chip c'è; con un chip che non conosce, BMC64-NG fa come prima e lo scrive in `NET-LOG.TXT`. È la correzione di Circle del 7 novembre 2026.
- **Il WiFi del Pi 400**: il Pi 400 ha un chip WiFi diverso da quello del Pi 4 e usa i tre file `brcmfmac43456-sdio` (`.bin`, `.txt` e `.clm_blob`) della cartella `/firmware` della scheda: ci sono già. Se mancano, o se il chip non parte, la macchina va avanti, anche all'accensione con **Network Device** su WiFi, e la finestra di **WiFi SSID** lo dice: *Missing in the firmware folder:* col nome del file che manca, da copiare dalla cartella `/firmware` dell'archivio della scheda intera, oppure *The WiFi chip did not start*.
- **Il WiFi dal menu**: nel menu **Network**, cartella **WiFi Settings**, la voce **WiFi SSID** cerca le reti senza riavviare e le elenca con la banda (MHz), il canale (CH) e il segnale (RSSI). Scelta la rete, **WiFi Country Code** va messo sul paese dove sei: col cursore sulla voce si scrivono le due lettere (IT per l'Italia, anche minuscole); a campo pieno una lettera ricomincia da capo e **DEL** lo svuota. Il codice si salva appena è completo e vale dal riavvio. Poi **Enter Password & Reboot**: la password si vede in chiaro mentre la scrivi, e una lunga scorre dentro il riquadro; **Save & Reboot** salva, passa al WiFi e riavvia.
- **Indirizzo automatico o fisso**: **IP Address Mode (reboot)** su **DHCP**, di fabbrica, prende l'indirizzo dal router, e i campi **Static IP**, **Netmask**, **Gateway** e **DNS** mostrano quelli ricevuti. Su **Static** si scrivono a mano; senza **Netmask** vale 255.255.255.0. Vale dal riavvio. Al router il Raspberry si presenta col nome **BMC64-NG**.
- **L'ora legale**: sotto **Timezone (reboot)** c'è **Daylight Saving (reboot)**: **Off**, **On**, oppure **Auto (EU)**, che la mette e la toglie da sola con le regole europee, dall'ultima domenica di marzo all'ultima di ottobre.
- **La scheda vista dal PC**: la voce **SD Card Sharing...** del menu principale apre la microSD al PC di casa, senza toglierla dal Raspberry. Su Windows, in Esplora file, basta scrivere il nome del Raspberry, che dice il modello e compare nel riquadro: `\\BMC64-NG-4` (Pi 4), `\\BMC64-NG-400`, `\\BMC64-NG-5` (Pi 5), `\\BMC64-NG-500` o `\\BMC64-NG-500P` (il 500+). Oppure `\\` e l'indirizzo che il riquadro mostra (per esempio `\\192.168.1.50`): compare la cartella **sdcard**, che si apre con un doppio clic; si può anche scrivere tutto, `\\192.168.1.50\sdcard`. Se il nome non risponde, l'indirizzo va sempre. Windows prova prima con l'utente del PC e poi chiede le credenziali: utente **bmc64**, password **bmc64**. La password si cambia con **Sharing Password** nel menu **Network**. Mentre la scheda è aperta al PC l'emulazione è ferma e i dischetti montati sono già salvati e restano montati, anche quelli partiti con l'**Autostart** (prima, dopo aver sfogliato un'altra cartella, uno poteva restare staccato); i file che la macchina sta usando non si possono cancellare né sovrascrivere dal PC. **Stop Sharing**, o uscire dal riquadro, chiude la condivisione e la macchina riparte. Serve la rete accesa (menu **Network**): senza rete il riquadro dice **Cannot start SD Card Sharing**. Il riquadro può restare aperto quanto vuoi. Finché la condivisione è accesa il Raspberry compare anche nella cartella **Rete** di Esplora file, col suo nome (per esempio **BMC64-NG-400**): un doppio clic e si apre. Con due Raspberry accesi insieme, ognuno ha il suo nome, e lo stesso nome lo vede anche il router. Se dal PC una lettura o una scrittura non riesce, il motivo esatto va nel file **SMB-LOG.TXT** della scheda.
- **I nomi lunghi anche dal PC**: nella condivisione i file hanno il loro nome intero, anche coi caratteri che prima non passavano (la lineetta lunga dei nomi delle guide, il greco, il giapponese), e dal PC si creano, si rinominano e si copiano cosí. Prima un nome con uno di quei caratteri compariva col suo nome corto (per esempio `BMC64-~3.PDF`), e una copia da lì su un'altra scheda poteva finire sopra un altro file. Anche nel menu i nomi con le lettere accentate ora si leggono giusti.
- **Il NAS, come sul MiSTer**: la voce **NAS** del menu principale ha **NAS IP Address** (l'indirizzo del NAS, per esempio 192.168.1.100), **NAS User Name** e **NAS Password** (vuoti, si entra come ospite) e **SMB Protocol**: **Auto**, di fabbrica, oppure SMB 2.0, 2.1, 3.0, 3.0.2 o 3.1.1 (SMB1 non c'è: è vecchio e i NAS di oggi lo tengono spento). **NAS Path** è il percorso sul NAS: prima la condivisione, poi la cartella da cui parte il navigatore. Di fabbrica è vuoto: sul NAS del MiSTer, per esempio, è `Mister\games\C64` (anche per la SuperCPU), `Mister\games\C128`, `Mister\games\Vic20`, `Mister\games\PET2001` e `Mister\games\C16` (Plus/4). Si scrive come gli altri campi, con `\` o con `/`, si vede sempre in chiaro e ogni macchina tiene il suo; finché è vuoto il NAS risponde **NAS Path not set**. **Test NAS Connection** prova subito la connessione e la cartella. Indirizzo, utente, password e protocollo valgono per tutte le macchine, il percorso per quella accesa; tutto si salva con **Save settings**, nel file `nas.txt` della scheda. Nel navigatore dei file la prima riga apre la scelta del volume: accanto a SD e USB c'è **NAS**, che parte dalla cartella della macchina (mentre si collega compare **Connecting to the NAS...**; un NAS spento risponde dopo una decina di secondi). Dischi, nastri, cartucce e programmi si usano come dalla scheda: il file si legge tutto quando si apre, e quello che il gioco salva arriva sul NAS appena si spegne la spia rossa del drive, e quando il disco si stacca; se in quel momento il NAS non risponde, il disco si salva sulla scheda, nella cartella `NAS-NON-SALVATI`, e niente va perso. File fino a 32 MB. Serve la rete accesa (menu **Network**). Il NAS e la condivisione della scheda non lavorano insieme: aprire **SD Card Sharing...** chiude il NAS, che si ricollega da solo quando serve.
- **Le password dopo il riavvio**: la password del WiFi (**Enter Password & Reboot**) e la **NAS Password** si vedono in chiaro mentre le scrivi; dopo il salvataggio e il riavvio si mostrano con gli asterischi: il primo tasto le cancella e si riscrivono in chiaro, e senza toccarle restano quelle salvate.
- **Il modem, come la Ultimate II+** (C64 e C128; il Plus/4 e il VIC-20 in fondo): **Modem (SwiftLink)** nel menu **Network**, spento di fabbrica: acceso occupa `$DE00`, e con l'EasyFlash va lasciato spento. **Modem Address** è di fabbrica `DE00`, quello che vogliono CCGMS e gli altri programmi per le BBS. Da CCGMS si chiama una BBS con `ATDT` e l'indirizzo con la porta, per esempio `ATDT bbs.retrocampus.com:6510`; senza porta si chiama la 80. Si possono anche ricevere chiamate, sulla porta 3000: il modem squilla (RING) ogni 3 secondi, fino a 10 volte; **ATA** risponde, e con **ATS0=1** risponde da solo. Comandi, risposte e registri sono quelli della Ultimate II+. Quello che arriva prima di **AT** si ignora, come sulla Ultimate II+: un carattere sporco non fa più rispondere ERROR al comando dopo. Sul **C64** e sul **C128** la linea si vede come sulla Ultimate II+ di fabbrica: il DSR è attivo solo a connessione aperta, il DCD sempre, e quando la connessione cade il programma se ne accorge; con C64 OS si sceglie il driver delle Ultimate (`slde.u6` per `DE00`). Sul **Plus/4** il modem sta sull'ACIA di serie, a `$FD00`, senza voci da accendere: col VICE c'è sempre, con Plus4Emu quando nel menu **Network** è scelta la rete. L'ACIA ce l'hanno il Plus/4 e il V364 (con Plus4Emu da 64K in su), non il C16 e il 232, come sul ferro; la linea si legge dal DCD, come su un Plus/4 vero. Sul **VIC-20** il modem sta sulla porta utente, il dispositivo 2 del KERNAL (per esempio `OPEN 2,2,0,CHR$(6)` a 300 baud): **Modem (Userport)** nel menu **Network**, spento di fabbrica, e **Modem Baud**, 300 di fabbrica, che deve essere la stessa del programma (300, 1200 o 2400). La porta utente tiene una cosa sola, come sul ferro: acceso il modem si spegne l'adattatore dei joystick della porta utente, e viceversa.
- **Il WiC64** (C64 e C64 Std, anche HTTPS): il modulo WiFi del C64 sulla porta utente, per i programmi scritti per lui. Nel menu **Network**, cartella **WiC64**: **Enabled**, **Default server**, **Time zone**, **Remote timeout (s)**, **Command logging** e **Network reporting** (**Emulated SSID/RSSI**, con **Emulated SSID** ed **Emulated RSSI (dBm)**, oppure **Actual adapter**). **Apply** vale subito, **Save & Apply** anche dopo il riavvio. Usa la rete del Raspberry, che dev'essere accesa. La porta utente tiene una cosa sola, come sul ferro: con l'adattatore dei joystick acceso il menu risponde **Userport already in use**. L'identità che il server dà al modulo sta nel file `/wic64.cfg` della scheda, e si scrive aprendo il menu.
- **Overclock e Underclock** guidato dal menu, con le frequenze già provate. Nel menu principale ci sono **CPU Clock...** (-20%, -10%, Stock, +10% o +20% della frequenza di serie di quel Raspberry) e **GPU Clock...** (Stock, +10% o +20%), e dopo l'OK si riavvia. **GPU Clock** cambia insieme il nucleo della GPU (**Core**) e la parte 3D (**V3D**), che disegnano lo schermo: di serie 500 e 500 MHz sul Pi 4 e sul 400, 910 e 960 MHz sul Pi 5 e sul 500. La GPU non si può rallentare: il suo nucleo compone anche lo schermo, e sotto la frequenza di serie tutto andava a metà velocità. In testa al menu **GPU Clock** ci sono le frequenze che il Raspberry sta usando; i MHz della GPU si vedono anche in cima al menu principale e nella seconda riga della barra di stato, accanto a quelli della CPU. Con la CPU a -10% o -20% la GPU resta alle sue frequenze, di serie o alzate (fino all'11 ottobre 2026 restava al minimo: sul 500 la barra diceva 500 MHz; se avevi scelto il -10% o il -20%, sceglilo di nuovo una volta). Come per la CPU, la scelta vale solo per il Raspberry su cui l'hai fatta: la stessa scheda su un altro Pi parte con le frequenze di quello.
- **Overclocking...**: per chi vuole andare oltre **CPU Clock** e **GPU Clock**, la voce **Overclocking...** del menu principale ha **Voltage Offset** (Auto, da -100 a +100 mV), **Temperature Limit** (Auto, da 60 a 85 gradi), **Core Clock** e **V3D Clock** (Auto, o dalla frequenza di serie in su). **Apply & Reboot** scrive tutto in config.txt e riavvia, **Restore Defaults** rimette Auto. Con la CPU alzata la tensione la decide **CPU Clock**, con la CPU abbassata non si può alzare; **Core Clock** e **V3D Clock** prendono il posto di **GPU Clock**, e viceversa. Da qui non si scrive mai `force_turbo`, quindi il bit della garanzia non si segna. Se il Raspberry non partisse più, da un PC si cancella in config.txt il blocco fra `# BMC64-OCEXTRA-BEGIN` e `# BMC64-OCEXTRA-END`.
- **La garanzia del Raspberry**: il firmware segna per sempre il bit della garanzia del Raspberry quando in config.txt trova `force_turbo=1` insieme a una tensione alzata (`over_voltage` o `over_voltage_delta`). BMC64-NG non le scrive mai insieme: **CPU Clock** a +10% e +20% alza la tensione senza `force_turbo`, e **GPU Clock** usa `force_turbo` solo se la CPU di quel Raspberry non è alzata; con la CPU alzata la GPU resta su con le sue frequenze minime fissate. Fino al 14 ottobre 2026 **CPU Clock** a +10% e +20% scriveva anche `force_turbo`: su un Raspberry partito così il bit può essere già segnato. Se hai la CPU alzata, scegli di nuovo il suo livello una volta, e il blocco in config.txt si riscrive nella forma nuova.
- **Il raffreddamento**: il Raspberry vuole sempre un buon raffreddamento, anche con un dissipatore passivo grande; il Pi 5 di più, e ancora di più col **GPU Clock** sopra **Stock**. Su un Pi 5 chiuso dentro la scocca di un C64, senza aria, con un dissipatore piccolo e la CPU a -20%, la GPU a +20% ha portato l'emulazione intorno al 60%; con la GPU su **Stock** andava al 100%, a 67 gradi. Se l'emulazione rallenta, prova prima la GPU su **Stock**. La temperatura si vede in cima al menu e nella seconda riga della barra di stato.
- **La ventola**: sul Pi 5 la ventola collegata alla sua presa (quella dell'Active Cooler o di una scocca), e sul Pi 4 la ventola col filo di comando sul piedino 8 (GPIO 14), come la Case Fan ufficiale del Pi 4 (5 V sul piedino 4, massa sul 6, comando sull'8), si regolano con la voce **Fan** del menu principale, sotto **CPU Clock...** e **GPU Clock...**. **Auto**, la scelta di partenza, fa come Raspberry Pi OS: ferma sotto i 50 gradi, poi 30% da 50 gradi, 50% da 60, 70% da 67,5 e 100% da 75, e ogni gradino torna giù 5 gradi più in basso. Oppure a mano, da **0%** a **100%** a passi di 10%. La scelta vale subito, senza riavvio, e resta in cmdline.txt (`ventola=`). Una ventola a due fili attaccata al 5 V (piedino 2 o 4) o al 3,3 V (piedino 1 o 17) e alla massa gira sempre, forte sul 5 V e più piano sul 3,3 V: quei piedini sono l'alimentazione, e nessun programma li può regolare. Sul Pi 400, sul Pi 500 e sul 500+ la voce non c'è; sul Pi 4 non c'è nemmeno con l'uscita VGA666 (DPI), che usa anche il piedino 8. Fino al 14 ottobre 2026 la ventola del Pi 5 restava ferma; quella del Pi 4 si regola dal 15 ottobre 2026.
- **La ventola con `gpiofanpin=`**: con `gpiofanpin=` in cmdline.txt, col numero di un altro piedino, la ventola su quel piedino si accende e si spegne da sola con la temperatura. Prima restava sempre accesa.
- **La GPU nella barra di stato**: il numero dopo **GPU**, in cima al menu principale e nella seconda riga della barra di stato, è la frequenza misurata della parte 3D (**V3D**), quella che disegna gli shader CRT e che il **GPU Clock** alza: di serie 500 MHz sul Pi 4 e sul 400, 960 sul Pi 5 e sul 500. Fino al 14 ottobre 2026 lì c'era il nucleo della GPU (910 sul Pi 5), e una V3D alzata non si vedeva.
- **La risoluzione in cima al menu**: la prima riga del menu principale, quella con la macchina e il video, finisce con la risoluzione che esce in quel momento, per esempio *C64 PAL 50Hz HDMI 1080p*: **720p** o **1080p** in HDMI, **768p** coi modi VGA 1024x768, **576p** e **480p** col composito. Vale dal Pi 4 in poi.
- **Temperatura e frequenza** della CPU visibili in cima al menu e sulla seconda riga della barra di stato.
- **Data e ora** in cima al menu principale, sotto la temperatura, per esempio *Sat 03 Oct 2026  11:45:20*: arrivano dalla rete (NTP), col fuso e l'ora legale del menu **Network**; finché non arrivano c'è scritto *Clock: waiting for network time*. In Italia: **Timezone (reboot)** su UTC+01:00 e **Daylight Saving (reboot)** su **Auto (EU)**. Con lo stesso fuso si vedono le ore dei file da **SD Card Sharing**.
- **L'ora dalla rete, dal menu**: nel menu **Network**, cartella **NTP**: **Enable NTP** (acceso di fabbrica), **Server 1** (di fabbrica pool.ntp.org), **Server 2**, che è la riserva, e **Sync interval** (15 min, Boot only, 6 h, 12 h, 24 h). **Save & Apply** vale subito, senza riavviare; **Sync now** chiede l'ora adesso; **Status**, **Current time** e **Synced** dicono com'è andata.
- **I file senza rete**: finché l'ora non arriva, i file che il Raspberry scrive sulla scheda prendono la data più recente fra i file della radice della scheda. Prima avevano la data zero, che Windows mostra come 1601.
- **La versione** è nella prima riga di **About**, per esempio *BMC64-NG Ver. 1.1*: dice quale versione c'è sulla scheda.
- **Quale C64 sta girando** lo dice la riga **Engine** di **About**: *VICE 3.10 x64sc* è il C64 cycle exact, *VICE 3.10 x64* il C64 Std, *VICE 3.10 xscpu64* la SuperCPU. Non la scrive il menu: la dà il motore del VICE dentro il kernel che è partito, quindi è sicura anche quando la voce scelta nello **Switch** non lo dice.
- **Barra di stato** con le unità attive (8 e9 ), con LED verde di accensione, rosso di utilizzo del drive, la traccia corrente comprese le mezze tracce, il contanastro.
- **Le spie di ogni modello di drive**: nella barra di stato ogni unità ha le spie del suo drive vero. Il 1541, il 1541-II e il 1570 hanno due spie tonde, la verde dell'accensione a sinistra e la rossa del lavoro a destra; il 1571 due barrette larghe, con la rossa dell'accensione a sinistra; il 1581 due barrette, con la rossa a sinistra, che la accende e la spegne il DOS del drive, come sul 1581 vero. Accanto c'è la traccia, con la mezza traccia (18.0, 18.5).
- **Il tasto RESTORE**, quello che sul C64 vero faceva l'interruzione non mascherabile.
- **Il guardiano**: se l'emulatore resta fermo dieci secondi lo schermo diventa nero, con un riquadro grande in inglese, **BMC64-NG HAS STOPPED**, che dice dove si è fermato e come aiutarci a correggere il difetto: fotografare lo schermo e aprire una segnalazione (issue) su github.com/lroby74/BMC64-NG/issues, allegando i file BLOCCO.TXT, BMC64-DIAG.TXT e PASSI.TXT della scheda (dopo un riavvio BLOCCO e PASSI si chiamano BLOCCO-PRIMA.TXT e PASSI-PRIMA.TXT). Sulla scheda si scrive BLOCCO.TXT con quello che serve a capire dove, e la riga dell'arresto va anche in fondo a BMC64-DIAG.TXT; BLOCCO-PRIMA.TXT resta finché non arriva un blocco nuovo, anche dopo più riaccensioni. Col menu aperto il tempo non conta: il menu può restare aperto per ore. Un errore vero (un assert o un panico) lo dice subito, col suo testo, anche col menu aperto.
- **In questa versione il guardiano è spento**: il riquadro **BMC64-NG HAS STOPPED** e BLOCCO.TXT non ci sono. **PASSI.TXT** invece si scrive sempre: tiene le ultime cose che la macchina ha cominciato e finito (l'avvio, le voci del menu, i tasti rapidi). Se il Raspberry si blocca, dopo la riaccensione quel giro si chiama PASSI-PRIMA.TXT: l'ultima riga **PRIMA:** senza il suo **FATTO:** dice dove si è fermato. Mandacelo con BMC64-DIAG.TXT.
- **L'alimentazione nel rapporto**: BMC64-DIAG.TXT dice anche se il firmware ha visto una sottotensione e quale firmware è sulla scheda. Se la riga **alimentazione** dice **sottotensione**, il primo sospetto è l'alimentatore o il cavo.
- **USB-LOG.TXT**: a ogni apertura del menu, se c'è qualcosa di nuovo, ci si scrive cosa la USB ha trovato (dispositivi, interfacce, il driver scelto) e i rapporti del mouse coi loro byte: se un controller USB non va, quel file dice perché.
- **USB-LOG.TXT sul Pi 4 e sul 400**: ogni avvio dice anche se viene da un'accensione o da un riavvio senza spegnere (un cambio di macchina, Reboot), e la versione del firmware del chip USB del Raspberry (VIA VL805). Fino alla revisione 1.2 del Pi 4 quel firmware sta in una memoria del chip; dalla 1.4, e nel Pi 400, la memoria non c'è e lo carica il Raspberry a ogni avvio. Così i registri di due Raspberry si confrontano anche per il firmware.
- **NET-LOG.TXT**: a ogni apertura del menu, se c'è qualcosa di nuovo, ci si scrive cosa ha fatto la rete: quando il cavo perde il collegamento e quando torna, con quanti secondi è rimasto giù, l'indirizzo dato dal router e i suoi rinnovi, il WiFi. Durante il gioco non si scrive niente, così l'emulazione non rallenta mai: se la rete si è persa, apri il menu e il file lo dice.
- **Le correzioni del VICE dopo la 3.10**: ci sono le correzioni di fedeltà del VICE di sviluppo (le versioni uscite dopo la 3.10): il registro seriale delle CIA (C64, C128, SuperCPU e i drive 1570, 1571 e 1581), la lettura in più dell'istruzione RTS come sul 6502 vero, l'editor 80 colonne originale del PET (e l'avvio automatico coi suoi editor a 50 e 60 Hz), il registro $FFF0 dei PET 8096 e 8296, sul C128 niente più blocco del VDC con R0=0, il kernal del modo C64 scelto da te (per esempio il JiffyDOS) che all'avvio non torna più quello di serie e i bit di $01 coi resistori di pull-up, due registri del TED del Plus/4, il joystick a destra e la colonna 7 della tastiera del VIC-20 come sulla macchina vera, il disturbo dei POT (mouse, paddle) solo quando cambia un POT collegato, i comandi T-W del drive virtuale, la SFX Sound Expander, il DigiMAX, la tapecart e la scheda HRE del PET. E, dal 10 ottobre, la pulizia della tastiera che dimentica anche i tasti in coda: dopo un riquadro, per esempio ALT+SHIFT+R e poi CANCEL, la prima pressione dello SHIFT non si perde più.
- **Le correzioni del VICE fino al 9 novembre 2026**: sui modelli C64C (chip 8565) del C64 cycle exact uno sprite che passa da multicolore ad alta risoluzione a metà riga ha i punti giusti, come sul ferro; un interrupt FLAG, CNT o SP delle CIA non si perde più; una mappa di tastiera rovinata non ferma più la macchina; l'elenco di un dischetto rovinato, con la catena dei blocchi chiusa su se stessa, non gira più all'infinito; quando due periferiche rispondono sullo stesso indirizzo di I/O vince quella giusta; un file `.crt` rovinato viene rifiutato; gli snapshot con le cartucce del Plus/4 si ricaricano, e si ricaricano giusti anche quelli con le cartucce MMC64, LT.Kernal, Magic Desk Plus e TPI del C64 e con molte cartucce del VIC-20; col reSIDfp le forme d'onda combinate (T+S, P+S, P+T+S) sono quelle nuove di libresidfp, rifatte sui campioni dei chip veri, e il fruscio di fondo è quello nuovo, otto volte più basso; dopo un Load Snapshot le linee CB1 e CB2 delle VIA (nei drive 1541 e 1571, nel VIC-20 e nel PET) tornano come erano; sul C128 lo snapshot tiene tutte le cartucce attaccate e non solo la prima; gli snapshot con le cartucce Silverrock e REX RAM-Floppy si ricaricano giusti; gli snapshot salvano anche la cartuccia Blackbox V9, con le sue ROM intere, e si ricaricano giusti quelli con l'IDE64 e con la Double Quick Brown Box da 256 KB; uno snapshot rovinato o modificato a mano (un modulo troppo corto, la RAM del PET più grande del vero, i registri di GeoRAM, RAMCart, RAMLink e Double Quick Brown Box fuori misura) viene rifiutato invece di scrivere fuori dalla memoria; gli orologi delle cartucce (RTC) negli snapshot tengono l'ora a 64 bit; nel 1571 i piedini liberi della porta del drive si leggono alti come sul drive vero: li guardano i caricatori veloci di Super Snapshot 5 e Action Replay 5 per distinguere un 1571 a 2 MHz da un 1541; e altre correzioni piccole.

---

## Come si comincia

**Prima di tutto, la microSD.** Formattala FAT32 con cluster (unità di allocazione) normali: 32 KB, come fa SD Card Formatter, oppure la dimensione standard di Windows. **Niente cluster da 512 byte**: il Raspberry leggerebbe i suoi file di avvio e il kernel a pezzettini e aspetterebbe 9-10 secondi prima che BMC64-NG cominci (con una scheda da 32 GB, 45 secondi). Con i cluster normali, dall'accensione al BASIC ci vogliono circa 11 secondi; anche il cambio di computer, che riavvia il Raspberry, ne guadagna.

1. Copia i tuoi file (giochi, programmi, brani SID ma sopratutto le rom di sistema dei computer Commodore e dei drive) sulla microSD: 
ci sono le cartelle giá create e ognuna di essere contiene le sottocartelle per i vari computer. I file `.SID` vanno in  una cartella chiamata `sids`.
2. Accendi.
3. **F12** apre il menu. Le frecce per muoversi, INVIO per scegliere, F12 di
   nuovo per uscire.
4. Quando hai sistemato le cose come ti piacciono, dal menu scegli
   **Save settings**. Al prossimo avvio le ritrovi.

Da dentro il menu, la voce **Shortcut Keys** mostra l'elenco delle scorciatoie
sempre aggiornato.

**Se vedi nero**: solo una delle due uscite HDMI del Raspberry porta il segnale principale. Se lo schermo resta nero, sposta il cavo sull'altra porta HDMI.

**Se cambi sistema** dal menu **Switch** e non tocchi un tasto entro 15 secondi, per sicurezza torna da solo a quello precedente. Se il nuovo sistema si vede bene, basta premere un tasto qualsiasi per tenerlo.

**Lo stesso vale per la modalità video**: anche se nel menu **Switch** scegli solo un'altra risoluzione o un'altra frequenza, per tenerla devi premere un tasto entro 15 secondi, altrimenti torna da sola quella di prima. La voce **Safety timer (15s)**, in fondo al menu **Switch**, toglie questi 15 secondi a tutte le macchine, ma prima chiede conferma: senza il timer, se il monitor non mostra la modalità scelta, lo schermo resta nero e bisogna correggere `config.txt` da un PC.

---

## Cosa NON c'è dentro, e perché

Sulla scheda non troverai le **ROM protette da copyright**: il JiffyDOS, le
ROM del C64 DTV, i file `.SID` della collection. Non è una dimenticanza — sono
materiale di altri, e non si distribuisce. Se ce l'hai, lo metti tu sulla tua
scheda e funziona.

Le ROM del Commodore originali (KERNAL, BASIC, character generator) invece ci
sono: quelle il VICE le distribuisce e sono libere.

---

# TUTTE LE SCORCIATOIE

Tutte le combinazioni usano il tasto **ALT**. Lo SHIFT, ove presente é per attivare la seconda funzione speciale

## Il menu

| tasti | cosa fa |
|---|---|
| **F12** | apre e chiude il menu |
| frecce, INVIO | dentro il menu: muoversi e scegliere |
| **←** e **→** | dentro il menu: chiudono e aprono una cartella. In una lista di file **←** va alla cartella sopra e **→** entra in quella sotto il cursore; **→** su un'immagine (D64, T64, TAP...) mostra i programmi che ha dentro, coi caratteri del Commodore, e **INVIO** su uno lo fa partire; **←** o **ESC** tornano alla lista |
| **INVIO** su un file di testo | in una lista di file lo mostra (.txt, .md, .nfo, .log, .ini, .cfg, .vkm e simili, fino a 8 KB); una freccia a destra dice se sopra o sotto c'è altro testo; **←** o **ESC** tornano alla lista |
| **INVIO** tenuto | su un nome troppo lungo per la riga ne mostra la fine, e lasciandolo la voce non si apre. Il ritardo si sceglie in **Prefs**, **Enter shows name end** (Off, da 100 ms a 1 s) |
| **↑** e **↓** su un campo di testo | lasciandolo, quello che hai scritto vale come con **INVIO** (non nella ricerca e nei nomi dei file da salvare) |
| **Reboot?** e **Power Off?** | le domande partono su **CANCEL**: per riavviare o spegnere si sale su **OK** |
| tasto **Super** (logo di Windows, lampone) | da solo apre e chiude il menu, come **F12**. Con la mappatura **Maxi Positional** resta il tasto Commodore |
| **AltGr** | in un campo di testo, con la mappatura **Symbolic** e la tastiera italiana o tedesca: **[ ] { } @ # ~** come sul PC |
| **SPAZIO** su una voce | nel menu principale (un interruttore, una scelta, un pulsante, un numero) la mette in uno dei cinque posti di **Quick Access...**, in cima al menu: si sceglie il posto e **INVIO**. **INVIO** su un posto porta alla sua voce, aprendo le cartelle |
| **ESC** nel menu principale | porta su **Quick Access...**; un secondo **ESC** chiude il menu. I posti si salvano con **Save settings** |
| Riaprendo il menu (**F12**) | si torna sull'ultima voce toccata, con le cartelle aperte come le avevi lasciate, sia uscendo con **F12** sia con **ESC** (il salto su **Quick Access...** del primo **ESC** non conta) |
| **ALT** + **↑** e **↓** | dentro il menu: su e giú a blocchi, come **PgUp** e **PgDn** (tenuti premuti, continuano). Con ogni tastiera, anche sul 500+, dove FN non arriva al computer. A menu chiuso sono PLAY e STOP del nastro |
| **PgUp** e **PgDn**, **F3** e **F5** | dentro il menu: su e giú a blocchi. Sul 400 e sul 500 sono **FN** + **↑** e **↓** |
| **Home** e **End**, **F1** e **F7** | dentro il menu: in cima e in fondo. Sul 400 e sul 500 **Home** e **End** sono **FN** + **←** e **→** |
| lettere | in una lista di file: la ricerca per nome, già mentre scrivi; INVIO porta sul primo trovato (DEL cancella) |
| una lettera | dentro il menu: porta alla voce successiva che comincia con quella lettera (S per Sound) |
| mouse | dentro il menu: la freccia sceglie la riga, il tasto sinistro fa INVIO e il destro torna indietro; la rotella scorre la lista e, su una voce coi numeri, li cambia (anche tenendo premuto il sinistro e spostando). A menu chiuso il mouse è del C64. Si spegne in **Prefs**, voce **Mouse in Menu** |
| **Prefs** > **Menu Scale %** e **Row Gap px** | la grandezza del menu, da 75 a 115 % (di serie 100, come prima), e lo spazio fra le righe, da 0 a 4 punti (con 4 si vedono 17 righe invece di 25). Si salvano con **Save settings** |

## Il nastro (registratore)

| tasti | cosa fa |
|---|---|
| **ALT + ↑** | PLAY |
| **ALT + ↓** | STOP |
| **ALT + →** | avanti veloce |
| **ALT + ←** | indietro |
| **ALT + SHIFT + →** | avanti veloce **a doppia velocità** |
| **ALT + SHIFT + ←** | indietro **a doppia velocità** |
| **ALT + CANC** | azzera il contanastro |
| **ALT + T** | monta un nastro |
| **ALT + SHIFT + T** | smonta il nastro |

Senza un nastro montato i comandi del registratore non fanno niente, né coi tasti qui sopra né dal menu o dai pulsanti del joystick: in basso compare **NO TAPE**. Sulla SuperCPU, che nel VICE il registratore non ce l'ha, compare **NO TAPE ON THIS MACHINE**.

## Dischi e cartucce

| tasti | cosa fa |
|---|---|
| **ALT + 8** | monta un dischetto nell'unità 8 |
| **ALT + SHIFT + 8** | smonta l'unità 8 |
| **ALT + 9** | monta un dischetto nell'unità 9 |
| **ALT + SHIFT + 9** | smonta l'unità 9 |
| **ALT + C** | inserisce una cartuccia |
| **ALT + SHIFT + C** | stacca la cartuccia |
| **ALT + A** | autostart: scegli un file PRG o Disk e parte da solo |
| **ALT + 0** | carica un file .REU dalla cartella /REU |
| **ALT + SHIFT + 0** | stacca il file .REU (la REU resta accesa) |

## Il modello del drive

I numeri da 1 a 7 scelgono il modello. **Senza SHIFT vale per l'unità 8, con
SHIFT per l'unità 9.**

| tasti | modello |
|---|---|
| **ALT + 1** | nessun drive |
| **ALT + 2** | 1541 |
| **ALT + 3** | 1541-II |
| **ALT + 4** | 1570 |
| **ALT + 5** | 1571 |
| **ALT + 6** | 1581 |
| **ALT + 7** | drive fisico (adattatore XUM1541) |
| **ALT + SHIFT + 1…7** | le stesse sette scelte, ma per l'**unità 9** |

Se l'adattatore XUM1541 non c'è, **ALT + 7** lo dice sullo schermo (NO XUM FOUND) e il drive resta quello di prima.

**La XUM che arriva tardi**: con il drive vero salvato, all'accensione e dopo un cambio di macchina la XUM1541 si aspetta fino a 12 secondi, poi l'unità torna al drive emulato e compare NO XUM FOUND. Serve alla XUM fatta con un Pro Micro, che ha il programma di avvio di Arduino: dopo ogni riavvio del Raspberry ci mette circa 10 secondi a presentarsi (la ZoomFloppy è pronta in 3). Fino al 6 novembre 2026 si aspettavano 3 secondi.

**La XUM su un Pro Micro dopo un riavvio**: l'attesa non sempre basta. Con una XUM1541 fatta con un Arduino Pro Micro il drive Commodore vero può funzionare solo alla prima accensione a freddo e smettere di rispondere dopo un cambio di macchina o un riavvio del Raspberry: quell'adattatore potrebbe non essere compatibile.

**La ROM del drive** scelta dal menu (**Drives**, **Change ROM...**) arriva subito al drive acceso di quel modello, che riparte con quella nuova; prima serviva un riavvio. Se il file non va bene, resta quella di prima.

Sul Plus/4 del VICE **ALT + 4** mette il 1551, il drive della macchina, al posto del 1570. In Plus4Emu i modelli sono tre: **ALT + 2** il 1541, **ALT + 3** il 1551 e **ALT + 4** il 1581, che apre subito l'elenco dei `.D81`.

Nel lettore SID **ALT + 1**, **ALT + 2**, **ALT + 3** e **ALT + 4** non cambiano il drive: spengono e riaccendono i SID, dal primo al quarto.

Coi brani a 8 SID anche **ALT + 5**, **ALT + 6**, **ALT + 7** e **ALT + 8**: dal quinto all'ottavo SID.

## Reset e spegnimento

| tasti | cosa fa |
|---|---|
| **ALT + R** | reset **duro** (svuota la memoria: è quello che fa ripartire un gioco) |
| **ALT + SHIFT + R** | reset **dolce** (non cancella la memoria, ideale per modificare o analizzare un codice in memoria) |
| **ALT + ESC** | riavvia il Raspberry — chiede conferma, si preme una seconda volta |
| **ALT + SHIFT + ESC** | spegne il Raspberry — stessa conferma, sui Raspberry PI 500 e 500+ é possibile usare il pulsante apposito) |
| **Fn + F10** (PI 400) | spegne il Raspberry, con la stessa conferma: si preme una seconda volta |
| **C= + F7**, tenuti 5 secondi | modo video sicuro, per quando lo schermo non si vede: torna al C64 PAL in HDMI 720p, salva come il riavvio e riavvia (C= è il CTRL di sinistra) |
| **ESC** da solo | annulla la conferma |

## Video

| tasti | cosa fa |
|---|---|
| **ALT + "\"** | passa fra 4:3 e 16:9 |
| **ALT + SHIFT + "\"** | la stessa cosa sull'**altro** schermo (solo x C128 in dual DISPLAY) |
| **ALT + SHIFT + "+"** | aumenta le scanline |
| **ALT + SHIFT + "−"** | diminuisce le scanline |
| **ALT + STAMP** | C128: passa fra lo schermo a 40 colonne e quello a 80 (simula il tastino dei monitor)|
| **ALT + SHIFT + STAMP** | C128: il tasto **40/80 DISPLAY** della tastiera vera |
| **ALT + F7** | C128: come sopra, il tasto 40/80 |
| **ALT + O** | toglie la barra di stato, e ripremendolo la rimette com'era |
| **ALT + SHIFT + O** | accende e spegne la seconda riga della barra di stato (temperatura, frequenza, velocità) |
| **SUPER + STAMP** | la fotografia dello schermo, nella cartella `/screenshots` della scheda (**Prefs** > **Screenshots**) |

## Suono

| tasti | cosa fa |
|---|---|
| **ALT + +** | alza il volume |
| **ALT + −** | abbassa il volume |
| **ALT + M** | silenzia |
| **ALT + S** | apre il lettore di file .SID; se cambi idea, ESC torna subito dov'eri, anche al brano che stava suonando |
| **ALT + SHIFT + S** | ferma il brano e resetta |
| **ALT + ,** | lettore SID: brano precedente |
| **ALT + .** | lettore SID: brano successivo |
| **ALT + P** | lettore SID: pausa, e ripremendolo riprende |
| **← →** senza ALT | lettore SID: 10 secondi indietro o avanti nel brano |
| **↑ ↓** senza ALT | lettore SID: alza e abbassa il volume |
| **1 … 9** senza ALT | lettore SID: spegne e riaccende una voce (1-3 primo SID, 4-6 secondo, 7-9 terzo) |
| **0, −, +** senza ALT | lettore SID: spegne e riaccende una voce del quarto SID (10, 11, 12) |
| **ALT + 1, 2, 3, 4** | lettore SID: spegne e riaccende il primo, il secondo, il terzo o il quarto SID |
| **F1, F2, F3, F4** | lettore SID: il lato del primo, del secondo, del terzo o del quarto SID: L, C, R e di nuovo L |
| **ALT + 5, 6, 7, 8** | lettore SID, coi brani a 8 SID: spegne e riaccende il quinto, il sesto, il settimo o l'ottavo SID |
| **F5, F6, F7, F8** | lettore SID, coi brani a 8 SID: il lato del quinto, del sesto, del settimo o dell'ottavo SID: L, C, R e di nuovo L |
| **V, T, N** senza ALT | lettore SID: VU meter, tempo, righe col nome del brano |
| **, . P M** senza ALT | lettore SID: brani, pausa e silenziatore, come con ALT (il volume con ↑ ↓, o con ALT + + e ALT + −) |
| **"\"** senza ALT | lettore SID: passa fra 4:3 e 16:9, come ALT + "\" |
| **S** senza ALT | lettore SID: passa fra reSID e reSIDfp (il nome per un secondo nella riga del volume) |

Il `+` e il `−` funzionano sia quelli della fila dei numeri sia quelli del
tastierino, su tastiera italiana come su quella americana.

## Altro

| tasti | cosa fa |
|---|---|
| **ALT + W** | warp: la macchina viene accelerata (utile per saltare pari noiose) |
| **ALT + J** | scambia le due porte joystick |
| **ALT + E** | accende e spegne la REU |
| **ALT + SHIFT + E** | cambia la misura della REU (512 KB ↔ 16 MB) |

## I tasti in più del Commodore 128

Il C128 ha otto tasti che il C64 non ha e che una tastiera da PC non prevede.
Stanno su **ALT + F1…F8**, nell'ordine in cui si trovano sulla macchina vera.

| tasti | tasto del C128 |
|---|---|
| **ALT + F1** | ESC |
| **ALT + F2** | TAB |
| **ALT + F3** | ALT |
| **ALT + F4** | CAPS LOCK |
| **ALT + F5** | HELP |
| **ALT + F6** | LINE FEED |
| **ALT + F7** | 40/80 DISPLAY |
| **ALT + F8** | NO SCROLL |

Sette di questi hanno anche un tasto da solo, come nel BMC64 di Randy: **F9** ESC, **F10** TAB, **F11** 40/80 DISPLAY, **FINE** HELP, **PAG GIÙ** LINE FEED, **BLOC SCORR** NO SCROLL, **PAUSA** CAPS LOCK.

---

## Crediti

**BMC64** è di Randy Rossi. **VICE** è del VICE Team. **Plus4Emu** è di
Istvan Varga. **Circle**, lo strato che permette di girare senza sistema
operativo, è di Rene Stange.

Parti di BMC64-NG vengono da **BMX** di kdre76 (baremetalx.de): il video KMS, il V3D e gli shader CRT, l'audio, la SuperCPU, le correzioni del WiFi, l'editor della mappa di tastiera, il mouse, le funzioni del menu, Quick Access, le fotografie dello schermo, il WiC64, Mapping Mode e Overclocking. Dentro ci sono anche miniz (licenza MIT), mbedtls (Apache 2.0) e i certificati di Mozilla (MPL 2.0).

**BMC64-NG** è di Cla-Bob Systems.

Software libero, licenza GNU GPL. Il testo completo è nel menu, sotto
**License**.
