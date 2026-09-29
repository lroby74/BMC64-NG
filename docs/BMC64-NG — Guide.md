# BMC64-NG

### A real Commodore inside a Raspberry Pi

---

## What it is

BMC64-NG turns a Raspberry Pi of the most recent generations (from the 4 onwards) into a Commodore computer.
It is not a program running inside Linux: **it is the only thing running on the Raspberry**.
There is no operating system underneath, there is no desktop, there is nothing to
launch. Switch it on, and after a few seconds **the blue C64 screen is in front of you**.

When you are done, switch off by pulling the power. Just like in 1982.
But if you want to do things in a more realistic and faithful way, press the right key or keys (it depends on the Pi model)
and the Raspberry Pi will shut down comfortably without pulling the plug.
---

## Why this specific version was born

The original project, **BMC64**, is by Randy Rossi, who had the idea and
made it work, but unfortunately it has some limits:
1) It was not designed to go beyond the Raspberry Pi 3 and its variants, which will be harder and harder to find on the market and in any case
offer limited performance compared to the models released later.
2) Because of what was said in point 1, it cannot be updated to the most recent version of Vice, 3.10, and it is still tied to 3.3 from 2018.
3) And for the same reason, it cannot run the C64 in Cycle Exact mode, which gives the emulation greater fidelity but requires hardware
power that the Pi 3 (let alone the models with even more limited hardware) cannot offer.
4) There are lots of features and conveniences missing from the original BMC64 that it was possible to add here, making
the user experience much more pleasant and immersive.

BMC64-NG is all of this: it is updated emulation, support for modern Raspberry Pis, and quite a few new things.

---

## What sets it apart from the previous version, BMC64

**The emulation is the real one.** Underneath there is **VICE 3.10**, the most
accurate Commodore emulator in existence, the one demo developers use
as their reference. Not a simplified rewrite: VICE, whole.

**Fidelity comes before convenience.** If the real Commodore did something
strange, BMC64-NG does it too. A flaw that the original machine also had
is not a flaw to be fixed: it is how it should be.

**The exact cycle.** On the C64 the emulation is *cycle exact*, the kind
that reproduces the behavior of the VIC-II cycle by cycle — it is needed for demos
and for games that push the graphics chip beyond its specifications.
Even on a Pi 4 at stock frequency it keeps up without any problem.

**The real drive too.** With an XUM1541 adapter you can connect a
**physical Commodore 1541, a 1571 or a 1581 (even more than one daisy-chained, if set to different device numbers)** and read your original floppy disks.
It is also possible to use a PI1541, which perfectly emulates a 1541 and a 1581
---

## The machines

| machine | notes |
|---|---|
| **Commodore 64** | in the *cycle exact* version (maximum precision) |
| **Commodore 64 Std** | the normal VICE version, not cycle exact: lighter. In the **Switch** menu the C64 entries are called **X64SC** (cycle exact) and **Std** |
| **Commodore 64 with SuperCPU** | the CMD accelerator at 20 MHz, with the free VICE ROM |
| **Commodore 64 GS** | the cartridge-only console, from the C64 **Model...** menu. As on the real console, only the "switch" keys work: F12, ALT + R, ALT + SHIFT + R, ALT + ESC, ALT + SHIFT + ESC, plus ALT + C and ALT + SHIFT + C to change cartridge and ALT + O and ALT + SHIFT + O for the status bar. The others say NOT ON C64 GS |
| **Commodore 128** | with both screens: the 40-column VIC-II and the 80-column VDC |
| **Commodore VIC 20** |
| **Commodore Plus/4** | with two different engines: the VICE one and **Plus4Emu**, considered more faithful |
| **Commodore PET** |

From the **Model...** menu the Commodore 64 can also be a **C64C**, a **C64 old** or an **SX-64**, besides the C64 GS. The SuperCPU runs at 100% on the Pi 4 and the Pi 400 too.

---

## The models it has been tested and certified working on

