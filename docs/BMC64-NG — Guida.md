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
1) Non é stata concepita per andare oltre il Raspberry PI 3 e varianti, che si troveranno con sempre maggiore difficoltá sul mercato e comunque
offrono prestazioni limitate rispetto ai modelli usciti in seguito.
2) A causa di quanto detto al punto 1, non puó essere aggiornato alla versione piú recente del Vice 3.10, ed é tutt'ora legato alla 3.3 del 2018.
3) E sempre per lo stesso motivo, non puó far funzionare il C64 nella modalitá Cycle Exact che dona maggiore fedeltá all'emulazione ma richiede potenza 
hardware che il PI 3 (tanto meno quelli piú limitati come hardware) puó offire.
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
- **Drive fisico** via adattatore XUM1541: colleghi un 1541 vero o altro drive Commodore e leggi i
  tuoi dischetti.
- **Creazione di dischetti vuoti** dal menu.
- **Due lettori di floppy USB**: nell'elenco dei file compaiono come **FLP1** e **FLP2**. I file e le immagini dei dischi si mettono sul floppy dal PC (dischetti FAT), e da BMC64-NG si usano come quelli della scheda.
- **Nastri** `.tap` e `.t64`, con il contanastro, i comandi del registratore e
  l'avanzamento veloce.
- **Cartucce** `.crt` e `.bin`.
- **La cartuccia a ogni accensione**: sul C64 e sul C128, per ritrovare una cartuccia a ogni accensione bisogna renderla predefinita dal menu **Cartridge** con la voce **Set current cart default**, che salva da sola. Vale anche al contrario: se la stacchi e non la vuoi piú all'accensione, dopo averla staccata scegli di nuovo **Set current cart default**.
- **Le cartucce EasyFlash salvano**: quello che il gioco scrive nella flash torna nel suo file .crt con la voce **Save EasyFlash Now** del menu **Cartridge**, da solo quando si stacca la cartuccia o se ne mette un'altra, e anche quando si spegne, si riavvia o si cambia macchina, se il gioco ci ha scritto.
- **Programmi** `.prg` caricati direttamente.
- **Autostart**: scegli il file e parte da solo, senza scrivere niente.
- **Il dischetto dell'autostart**: con l'emulazione vera del drive, un `.prg` lanciato con l'autostart il VICE lo mette in un dischetto che scrive nella radice della scheda, `autostart-` e il nome della macchina, per esempio `autostart-PLUS4.d64`. Si può cancellare: alla volta dopo si rifà.
- **I nomi di file lunghi**: dischi, nastri, cartucce e programmi con nomi fino a 255 caratteri, il massimo della scheda, si attaccano e partono dal menu come gli altri. Prima un nome molto lungo bloccava la macchina.
- **Ricerca nei file**, come sulla Kung Fu Flash 2: dentro una cartella scrivi le prime lettere del nome (per esempio `dra`) e premi INVIO: restano solo i file e le cartelle che cominciano cosí, maiuscole o minuscole. Si possono usare i jolly della KFF2: il punto interrogativo vale un carattere qualunque, e `*lair` trova i nomi che contengono "lair". DEL cancella; cambiando cartella la ricerca sparisce.
- **Se nessun nome comincia cosí**, la stessa ricerca si rifà dentro al nome: scrivendo `12` fra i file di Dragon's Lair esce `DrL-12-850.reu`, e scrivendo `II` esce `Turrican II.tap`.
- **I file del Mac**: i file `._qualcosa` che un Mac lascia sulla scheda accanto agli altri non compaiono negli elenchi del menu, come nel BMC64 di Randy.
- **Immagini .REU**: i giochi e le demo che usano la REU la caricano da un file. I file vanno nella cartella `/REU`: **ALT + 0** ne carica uno, **ALT + SHIFT + 0** lo stacca.
- **Il contenuto della REU nel suo file**: nel menu **Ram Expansion Unit (REU)**, cartella **Ram Image (optional)**, la voce **Auto-save image**, spenta di fabbrica. Accesa, il file .REU si riscrive col contenuto della REU quando si spegne o si riavvia, dal menu o coi tasti, e quando si cambia macchina. Staccando la corrente non si salva. Per le immagini dei giochi, come Dragon's Lair, conviene lasciarla spenta.
- **Se il file non si può scrivere** (per esempio perché è di sola lettura), allo spegnimento il Raspberry resta acceso e lo dice (NOT SAVED); chiedendo di nuovo di spegnere, si spegne lo stesso.

