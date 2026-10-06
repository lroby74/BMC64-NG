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
- **The sound of the disk going in and out**: with **Drive sound emulation** on (**Prefs** menu), attaching and detaching an image makes the sound of the disk going into and out of the real drive, together with the motor and head sounds. They come from the Ultimate-II+. The 1581 has its own (motor, head, disk in and out), also taken from the Ultimate-II+: it is a 3.5 inch drive and much quieter.
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
- **Mac files**: the `._something` files a Mac leaves on the card next to the others do not appear in the menu lists, as in Randy's BMC64.
- **.REU images**: the games and demos that use the REU load it from a file. The files go in the `/REU` folder: **ALT + 0** loads one, **ALT + SHIFT + 0** detaches it.
- **The REU contents in its file**: in the **Ram Expansion Unit (REU)** menu, **Ram Image (optional)** folder, the **Auto-save image** entry, off by default. When on, the .REU file is rewritten with the contents of the REU when you power off or restart, from the menu or with the keys, and when you switch machine. Pulling the plug does not save. For game images, like Dragon's Lair, better leave it off.
- **If the file cannot be written** (for example because it is read-only), at power-off the Raspberry stays on and says so (NOT SAVED); asking again to power off, it turns off anyway.

### Video

- **Scanlines** adjustable from 0 to 100, for the look of a tube monitor.
- **Aspect ratio** 4:3 or 16:9, switchable on the fly.
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
- **The Pi 4 B composite output**: in the **Switch** menu every machine also has **PAL Composite 576p@50Hz** and **NTSC Composite 480p@60Hz**. After the restart video and sound come out of the 3.5 mm jack (4-pole AV cable) with the real machine's signal, 50 or 60 Hz non-interlaced; HDMI turns off and there is no CRT shader. The borders start as on the original BMC64, that is as a real C64 on a CRT monitor; if you adjust them from the **Video** menu and save, the composite output keeps its own, separate from the HDMI ones. The speed follows the TV: its refresh is measured at power-on. You go back to HDMI with an HDMI entry of **Switch**, or by holding the Commodore key (left CTRL by default) and F7 for 5 seconds and then releasing F7. The Pi 400, Pi 5 and Pi 500 have no composite jack, and on them the entries do not appear.
- **Status bars and centering on composite**: on composite the two status bars are drawn for its lines (288 in PAL, 240 in NTSC), with whole characters. In PAL **H Center** at 0 puts the picture in the middle of a CRT monitor, and moving it by a few pixels no longer slows down the emulation.
- **No 16:9 and 4:3 on composite**: on composite the **Aspect 4:3** and **Aspect 16:9** entries of the **Video** menu are not there, and the 16:9 and 4:3 hotkey (ALT + "\", and in the SID player also "\" without ALT) says NOT ON COMPOSITE: the composite gives the proportions, and touching them threw the picture off. On HDMI everything is as before.
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
- **The SID player on the Pi 4: the limits**: reSIDfp needs much more computing than reSID. On the Pi 4 at the stock clock, tunes with up to 4 SIDs hold with reSIDfp; with 8-SID tunes reSIDfp needs the overclock: raise the **CPU Clock** of the main menu to **+20%**, or stay at the stock clock with reSID, which needs less computing (**S** key or **SID Engine** of the **Sound** menu).
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
- **Keyset**: using the keyboard as a joystick, with the keys you choose. **PotX** and **PotY** are the second and third button, for the games that use them, as on USB gamepads; if you do not need them, give them the same key as Fire.
- **Mouse** (the Commodore 1351).
- **The spinner as a paddle**: on the C64, C128 and VIC-20, in the **Joyports** menu, set **Port 1** or **Port 2** to **USB Spinner (Paddle)**: the wheel of a USB mouse works as a paddle, as on the MiSTer. A spinner that the Raspberry sees as a mouse with a wheel works too, like the one of the TAITO EGRET II mini. Each click of the wheel moves the paddle by one step, from 0 to 255, and any mouse button is the paddle button, beyond the fifth too: on the TAITO panel the two big buttons fire as well. The direction is the one of the TAITO panel (tried with Omega Race and Le Mans). **Spinner Sensitivity**, in the same menu, sets how far the paddle moves at each click: from 25% to 400%, and 100% is one step per click. To the PC the TAITO spinner is a mouse: the trackball moves the pointer and the spinner is the wheel, up and down. A USB mouse that does not declare the "boot" class now also goes to the mouse driver, with its wheel. The 1351 mouse and the spinner do not go together: choosing one sets the other back to **None**; and if the other port changes, the spinner no longer slows the machine down. As on the real Commodore, on the C64 and C128 holding the paddle button down on port 1 blocks the keyboard: that line is also a keyboard row.
- **Keyboard maps**, positional and symbolic, with US, IT, UK and DE layouts.
- **The keyboard from the menu**: in the **Keyboard** menu the **Mapping** entry chooses between **Positional** and **Symbolic**, and **Layout** between US, Italiana, British and Deutsch (the last three with **Mapping** set to **Symbolic**). The Pi 400 and Pi 500 keyboards work like a USB keyboard. With **Symbolic**, the **+** and the other keys typed with SHIFT no longer come out, now and then, as graphic characters.
- **A real C64 keyboard on a Keyrah V2**: with the **Positional** mapping and the Keyrah switch down (the emulator mode) every key works, **RESTORE** too: the Keyrah sends it as **Page Up**, which in the Positional mappings of the C64, C128, VIC-20 and SuperCPU is the second RESTORE key, next to **PrtScn** of the Pi 400 and 500, which stays as it was. In the **Keyboard** menu the **Restore key** line shows both. The two DB9 joystick ports of the Keyrah work with the **NUMPAD 17930** and **NUMPAD 64825** keysets, or they can be remapped in **Custom Keyset 1** and **2** to be used instead of the NUMPAD ones.
- **The < > key of ISO keyboards**: in the **Positional** mappings of the C64, C128, VIC-20, SuperCPU and Plus/4 the key next to the left SHIFT (the one with < and > on Italian and European keyboards) types the pound sign **£**, as in Randy's BMC64; before it did nothing. In the SID player it stays the 4:3 and 16:9 key.
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
- **The Pi 400 WiFi**: the Pi 400 has a different WiFi chip from the Pi 4 and uses the three files `brcmfmac43456-sdio` (`.bin`, `.txt` and `.clm_blob`) in the `/firmware` folder of the card: they are already there. If they are missing, or the chip does not start, the machine keeps running, at power-on too with **Network Device** set to WiFi, and the **WiFi SSID** window says so (*WiFi firmware missing on the SD card* or *The WiFi chip did not start*).
- **WiFi from the menu**: in the **Network** menu, **WiFi Settings** folder, the **WiFi SSID** item scans for networks without rebooting and lists them with the band (MHz), the channel (CH) and the signal (RSSI). Once the network is chosen, set **WiFi Country Code** to the country you are in: with the cursor on the item, type the two letters (IT for Italy, lower case too); on a full field a letter starts again from the beginning and **DEL** empties it. The code is saved as soon as it is complete and applies from the next reboot. Then **Enter Password & Reboot**: the password is shown in clear while you type it, and a long one scrolls inside the box; **Save & Reboot** saves, switches to WiFi and reboots.
- **Automatic or fixed address**: **IP Address Mode (reboot)** on **DHCP**, the default, gets the address from the router, and the **Static IP**, **Netmask**, **Gateway** and **DNS** fields show the ones received. On **Static** you type them in; without a **Netmask**, 255.255.255.0 is used. It takes effect after a reboot. The Raspberry introduces itself to the router with the name **BMC64-NG**.
- **Daylight saving time**: under **Timezone (reboot)** there is **Daylight Saving (reboot)**: **Off**, **On**, or **Auto (EU)**, which switches it on and off by itself with the European rules, from the last Sunday of March to the last Sunday of October.
- **The card seen from the PC**: the **SD Card Sharing...** item of the main menu opens the microSD to the home PC, without taking it out of the Raspberry. On Windows, in File Explorer, just type `\\BMC64-NG`, or `\\` and the address shown in the box (for example `\\192.168.1.50`): the **sdcard** folder appears, and a double click opens it; you can also type it all, `\\192.168.1.50\sdcard`. If the name does not answer, the address always works. Windows first tries with the PC user and then asks for credentials: user **bmc64**, password **bmc64**. The password is changed with **Sharing Password** in the **Network** menu. While the card is open to the PC the emulation is paused and the attached disks are already saved; the files the machine is using cannot be deleted or overwritten from the PC. **Stop Sharing**, or leaving the box, closes the sharing and the machine starts again. The network must be on (**Network** menu): without a network the box says **Cannot start SD Card Sharing**. The box can stay open as long as you like. While the sharing is on, the Raspberry also appears in the **Network** folder of File Explorer, as **BMC64-NG**: a double click opens it.
- **Long names from the PC too**: in the share the files have their full names, even with the characters that did not get through before (the long dash in the names of the guides, Greek, Japanese), and from the PC they can be created, renamed and copied that way. Before, a name with one of those characters showed up with its short name (for example `BMC64-~3.PDF`), and a copy from there to another card could land on top of another file. In the menu too, names with accented letters now read correctly.
- **The NAS, as on the MiSTer**: the **NAS** item of the main menu has **NAS IP Address** (the address of the NAS, for example 192.168.1.100), **NAS User Name** and **NAS Password** (empty, you enter as a guest) and **SMB Protocol**: **Auto**, the default, or SMB 2.0, 2.1, 3.0, 3.0.2 or 3.1.1 (there is no SMB1: it is old and today's NAS keep it off). **NAS Path** is the path on the NAS: first the share, then the folder the browser starts from. By default it is empty: on the MiSTer's NAS, for example, it is `Mister\games\C64` (also for the SuperCPU), `Mister\games\C128`, `Mister\games\Vic20`, `Mister\games\PET2001` and `Mister\games\C16` (Plus/4). You type it like the other fields, with `\` or with `/`, it is always shown in clear and every machine keeps its own; while it is empty the NAS answers **NAS Path not set**. **Test NAS Connection** tries the connection and the folder at once. Address, user, password and protocol are the same for all the machines, the path is the one of the machine that is on; everything is saved with **Save settings**, in the `nas.txt` file on the card. In the file browser the first line opens the choice of the volume: next to SD and USB there is **NAS**, which starts from the folder of the machine (while it connects **Connecting to the NAS...** appears; a NAS that is off answers after about ten seconds). Disks, tapes, cartridges and programs are used as from the card: the file is read whole when it is opened, and what the game saves reaches the NAS as soon as the red light of the drive goes off, and when the disk is detached; if at that moment the NAS does not answer, the disk is saved on the card, in the `NAS-NON-SALVATI` folder, and nothing is lost. Files up to 32 MB. The network must be on (**Network** menu). The NAS and the card sharing do not work together: opening **SD Card Sharing...** closes the NAS, which connects again by itself when needed.
- **The passwords after the reboot**: the WiFi password (**Enter Password & Reboot**) and the **NAS Password** are shown in clear while you type them; after saving and rebooting they are shown as asterisks: the first key clears them and you type them again in clear, and if you do not touch them the saved ones stay.
- **The modem, like the Ultimate II+** (C64 and C128 only): **Modem (SwiftLink)** in the **Network** menu, off by default: when on it takes `$DE00`, and with the EasyFlash it must stay off. **Modem Address** is `DE00` by default, the one CCGMS and the other BBS programs want. From CCGMS you call a BBS with `ATDT` and the address with the port, for example `ATDT bbs.retrocampus.com:6510`; without a port, port 80 is called. Incoming calls work too, on port 3000: the modem rings (RING) every 3 seconds, up to 10 times; **ATA** answers, and with **ATS0=1** it answers by itself. Commands, responses and registers are those of the Ultimate II+.
- **Overclock and Underclock** guided from the menu, with frequencies already tested. In the main menu there are **CPU Clock...** (-20%, -10%, Stock, +10% or +20% of the stock frequency of that Raspberry) and **GPU Clock...** (Stock, +10% or +20%), and after OK it restarts. **GPU Clock** changes together the core of the GPU (**Core**) and the 3D part (**V3D**), which draw the screen: stock 500 and 500 MHz on the Pi 4 and the 400, 910 and 960 MHz on the Pi 5 and the 500. The GPU cannot be slowed down: its core also composes the screen, and below the stock frequency everything ran at half speed. At the top of the **GPU Clock** menu there are the frequencies the Raspberry is using; the GPU MHz are also shown at the top of the main menu and on the second line of the status bar, next to the CPU ones. As for the CPU, the choice is only for the Raspberry where you made it: the same card on another Pi starts with the frequencies of that one.
- **Temperature and frequency** of the CPU visible at the top of the menu and on the second line of the status bar.
- **Date and time** at the top of the main menu, below the temperature, for example *Sat 03 Oct 2026  11:45:20*: they come from the network (NTP), with the time zone and daylight saving of the **Network** menu; until they arrive it says *Clock: waiting for network time*. In Italy: **Timezone (reboot)** on UTC+01:00 and **Daylight Saving (reboot)** on **Auto (EU)**. The times of the files seen through **SD Card Sharing** use the same time zone.
- **The version** is on the first line of **About**, for example *BMC64-NG Ver. 1.1*: it tells which version is on the card.
- **Which C64 is running** is shown by the **Engine** line of **About**: *VICE 3.10 x64sc* is the cycle-exact C64, *VICE 3.10 x64* the Std C64, *VICE 3.10 xscpu64* the SuperCPU. The menu does not write it: it comes from the VICE engine inside the kernel that started, so it is reliable even when the entry chosen in **Switch** does not say it.
- **Status bar** with the active drives (8 and 9), with the green power LED, the red drive activity LED, the current track including half tracks, the tape counter.
- **The lights of each drive model**: in the status bar each drive has the lights of its real model. The 1541, 1541-II and 1570 have two round lights, the green power light on the left and the red activity light on the right; the 1571 two wide bars, with the red power light on the left; the 1581 two bars, with the red one on the left, switched on and off by the drive's DOS, as on the real 1581. Next to them is the track, with the half track (18.0, 18.5).
- **The RESTORE key**, the one that on the real C64 triggered the non-maskable interrupt.
- **The watchdog**: if the emulator stays stuck for ten seconds the screen turns black, with a big box in English, **BMC64-NG HAS STOPPED**, that says where it stopped and how to help us fix the bug: take a photo of the screen and open an issue on github.com/lroby74/BMC64-NG/issues, attaching the files BLOCCO.TXT, BMC64-DIAG.TXT and PASSI.TXT from the card (after a restart BLOCCO and PASSI are called BLOCCO-PRIMA.TXT and PASSI-PRIMA.TXT). BLOCCO.TXT is written on the card with what is needed to understand where, and the stop line also goes at the end of BMC64-DIAG.TXT; BLOCCO-PRIMA.TXT stays until a new stop comes, even after several restarts. With the menu open time does not count: the menu can stay open for hours. A real error (an assert or a panic) is reported at once, with its text, even with the menu open.
- **USB-LOG.TXT**: every time the menu opens, if there is something new, it records what the USB found (devices, interfaces, the driver chosen) and the mouse reports with their bytes: if a USB controller does not work, that file says why.
- **The VICE fixes after 3.10**: the fidelity fixes of the development VICE (the versions released after 3.10) are in: the CIA serial register (C64, C128, SuperCPU and the 1570, 1571 and 1581 drives), the extra read of the RTS instruction as on the real 6502, the PET's original 80-column editor (and autostart with its 50 and 60 Hz editors), the $FFF0 register of the PET 8096 and 8296, on the C128 no more VDC lock-up with R0=0, the C64 mode kernal you chose (for example JiffyDOS) that is no longer replaced by the stock one at startup and the $01 bits with the pull-up resistors, two TED registers of the Plus/4, joystick right and keyboard column 7 of the VIC-20 as on the real machine, the POT noise (mouse, paddles) only when a connected POT changes, the virtual drive's T-W commands, the SFX Sound Expander, the DigiMAX, the tapecart and the PET HRE board.

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

---

## Credits

**BMC64** is by Randy Rossi. **VICE** is by the VICE Team. **Plus4Emu** is by
Istvan Varga. **Circle**, the layer that makes it possible to run without an operating
system, is by Rene Stange.

**BMC64-NG** is by Cla-Bob Systems.

Free software, GNU GPL license. The full text is in the menu, under
**License**.