- Raspberry Pi 4 Model B
- Raspberry Pi 400
- Raspberry Pi 5
- Raspberry Pi 500
- Raspberry Pi 500+

You need a microSD card and a USB keyboard (only for the 4 and the 5). Everything else is optional.

---

## The features

### Disks, tapes, cartridges

- **Floppy disks** on up to **four drives** at the same time (8, 9, 10, 11).
  Fifteen formats: `.d64` `.d67` `.d71` `.d80` `.d81` `.d82` `.d1m` `.d2m`
  `.d4m` `.g64` `.g71` `.g41` `.p64` `.x64` `.dhd`.
- **Drive models** chosen one by one: 1541, 1541-II, 1570, 1571, 1581,
  or none. Each of the four drives can be a different model.
  Plus the 1551 on the C16 / Plus4
- **Plus4Emu, the drive without a disk**: a 1541 or a 1551 without a disk finds nothing, like the real one. ALT + 2 (1541) and ALT + 3 (1551) work like ALT + 4 with the 1581: they set the drive and open the list of `.D64` files to attach right away. The files of the folder on the card are read with **IEC FileSystem** on (in the menu, Drive 8 or Drive 9), and the choice is saved. Plus4Emu has no drive and tape sounds: its engine does not have them. The VICE Plus/4 has them.
- **True 1541 emulation**: the drive has its own processor and its own
  program, exactly like the original. It is needed for fast loaders and for
  the copy protections of vintage games.
- **Physical drive** via an XUM1541 adapter: connect a real 1541 or another Commodore drive and read
  your floppy disks.
- **Creation of blank floppy disks** from the menu.
- **Two USB floppy drives**: they appear in the file list as **FLP1** and **FLP2**. You put files and disk images on the floppy with your PC (FAT disks), and BMC64-NG uses them like the ones on the card.
- **Tapes** `.tap` and `.t64`, with the tape counter, the recorder controls and
  fast forward.
- **Cartridges** `.crt` and `.bin`.
- **The cartridge at every power-on**: on the C64 and the C128, to find a cartridge again at every power-on you must make it the default from the **Cartridge** menu with the **Set current cart default** entry, which saves by itself. It also works the other way round: if you remove it and no longer want it at power-on, after removing it choose **Set current cart default** again.
- **EasyFlash cartridges save**: what the game writes into the flash goes back into its .crt file with the **Save EasyFlash Now** entry of the **Cartridge** menu, by itself when the cartridge is removed or another one is inserted, and also when you power off, restart or switch machine, if the game wrote to it.
- **Programs** `.prg` loaded directly.
- **Autostart**: choose the file and it starts by itself, without typing anything.
- **The autostart disk**: with true drive emulation, VICE puts a `.prg` started with autostart on a disk it writes in the root of the card, `autostart-` plus the machine name, for example `autostart-PLUS4.d64`. You can delete it: it is made again next time.
- **Long file names**: disks, tapes, cartridges and programs with names up to 255 characters, the most the card allows, attach and start from the menu like the others. Before, a very long name froze the machine.
- **File search**, like on the Kung Fu Flash 2: inside a folder type the first letters of the name (for example `dra`) and press ENTER: only the files and folders that start that way remain, upper or lower case. You can use the KFF2 wildcards: the question mark stands for any single character, and `*lair` finds the names that contain "lair". BACKSPACE deletes; changing folder makes the search go away.
- **If no name starts that way**, the same search is done again inside the names: typing `12` among the Dragon's Lair files brings up `DrL-12-850.reu`, and typing `II` brings up `Turrican II.tap`.
- **.REU images**: the games and demos that use the REU load it from a file. The files go in the `/REU` folder: **ALT + 0** loads one, **ALT + SHIFT + 0** detaches it.
- **The REU contents in its file**: in the **Ram Expansion Unit (REU)** menu, **Ram Image (optional)** folder, the **Auto-save image** entry, off by default. When on, the .REU file is rewritten with the contents of the REU when you power off or restart, from the menu or with the keys, and when you switch machine. Pulling the plug does not save. For game images, like Dragon's Lair, better leave it off.
- **If the file cannot be written** (for example because it is read-only), at power-off the Raspberry stays on and says so (NOT SAVED); asking again to power off, it turns off anyway.

