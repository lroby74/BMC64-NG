# BMC64-NG — Shortcut Keys

Every shortcut of BMC64-NG, one per line. They are the quickest way to use it: most of what you would do from the menu has its own key.

All the combinations use the **ALT** key. SHIFT, where present, activates the second special function

## The menu

| keys | what it does |
|---|---|
| **F12** | opens and closes the menu |
| arrow keys, ENTER | inside the menu: move and choose |
| letters, ENTER | in a file list: search by name (BACKSPACE deletes) |
| a letter | inside the menu: jumps to the next entry that starts with that letter (S for Sound) |

## The tape (recorder)

| keys | what it does |
|---|---|
| **ALT + ↑** | PLAY |
| **ALT + ↓** | STOP |
| **ALT + →** | fast forward |
| **ALT + ←** | rewind |
| **ALT + SHIFT + →** | fast forward **at double speed** |
| **ALT + SHIFT + ←** | rewind **at double speed** |
| **ALT + DEL** | resets the tape counter |
| **ALT + T** | attaches a tape |
| **ALT + SHIFT + T** | detaches the tape |

Without a tape attached the recorder controls do nothing, neither with the keys above nor from the menu or the joystick buttons: **NO TAPE** appears at the bottom. On the SuperCPU, which has no recorder in VICE, **NO TAPE ON THIS MACHINE** appears.

## Disks and cartridges

| keys | what it does |
|---|---|
| **ALT + 8** | attaches a floppy disk to drive 8 |
| **ALT + SHIFT + 8** | detaches drive 8 |
| **ALT + 9** | attaches a floppy disk to drive 9 |
| **ALT + SHIFT + 9** | detaches drive 9 |
| **ALT + C** | inserts a cartridge |
| **ALT + SHIFT + C** | removes the cartridge |
| **ALT + A** | autostart: choose a PRG or Disk file and it starts by itself |
| **ALT + 0** | loads a .REU file from the /REU folder |
| **ALT + SHIFT + 0** | detaches the .REU file (the REU stays on) |

## The drive model

The numbers from 1 to 7 choose the model. **Without SHIFT it applies to drive 8, with
SHIFT to drive 9.**

| keys | model |
|---|---|
| **ALT + 1** | no drive |
| **ALT + 2** | 1541 |
| **ALT + 3** | 1541-II |
| **ALT + 4** | 1570 |
| **ALT + 5** | 1571 |
| **ALT + 6** | 1581 |
| **ALT + 7** | physical drive (XUM1541 adapter) |
| **ALT + SHIFT + 1…7** | the same seven choices, but for **drive 9** |

If the XUM1541 adapter is not there, **ALT + 7** says so on the screen (NO XUM FOUND) and the drive stays the one it was.

**The drive ROM** chosen from the menu (**Drives**, **Change ROM...**) reaches the running drive of that model at once, which restarts with the new one; before, a restart was needed. If the file is not good, the previous one stays.

On the VICE Plus/4 **ALT + 4** sets the 1551, the machine's own drive, in place of the 1570. In Plus4Emu there are three models: **ALT + 2** the 1541, **ALT + 3** the 1551 and **ALT + 4** the 1581, which opens the list of `.D81` files right away.

In the SID player **ALT + 1**, **ALT + 2**, **ALT + 3** and **ALT + 4** do not change the drive: they turn the SIDs off and on, from the first to the fourth.

With 8-SID tunes also **ALT + 5**, **ALT + 6**, **ALT + 7** and **ALT + 8**: from the fifth to the eighth SID.

## Reset and shutdown

| keys | what it does |
|---|---|
| **ALT + R** | **hard** reset (clears the memory: it is the one that restarts a game) |
| **ALT + SHIFT + R** | **soft** reset (does not clear the memory, ideal for changing or analyzing code in memory) |
| **ALT + ESC** | restarts the Raspberry — asks for confirmation, press it a second time |
| **ALT + SHIFT + ESC** | shuts down the Raspberry — same confirmation, on the Raspberry Pi 500 and 500+ you can use the dedicated button) |
| **ESC** alone | cancels the confirmation |

