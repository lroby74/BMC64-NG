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
1) It was born for the Raspberry Pi up to the Pi 3 and its variants, which will be harder and harder to find on the market and in any case
offer limited performance compared to the models released later. Only since version 5.4 (October 2026) does it also run on the Pi 4 and Pi 5, still in testing (beta).
2) Because of what was said in point 1, it cannot be updated to the most recent version of Vice, 3.10, and it is still tied to 3.3 from 2018.
3) It runs the C64 in Cycle Exact mode, which gives the emulation greater fidelity, only on the Pi 4 and Pi 5, since 5.4: the Pi 3 (let alone the models with even
more limited hardware) does not have the power to run it.
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

**Note:** an XUM1541 built on an Arduino Pro Micro may not be compatible: with it the real Commodore drive works only after the first power-on from cold, and after a machine change or a restart of the Raspberry it stops answering. BMC64-NG is tested with a ZoomFloppy.

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

On the **PET** and the **Plus/4** the model entry tells the model that is running, for example **Model: 8032** or **Model: Plus/4 (PAL)**, and it changes as soon as you choose another one (**Model: Custom** if the settings match no model).

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
- **`.nib` images**, the low-level copies made with nibtools, protected disks included: you choose them like the other disks, from **Attach Disk...** or **Autostart Prg/Disk...**. BMC64-NG converts them to `.g64` with the nibconv code of nibtools, in the `/nib-g64` folder of the card, and attaches that; the `.nib` is never changed. The `.g64` is made again each time the `.nib` is attached: to keep what a game saves on the disk, attach the `.g64` from the `/nib-g64` folder. Plus4Emu does not read them: it does not read `.g64` files either.
- **Drive models** chosen one by one: 1541, 1541-II, 1570, 1571, 1581,
  or none. Each of the four drives can be a different model.
  Plus the 1551 on the C16 / Plus4
- **The sound of the disk going in and out**: with **Drive sound emulation** on (**Prefs** menu), attaching and detaching an image makes the sound of the disk going into and out of the real drive, together with the motor and head sounds. They come from the Ultimate-II+. The 1581 has its own (motor, head, disk in and out), also taken from the Ultimate-II+: it is a 3.5 inch drive and much quieter.
- **Plus4Emu, the drive without a disk**: a 1541 or a 1551 without a disk finds nothing, like the real one. ALT + 2 (1541) and ALT + 3 (1551) work like ALT + 4 with the 1581: they set the drive and open the list of `.D64` files to attach right away. The files of the folder on the card are read with **IEC FileSystem** on (in the menu, Drive 8 or Drive 9), and the choice is saved. Plus4Emu has no drive and tape sounds: its engine does not have them. The VICE Plus/4 has them.
- **True 1541 emulation**: the drive has its own processor and its own
  program, exactly like the original. It is needed for fast loaders and for
  the copy protections of vintage games.
- **Drives that talk to each other**: with two or more drives on, programs that make two drives talk on the serial cable without going through the computer now work: before touching the cable a drive brings the others up to its time, as the computer does with the drives. With one drive nothing changes; with two or more drives on, the Raspberry does a little more computing.
- **Physical drive** via an XUM1541 adapter: connect a real 1541 or another Commodore drive and read
  your floppy disks.
- **The XUM1541 built on an Arduino Pro Micro** may not be compatible: with it the real Commodore drive works only after the first power-on from cold, and after a machine change or a restart of the Raspberry it stops answering. BMC64-NG is tested with a ZoomFloppy.
- **The real drive can be chosen even without the drive ROMs**: in the **Drives** menu, a unit's **Change Model...** has the **Real drive** entry (grey if the XUM1541 adapter is not there), like **ALT + 7**. The emulated models (1541, 1571, 1581...) can be chosen only if their ROMs are in the card's `DRIVES` folder. Until 6 November 2026, without any drive ROM, the **Real drive** entry was missing.
- **Programs with their own fast loader, on the physical drive**: many games and copiers put a loader of their own into the drive, which talks to the computer on wires the XUM1541 adapter does not carry: through the adapter they cannot work, and it is the same with VICE on a PC. The machine does not stop, though: the drive finishes the file by itself (**FAST STOP** on the status bar), the program keeps waiting, and Reset puts everything back, with the Pi1541 disk still attached. What loads from BASIC without a loader of its own works, with the BMC64-NG fast loader.
- **Drive on XUM1541: Pi1541 or Commodore drive**: in the **Drives** menu choose what is on the XUM1541 adapter. With **Commodore**, for a real 1541 or another Commodore drive, the drive gets the line reset as on a real Commodore: at every power-on and machine switch and at every reset. So a drive left stuck by the previous machine starts again by itself, without switching it off. With **Pi1541 + Commodore** you keep a Commodore drive and a Pi1541 on the same cable, for example the 1541 on unit 8 and the Pi1541 on unit 9 (each with its own number, and both units set to physical drive): resets work as with the Pi1541, and at power-on the line is pulled only if a drive was left stuck. With **Pi1541**, the default, resets do not touch the line, because the Pi1541 would unmount the .D64. A Pi1541 that restarts like the real drive on reset, without unmounting the .D64, goes with **Commodore**, also together with a Commodore drive. The choice is one for all machines and is kept by itself, without **Save settings**.
- **Directory of Drive on XUM1541**: in the **Drives** menu it shows the directory of the disk in each drive on the XUM1541 adapter (a Pi1541 or a real Commodore drive; with more drives, one after the other), read straight from the disk sectors, as copy programs do. It shows every entry, also the ones the BASIC **LOAD"$"** does not show: some disks have characters in their names that stop **LIST** halfway (the Super Kit 1541, for example), and a real C64 does the same. Control characters in the names show in reverse, as on the C64. The lines do not start anything: **Left** or **ESC** go back. It reads 1541, 1570, 1571 and 1581 disks; if the drive is busy or does not answer, the page says so.
- **The fast loader with two drives on the cable**: the BMC64-NG fast loader uses a wire that every drive switched on hears, so it works only with one drive alone on the cable. With another drive switched on the file loads at normal speed, and the status bar says so: **FAST OFF: DRIVE 9 ON BUS TOO** (with the number of the other drive). Switch the other drive off and the speed comes back.
- **Reset Drive on XUM1541**: in the **Drives** menu it puts the adapter back on its feet and restarts every drive on it, and the page says how it went, drive by drive. A Pi1541 restarts with the UJ command and the image stays mounted. A Commodore drive gets the line reset, held until the drive takes it (up to 3 seconds); if it does not restart, UJ is sent. **restarted (bus reset)** means the line reset reached it, and the LED lit up for a moment; **restarted with UJ** that the line does not reach it (the cable, or the reset input of the drive) and it restarted with the command, without lighting the LED, as a real 1541 does with UJ; **does not answer** that it must be switched off and on; **not on the bus** that nobody is at that number.
- **Reset Drive on XUM1541, how long the drive takes**: with a Commodore drive the reset line stays held until the drive takes it (when it takes it, it holds the DATA and CLOCK wires), up to 3 seconds, and the page says after how long: **Bus reset taken, N ms**. **Bus reset not taken, 3 s** means that in 3 seconds no drive took it: the reset wire does not reach it (the cable, or the reset input of the drive) and the UJ command is tried; if the drive does not answer that either, it must be switched off and on. **Bus reset held N ms** means that the bus was already held by a stuck drive: the line is held for one second, because there is no way to see when it takes it. After a reset or a UJ command, BMC64-NG waits until the drive has finished restarting (it lets the DATA wire go) before calling it, because a real 1541 called while it restarts stops; and a stuck drive that holds the bus costs 2 seconds, not 15 at every command.
- **Real Drive: Try the fast loader**: in the **Drives** menu it loads the first file of the disk in the drive on the XUM1541 adapter twice, with the BMC64-NG fast loader and the usual way, and says how fast each one is and whether the bytes are the same. It works only with a 1541 alone on the cable: with another drive switched on, or with a 1570, a 1571 or a 1581, the page says so. If the drive does not answer, the page says **Drive 8 does not answer.** (with the unit number) and suggests **Reset Drive on XUM1541** or switching the drive off and on.
- **Creation of blank floppy disks** from the menu.
- **The disk at every power-on**: in the **Drives** menu, **Default disk**: **Disk image** chooses an image and **Drive** the unit (8, 9, 10 or 11; with **None** it is not used). At every power-on that image is attached by itself, if the unit is empty, and if the unit is off it is switched on by itself. By default there is no image and nothing happens. It is kept with **Save settings**. With **PRG loading** set to **Via disk image**, a `.prg` started with autostart puts its disk on unit 8 in place of this one; with **Direct to RAM** it does not.
- **Written disks reach the card by themselves**: in **Prefs**, **Flush disk writes** chooses when what is written to a disk ends up in the file on the card: **On write** (the default) shortly after the drive stops writing, **On detach** only when the disk is detached or the drive moves to another track, **On write (log)** like On write, and it writes it in the log. If a game stutters right after a save, try **On detach**. With Plus4Emu the entry is not there. Among the hotkey choices there is also **Flush Disks**, which does it at once.
- **Two USB floppy drives**: they appear in the file list as **FLP1** and **FLP2**. You put files and disk images on the floppy with your PC (FAT disks), and BMC64-NG uses them like the ones on the card.
- **Tapes** `.tap` and `.t64`, with the tape counter, the recorder controls and
  fast forward.