### Video

- **Scanlines** adjustable from 0 to 100, for the look of a tube monitor.
- **Aspect ratio** 4:3 or 16:9, switchable on the fly.
- **4:3 and 16:9 on the custom modes too**: on the modes with the machine's exact frequency (768x545 and 768x525) the TV stretches the picture over the whole screen, and the pixels are not square. The TV's real shape is read from the TV itself at power-on (if it does not say, 16:9 is assumed): 16:9 fills the screen and 4:3 comes out really 4:3, as at 720p.
- **A moved picture does not slow down**: with **H Center** and **V Center**, or by widening it, the picture can go a little past the edges: the part outside is simply cut. Before, as soon as it went past by even one line, the emulation could drop to 50%.
- **The NTSC VIC-20 with its own KERNAL**: in the VIC-20 NTSC modes the American KERNAL (901486-06) starts by itself, and it puts the screen in the middle as on a real NTSC VIC-20; in the PAL modes the European one (901486-07). A different ROM chosen from the menu stays yours.
- **Picture adjustment**: brightness, contrast, color, gamma, and the
  geometry (width, height, centering).
- **The C128 dual screen**: the 40-column VIC-II and the 80-column VDC are
  two separate outputs, each with its own adjustments. You switch with one key.
  And if you connect two monitors, you get the dual 40 and 80 column output just like back then.
- **The dual screen in 16:9 too**: with two monitors each of the two C128 screens fills the whole TV, in 4:3 as well as in 16:9.
- **The two monitors from the menu**: in the C128 **Video** menu, **Active Display** set to **Two HDMI** sends the VIC-II to one HDMI and the VDC to the other (the first time the menu asks to restart). First set **Second Display Present** to **On**: while it is **Off**, turning **Active Display** with the arrows, with ENTER or with the **Cycle Display** key skips **Two HDMI** and asks nothing. Core 3 draws the two screens, so the emulation core is left entirely to the machine.
- **The main HDMI output**: in the **Video** menu the **Main HDMI Output** entry chooses which of the two HDMI ports carries the main screen, and the HDMI audio with it. With ENTER the menu asks to restart, and the choice holds from then on. On the C128 with two monitors it swaps the two screens. If at power-on the chosen port has no monitor or does not respond, the screen goes to the other one.
- **VGA monitors**: with an HDMI-to-VGA adapter a PC VGA monitor works too. In the **Switch** menu every machine has **VGA 1024x768** modes at 50 and 60 Hz (about 40 and 48 kHz: a monitor that only reaches 31 kHz cannot lock onto them).
- **Screenshot** saved on the card.

### Sound

- **The real SID**, emulated with reSID: both the 6581 of the first C64s and the 8580 of the
  later ones.
- **The SID Card on the Plus/4 and the PET**: these machines do not have a SID, and just like back then you add one with a cartridge. In the **Sound** menu there is the **SID Card** entry, off by default, with the model (6581 or 8580), the address (`$FD40` or `$FE80` on the Plus/4, `$8F00` or `$E900` on the PET), the filter and the engine (**SID Card Engine**: ReSid by default, or ReSIDfp with its **reSIDfp Settings** folder).
- **Output of your choice**: HDMI, headphone jack, or an external **USB DAC** for
  those who want the best quality.
- **.SID file player**: a `/sids` folder on the card, and the menu lets you
  browse and play the tunes of the SID collection, including those made for two and three SIDs.