## Video

| keys | what it does |
|---|---|
| **ALT + "\"** | switches between 4:3 and 16:9 |
| **ALT + SHIFT + "\"** | the same thing on the **other** screen (C128 only, in dual DISPLAY) |
| **ALT + SHIFT + "+"** | more scanlines |
| **ALT + SHIFT + "−"** | fewer scanlines |
| **ALT + PRTSCN** | C128: switches between the 40-column screen and the 80-column one (simulates the little switch on the monitors)|
| **ALT + SHIFT + PRTSCN** | C128: the **40/80 DISPLAY** key of the real keyboard |
| **ALT + F7** | C128: as above, the 40/80 key |
| **ALT + O** | removes the status bar, and pressing it again puts it back as it was |
| **ALT + SHIFT + O** | turns the second line of the status bar on and off (temperature, frequency, speed) |

## Sound

| keys | what it does |
|---|---|
| **ALT + +** | volume up |
| **ALT + −** | volume down |
| **ALT + M** | mute |
| **ALT + S** | opens the .SID file player; if you change your mind, ESC goes straight back to where you were, also to the tune that was playing |
| **ALT + SHIFT + S** | stops the tune and resets |
| **ALT + ,** | SID player: previous tune |
| **ALT + .** | SID player: next tune |
| **ALT + P** | SID player: pause, and pressing it again resumes |
| **← →** without ALT | SID player: 10 seconds back or forward in the tune |
| **↑ ↓** without ALT | SID player: volume up and down |
| **1 … 9** without ALT | SID player: turns one voice off and on (1-3 first SID, 4-6 second, 7-9 third) |
| **0, −, +** without ALT | SID player: turns one voice of the fourth SID off and on (10, 11, 12) |
| **ALT + 1, 2, 3, 4** | SID player: turns the first, the second, the third or the fourth SID off and on |
| **F1, F2, F3, F4** | SID player: the side of the first, the second, the third or the fourth SID: L, C, R and back to L |
| **ALT + 5, 6, 7, 8** | SID player, with 8-SID tunes: turns the fifth, the sixth, the seventh or the eighth SID off and on |
| **F5, F6, F7, F8** | SID player, with 8-SID tunes: the side of the fifth, the sixth, the seventh or the eighth SID: L, C, R and back to L |
| **V, T, N** without ALT | SID player: VU meters, time, lines with the tune name |
| **, . P M** without ALT | SID player: tunes, pause and mute, as with ALT (the volume with ↑ ↓, or with ALT + + and ALT + −) |
| **"\"** without ALT | SID player: switches between 4:3 and 16:9, as ALT + "\" |
| **S** without ALT | SID player: switches between reSID and reSIDfp (the name for a second in the volume line) |

The `+` and the `−` work both on the number row and on the numeric
keypad, on the Italian keyboard as well as on the American one.

## Other

| keys | what it does |
|---|---|
| **ALT + W** | warp: the machine is sped up (useful to skip boring parts) |
| **ALT + J** | swaps the two joystick ports |
| **ALT + E** | turns the REU on and off |
| **ALT + SHIFT + E** | changes the size of the REU (512 KB ↔ 16 MB) |

## The extra keys of the Commodore 128

The C128 has eight keys that the C64 does not have and that a PC keyboard does not provide.
They are on **ALT + F1…F8**, in the order in which they appear on the real machine.

| keys | C128 key |
|---|---|
| **ALT + F1** | ESC |
| **ALT + F2** | TAB |
| **ALT + F3** | ALT |
| **ALT + F4** | CAPS LOCK |
| **ALT + F5** | HELP |
| **ALT + F6** | LINE FEED |
| **ALT + F7** | 40/80 DISPLAY |
| **ALT + F8** | NO SCROLL |