- **The tape sound**: in **Prefs**, **Tape sound emulation** (off by default) lets you hear the tape loading, as with the real recorder, and **Tape sound emulation volume** sets its volume. It plays together with the SID music too.
- **Cartridges** `.crt` and `.bin`.
- **The cartridge at every power-on**: on the C64 and the C128, to find a cartridge again at every power-on you must make it the default from the **Cartridge** menu with the **Set current cart default** entry, which saves by itself. It also works the other way round: if you remove it and no longer want it at power-on, after removing it choose **Set current cart default** again.
- **EasyFlash cartridges save**: what the game writes into the flash goes back into its .crt file with the **Save EasyFlash Now** entry of the **Cartridge** menu, by itself when the cartridge is removed or another one is inserted, and also when you power off, restart or switch machine, if the game wrote to it.
- **Magic Desk Plus cartridges** (C64 and C128): what the program writes to the cartridge's SRAM and EEPROM stays in two files next to the `.crt`, with the same name and the extensions `.srm` and `.eep`, and it is there again the next time. In the **Cartridge** menu, **Save Magic Desk Plus Now** writes them at once; **Auto-save Magic Desk Plus**, on by default because the real cartridge keeps its memory even when switched off, writes them by itself when the cartridge is detached or another one is attached, and also when you power off, restart or switch machine.
- **Programs** `.prg` loaded directly.
- **Autostart**: choose the file and it starts by itself, without typing anything.
- **Autostart on the PET**: the PET has **Autostart Prg...** too, with `.prg` files only: the list shows only those, and the chosen program loads and starts by itself.
- **Autostart on the PET like the real LOAD**: a `.prg` started from **Autostart Prg...** finds memory as after a real LOAD from the drive: the start no longer touches the two bytes `$C7`/`$C8`, which after power-on point to the editor's CRTC table and which the real LOAD leaves alone. So the *No Pets Allowed* demo on the 4032, which puts the normal screen back by reading that table, goes on. Until 9 November 2026 the start wrote the program's address there, and that demo, with A or with B, lost the screen after fifteen seconds.
- **The PetSpeed key of the PET**: in the PET **Machine** menu the **PetSpeed dongle** item puts on the tape port the key that the PetSpeed compiler wants in order to start. As on the real thing the key takes the port: while it is there, the tape recorder is not. It is kept with **Save settings**.
- **The PET with the CRTC out of sync**: if a program leaves the PET's CRTC without vertical sync, the machine does not stop: like the real monitor, which keeps running without sync, the screen keeps redrawing and the menu answers. Until 8 November 2026 the machine looked frozen in that case, with a continuous noise, and had to be switched off.
- **The PET at full speed when the frame gets shorter**: if a program reprograms the CRTC and the frame gets shorter (the *No Pets Allowed* demo on the 4032 with the SID, two minutes and twenty-three seconds after the key), the picture keeps the same size, the missing lines stay black and the emulation runs at full speed, also after a reset. Until 9 November 2026 in that case every frame was composed by the processor instead of the video chip, and the emulation dropped to 20-25% until the restart.
- **The PET picture always in its place**: if a program reprograms the CRTC (the *No Pets Allowed* demo on the 4032), the picture stays in the middle of the screen from start to end, and after a reset BASIC is back where it was at power-on. Until 10 November 2026, some thirty seconds after the key, the picture moved 56 lines down, cut at the bottom (at the end of the demo it moved some thirty lines up instead), and stayed shifted even after a reset, until a restart or another run of the demo.
- **The microSD reader that recovers**: if a transfer from the card stops half-way, BMC64-NG puts the card reader back in order, recovers the card and does the read or write again from the start, as Linux does. It applies to everything that uses the card, **SD Card Sharing** included. When it happens, `SMB-LOG.TXT` on the card says how many times the reader recovered by itself and where it had stopped.
- **The autostart disk**: with true drive emulation, VICE puts a `.prg` started with autostart on a disk it writes in the root of the card, `autostart-` plus the machine name, for example `autostart-PLUS4.d64`. You can delete it: it is made again next time.
- **PRG loading**: the **PRG loading** item of the main menu, under **Autostart Warp** (on the PET under **Autostart Prg...**), says how a `.prg` starts with autostart. **Via disk image** (the default) works as above: the autostart disk if drive 8 is there, and straight into RAM without a drive. **Direct to RAM** always puts it straight into RAM, even with the drive on. It is kept with **Save settings**.
- **Long file names**: disks, tapes, cartridges and programs with names up to 255 characters, the most the card allows, attach and start from the menu like the others. Before, a very long name froze the machine. In the list, a name that does not fit the line scrolls when the cursor is on it, so you can read all of it.
- **File search**, like on the Kung Fu Flash 2: inside a folder type the first letters of the name (for example `dra`): already as you type, with no ENTER, only the files and folders that start that way remain, upper or lower case, and ENTER takes you to the first one. You can use the KFF2 wildcards: the question mark stands for any single character, and `*lair` finds the names that contain "lair". BACKSPACE deletes; changing folder makes the search go away.
- **If no name starts that way**, the same search is done again inside the names: typing `12` among the Dragon's Lair files brings up `DrL-12-850.reu`, and typing `II` brings up `Turrican II.tap`.
- **Mac files**: the `._something` files a Mac leaves on the card next to the others do not appear in the menu lists, as in Randy's BMC64.
- **.REU images**: the games and demos that use the REU load it from a file. The files go in the `/REU` folder: **ALT + 0** loads one, **ALT + SHIFT + 0** detaches it.
- **The REU contents in its file**: in the **Ram Expansion Unit (REU)** menu, **Ram Image (optional)** folder, the **Auto-save image** entry, off by default. When on, the .REU file is rewritten with the contents of the REU when you power off or restart, from the menu or with the keys, and when you switch machine. Pulling the plug does not save. For game images, like Dragon's Lair, better leave it off.
- **If the file cannot be written** (for example because it is read-only), at power-off the Raspberry stays on and says so (NOT SAVED); asking again to power off, it turns off anyway.
- **The disks before powering off or restarting**: when you power off or restart, from the menu or with the keys, and when you switch machine, the disks and the tape are detached before the reset: what the drive had just written, and VICE still kept in memory, reaches the card.

### Video

- **Scanlines** adjustable from 0 to 100, for the look of a tube monitor.
- **Aspect ratio** 4:3 or 16:9, switchable on the fly.
- **A moved picture does not slow down**: with **H Center** and **V Center**, or by widening it, the picture can go a little past the edges: the part outside is simply cut. Before, as soon as it went past by even one line, the emulation could drop to 50%.
- **The NTSC VIC-20 with its own KERNAL**: in the VIC-20 NTSC modes the American KERNAL (901486-06) starts by itself, and it puts the screen in the middle as on a real NTSC VIC-20; in the PAL modes the European one (901486-07). A different ROM chosen from the menu stays yours.
- **The NTSC Plus/4 with its own KERNAL**: in the NTSC modes of the VICE Plus/4 (and of the C16) the American KERNAL (318005-05) starts by itself, as on a real NTSC Plus/4; in the PAL modes the European one (318004-05). A different ROM chosen from the menu stays yours.
- **Picture adjustment**: brightness, contrast, color, gamma, and the
  geometry (width, height, centering).
- **Your own CRT shader presets**: in the **CRT Shader** menu, **Save as preset...** saves the current adjustments in a `.crt` file in the `/crt` folder, with the name you type (if it already exists, the menu asks whether to overwrite it). If you choose another preset with adjustments changed and not saved, the menu asks what to do with them: **Save as preset...**, **Discard changes** or **Cancel**. **Save settings** remembers the preset, or the adjustments changed by hand, and at startup it starts from there. A settings file from V1.3 does not know this yet: at startup the **Default** preset applies until you save once.
- **The C128 dual screen**: the 40-column VIC-II and the 80-column VDC are
  two separate outputs, each with its own adjustments. You switch with one key.
  And if you connect two monitors, you get the dual 40 and 80 column output just like back then.
