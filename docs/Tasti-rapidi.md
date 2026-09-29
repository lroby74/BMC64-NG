# BMC64-NG — Tasti rapidi

Tutte le scorciatoie di BMC64-NG, una per riga. Sono il modo più veloce di usarlo: quasi tutto quello che si fa dal menu ha il suo tasto.

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

Sul Plus/4 del VICE **ALT + 4** mette il 1551, il drive della macchina, al posto del 1570. In Plus4Emu i modelli sono tre: **ALT + 2** il 1541, **ALT + 3** il 1551 e **ALT + 4** il 1581, che apre subito l'elenco dei `.D81`.

Nel lettore SID **ALT + 1**, **ALT + 2**, **ALT + 3** e **ALT + 4** non cambiano il drive: spengono e riaccendono i SID, dal primo al quarto.

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
| **ALT + S** | apre il lettore di file .SID |
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
| **V, T, N** senza ALT | lettore SID: VU meter, tempo, righe col nome del brano |
| **, . P M** senza ALT | lettore SID: brani, pausa e silenziatore, come con ALT (il volume con ↑ ↓, o con ALT + + e ALT + −) |
| **"\"** senza ALT | lettore SID: passa fra 4:3 e 16:9, come ALT + "\" |

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