### Video

- **Scanline** regolabili da 0 a 100, per l'aspetto del monitor a tubo.
- **Rapporto d'immagine** 4:3 o 16:9, commutabile al volo.
- **L'immagine spostata non rallenta**: con **H Center** e **V Center**, o allargandola, l'immagine può uscire un po' dai bordi: la parte fuori si taglia e basta. Prima, appena usciva anche di una riga, l'emulazione poteva scendere al 50%.
- **Il VIC-20 NTSC col suo KERNAL**: nei modi NTSC del VIC-20 parte da solo il KERNAL americano (901486-06), che mette lo schermo al centro come su un VIC-20 NTSC vero; nei modi PAL quello europeo (901486-07). Una ROM diversa scelta dal menu resta la tua.
- **Regolazione dell'immagine**: luminosità, contrasto, colore, gamma, e la
  geometria (larghezza, altezza, centratura).
- **Il doppio schermo del C128**: il VIC-II a 40 colonne e il VDC a 80 sono
  due uscite separate, ognuna con le sue regolazioni. Si commuta con un tasto.
  E se le colleghi due monitor, avrai il doppio output 40 e 80 colonne proprio come allora.
- **Il doppio schermo anche in 16:9**: con due monitor ognuno dei due schermi del C128 riempie tutto il televisore, in 4:3 come in 16:9.
- **I due monitor dal menu**: nel menu **Video** del C128, **Active Display** su **Two HDMI** manda il VIC-II su una HDMI e il VDC sull'altra (la prima volta il menu chiede di riavviare). Prima metti **Second Display Present** su **On**: finché è su **Off**, girando **Active Display** con le frecce, con INVIO o col tasto **Cycle Display**, **Two HDMI** si salta e non chiede niente. I due schermi li disegna il core 3, così il core dell'emulazione resta tutto alla macchina.
- **L'uscita HDMI principale**: nel menu **Video** la voce **Main HDMI Output** sceglie su quale delle due porte HDMI va lo schermo principale, e con lui l'audio HDMI. Con INVIO il menu chiede di riavviare, e la scelta vale da lí in poi. Sul C128 con due monitor scambia i due schermi. Se all'accensione la porta scelta non ha il monitor o non risponde, lo schermo va sull'altra.
- **L'uscita composita del Pi 4 B**: nel menu **Switch** ogni macchina ha anche **PAL Composite 576p@50Hz** e **NTSC Composite 480p@60Hz**. Dopo il riavvio immagine e suono escono dal jack da 3,5 mm (cavo AV a 4 poli) col segnale della macchina vera, 50 o 60 Hz non interlacciato; la HDMI si spegne e lo shader CRT non c'è. I bordi partono come sul BMC64 originale, cioè come un C64 vero su un monitor a tubo; se li ritocchi dal menu **Video** e salvi, il composito tiene i suoi, separati da quelli della HDMI. La velocità segue il televisore: il suo passo si misura all'accensione. Si torna alla HDMI con una voce HDMI dello **Switch**, oppure tenendo premuti il tasto Commodore (di fabbrica CTRL di sinistra) e F7 per 5 secondi e lasciando F7. Il Pi 400, il Pi 5 e il Pi 500 il jack composito non ce l'hanno, e da loro le voci non compaiono.
- **Le barre e la centratura sul composito**: col composito le due barre di stato sono disegnate per le sue righe (288 in PAL, 240 in NTSC), coi caratteri interi. In PAL **H Center** a 0 mette l'immagine al centro di un monitor a tubo, e spostarla di qualche punto non rallenta più l'emulazione.
- **Niente 16:9 e 4:3 sul composito**: col composito le voci **Aspect 4:3** e **Aspect 16:9** del menu **Video** non ci sono, e il tasto rapido del 16:9 e 4:3 (ALT + "\", e nel lettore SID anche "\" senza ALT) dice NOT ON COMPOSITE: le proporzioni le dà il composito, e toccarle sballava l'immagine. Sulla HDMI tutto come prima.
- **Monitor VGA**: con un adattatore da HDMI a VGA va anche un monitor VGA da PC. Nel menu **Switch** ogni macchina ha i modi **VGA 1024x768** a 50 e 60 Hz (circa 40 e 48 kHz: un monitor che arriva solo a 31 kHz non li aggancia).
- **Fotografia dello schermo** salvata sulla scheda.

### Suono

- **Il SID vero**, emulato con reSID: sia il 6581 dei primi C64 sia l'8580 dei
  successivi.
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
- **Il lettore SID sul Pi 4: i limiti**: il reSIDfp chiede molto più calcolo del reSID. Sul Pi 4 col clock di serie i brani fino a 4 SID col reSIDfp reggono; coi brani a 8 SID il reSIDfp vuole l'overclock: alza il **CPU Clock** del menu principale a **+20%**, oppure resta al clock di serie col reSID, che chiede meno calcolo (tasto **S** o **SID Engine** del menu **Sound**).
- **Il campionamento del SID**: nel menu **Sound** la voce **SID Resampling** è di fabbrica su **Resampling**, il modo piú fedele: il suono del SID arriva alle cuffie senza i disturbi che gli altri modi aggiungono sugli acuti. **Fast Resampling** suona uguale, ma usa piú memoria e ci mette di piú a prepararsi; **Interpolation** e **Fast** costano meno al Raspberry e sono meno fedeli.
- **I filtri del reSID**: nel menu **Sound** la cartella **reSID Filter Settings** ha le regolazioni del 6581 e dell'8580: **Passband %** (di fabbrica 90), **Gain %** (97) e **Filter Bias (mV)**. Valgono per tutti i SID di quel modello. Passband e Gain contano solo con **SID Resampling** su **Resampling** o **Fast Resampling**; il suono cambia quando si chiude il menu, e **Save settings** li tiene.
- **reSIDfp, il terzo motore del SID**: nel menu **Sound** la voce **SID Engine** ha anche **ReSIDfp**, accanto a **ReSid**, che resta quello di fabbrica. È il motore del SID più recente del VICE, e il più fedele al chip vero: le forme d'onda combinate (quelle che il SID produce quando una voce ne usa due insieme) sono prese da misure su chip veri, e il filtro del 6581 ha la sua distorsione. È allineato all'ultima versione dei suoi autori (libresidfp, 22 settembre 2026), che ha rifatto le forme d'onda combinate del 6581 più vicine ai chip veri. La cartella **reSIDfp Settings** ha le sue regolazioni: **Combined Waveforms** (Weak, Average, Strong), **Background Noise** (qui sotto), e per il **6581** **Filter Curve**, **Filter Range** e **Old Caps (2200pF)**, per l'**8580** **Filter Curve**. Il suono cambia quando chiudi il menu; **Save settings** tiene tutto. Chiede un po' più di calcolo del reSID, e il secondo SID resta sul suo core. C'è anche per la SID Card del Plus/4 e del PET. Non c'è sul C64 DTV, che ha un SID suo.
- **Il fruscio di fondo di reSIDfp**: un C64 vero non è mai del tutto silenzioso, e nelle sue registrazioni c'è sempre un leggero fruscio di fondo. reSIDfp lo aggiunge al suono per somigliare al chip vero: la voce **Background Noise** in **reSIDfp Settings** lo regola da **OFF** (di fabbrica: con la macchina ferma, silenzio come col reSID) a **100%** (il livello scelto dagli autori di reSIDfp per somigliare alle registrazioni dei C64 veri), a passi del 10%. Per un suono più realistico alzalo; se ti disturba, lascialo su OFF. Si sente soprattutto quando non suona niente, per esempio fermi al BASIC. Ogni SID ha il suo fruscio, indipendente dagli altri: con due, tre o quattro SID si sommano, come farebbero quelli di altrettanti chip veri.
- **Stereo o mono**: nel menu **Sound**, subito sotto **Audio out**, la voce **Channels** sceglie come escono i SID quando sono piú di uno (il **Dual SID**, o un brano a 2 o 3 SID nel lettore). **Stereo**, di fabbrica: ogni SID dal suo lato (con 3 SID il primo a sinistra, il terzo a destra e il secondo su tutti e due). **Mono**: tutti i SID insieme su tutti e due i lati. **Save settings** la tiene. C'è su C64, C128 e SuperCPU; le altre macchine hanno un SID solo, o nessuno, e il suono è già mono.
- **Il tempo del brano**: nel lettore SID, a sinistra della terza riga, i minuti e i secondi del brano. È il tempo della musica: in pausa si ferma, e cambiando brano riparte da zero. A destra c'è ogni quanto suona il brano: PAL 50Hz, NTSC 60Hz, oppure CIA col numero che il brano ha messo nel timer.
- **Il volume nel lettore SID**: per un secondo dopo ogni tasto del volume compare una barra orizzontale a segmenti, fra le scritte sotto le colonne (per esempio 1 8580 R) e la riga del brano (DEFAULT TUNE), con la percentuale a destra, e MUTE in rosso quando il volume è a zero.
- **Dentro il lettore SID** la barra di stato non compare, per non coprire le colonne, e ALT + W non fa niente: a velocità doppia un brano non si ascolta.
- **Il DAC USB anche a computer acceso**: con **Audio out** su **USB DAC** il suono passa al DAC appena lo colleghi e torna sull'HDMI quando lo stacchi, e vanno anche i DAC che lavorano solo a 48 kHz, con tutte le macchine, Plus4Emu compreso. Non serve piú collegarlo prima di accendere.
- **Se il DAC USB sparisce per un attimo** (qualche DAC economico si stacca e si riattacca da solo), il suono aspetta tre secondi prima di tornare sull'HDMI: se il DAC torna, riprende da lí.
- **Volume** regolabile da tastiera, e il silenziatore.
- **Il 100% è il suono com'è**, senza amplificazioni che lo storcano. Arrivato al 100% ALT + + non fa piú niente, e a zero ALT + − nemmeno: la barra di stato dice VOLUME 100% MAX, oppure MUTE.

### Comandi e controlli

- **Joystick USB** e gamepad, fino a quattro porte.
- **Joystick sui GPIO**: se hai un adattatore per joystick d'epoca collegato ai
  pin del Raspberry, funziona.
- **Keyset**: usare la tastiera come joystick, con i tasti che scegli tu. **PotX** e **PotY** sono il secondo e il terzo pulsante, per i giochi che li usano, come sui gamepad USB; se non ti servono, dagli lo stesso tasto del Fire.
- **Mouse** (il 1351 del Commodore).
- **Lo spinner come paddle**: su C64, C128 e VIC-20, nel menu **Joyports**, **Port 1** o **Port 2** su **USB Spinner (Paddle)**: la rotella di un mouse USB fa da paddle, come sul MiSTer. Va bene anche uno spinner che per il Raspberry è un mouse con la rotella, come quello del TAITO EGRET II mini. Ogni scatto della rotella sposta la paddle di un passo, da 0 a 255, e uno qualunque dei tasti del mouse è il tasto della paddle, anche oltre il quinto: sul pannello del TAITO sparano anche i due pulsanti grandi. Il verso è quello del pannello del TAITO (provato con Omega Race e Le Mans). **Spinner Sensitivity**, nello stesso menu, dice quanto si muove la paddle a ogni scatto: da 25% a 400%, e 100% è un passo per scatto. Lo spinner del TAITO per il PC è un mouse: la trackball muove il puntatore e lo spinner è la rotella su e giù. Anche un mouse USB che non dichiara la classe "boot" adesso va al driver del mouse, con la rotella. Il mouse 1351 e lo spinner non stanno insieme: scegliendone uno, l'altro torna su **None**; e se l'altra porta cambia, lo spinner non rallenta più la macchina. Come sul Commodore vero, sul C64 e sul C128 il tasto della paddle tenuto giù sulla porta 1 blocca la tastiera: quella linea è anche una riga della tastiera.
- **Mappe di tastiera** posizionale e simbolica con layout US, IT, UK e DE.
- **La tastiera dal menu**: nel menu **Keyboard** la voce **Mapping** sceglie fra **Positional** e **Symbolic**, e **Layout** fra US, Italiana, British e Deutsch (le ultime tre con **Mapping** su **Symbolic**). Le tastiere del Pi 400 e del Pi 500 vanno come una tastiera USB. Con **Symbolic** il **+** e gli altri tasti battuti con lo SHIFT non escono più, ogni tanto, come caratteri grafici.
- **La tastiera vera del C64 con la Keyrah V2**: con la mappatura **Positional** e il deviatore della Keyrah in basso (il modo per gli emulatori) vanno tutti i tasti, anche **RESTORE**: la Keyrah lo manda come **Page Up**, che nelle mappature Positional di C64, C128, VIC-20 e SuperCPU è il secondo tasto RESTORE, accanto a **PrtScn** dei Pi 400 e 500, che resta com'era. Nel menu **Keyboard** la riga **Restore key** li dice tutti e due. Le due porte joystick DB9 della Keyrah vanno coi keyset **NUMPAD 17930** e **NUMPAD 64825**, oppure si rimappano in **Custom Keyset 1** e **2** per usarle al posto di quelli NUMPAD.
- **Il tasto < > delle tastiere ISO**: nelle mappature **Positional** di C64, C128, VIC-20, SuperCPU e Plus/4 il tasto accanto allo SHIFT di sinistra (quello di < e > sulle tastiere italiane ed europee) scrive la sterlina **£**, come nel BMC64 di Randy; prima non faceva niente. Nel lettore SID resta il tasto del 4:3 e 16:9.
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
- **Il WiFi del Pi 400**: il Pi 400 ha un chip WiFi diverso da quello del Pi 4 e usa i tre file `brcmfmac43456-sdio` (`.bin`, `.txt` e `.clm_blob`) della cartella `/firmware` della scheda: ci sono già. Se mancano, o se il chip non parte, la macchina va avanti, anche all'accensione con **Network Device** su WiFi, e la finestra di **WiFi SSID** lo dice (*WiFi firmware missing on the SD card* oppure *The WiFi chip did not start*).
- **Il WiFi dal menu**: nel menu **Network**, cartella **WiFi Settings**, la voce **WiFi SSID** cerca le reti senza riavviare e le elenca con la banda (MHz), il canale (CH) e il segnale (RSSI). Scelta la rete, **WiFi Country Code** va messo sul paese dove sei: col cursore sulla voce si scrivono le due lettere (IT per l'Italia, anche minuscole); a campo pieno una lettera ricomincia da capo e **DEL** lo svuota. Il codice si salva appena è completo e vale dal riavvio. Poi **Enter Password & Reboot**: la password si vede in chiaro mentre la scrivi, e una lunga scorre dentro il riquadro; **Save & Reboot** salva, passa al WiFi e riavvia.
- **Indirizzo automatico o fisso**: **IP Address Mode (reboot)** su **DHCP**, di fabbrica, prende l'indirizzo dal router, e i campi **Static IP**, **Netmask**, **Gateway** e **DNS** mostrano quelli ricevuti. Su **Static** si scrivono a mano; senza **Netmask** vale 255.255.255.0. Vale dal riavvio. Al router il Raspberry si presenta col nome **BMC64-NG**.
- **L'ora legale**: sotto **Timezone (reboot)** c'è **Daylight Saving (reboot)**: **Off**, **On**, oppure **Auto (EU)**, che la mette e la toglie da sola con le regole europee, dall'ultima domenica di marzo all'ultima di ottobre.
- **La scheda vista dal PC**: la voce **SD Card Sharing...** del menu principale apre la microSD al PC di casa, senza toglierla dal Raspberry. Su Windows, in Esplora file, basta scrivere `\\BMC64-NG`, oppure `\\` e l'indirizzo che il riquadro mostra (per esempio `\\192.168.1.50`): compare la cartella **sdcard**, che si apre con un doppio clic; si può anche scrivere tutto, `\\192.168.1.50\sdcard`. Se il nome non risponde, l'indirizzo va sempre. Windows prova prima con l'utente del PC e poi chiede le credenziali: utente **bmc64**, password **bmc64**. La password si cambia con **Sharing Password** nel menu **Network**. Mentre la scheda è aperta al PC l'emulazione è ferma e i dischetti montati sono già salvati; i file che la macchina sta usando non si possono cancellare né sovrascrivere dal PC. **Stop Sharing**, o uscire dal riquadro, chiude la condivisione e la macchina riparte. Serve la rete accesa (menu **Network**): senza rete il riquadro dice **Cannot start SD Card Sharing**. Il riquadro può restare aperto quanto vuoi. Finché la condivisione è accesa il Raspberry compare anche nella cartella **Rete** di Esplora file, come **BMC64-NG**: un doppio clic e si apre.
- **I nomi lunghi anche dal PC**: nella condivisione i file hanno il loro nome intero, anche coi caratteri che prima non passavano (la lineetta lunga dei nomi delle guide, il greco, il giapponese), e dal PC si creano, si rinominano e si copiano cosí. Prima un nome con uno di quei caratteri compariva col suo nome corto (per esempio `BMC64-~3.PDF`), e una copia da lì su un'altra scheda poteva finire sopra un altro file. Anche nel menu i nomi con le lettere accentate ora si leggono giusti.
- **Il NAS, come sul MiSTer**: la voce **NAS** del menu principale ha **NAS IP Address** (l'indirizzo del NAS, per esempio 192.168.1.100), **NAS User Name** e **NAS Password** (vuoti, si entra come ospite) e **SMB Protocol**: **Auto**, di fabbrica, oppure SMB 2.0, 2.1, 3.0, 3.0.2 o 3.1.1 (SMB1 non c'è: è vecchio e i NAS di oggi lo tengono spento). **NAS Path** è il percorso sul NAS: prima la condivisione, poi la cartella da cui parte il navigatore. Di fabbrica è vuoto: sul NAS del MiSTer, per esempio, è `Mister\games\C64` (anche per la SuperCPU), `Mister\games\C128`, `Mister\games\Vic20`, `Mister\games\PET2001` e `Mister\games\C16` (Plus/4). Si scrive come gli altri campi, con `\` o con `/`, si vede sempre in chiaro e ogni macchina tiene il suo; finché è vuoto il NAS risponde **NAS Path not set**. **Test NAS Connection** prova subito la connessione e la cartella. Indirizzo, utente, password e protocollo valgono per tutte le macchine, il percorso per quella accesa; tutto si salva con **Save settings**, nel file `nas.txt` della scheda. Nel navigatore dei file la prima riga apre la scelta del volume: accanto a SD e USB c'è **NAS**, che parte dalla cartella della macchina (mentre si collega compare **Connecting to the NAS...**; un NAS spento risponde dopo una decina di secondi). Dischi, nastri, cartucce e programmi si usano come dalla scheda: il file si legge tutto quando si apre, e quello che il gioco salva arriva sul NAS appena si spegne la spia rossa del drive, e quando il disco si stacca; se in quel momento il NAS non risponde, il disco si salva sulla scheda, nella cartella `NAS-NON-SALVATI`, e niente va perso. File fino a 32 MB. Serve la rete accesa (menu **Network**). Il NAS e la condivisione della scheda non lavorano insieme: aprire **SD Card Sharing...** chiude il NAS, che si ricollega da solo quando serve.
- **Le password dopo il riavvio**: la password del WiFi (**Enter Password & Reboot**) e la **NAS Password** si vedono in chiaro mentre le scrivi; dopo il salvataggio e il riavvio si mostrano con gli asterischi: il primo tasto le cancella e si riscrivono in chiaro, e senza toccarle restano quelle salvate.
- **Il modem, come la Ultimate II+** (solo C64 e C128): **Modem (SwiftLink)** nel menu **Network**, spento di fabbrica: acceso occupa `$DE00`, e con l'EasyFlash va lasciato spento. **Modem Address** è di fabbrica `DE00`, quello che vogliono CCGMS e gli altri programmi per le BBS. Da CCGMS si chiama una BBS con `ATDT` e l'indirizzo con la porta, per esempio `ATDT bbs.retrocampus.com:6510`; senza porta si chiama la 80. Si possono anche ricevere chiamate, sulla porta 3000: il modem squilla (RING) ogni 3 secondi, fino a 10 volte; **ATA** risponde, e con **ATS0=1** risponde da solo. Comandi, risposte e registri sono quelli della Ultimate II+.
- **Overclock e Underclock** guidato dal menu, con le frequenze già provate. Nel menu principale ci sono **CPU Clock...** (-20%, -10%, Stock, +10% o +20% della frequenza di serie di quel Raspberry) e **GPU Clock...** (Stock, +10% o +20%), e dopo l'OK si riavvia. **GPU Clock** cambia insieme il nucleo della GPU (**Core**) e la parte 3D (**V3D**), che disegnano lo schermo: di serie 500 e 500 MHz sul Pi 4 e sul 400, 910 e 960 MHz sul Pi 5 e sul 500. La GPU non si può rallentare: il suo nucleo compone anche lo schermo, e sotto la frequenza di serie tutto andava a metà velocità. In testa al menu **GPU Clock** ci sono le frequenze che il Raspberry sta usando; i MHz della GPU si vedono anche in cima al menu principale e nella seconda riga della barra di stato, accanto a quelli della CPU. Come per la CPU, la scelta vale solo per il Raspberry su cui l'hai fatta: la stessa scheda su un altro Pi parte con le frequenze di quello.
- **Temperatura e frequenza** della CPU visibili in cima al menu e sulla seconda riga della barra di stato.
- **Data e ora** in cima al menu principale, sotto la temperatura, per esempio *Sat 03 Oct 2026  11:45:20*: arrivano dalla rete (NTP), col fuso e l'ora legale del menu **Network**; finché non arrivano c'è scritto *Clock: waiting for network time*. In Italia: **Timezone (reboot)** su UTC+01:00 e **Daylight Saving (reboot)** su **Auto (EU)**. Con lo stesso fuso si vedono le ore dei file da **SD Card Sharing**.
- **La versione** è nella prima riga di **About**, per esempio *BMC64-NG Ver. 1.1*: dice quale versione c'è sulla scheda.
- **Quale C64 sta girando** lo dice la riga **Engine** di **About**: *VICE 3.10 x64sc* è il C64 cycle exact, *VICE 3.10 x64* il C64 Std, *VICE 3.10 xscpu64* la SuperCPU. Non la scrive il menu: la dà il motore del VICE dentro il kernel che è partito, quindi è sicura anche quando la voce scelta nello **Switch** non lo dice.
- **Barra di stato** con le unità attive (8 e9 ), con LED verde di accensione, rosso di utilizzo del drive, la traccia corrente comprese le mezze tracce, il contanastro.
- **Le spie di ogni modello di drive**: nella barra di stato ogni unità ha le spie del suo drive vero. Il 1541, il 1541-II e il 1570 hanno due spie tonde, la verde dell'accensione a sinistra e la rossa del lavoro a destra; il 1571 due barrette larghe, con la rossa dell'accensione a sinistra; il 1581 due barrette, con la rossa a sinistra, che la accende e la spegne il DOS del drive, come sul 1581 vero. Accanto c'è la traccia, con la mezza traccia (18.0, 18.5).
- **Il tasto RESTORE**, quello che sul C64 vero faceva l'interruzione non mascherabile.
- **Il guardiano**: se l'emulatore resta fermo dieci secondi lo schermo diventa nero, con un riquadro grande in inglese, **BMC64-NG HAS STOPPED**, che dice dove si è fermato e come aiutarci a correggere il difetto: fotografare lo schermo e aprire una segnalazione (issue) su github.com/lroby74/BMC64-NG/issues, allegando i file BLOCCO.TXT, BMC64-DIAG.TXT e PASSI.TXT della scheda (dopo un riavvio BLOCCO e PASSI si chiamano BLOCCO-PRIMA.TXT e PASSI-PRIMA.TXT). Sulla scheda si scrive BLOCCO.TXT con quello che serve a capire dove, e la riga dell'arresto va anche in fondo a BMC64-DIAG.TXT; BLOCCO-PRIMA.TXT resta finché non arriva un blocco nuovo, anche dopo più riaccensioni. Col menu aperto il tempo non conta: il menu può restare aperto per ore. Un errore vero (un assert o un panico) lo dice subito, col suo testo, anche col menu aperto.
- **USB-LOG.TXT**: a ogni apertura del menu, se c'è qualcosa di nuovo, ci si scrive cosa la USB ha trovato (dispositivi, interfacce, il driver scelto) e i rapporti del mouse coi loro byte: se un controller USB non va, quel file dice perché.
- **Le correzioni del VICE dopo la 3.10**: ci sono le correzioni di fedeltà del VICE di sviluppo (le versioni uscite dopo la 3.10): il registro seriale delle CIA (C64, C128, SuperCPU e i drive 1570, 1571 e 1581), la lettura in più dell'istruzione RTS come sul 6502 vero, l'editor 80 colonne originale del PET (e l'avvio automatico coi suoi editor a 50 e 60 Hz), il registro $FFF0 dei PET 8096 e 8296, sul C128 niente più blocco del VDC con R0=0, il kernal del modo C64 scelto da te (per esempio il JiffyDOS) che all'avvio non torna più quello di serie e i bit di $01 coi resistori di pull-up, due registri del TED del Plus/4, il joystick a destra e la colonna 7 della tastiera del VIC-20 come sulla macchina vera, il disturbo dei POT (mouse, paddle) solo quando cambia un POT collegato, i comandi T-W del drive virtuale, la SFX Sound Expander, il DigiMAX, la tapecart e la scheda HRE del PET.

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
| lettere, INVIO | in una lista di file: la ricerca per nome (DEL cancella) |
| una lettera | dentro il menu: porta alla voce successiva che comincia con quella lettera (S per Sound) |

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

---

## Crediti

**BMC64** è di Randy Rossi. **VICE** è del VICE Team. **Plus4Emu** è di
Istvan Varga. **Circle**, lo strato che permette di girare senza sistema
operativo, è di Rene Stange.

**BMC64-NG** è di Cla-Bob Systems.

Software libero, licenza GNU GPL. Il testo completo è nel menu, sotto
**License**.