- **The dual screen in 16:9 too**: with two monitors each of the two C128 screens fills the whole TV, in 4:3 as well as in 16:9.
- **The two monitors from the menu**: in the C128 **Video** menu, **Active Display** set to **Two HDMI** sends the VIC-II to one HDMI and the VDC to the other (the first time the menu asks to restart). First set **Second Display Present** to **On**: while it is **Off**, turning **Active Display** with the arrows, with ENTER or with the **Cycle Display** key skips **Two HDMI** and asks nothing. Core 3 draws the two screens, so the emulation core is left entirely to the machine.
- **The main HDMI output**: in the **Video** menu the **Main HDMI Output** entry chooses which of the two HDMI ports carries the main screen, and the HDMI audio with it. With ENTER the menu asks to restart, and the choice holds from then on. On the C128 with two monitors it swaps the two screens. If at power-on the chosen port has no monitor or does not respond, the screen goes to the other one.
- **The HDMI resolution, 720p or 1080p**: in the **Video** menu the **HDMI Resolution** entry chooses between **720p** and **1080p** for the running machine, PAL or NTSC as it is now. With ENTER the menu asks to restart. In the **Switch** menu every machine has its **720p** and **1080p** entries next to each other. The entry is not there with the composite output or the VGA modes.
- **A new card starts at 1080p**: a card made with the full V1.4 archive starts at 1080p, and **HDMI Resolution** brings it back to 720p. Those who update with the Update archive keep what they have.
- **The CRT shader at 1080p on the Pi 4 and the 400**: the GPU draws at most a 720p frame and the scaler brings it to 1080p with a soft filter, so at 1080p the GPU does the same work as at 720p. Until 6 November 2026, at 1080p with the shader effects on, the GPU did not finish a frame in time and the machine ran at half speed. At 720p and on the Pi 5 nothing changes.
- **The CRT shader with all the effects, from the Pi 4 on**: if the GPU cannot draw a frame in time (it happens with many effects on together: on the Pi 4 and the 400 already at 720p, on the Pi 5 and the 500 at 1080p), BMC64-NG draws a smaller frame, just small enough to fit, and the scaler brings it to the size of the screen. So the machine stays at 100%, with a slightly softer picture; turning effects off, the GPU goes back little by little to the full frame. With the shader's default settings nothing changes. Until 7 November 2026, with all the effects on, the machine ran at half speed.
- **On the Pi 5 and the 500** the smaller frame of the CRT shader is only narrower: the height stays full, and the scaler only widens it.
- **The CRT shader with the picture moved**: if **H Center** or **V Center** take part of the picture off the screen (on the VIC-20, for example, to hide a border), the CRT shader stays on and works on the part you see, cut as without the shader. Until 8 November 2026, in that case the shader switched itself off and the picture stayed without effects.
- **The Pi 4 B composite output**: in the **Switch** menu every machine also has **PAL Composite 576p@50Hz** and **NTSC Composite 480p@60Hz**. After the restart video and sound come out of the 3.5 mm jack (4-pole AV cable) with the real machine's signal, 50 or 60 Hz non-interlaced; HDMI turns off and there is no CRT shader. The borders start as on the original BMC64, that is as a real C64 on a CRT monitor; if you adjust them from the **Video** menu and save, the composite output keeps its own, separate from the HDMI ones. The speed follows the TV: its refresh is measured at power-on. You go back to HDMI with an HDMI entry of **Switch**, or by holding the Commodore key (left CTRL by default) and F7 for 5 seconds and then releasing F7. The Pi 400, Pi 5 and Pi 500 have no composite jack, and on them the entries do not appear.
- **Status bars and centering on composite**: on composite the two status bars are drawn for its lines (288 in PAL, 240 in NTSC), with whole characters. In PAL **H Center** at 0 puts the picture in the middle of a CRT monitor, and moving it by a few pixels no longer slows down the emulation.
- **No 16:9 and 4:3 on composite**: on composite the **Aspect 4:3** and **Aspect 16:9** entries of the **Video** menu are not there, and the 16:9 and 4:3 hotkey (ALT + "\", and in the SID player also "\" without ALT) says NOT ON COMPOSITE: the composite gives the proportions, and touching them threw the picture off. On HDMI everything is as before.
- **VGA monitors**: with an HDMI-to-VGA adapter a PC VGA monitor works too. In the **Switch** menu every machine has **VGA 1024x768** modes at 50 and 60 Hz (about 40 and 48 kHz: a monitor that only reaches 31 kHz cannot lock onto them).
- **The first Pi 5 boards (C1 chip)**: in V1.3, on the first Pi 5 boards (with the BCM2712 C1 chip) the screen drawn by the GPU, the one of the CRT shaders, stayed black. From V1.4 BMC64-NG drives them like the Pi 5 boards with the D0 chip, the Pi 500 and the 500+: picture and CRT shaders on all of them. In the BMC64-DIAG.TXT file of the card the *pi5kms chip C* lines say that the Pi 5 has the C1 chip. If the screen stayed black, `pi5kms=0` in cmdline.txt brings back the firmware screen, without shaders.
- **The VGA monitor next to the HDMI one**: if you keep an HDMI monitor on one port and the VGA one, with its adapter, on the other, in the **Video** menu the **VGA Monitor On** entry tells which port the VGA is on: **HDMI 0** or **HDMI 1** (**None** by default). From then on the **Switch** entries with VGA in their name send the screen to the VGA monitor and the others to the HDMI monitor, with a single restart: no need to go through **Main HDMI Output** as well. The entry holds for all machines, is written to the card at once and counts from the next **Switch** choice. Going back with the Commodore key and F7 also lands on the HDMI monitor, and if you do not press a key within 15 seconds everything goes back as it was, port included. With **None**, **Switch** leaves the screen on the port where it is. On the C128 with two monitors the entry chosen in **Switch** decides where the VIC-II goes, and the VDC goes to the other one.
- **Screenshot** saved on the card.
- **Screenshots**: **SUPER + PRTSCN** (the Windows or raspberry logo key, together with PRTSCN), or **Take screenshot** in **Prefs** > **Screenshots**, saves a picture in the `/screenshots` folder of the card: `bmc64ng-0001.png`, then 0002 and on. **Capture point** says which image is taken: **Emulator** the Commodore screen as it is, **Scaled** already enlarged, **CRT**, the default, with the CRT shader, **Screen** what reaches the monitor. **Size** chooses the size, **Format** PNG or PPM, and **Save settings** keeps them. The machine stops for the time it takes to write to the card, then it goes on from where it was.

### Sound

- **The real SID**, emulated with reSID: both the 6581 of the first C64s and the 8580 of the
  later ones.
- **The 8580 digi boost**: in the **Sound** menu, **SID Model**, **SID2 Model** and **SID Card Model** also have **8580 + Digi Boost**: the 8580 with the modification that on the real thing makes digitized samples audible, which on a plain 8580 can barely be heard. It works with reSID and reSIDfp. It is kept with **Save settings**.
- **The SID Card on the Plus/4 and the PET**: these machines do not have a SID, and just like back then you add one with a cartridge. In the **Sound** menu there is the **SID Card** entry, off by default, with the model (6581 or 8580), the address (`$FD40` or `$FE80` on the Plus/4, `$8F00` or `$E900` on the PET), the filter and the engine (**SID Card Engine**: ReSid by default, or ReSIDfp with its **reSIDfp Settings** folder).
- **Output of your choice**: HDMI, headphone jack, or an external **USB DAC** for
  those who want the best quality.
- **.SID file player**: a `/sids` folder on the card, and the menu lets you
  browse and play the tunes of the SID collection, including those made for two and three SIDs.
- **In the SID player** ALT + , and ALT + . go to the previous and the next tune; ALT + P pauses without stopping the tune, and pressing it again resumes.
- **In the SID player without ALT too**: , and . go to the previous and the next tune, P pauses, M mutes. The volume goes up and down with the up and down arrows, or with ALT + + and ALT + −: without ALT, 0, − and + are the voices of the fourth SID.
- **Forward and back in the tune**: in the SID player the right and left arrows, without ALT, jump 10 seconds forward or back, and +10 SEC or -10 SEC appears at the bottom; pressing them several times adds the jumps up (three times right is 30 seconds). The player races there silently, and from there the tune plays on. While the tune plays, the player keeps a snapshot of the machine in memory for every second: going back it resumes at once from the snapshot of that point, without restarting the tune, and the music takes exactly the same path again; going forward too, if it has already played that point, it resumes from its snapshot instead of racing. Within the first 10 seconds the tune simply starts again. While paused the arrows do nothing.
- **The voices one by one**: in the SID player the keys 1 to 9 turn one voice at a time off and on, 1 to 3 those of the first SID, 4 to 6 of the second, 7 to 9 of the third; ALT + 1, ALT + 2 and ALT + 3 turn a whole SID off and on. Leaving the player turns all the voices back on.
- **V, T and N** in the SID player show and hide the VU meters, the tune time and the lines at the top with the tune name.
- **The other ALT keys** do nothing in the SID player, and NOT IN SID PLAYER appears at the bottom: the player keys, the volume, ALT + S, ALT + SHIFT + S, ALT + ESC and ALT + SHIFT + ESC still work.
- **4:3 and 16:9 in the SID player**: the "\" key switches between 4:3 and 16:9 without ALT too, and ALT + "\" works as it does outside the player. With an Italian keyboard, without ALT, both the key with \ at the top left, next to 1, and the < > key next to the left SHIFT work. Outside the player, "\" without ALT stays a C64 key.
- **Stereo with more than one SID**: with two SIDs the first plays on the left and the second on the right; with three, the first on the left, the third on the right and the second in the center.
- **Tunes with 4 SIDs**: the player also plays 4-SID files (the SID-Wizard ones, PSID v4E format): at the start the first and second SID on the left, the third and fourth on the right, and the VU meters become 12 bars, at least as wide as with three SIDs and a little wider when there is room, running from the first 0 of the time, on the left, to the end of the text on the right (PAL 50Hz); under each group the SID number, the model and the side (for example 1 8580 L). Keys 1 to 9, then 0, − and + turn the 12 voices off and on (10, 11 and 12 are the fourth SID's); ALT + 1, 2, 3 and 4 the whole SID.
- **SIDs on more cores**: the emulator's core no longer computes all the SIDs alone: with 2 or 3 SIDs, and in the player up to 8, core 2 always helps it and core 3 whenever the screen does not keep it busy. With 3 SIDs each one has its own core; in the player, with 4 to 8 SID tunes, the SIDs are split in groups of three (with 8 SIDs: the emulator the 1st, the 4th and the 7th, core 2 the 2nd, the 5th and the 8th, core 3 the 3rd and the 6th); when core 3 is busy, in pairs (with 4 SIDs core 2 does the second and the fourth). The sound is identical: only who does the computing changes, which matters above all with reSIDfp, as it needs more of it. With sid_core3=0 in cmdline.txt core 3 is left to the screen only.
- **The side of each SID, with F1-F4**: in the SID player F1, F2, F3 and F4 move the first, the second, the third and the fourth SID to the left (L), the centre (C) or the right (R): each press gives the next side, L, C, R and back to L. It works in all four players, from 1 to 4 SIDs. The start is: one SID in the centre; two SIDs L and R; three L, C and R; four L, L, R and R. The side shows under the columns, next to the model, and a notice tells it when it changes. The chosen sides stay for all the tunes with the same number of SIDs while you are in the player; leaving it, the starting sides come back. With **Channels** set to **Mono** (**Sound** menu) there are no sides, and F1-F4 say CHANNELS: MONO.
- **reSID or reSIDfp live, with the S key**: in the SID player the **S** key (without ALT) switches between reSID and reSIDfp while the tune plays, and the engine name (ENGINE: reSID or ENGINE: reSIDfp) shows for a second in the volume bar line. The choice stays for the other tunes while you are in the player; leaving it, the **SID Engine** of the **Sound** menu comes back. It works with 8-SID tunes too, which start with reSID. Outside the player S stays a C64 key.
- **One SID on a separate core**: with two or three SIDs the second one is computed by another of the four cores of the Raspberry, together with the others; in the SID player, in stereo, also with four-SID tunes. The sound is the same, and the emulation core has more headroom.
- **Tunes with 8 SIDs**: the player also plays files with more than four SIDs, up to eight (PSID v4E format), for example The Tuneful Eight, four 2-SID tunes written to play together on eight SIDs. The VU meters become 24 bars half as wide; under each group the SID number and the side, and below that the model. At the start the odd SIDs are on the left and the even ones on the right, as on the Ultimate 64. **F1**…**F8** change the side of each SID like F1-F4 in the other players, **ALT + 1**…**ALT + 8** turn a whole SID off and on; there are no single voices (the keys 1 to 9 remind you: ALT+1-8: SID ON/OFF). With these tunes the player starts with reSID, whatever **SID Engine** is chosen, and on the Pi 4 and the 400 with the **Fast** mode: with eight SIDs **Resampling** does not fit, not even using more cores. The **S** key switches them to reSIDfp and back. The SIDs are split among the cores as in the other tunes. Leaving the player, or with the next tune, the previous engine and mode come back; the other tunes play as before.
- **The model of each SID**: with tunes for more SIDs each SID plays with the model written in the file, 6581 or 8580, up to the eighth; if the file does not say, with the model of the first SID. Before, from the fourth on they played with the model of the third.
- **The SID player on the Pi 4: the limits**: reSIDfp needs much more computing than reSID. On the Pi 4 at the stock clock, tunes with up to 4 SIDs hold with reSIDfp; with 8-SID tunes reSIDfp needs the overclock: raise the **CPU Clock** of the main menu to **+20%**, or stay at the stock clock with reSID, which needs less computing (**S** key or **SID Engine** of the **Sound** menu).
- **SID sampling**: in the **Sound** menu the **SID Resampling** entry is set to **Resampling** by default, the most faithful mode: the SID sound reaches your ears without the noise the other modes add to the high notes. **Fast Resampling** sounds the same, but uses more memory and takes longer to get ready; **Interpolation** and **Fast** cost the Raspberry less and are less faithful.
- **The reSID filters**: in the **Sound** menu the **reSID Filter Settings** folder has the settings of the 6581 and of the 8580: **Passband %** (90 by default), **Gain %** (97) and **Filter Bias (mV)**. They apply to all the SIDs of that model. Passband and Gain only count with **SID Resampling** set to **Resampling** or **Fast Resampling**; the sound changes when the menu is closed, and **Save settings** keeps them.
- **reSIDfp, the third SID engine**: in the **Sound** menu the **SID Engine** entry also has **ReSIDfp**, next to **ReSid**, which stays the default. It is VICE's newest SID engine, and the most faithful to the real chip: the combined waveforms (what the SID produces when a voice uses two waveforms at once) come from measurements of real chips, and the 6581 filter has its distortion. It is aligned with its authors' latest version (libresidfp, 13 October 2026), which reworked the 6581 combined waveforms closer to the real chips, and the background hiss. The **reSIDfp Settings** folder has its settings: **Combined Waveforms** (Weak, Average, Strong), **Background Noise** (below), and for the **6581** **Composer Profile...** (below), **Filter Curve**, **Filter Range** and **Old Caps (2200pF)**, for the **8580** **Filter Curve**. The sound changes when you close the menu; **Save settings** keeps everything. It needs a little more computing than reSID, and the second SID stays on its own core. It is also there for the SID Card of the Plus/4 and the PET. It is not there on the C64 DTV, which has a SID of its own.
- **reSIDfp's background hiss**: a real C64 is never completely silent, and its recordings always have a slight background hiss. reSIDfp adds it to the sound to resemble the real chip: the **Background Noise** entry in **reSIDfp Settings** sets it from **OFF** (silence when the machine is idle, as with reSID) to **200%**, in steps of 10%. The default is **100%**, the level chosen by the reSIDfp authors to resemble recordings of real C64s, the same as VICE (since V1.4 it is their new level, eight times lower than before); **200%** is twice that. If it bothers you, set it to OFF. You hear it mostly when nothing is playing, for example at the BASIC prompt. Each SID has its own hiss, independent of the others: with two, three or four SIDs they add up, as those of as many real chips would.
- **6581 composer profiles**: the 6581 filter varied a lot from one chip to another, even between chips made in the same factory on the same day, and each musician wrote on their own. In **reSIDfp Settings** > **6581** the **Composer Profile...** entry opens a list of 58 composers (Rob Hubbard, Martin Galway, Jeroen Tel, Ben Daglish...): pick one and the 6581 filter takes the **Filter Range** of the chip they wrote on, with factory **Filter Curve** and **Combined Waveforms**. The list comes from sidplayfp, as in the development VICE. The profile in use has an asterisk in brackets next to it, until you change one of those three entries by hand; **None** restores the factory values. **Save settings** keeps it. It applies only to the 6581 and only with the ReSIDfp engine.
- **Stereo or mono**: in the **Sound** menu, just below **Audio out**, the **Channels** entry chooses how the SIDs come out when there is more than one (**Dual SID**, or a 2- or 3-SID tune in the player). **Stereo**, the default: each SID on its own side (with 3 SIDs the first on the left, the third on the right and the second on both). **Mono**: all the SIDs together on both sides. **Save settings** keeps it. It is there on the C64, C128 and SuperCPU; the other machines have a single SID, or none, and the sound is already mono.
- **The tune time**: in the SID player, on the left of the third line, the minutes and seconds of the tune. It is the time of the music: it stops when paused, and starts again from zero when you change tune. On the right is how often the tune plays: PAL 50Hz, NTSC 60Hz, or CIA with the number the tune has put in the timer.
- **The volume in the SID player**: for one second after each volume key a horizontal segmented bar appears, between the labels under the columns (for example 1 8580 R) and the tune line (DEFAULT TUNE), with the percentage on its right, and MUTE in red when the volume is at zero.
- **Inside the SID player** the status bar does not appear, so as not to cover the columns, and ALT + W does nothing: a tune cannot be listened to at double speed.
- **The USB DAC even with the computer on**: with **Audio out** set to **USB DAC** the sound moves to the DAC as soon as you plug it in and goes back to HDMI when you unplug it, and DACs that only work at 48 kHz work too, with every machine, Plus4Emu included. There is no longer any need to plug it in before switching on.
- **If the USB DAC disappears for a moment** (some cheap DACs disconnect and reconnect by themselves), the sound waits three seconds before going back to HDMI: if the DAC comes back, it carries on from there.
- **USB DACs and headphones with more channels**: mono sound comes out of the two front channels on devices with 4, 6 or 8 channels too, like 7.1 headphones. Before, it came out of the front left one only.
- **Volume** adjustable from the keyboard, and mute.
- **100% is the sound as it is**, with no amplification to distort it. Once at 100% ALT + + does nothing more, and at zero neither does ALT + −: the status bar says VOLUME 100% MAX, or MUTE.

### Controls

- **USB joysticks** and gamepads, up to four ports, ten with the V8 adapter on the user port.
- **The gamepad buttons**: in the **Joyports** menu, **Configure USB Gamepad 1...** (and 2, 3, 4): **Mapping Mode** on **Automatic**, the default, uses the right profile for the pads BMC64-NG knows (Xbox 360, M30) and a generic one for the others, and sets it again at every connection. On **Custom** the assignments are yours and they stay: **Assignments by Button...** gives a function to each button, **Assignments by Function...** a button to each function (you choose the function and press the button on the gamepad), **Clear a Button...** removes an assignment. **Reset to Default Bindings** goes back to **Automatic**. It is kept with **Save settings**.
- **The 8BitDo M30 2.4G seen as a Switch Pro controller**: the receiver can stay in Switch mode, and then the buttons are wrong. With the pad connected, hold Home (the heart) and D-pad Down for 5 seconds, until the LEDs flash: the receiver goes back to its own 6-button mode and keeps it.
- **The gamepad POTs**: on the same page, **POT X Up Value** and **POT Y Up Value** are 255 by default, **POT X Down Value** and **POT Y Down Value** 0, and the computer really reads these numbers: the Up ones at rest, the Down ones with the button pressed. With **Hat+POTX/Y Paddles** the analog axes of the gamepad reach the SID as paddles.
- **Joysticks on the GPIO**: if you have an adapter for vintage joysticks connected to the
  pins of the Raspberry, it works.
- **Keyset**: using the keyboard as a joystick, with the keys you choose. **PotX** and **PotY** are the second and third button, for the games that use them, as on USB gamepads; if you do not need them, give them the same key as Fire.
- **Mouse**: in the **Joyports** menu, set **Port 1** or **Port 2** to **USB Mouse**. The **Mouse** > **Type** menu says which mouse it is (the Commodore 1351 by default; then NEOS, Amiga, Atari CX-22, Atari ST, SmartMouse, Micromys and the Commodore 1350, which the computer reads as a joystick), and **Sensitivity (%)** how much it moves, from 10 to 200 (100 by default). The **Mouse** menu is there on the C64, SuperCPU, C128, VIC-20 and Plus/4; it is kept with **Save settings**.
- **Two more joysticks on the user port** (C64, C128, VIC-20, PET and Plus/4 with VICE): in the **Joyports** menu, **Userport Joystick Adapter**, with **Enable** and **Adapter Type**. The types are the ones VICE 3.10 has for that machine: CGA, PET, OEM, HIT, Kingsoft, Starbyte and WOJ on the C64 (the SuperCPU too) and the C128; CGA, PET, OEM and WOJ on the VIC-20 and the PET; Synergy and WOJ on the Plus/4. The others stay grey. The type saved with the settings comes back right at startup.
- **The V8 adapter, eight joysticks on the user port** (C64, SuperCPU, C128, VIC-20 and PET): in the same menu set **Adapter Type** to **V8**. Programs written for it read it. **Port 3** and **Port 4** are the first two joysticks, the **V8 Ports 5-10** folder has the other six. It is kept with **Save settings**.
- **The spinner as a paddle**: on the C64, C128 and VIC-20, in the **Joyports** menu, set **Port 1** or **Port 2** to **USB Spinner (Paddle)**: the wheel of a USB mouse works as a paddle, as on the MiSTer. A spinner that the Raspberry sees as a mouse with a wheel works too, like the one of the TAITO EGRET II mini. Each click of the wheel moves the paddle by one step, from 0 to 255, and any mouse button is the paddle button, beyond the fifth too: on the TAITO panel the two big buttons fire as well. The direction is the one of the TAITO panel (tried with Omega Race and Le Mans). **Spinner Sensitivity**, in the same menu, sets how far the paddle moves at each click: from 25% to 400%, and 100% is one step per click. To the PC the TAITO spinner is a mouse: the trackball moves the pointer and the spinner is the wheel, up and down. A USB mouse that does not declare the "boot" class now also goes to the mouse driver, with its wheel. The 1351 mouse and the spinner do not go together: choosing one sets the other back to **None**; and if the other port changes, the spinner no longer slows the machine down. As on the real Commodore, on the C64 and C128 holding the paddle button down on port 1 blocks the keyboard: that line is also a keyboard row.
- **Autofire**: in the main menu, right below **Joyports**, the **Autofire** menu has **Port 1** and **Port 2** (on the VIC-20 there is only one port; on the PET and on Plus4Emu the menu is not there). **Off**, the default, leaves the fire button as it is; with **Slow** and **Fast**, while you hold the fire button down, the port fires by itself about 7 or 14 shots per second. It works with whatever joystick is on that port: USB gamepad, keyboard or GPIO. It is kept with **Save settings**.
- **Keyboard maps**, positional and symbolic, with US, IT, UK and DE layouts.
- **The keyboard from the menu**: in the **Keyboard** menu the **Mapping** entry chooses between **Positional** and **Symbolic**, and **Layout** between US, Italiana, British and Deutsch (the last three with **Mapping** set to **Symbolic**). The Pi 400 and Pi 500 keyboards work like a USB keyboard. With **Symbolic**, the **+** and the other keys typed with SHIFT no longer come out, now and then, as graphic characters.
- **Your own keyboard map** (from BMX): in the **Keyboard** menu set **Mapping** to **Custom** and open **Mapping Editor...**: one row for every key of the machine (C64, C128, VIC-20, SuperCPU, PET and Plus/4 with VICE). On a row, ENTER and then the PC key to give it; SHIFT+ENTER adds another one, DEL removes the one shown and SHIFT+DEL all of them, Left and Right scroll through the ones on the same row. If that PC key already belonged to another key, the menu asks whether to move it, starting from CANCEL. **Save & Apply** saves it and uses it at once, **Restore Defaults...** goes back to the **Positional** one. It is written in the **user_pos.vkm** file of the machine's folder, which at first is the same as the **Positional** one.
- **A real C64 keyboard on a Keyrah V2**: with the **Positional** mapping and the Keyrah switch down (the emulator mode) every key works, **RESTORE** too: the Keyrah sends it as **Page Up**, which in the Positional mappings of the C64, C128, VIC-20 and SuperCPU is the second RESTORE key, next to **PrtScn** of the Pi 400 and 500, which stays as it was. In the **Keyboard** menu the **Restore key** line shows both. The two DB9 joystick ports of the Keyrah work with the **NUMPAD 17930** and **NUMPAD 64825** keysets, or they can be remapped in **Custom Keyset 1** and **2** to be used instead of the NUMPAD ones.
- **Restart and shutdown from the real C64 keyboard**: in the **Keyboard** menu the entries **C= + F1 Hotkey**, **C= + F3 Hotkey**, **C= + F5 Hotkey** and **C= + F7 Hotkey** and the four **CTRL + F1…F7 Hotkey** have two more choices, **Reboot BMC64** and **Power Off BMC64**: they restart and shut down the Raspberry also from the C64 keyboard on the Keyrah, which has neither ALT nor a power key. The hotkey brings up at the bottom the same question as ALT + ESC and ALT + SHIFT + ESC (REBOOT? or POWER OFF?): the same hotkey a second time, within five seconds, confirms; **ESC** (RUN/STOP on the C64 keyboard) cancels. If the other one's question is open, the key changes it into its own and the five seconds start again. They work with any keyboard and on every machine, also on the C64 GS, and the choice is kept with **Save settings**.
- **Attach, the screenshot and the SID box from the hotkeys**: the same **C= + F1…F7 Hotkey** and **CTRL + F1…F7 Hotkey** entries of the **Keyboard** menu also have BMX's eight choices. **Attach Tape**, **Attach Cart**, **Attach Drive 8**, **Attach Drive 9**, **Attach Drive 10** and **Attach Drive 11** open the file list while the machine runs, like **ALT + T**, **ALT + C**, **ALT + 8** and **ALT + 9**: RETURN attaches, ESC (RUN/STOP on the C64 keyboard) closes. **Screenshot** takes the picture of the screen, like **SUPER + PRTSCN**. **SID OSD** opens a box with the SID entries of the **Sound** menu (the model, the filter, the reSID or reSIDfp settings for that model, **Dual SID**, the address and the model of the second SID; on the Plus/4 and the PET the SID Card): you change them with the arrow keys while the music plays, the same hotkey or ESC closes it, and **Save settings** keeps them. Where the machine does not have the thing the choice is grey: Attach Tape on the SuperCPU, Attach Cart on the PET, SID OSD on the VIC-20 and on Plus4Emu. On the C64 GS the cartridge and the screenshot work; in the SID player the attach choices and the box do not open.
- **USB devices at power-on, on the Pi 5 and Pi 500**: a USB device that is not recognised at power-on is tried again by itself, up to three times, as Linux does.
- **USB devices after a restart, on the Pi 4 and Pi 400**: a device that after a restart or a machine switch does not make itself known is tried again by itself, up to four times. From the Pi 4 on, at startup the USB waits for the devices as long as BMC64 5.4.5 does, a little less than before. If the fault stays, the USB-LOG.TXT file tells what happened.
- **USB Off at Restart**, on the Pi 4 and Pi 400: if a USB device does not answer after a restart or a machine switch until you unplug it and plug it in again, the **USB Off at Restart** entry of the main menu, above **Save settings**, switches all the USB sockets off before every restart made from the menu, machine switches included, for the chosen time, from **1.0 s** to **5.0 s**: it is like unplugging the devices and plugging them in again. The default is **Off**. The arrows change the choice; with ENTER the menu asks to restart, and that restart already switches the sockets off. The sockets all switch off together.
- **The < > key of ISO keyboards**: in the **Positional** mappings of the C64, C128, VIC-20, SuperCPU and Plus/4 the key next to the left SHIFT (the one with < and > on Italian and European keyboards) types the pound sign **£**, as in Randy's BMC64; before it did nothing. In the SID player it stays the 4:3 and 16:9 key.
- **The Caps Lock of a USB keyboard**: it works as SHIFT LOCK also with the **Positional** mapping of the C64, SuperCPU, Plus/4 and Plus4Emu, and on the Plus/4 also with **Symbolic**, as in VICE; before, there it did nothing. It also works together with a real C64 keyboard on a Keyrah: the C64 SHIFT LOCK and the Caps Lock of the USB keyboard both count, like two SHIFT keys. On the Plus/4 the **left SHIFT** now shifts too (on the USB keyboard and on the Keyrah, and so also the C64 SHIFT LOCK): before, only the right one did. On the C128 and the PET Caps Lock works as SHIFT LOCK too, with every mapping, as in VICE: before, on the C128 with **Positional** it was the C128 **CAPS LOCK** key (as in Randy's BMC64), and on the PET it did nothing. The C128 **CAPS LOCK** stays on **ALT+F4**.
- **Logitech keyboards with the Bolt receiver** (for example the MX Keys Mini): the keys come out right and F12 opens the menu. Before, with the receiver that is also a mouse, K typed A, L typed E and F12 did nothing. It is Circle's fix, the same as in Randy's BMC64.
- **USB mice that declare the buttons before the pointer** (for example the Microsoft 045e:0737): now the buttons work and the pointer moves right. It is the Circle fix of 4 November 2026. Also, a mouse with an unusual descriptor no longer stops the Raspberry.
- **A USB device unplugged while the Raspberry was sending it data** (for example the Caps Lock LED to a keyboard) no longer stops the Raspberry, from the Pi 4 on: it is the Circle fix of 4 November 2026.
- **The eight extra keys of the C128** (ESC, TAB, ALT, CAPS LOCK, HELP,
  LINE FEED, 40/80, NO SCROLL), which do not exist on a PC keyboard.
- **The C128 keypad**: on the C128 the numeric keypad of the PC keyboard works as the C128 numeric keypad.

### Everything else

- **Snapshots**: you save the exact state of the machine and pick it up later.
- **Load right after Save**: opening **Load Snapshot** after saving, the cursor is already on the file just saved, as in Randy's BMC64.
- **REU**: the Commodore memory expansion, from 512 KB to 16 MB.
- **Warp**: the machine runs as fast as the Raspberry allows, to skip
  long loading times.
- **Network and WiFi**: configurable from the menu.
- **Networking on every machine**, the Pi 5 and Pi 500 included: by cable or WiFi, from the **Network** menu (**Network Device**: Off, Ethernet or WiFi). On the Pi 5 and Pi 500 the WiFi uses the three files `brcmfmac43455-sdio.raspberrypi,5-model-b` (`.bin`, `.txt` and `.clm_blob`) in the `/firmware` folder of the card: they are already there.
- **Wired network on the Pi 5 of revision 1.7**: these Pi 5 boards have a different Ethernet chip (MaxLinear MXL86110 instead of the Broadcom BCM54213PE): BMC64-NG recognises both and sets up each one in its own way. In `NET-LOG.TXT` the `PHY ID` line tells which chip is there; with a chip it does not know, BMC64-NG does as before and writes so in `NET-LOG.TXT`. It is the Circle fix of 7 November 2026.
- **The Pi 400 WiFi**: the Pi 400 has a different WiFi chip from the Pi 4 and uses the three files `brcmfmac43456-sdio` (`.bin`, `.txt` and `.clm_blob`) in the `/firmware` folder of the card: they are already there. If they are missing, or the chip does not start, the machine keeps running, at power-on too with **Network Device** set to WiFi, and the **WiFi SSID** window says so: *Missing in the firmware folder:* with the name of the missing file, to copy from the `/firmware` folder of the whole-card archive, or *The WiFi chip did not start*.
- **WiFi from the menu**: in the **Network** menu, **WiFi Settings** folder, the **WiFi SSID** item scans for networks without rebooting and lists them with the band (MHz), the channel (CH) and the signal (RSSI). Once the network is chosen, set **WiFi Country Code** to the country you are in: with the cursor on the item, type the two letters (IT for Italy, lower case too); on a full field a letter starts again from the beginning and **DEL** empties it. The code is saved as soon as it is complete and applies from the next reboot. Then **Enter Password & Reboot**: the password is shown in clear while you type it, and a long one scrolls inside the box; **Save & Reboot** saves, switches to WiFi and reboots.
- **Automatic or fixed address**: **IP Address Mode (reboot)** on **DHCP**, the default, gets the address from the router, and the **Static IP**, **Netmask**, **Gateway** and **DNS** fields show the ones received. On **Static** you type them in; without a **Netmask**, 255.255.255.0 is used. It takes effect after a reboot. The Raspberry introduces itself to the router with the name **BMC64-NG**.
- **Daylight saving time**: under **Timezone (reboot)** there is **Daylight Saving (reboot)**: **Off**, **On**, or **Auto (EU)**, which switches it on and off by itself with the European rules, from the last Sunday of March to the last Sunday of October.
- **The card seen from the PC**: the **SD Card Sharing...** item of the main menu opens the microSD to the home PC, without taking it out of the Raspberry. On Windows, in File Explorer, just type the name of the Raspberry, which tells the model and appears in the box: `\\BMC64-NG-4` (Pi 4), `\\BMC64-NG-400`, `\\BMC64-NG-5` (Pi 5), `\\BMC64-NG-500` or `\\BMC64-NG-500P` (the 500+). Or `\\` and the address shown in the box (for example `\\192.168.1.50`): the **sdcard** folder appears, and a double click opens it; you can also type it all, `\\192.168.1.50\sdcard`. If the name does not answer, the address always works. Windows first tries with the PC user and then asks for credentials: user **bmc64**, password **bmc64**. The password is changed with **Sharing Password** in the **Network** menu. While the card is open to the PC the emulation is paused and the attached disks are already saved and stay attached, the ones started with **Autostart** too (before, after browsing another folder, one could be left detached); the files the machine is using cannot be deleted or overwritten from the PC. **Stop Sharing**, or leaving the box, closes the sharing and the machine starts again. The network must be on (**Network** menu): without a network the box says **Cannot start SD Card Sharing**. The box can stay open as long as you like. While the sharing is on, the Raspberry also appears in the **Network** folder of File Explorer, with its name (for example **BMC64-NG-400**): a double click opens it. With two Raspberry Pis on at the same time, each one has its own name, and the router sees the same name. If a read or a write from the PC fails, the exact reason goes into the **SMB-LOG.TXT** file on the card.
- **Long names from the PC too**: in the share the files have their full names, even with the characters that did not get through before (the long dash in the names of the guides, Greek, Japanese), and from the PC they can be created, renamed and copied that way. Before, a name with one of those characters showed up with its short name (for example `BMC64-~3.PDF`), and a copy from there to another card could land on top of another file. In the menu too, names with accented letters now read correctly.
- **The NAS, as on the MiSTer**: the **NAS** item of the main menu has **NAS IP Address** (the address of the NAS, for example 192.168.1.100), **NAS User Name** and **NAS Password** (empty, you enter as a guest) and **SMB Protocol**: **Auto**, the default, or SMB 2.0, 2.1, 3.0, 3.0.2 or 3.1.1 (there is no SMB1: it is old and today's NAS keep it off). **NAS Path** is the path on the NAS: first the share, then the folder the browser starts from. By default it is empty: on the MiSTer's NAS, for example, it is `Mister\games\C64` (also for the SuperCPU), `Mister\games\C128`, `Mister\games\Vic20`, `Mister\games\PET2001` and `Mister\games\C16` (Plus/4). You type it like the other fields, with `\` or with `/`, it is always shown in clear and every machine keeps its own; while it is empty the NAS answers **NAS Path not set**. **Test NAS Connection** tries the connection and the folder at once. Address, user, password and protocol are the same for all the machines, the path is the one of the machine that is on; everything is saved with **Save settings**, in the `nas.txt` file on the card. In the file browser the first line opens the choice of the volume: next to SD and USB there is **NAS**, which starts from the folder of the machine (while it connects **Connecting to the NAS...** appears; a NAS that is off answers after about ten seconds). Disks, tapes, cartridges and programs are used as from the card: the file is read whole when it is opened, and what the game saves reaches the NAS as soon as the red light of the drive goes off, and when the disk is detached; if at that moment the NAS does not answer, the disk is saved on the card, in the `NAS-NON-SALVATI` folder, and nothing is lost. Files up to 32 MB. The network must be on (**Network** menu). The NAS and the card sharing do not work together: opening **SD Card Sharing...** closes the NAS, which connects again by itself when needed.
- **The passwords after the reboot**: the WiFi password (**Enter Password & Reboot**) and the **NAS Password** are shown in clear while you type them; after saving and rebooting they are shown as asterisks: the first key clears them and you type them again in clear, and if you do not touch them the saved ones stay.
- **The modem, like the Ultimate II+** (C64 and C128; the Plus/4 and the VIC-20 at the end): **Modem (SwiftLink)** in the **Network** menu, off by default: when on it takes `$DE00`, and with the EasyFlash it must stay off. **Modem Address** is `DE00` by default, the one CCGMS and the other BBS programs want. From CCGMS you call a BBS with `ATDT` and the address with the port, for example `ATDT bbs.retrocampus.com:6510`; without a port, port 80 is called. Incoming calls work too, on port 3000: the modem rings (RING) every 3 seconds, up to 10 times; **ATA** answers, and with **ATS0=1** it answers by itself. Commands, responses and registers are those of the Ultimate II+. Whatever arrives before **AT** is ignored, as on the Ultimate II+: a stray character no longer makes the next command answer ERROR. On the **C64** and the **C128** the line shows as on a factory-set Ultimate II+: DSR is active only while connected, DCD always, and when the connection drops the program notices; with C64 OS choose the Ultimate driver (`slde.u6` for `DE00`). On the **Plus/4** the modem sits on the built-in ACIA, at `$FD00`, with nothing to switch on: with VICE it is always there, with Plus4Emu when a network is chosen in the **Network** menu. The Plus/4 and the V364 have the ACIA (with Plus4Emu from 64K up), the C16 and the 232 do not, as on the real hardware; the line is read from DCD, as on a real Plus/4. On the **VIC-20** the modem sits on the user port, KERNAL device 2 (for example `OPEN 2,2,0,CHR$(6)` at 300 baud): **Modem (Userport)** in the **Network** menu, off by default, and **Modem Baud**, 300 by default, which must match the program (300, 1200 or 2400). The user port holds one thing only, as on the real hardware: turning the modem on turns the user port joystick adapter off, and the other way round.
- **The WiC64** (C64 and C64 Std, HTTPS too): the C64 WiFi module on the user port, for the programs written for it. In the **Network** menu, **WiC64** folder: **Enabled**, **Default server**, **Time zone**, **Remote timeout (s)**, **Command logging** and **Network reporting** (**Emulated SSID/RSSI**, with **Emulated SSID** and **Emulated RSSI (dBm)**, or **Actual adapter**). **Apply** works at once, **Save & Apply** also after a restart. It uses the Raspberry network, which must be on. The user port holds one thing only, as on the real thing: with the joystick adapter on, the menu answers **Userport already in use**. The identity the server gives the module is in the `/wic64.cfg` file on the card, and it is written when the menu opens.
- **Overclock and Underclock** guided from the menu, with frequencies already tested. In the main menu there are **CPU Clock...** (-20%, -10%, Stock, +10% or +20% of the stock frequency of that Raspberry) and **GPU Clock...** (Stock, +10% or +20%), and after OK it restarts. **GPU Clock** changes together the core of the GPU (**Core**) and the 3D part (**V3D**), which draw the screen: stock 500 and 500 MHz on the Pi 4 and the 400, 910 and 960 MHz on the Pi 5 and the 500. The GPU cannot be slowed down: its core also composes the screen, and below the stock frequency everything ran at half speed. At the top of the **GPU Clock** menu there are the frequencies the Raspberry is using; the GPU MHz are also shown at the top of the main menu and on the second line of the status bar, next to the CPU ones. With the CPU at -10% or -20% the GPU keeps its frequencies, stock or raised (until 11 October 2026 it stayed at its minimum: on the 500 the bar said 500 MHz; if you had chosen -10% or -20%, choose it again once). As for the CPU, the choice is only for the Raspberry where you made it: the same card on another Pi starts with the frequencies of that one.
- **Overclocking...**: for those who want to go beyond **CPU Clock** and **GPU Clock**, the **Overclocking...** item of the main menu has **Voltage Offset** (Auto, from -100 to +100 mV), **Temperature Limit** (Auto, from 60 to 85 degrees), **Core Clock** and **V3D Clock** (Auto, or from the stock frequency up). **Apply & Reboot** writes everything to config.txt and restarts, **Restore Defaults** sets Auto again. With the CPU raised the voltage is decided by **CPU Clock**, with the CPU lowered it cannot go up; **Core Clock** and **V3D Clock** take the place of **GPU Clock**, and vice versa. `force_turbo` is never written from here, so the warranty bit is not set. If the Raspberry did not start any more, from a PC delete in config.txt the block between `# BMC64-OCEXTRA-BEGIN` and `# BMC64-OCEXTRA-END`.
- **The Raspberry Pi warranty**: the firmware permanently sets the warranty bit of the Raspberry Pi when it finds `force_turbo=1` in config.txt together with a raised voltage (`over_voltage` or `over_voltage_delta`). BMC64-NG never writes the two together: **CPU Clock** at +10% and +20% raises the voltage without `force_turbo`, and **GPU Clock** uses `force_turbo` only if the CPU of that Raspberry Pi is not raised; with the CPU raised the GPU stays up with its minimum frequencies pinned. Until 14 October 2026 **CPU Clock** at +10% and +20% also wrote `force_turbo`: on a Raspberry Pi that booted that way the bit may already be set. If your CPU is raised, choose its level again once, and the block in config.txt is rewritten in the new form.
- **Cooling**: the Raspberry always wants good cooling, even with a big passive heatsink; the Pi 5 more, and even more with the **GPU Clock** above **Stock**. On a Pi 5 closed inside a C64 case, without air, with a small heatsink and the CPU at -20%, the GPU at +20% brought the emulation to around 60%; with the GPU on **Stock** it ran at 100%, at 67 degrees. If the emulation slows down, try the GPU on **Stock** first. The temperature is shown at the top of the menu and on the second line of the status bar.
- **The fan**: on the Pi 5 a fan plugged into its connector (the Active Cooler, or a case fan), and on the Pi 4 a fan with its control wire on pin 8 (GPIO 14), like the official Pi 4 Case Fan (5 V on pin 4, ground on pin 6, control on pin 8), are set with the **Fan** item of the main menu, below **CPU Clock...** and **GPU Clock...**. **Auto**, the default, works like Raspberry Pi OS: off below 50 degrees, then 30% from 50 degrees, 50% from 60, 70% from 67.5 and 100% from 75, and each step comes back down 5 degrees lower. Or by hand, from **0%** to **100%** in steps of 10%. The choice takes effect at once, with no reboot, and is kept in cmdline.txt (`ventola=`). A two-wire fan connected to 5 V (pin 2 or 4) or to 3.3 V (pin 1 or 17) and to ground always spins, fast on 5 V and slower on 3.3 V: those pins are the power supply, and no program can control them. On the Pi 400, the Pi 500 and the 500+ the item is not there; on the Pi 4 it is not there with the VGA666 output (DPI) either, which also uses pin 8. Until 14 October 2026 the Pi 5 fan stayed still; the Pi 4 one can be set since 15 October 2026.
- **The fan with `gpiofanpin=`**: with `gpiofanpin=` in cmdline.txt, with the number of another pin, the fan on that pin switches on and off by itself with the temperature. Before, it stayed on all the time.
- **The GPU on the status bar**: the number after **GPU**, at the top of the main menu and on the second line of the status bar, is the measured frequency of the 3D block (**V3D**), the one that draws the CRT shaders and that **GPU Clock** raises: by default 500 MHz on the Pi 4 and 400, 960 on the Pi 5 and 500. Until 14 October 2026 it showed the GPU core (910 on the Pi 5), and a raised V3D did not show.
- **The resolution at the top of the menu**: the first line of the main menu, the one with the machine and the video, ends with the resolution going out at that moment, for example *C64 PAL 50Hz HDMI 1080p*: **720p** or **1080p** over HDMI, **768p** with the VGA 1024x768 modes, **576p** and **480p** with the composite output. From the Pi 4 on.
- **Temperature and frequency** of the CPU visible at the top of the menu and on the second line of the status bar.
- **Date and time** at the top of the main menu, below the temperature, for example *Sat 03 Oct 2026  11:45:20*: they come from the network (NTP), with the time zone and daylight saving of the **Network** menu; until they arrive it says *Clock: waiting for network time*. In Italy: **Timezone (reboot)** on UTC+01:00 and **Daylight Saving (reboot)** on **Auto (EU)**. The times of the files seen through **SD Card Sharing** use the same time zone.
- **The time from the network, from the menu**: in the **Network** menu, **NTP** folder: **Enable NTP** (on by default), **Server 1** (pool.ntp.org by default), **Server 2**, the spare one, and **Sync interval** (15 min, Boot only, 6 h, 12 h, 24 h). **Save & Apply** works at once, without a restart; **Sync now** asks for the time now; **Status**, **Current time** and **Synced** tell how it went.
- **Files without a network**: until the time arrives, the files the Raspberry writes on the card take the most recent date among the files in the root of the card. Before, they had the zero date, which Windows shows as 1601.
- **The version** is on the first line of **About**, for example *BMC64-NG Ver. 1.1*: it tells which version is on the card.
- **Which C64 is running** is shown by the **Engine** line of **About**: *VICE 3.10 x64sc* is the cycle-exact C64, *VICE 3.10 x64* the Std C64, *VICE 3.10 xscpu64* the SuperCPU. The menu does not write it: it comes from the VICE engine inside the kernel that started, so it is reliable even when the entry chosen in **Switch** does not say it.
- **Status bar** with the active drives (8 and 9), with the green power LED, the red drive activity LED, the current track including half tracks, the tape counter.
- **The lights of each drive model**: in the status bar each drive has the lights of its real model. The 1541, 1541-II and 1570 have two round lights, the green power light on the left and the red activity light on the right; the 1571 two wide bars, with the red power light on the left; the 1581 two bars, with the red one on the left, switched on and off by the drive's DOS, as on the real 1581. Next to them is the track, with the half track (18.0, 18.5).
- **The RESTORE key**, the one that on the real C64 triggered the non-maskable interrupt.
- **The watchdog**: if the emulator stays stuck for ten seconds the screen turns black, with a big box in English, **BMC64-NG HAS STOPPED**, that says where it stopped and how to help us fix the bug: take a photo of the screen and open an issue on github.com/lroby74/BMC64-NG/issues, attaching the files BLOCCO.TXT, BMC64-DIAG.TXT and PASSI.TXT from the card (after a restart BLOCCO and PASSI are called BLOCCO-PRIMA.TXT and PASSI-PRIMA.TXT). BLOCCO.TXT is written on the card with what is needed to understand where, and the stop line also goes at the end of BMC64-DIAG.TXT; BLOCCO-PRIMA.TXT stays until a new stop comes, even after several restarts. With the menu open time does not count: the menu can stay open for hours. A real error (an assert or a panic) is reported at once, with its text, even with the menu open.
- **In this version the watchdog is off**: the **BMC64-NG HAS STOPPED** box and BLOCCO.TXT are not there. **PASSI.TXT** is always written, though: it keeps the last things the machine started and finished (the start-up, the menu entries, the hotkeys). If the Raspberry freezes, after the restart that run is called PASSI-PRIMA.TXT: the last **PRIMA:** line without its **FATTO:** says where it stopped. Send it to us with BMC64-DIAG.TXT.
- **Power in the report**: BMC64-DIAG.TXT also says whether the firmware has seen an under-voltage and which firmware is on the card. If the **alimentazione** line says **sottotensione** (under-voltage), the first suspect is the power supply or the cable.
- **USB-LOG.TXT**: every time the menu opens, if there is something new, it records what the USB found (devices, interfaces, the driver chosen) and the mouse reports with their bytes: if a USB controller does not work, that file says why.
- **USB-LOG.TXT on the Pi 4 and the 400**: every start also says whether it comes from a power-on or from a restart without switching off (a machine switch, Reboot), and the version of the firmware of the Raspberry's USB chip (VIA VL805). Up to revision 1.2 of the Pi 4 that firmware is in a memory of the chip; from 1.4, and in the Pi 400, that memory is not there and the Raspberry loads it at every start. So the logs of two Raspberry Pis can be compared for the firmware too.
- **NET-LOG.TXT**: every time the menu opens, if there is something new, it records what the network did: when the cable loses the link and when it comes back, with how many seconds it stayed down, the address given by the router and its renewals, the WiFi. Nothing is written while you play, so the emulation never slows down: if the network was lost, open the menu and the file tells you.
- **The VICE fixes after 3.10**: the fidelity fixes of the development VICE (the versions released after 3.10) are in: the CIA serial register (C64, C128, SuperCPU and the 1570, 1571 and 1581 drives), the extra read of the RTS instruction as on the real 6502, the PET's original 80-column editor (and autostart with its 50 and 60 Hz editors), the $FFF0 register of the PET 8096 and 8296, on the C128 no more VDC lock-up with R0=0, the C64 mode kernal you chose (for example JiffyDOS) that is no longer replaced by the stock one at startup and the $01 bits with the pull-up resistors, two TED registers of the Plus/4, joystick right and keyboard column 7 of the VIC-20 as on the real machine, the POT noise (mouse, paddles) only when a connected POT changes, the virtual drive's T-W commands, the SFX Sound Expander, the DigiMAX, the tapecart and the PET HRE board. And, since 10 October, the keyboard clear that also forgets the queued keys: after a dialog, for example ALT+SHIFT+R and then CANCEL, the first press of SHIFT is no longer lost.
- **The VICE fixes up to 9 November 2026**: on the C64C models (8565 chip) of the cycle exact C64 a sprite that changes from multicolor to high resolution in the middle of a line has the right dots, as on the real thing; a FLAG, CNT or SP interrupt of the CIAs is no longer lost; a broken keyboard map no longer stops the machine; the directory of a broken disk, with the block chain closed on itself, no longer goes round forever; when two devices answer on the same I/O address the right one wins; a broken `.crt` file is refused; snapshots with Plus/4 cartridges load again, and those with the MMC64, LT.Kernal, Magic Desk Plus and TPI cartridges of the C64 and with many VIC-20 cartridges load back correctly; with reSIDfp the combined waveforms (T+S, P+S, P+T+S) are the new ones of libresidfp, fitted again on samples of real chips, and the background hiss is the new one, eight times lower; after Load Snapshot the CB1 and CB2 lines of the VIAs (in the 1541 and 1571 drives, the VIC-20 and the PET) come back as they were; on the C128 the snapshot keeps all the attached cartridges and not only the first one; snapshots with the Silverrock and REX RAM-Floppy cartridges load back correctly; snapshots also save the Blackbox V9 cartridge, with its full ROMs, and those with the IDE64 and with the 256 KB Double Quick Brown Box load back correctly; a broken or hand-modified snapshot (a module too short, a PET RAM larger than the real one, GeoRAM, RAMCart, RAMLink and Double Quick Brown Box registers out of range) is refused instead of writing outside memory; the cartridge clocks (RTC) keep the time at 64 bits in snapshots; in the 1571 the unused pins of the drive's port read high as on the real drive: the fast loaders of Super Snapshot 5 and Action Replay 5 look at them to tell a 1571 in 2 MHz mode from a 1541; and other small fixes.

---

## Getting started

**First of all, the microSD card.** Format it as FAT32 with a normal cluster (allocation unit) size: 32 KB, as SD Card Formatter does, or the Windows default. **No 512-byte clusters**: the Raspberry Pi would read its start-up files and the kernel in tiny pieces and wait 9-10 seconds before BMC64-NG begins (45 seconds with a 32 GB card). With normal clusters, power-on to the BASIC prompt takes about 11 seconds; changing computer, which restarts the Pi, gains the same.

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
| **←** and **→** | inside the menu: close and open a folder. In a file list **←** goes to the folder above and **→** enters the one under the cursor; **→** on an image (D64, T64, TAP...) shows the programs inside it, with the Commodore characters, and **ENTER** on one starts it; **←** or **ESC** go back to the list |
| **ENTER** on a text file | in a file list shows it (.txt, .md, .nfo, .log, .ini, .cfg, .vkm and the like, up to 8 KB); an arrow on the right tells whether there is more text above or below; **←** or **ESC** go back to the list |
| **ENTER** held | on a name too long for the row shows its end, and on release the entry does not open. The delay is chosen in **Prefs**, **Enter shows name end** (Off, from 100 ms to 1 s) |
| **↑** and **↓** on a text field | leaving it, what you typed counts as with **ENTER** (not in the search and in the names of files to save) |
| **Reboot?** and **Power Off?** | the questions start on **CANCEL**: to restart or shut down you go up to **OK** |
| **Super** key (Windows logo, raspberry) | on its own opens and closes the menu, like **F12**. With the **Maxi Positional** mapping it stays the Commodore key |
| **AltGr** | in a text field, with the **Symbolic** mapping and an Italian or German keyboard: **[ ] { } @ # ~** as on the PC |
| **SPACE** on an entry | in the main menu (a switch, a choice, a button, a number) puts it in one of the five slots of **Quick Access...**, at the top of the menu: choose the slot and **ENTER**. **ENTER** on a slot takes you to its entry, opening the folders |
| **ESC** in the main menu | goes to **Quick Access...**; a second **ESC** closes the menu. The slots are kept with **Save settings** |
| Opening the menu again (**F12**) | you are back on the last item you touched, with the folders open as you left them, whether you left with **F12** or with **ESC** (the jump to **Quick Access...** of the first **ESC** does not count) |
| **ALT** + **↑** and **↓** | inside the menu: up and down by blocks, like **PgUp** and **PgDn** (held down, they keep going). With any keyboard, the 500+ too, where FN does not reach the computer. With the menu closed they are the tape PLAY and STOP |
| **PgUp** and **PgDn**, **F3** and **F5** | inside the menu: up and down by blocks. On the 400 and the 500 they are **FN** + **↑** and **↓** |
| **Home** and **End**, **F1** and **F7** | inside the menu: top and bottom. On the 400 and the 500 **Home** and **End** are **FN** + **←** and **→** |
| letters | in a file list: search by name, already as you type; ENTER takes you to the first match (BACKSPACE deletes) |
| a letter | inside the menu: jumps to the next entry that starts with that letter (S for Sound) |
| mouse | inside the menu: the pointer picks the row, the left button is ENTER and the right one goes back; the wheel scrolls the list and, on an item with numbers, changes them (also by holding the left button and moving). With the menu closed the mouse belongs to the C64. It is turned off in **Prefs**, item **Mouse in Menu** |
| **Prefs** > **Menu Scale %** and **Row Gap px** | the size of the menu, from 75 to 115 % (100 by default, as before), and the space between the rows, from 0 to 4 pixels (with 4 you see 17 rows instead of 25). They are kept with **Save settings** |

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

**The XUM that arrives late**: with the real drive saved, at power-on and after a machine change the XUM1541 is waited for up to 12 seconds, then the unit goes back to the emulated drive and NO XUM FOUND appears. This is for the XUM built on a Pro Micro, which has the Arduino boot program: after every restart of the Raspberry it takes about 10 seconds to show up (the ZoomFloppy is ready in 3). Until 6 November 2026 the wait was 3 seconds.

**The XUM on a Pro Micro after a restart**: the wait is not always enough. With an XUM1541 built on an Arduino Pro Micro the real Commodore drive may work only after the first power-on from cold, and stop answering after a machine change or a restart of the Raspberry: that adapter may not be compatible.

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
| **Fn + F10** (Pi 400) | shuts down the Raspberry, with the same confirmation: press it a second time |
| **C= + F7**, held for 5 seconds | safe video mode, for when the screen cannot be seen: back to the C64 PAL on HDMI 720p, saves as a restart does and restarts (C= is the left CTRL) |
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
| **SUPER + PRTSCN** | takes a screenshot, into the `/screenshots` folder of the card (**Prefs** > **Screenshots**) |

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

Seven of them also have a key of their own, as in Randy's BMC64: **F9** ESC, **F10** TAB, **F11** 40/80 DISPLAY, **END** HELP, **PAGE DOWN** LINE FEED, **SCROLL LOCK** NO SCROLL, **PAUSE** CAPS LOCK.

---

## Credits

**BMC64** is by Randy Rossi. **VICE** is by the VICE Team. **Plus4Emu** is by
Istvan Varga. **Circle**, the layer that makes it possible to run without an operating
system, is by Rene Stange.

Parts of BMC64-NG come from **BMX** by kdre76 (baremetalx.de): the KMS video, V3D and the CRT shaders, audio, the SuperCPU, the WiFi fixes, the keyboard map editor, the mouse, the menu features, Quick Access, screenshots, the WiC64, Mapping Mode and Overclocking. Inside there are also miniz (MIT license), mbedtls (Apache 2.0) and the Mozilla certificates (MPL 2.0).

**BMC64-NG** is by Cla-Bob Systems.

Free software, GNU GPL license. The full text is in the menu, under
**License**.
