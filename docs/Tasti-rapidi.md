# BMC64-NG — Tasti rapidi

Tutte le scorciatoie di BMC64-NG, una per riga. Sono il modo più veloce di usarlo: quasi tutto quello che si fa dal menu ha il suo tasto.

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