- **In the SID player** ALT + , and ALT + . go to the previous and the next tune; ALT + P pauses without stopping the tune, and pressing it again resumes.
- **In the SID player without ALT too**: , and . go to the previous and the next tune, P pauses, M mutes. The volume goes up and down with the up and down arrows, or with ALT + + and ALT + −: without ALT, 0, − and + are the voices of the fourth SID.
- **Forward and back in the tune**: in the SID player the right and left arrows, without ALT, jump 10 seconds forward or back, and +10 SEC or -10 SEC appears at the bottom; pressing them several times adds the jumps up (three times right is 30 seconds). The player races there silently, and from there the tune plays on; going back, the tune starts again from the beginning and races to the chosen point, and within the first 10 seconds it simply starts again. While paused the arrows do nothing.
- **The voices one by one**: in the SID player the keys 1 to 9 turn one voice at a time off and on, 1 to 3 those of the first SID, 4 to 6 of the second, 7 to 9 of the third; ALT + 1, ALT + 2 and ALT + 3 turn a whole SID off and on. Leaving the player turns all the voices back on.
- **V, T and N** in the SID player show and hide the VU meters, the tune time and the lines at the top with the tune name.
- **The other ALT keys** do nothing in the SID player, and NOT IN SID PLAYER appears at the bottom: the player keys, the volume, ALT + S, ALT + SHIFT + S, ALT + ESC and ALT + SHIFT + ESC still work.
- **4:3 and 16:9 in the SID player**: the "\" key switches between 4:3 and 16:9 without ALT too, and ALT + "\" works as it does outside the player. With an Italian keyboard, without ALT, both the key with \ at the top left, next to 1, and the < > key next to the left SHIFT work. Outside the player, "\" without ALT stays a C64 key.
- **Stereo with more than one SID**: with two SIDs the first plays on the left and the second on the right; with three, the first on the left, the third on the right and the second in the center.
- **Tunes with 4 SIDs**: the player also plays 4-SID files (the SID-Wizard ones, PSID v4E format): at the start the first and second SID on the left, the third and fourth on the right, and the VU meters become 12 bars, at least as wide as with three SIDs and a little wider when there is room, running from the first 0 of the time, on the left, to the end of the text on the right (PAL 50Hz); under each group the SID number, the model and the side (for example 1 8580 L). Keys 1 to 9, then 0, − and + turn the 12 voices off and on (10, 11 and 12 are the fourth SID's); ALT + 1, 2, 3 and 4 the whole SID.
- **The side of each SID, with F1-F4**: in the SID player F1, F2, F3 and F4 move the first, the second, the third and the fourth SID to the left (L), the centre (C) or the right (R): each press gives the next side, L, C, R and back to L. It works in all four players, from 1 to 4 SIDs. The start is: one SID in the centre; two SIDs L and R; three L, C and R; four L, L, R and R. The side shows under the columns, next to the model, and a notice tells it when it changes. The chosen sides stay for all the tunes with the same number of SIDs while you are in the player; leaving it, the starting sides come back. With **Channels** set to **Mono** (**Sound** menu) there are no sides, and F1-F4 say CHANNELS: MONO.
- **One SID on a separate core**: with two or three SIDs the second one is computed by another of the four cores of the Raspberry, together with the others; in the SID player, in stereo, also with four-SID tunes. The sound is the same, and the emulation core has more headroom.
- **SID sampling**: in the **Sound** menu the **SID Resampling** entry is set to **Resampling** by default, the most faithful mode: the SID sound reaches your ears without the noise the other modes add to the high notes. **Fast Resampling** sounds the same, but uses more memory and takes longer to get ready; **Interpolation** and **Fast** cost the Raspberry less and are less faithful.
- **The reSID filters**: in the **Sound** menu the **reSID Filter Settings** folder has the settings of the 6581 and of the 8580: **Passband %** (90 by default), **Gain %** (97) and **Filter Bias (mV)**. They apply to all the SIDs of that model. Passband and Gain only count with **SID Resampling** set to **Resampling** or **Fast Resampling**; the sound changes when the menu is closed, and **Save settings** keeps them.
- **reSIDfp, the third SID engine**: in the **Sound** menu the **SID Engine** entry also has **ReSIDfp**, next to **ReSid**, which stays the default. It is VICE's newest SID engine, and the most faithful to the real chip: the combined waveforms (what the SID produces when a voice uses two waveforms at once) come from measurements of real chips, and the 6581 filter has its distortion. It is aligned with its authors' latest version (libresidfp, 22 September 2026), which reworked the 6581 combined waveforms closer to the real chips. The **reSIDfp Settings** folder has its settings: **Combined Waveforms** (Weak, Average, Strong), **Background Noise** (below), and for the **6581** **Filter Curve**, **Filter Range** and **Old Caps (2200pF)**, for the **8580** **Filter Curve**. The sound changes when you close the menu; **Save settings** keeps everything. It needs a little more computing than reSID, and the second SID stays on its own core. It is also there for the SID Card of the Plus/4 and the PET. It is not there on the C64 DTV, which has a SID of its own.
- **reSIDfp's background hiss**: a real C64 is never completely silent, and its recordings always have a slight background hiss. reSIDfp adds it to the sound to resemble the real chip: the **Background Noise** entry in **reSIDfp Settings** sets it from **OFF** (the default: silence when the machine is idle, as with reSID) to **100%** (the level chosen by the reSIDfp authors to resemble recordings of real C64s), in steps of 10%. For a more realistic sound turn it up; if it bothers you, leave it OFF. You hear it mostly when nothing is playing, for example at the BASIC prompt. Each SID has its own hiss, independent of the others: with two, three or four SIDs they add up, as those of as many real chips would.
- **Stereo or mono**: in the **Sound** menu, just below **Audio out**, the **Channels** entry chooses how the SIDs come out when there is more than one (**Dual SID**, or a 2- or 3-SID tune in the player). **Stereo**, the default: each SID on its own side (with 3 SIDs the first on the left, the third on the right and the second on both). **Mono**: all the SIDs together on both sides. **Save settings** keeps it. It is there on the C64, C128 and SuperCPU; the other machines have a single SID, or none, and the sound is already mono.
- **The tune time**: in the SID player, on the left of the third line, the minutes and seconds of the tune. It is the time of the music: it stops when paused, and starts again from zero when you change tune. On the right is how often the tune plays: PAL 50Hz, NTSC 60Hz, or CIA with the number the tune has put in the timer.
- **The volume in the SID player**: for one second after each volume key a horizontal segmented bar appears, between the labels under the columns (for example 1 8580 R) and the tune line (DEFAULT TUNE), with the percentage on its right, and MUTE in red when the volume is at zero.
- **Inside the SID player** the status bar does not appear, so as not to cover the columns, and ALT + W does nothing: a tune cannot be listened to at double speed.
- **The USB DAC even with the computer on**: with **Audio out** set to **USB DAC** the sound moves to the DAC as soon as you plug it in and goes back to HDMI when you unplug it, and DACs that only work at 48 kHz work too, with every machine, Plus4Emu included. There is no longer any need to plug it in before switching on.
- **If the USB DAC disappears for a moment** (some cheap DACs disconnect and reconnect by themselves), the sound waits three seconds before going back to HDMI: if the DAC comes back, it carries on from there.
- **Volume** adjustable from the keyboard, and mute.
- **100% is the sound as it is**, with no amplification to distort it. Once at 100% ALT + + does nothing more, and at zero neither does ALT + −: the status bar says VOLUME 100% MAX, or MUTE.

### Controls

- **USB joysticks** and gamepads, up to four ports.
- **Joysticks on the GPIO**: if you have an adapter for vintage joysticks connected to the
  pins of the Raspberry, it works.
- **Keyset**: using the keyboard as a joystick, with the keys you choose.
- **Mouse** (the Commodore 1351).
- **Keyboard maps**, positional and symbolic, with US, IT, UK and DE layouts.
- **The keyboard from the menu**: in the **Keyboard** menu the **Mapping** entry chooses between **Positional** and **Symbolic**, and **Layout** between US, Italiana, British and Deutsch (the last three with **Mapping** set to **Symbolic**). The Pi 400 and Pi 500 keyboards work like a USB keyboard.
- **The eight extra keys of the C128** (ESC, TAB, ALT, CAPS LOCK, HELP,
  LINE FEED, 40/80, NO SCROLL), which do not exist on a PC keyboard.
- **The C128 keypad**: on the C128 the numeric keypad of the PC keyboard works as the C128 numeric keypad.

### Everything else

- **Snapshots**: you save the exact state of the machine and pick it up later.
- **REU**: the Commodore memory expansion, from 512 KB to 16 MB.
- **Warp**: the machine runs as fast as the Raspberry allows, to skip
  long loading times.
- **Network and WiFi**: configurable from the menu.
- **Overclock and Underclock** guided from the menu, with frequencies already tested.
- **Temperature and frequency** of the CPU visible at the top of the menu and on the second line of the status bar.
- **Status bar** with the active drives (8 and 9), with the green power LED, the red drive activity LED, the current track including half tracks, the tape counter.
- **The RESTORE key**, the one that on the real C64 triggered the non-maskable interrupt.

---

## Getting started

1. Copy your files (games, programs, SID tunes but above all the system ROMs of the Commodore computers and of the drives) to the microSD:
the folders are already there, and each of them contains the subfolders for the various computers. The `.SID` files go in a folder called `sids`.
2. Switch on.
3. **F12** opens the menu. The arrow keys to move, ENTER to choose, F12
   again to exit.
4. When you have set things up the way you like them, choose
   **Save settings** from the menu. You will find them again at the next start.

From inside the menu, the **Shortcut Keys** entry shows the list of shortcuts,
always up to date.

**If you see black**: only one of the two HDMI outputs of the Raspberry carries the main signal. If the screen stays black, move the cable to the other HDMI port.

**If you change system** from the **Switch** menu and do not touch a key within 15 seconds, for safety it goes back by itself to the previous one. If the new system displays well, just press any key to keep it.

**The same goes for the video mode**: even if in the **Switch** menu you only choose another resolution or another refresh rate, to keep it you must press a key within 15 seconds, otherwise the previous one comes back by itself. The **Safety timer (15s)** entry, at the bottom of the **Switch** menu, removes these 15 seconds for all machines, but it asks for confirmation first: without the timer, if the monitor cannot show the chosen mode, the screen stays black and `config.txt` has to be fixed on a PC.

---

## What is NOT inside, and why

On the card you will not find the **copyright-protected ROMs**: JiffyDOS, the
C64 DTV ROMs, the `.SID` files of the collection. It is not an oversight — they are
other people's material, and it is not distributed. If you have it, you put it on your
card yourself and it works.

The original Commodore ROMs (KERNAL, BASIC, character generator), on the other hand, are
there: VICE distributes those and they are free.

---

# ALL THE SHORTCUTS

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

On the VICE Plus/4 **ALT + 4** sets the 1551, the machine's own drive, in place of the 1570. In Plus4Emu there are three models: **ALT + 2** the 1541, **ALT + 3** the 1551 and **ALT + 4** the 1581, which opens the list of `.D81` files right away.

In the SID player **ALT + 1**, **ALT + 2**, **ALT + 3** and **ALT + 4** do not change the drive: they turn the SIDs off and on, from the first to the fourth.

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
| **ALT + S** | opens the .SID file player |
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
| **V, T, N** without ALT | SID player: VU meters, time, lines with the tune name |
| **, . P M** without ALT | SID player: tunes, pause and mute, as with ALT (the volume with ↑ ↓, or with ALT + + and ALT + −) |
| **"\"** without ALT | SID player: switches between 4:3 and 16:9, as ALT + "\" |

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

---

## Credits

**BMC64** is by Randy Rossi. **VICE** is by the VICE Team. **Plus4Emu** is by
Istvan Varga. **Circle**, the layer that makes it possible to run without an operating
system, is by Rene Stange.

**BMC64-NG** is by Cla-Bob Systems.

Free software, GNU GPL license. The full text is in the menu, under
**License**.
