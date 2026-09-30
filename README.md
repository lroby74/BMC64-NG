# BMC64-NG

**A bare-metal Commodore emulator for the Raspberry Pi 4, 400, 5 and 500, built on VICE 3.10.**

There's no Linux underneath: the Pi boots straight into the Commodore.

> **Learn the shortcut keys: they make a huge difference.** About 60 ALT key combinations handle tapes, disks, cartridges, resets, video, sound and the SID player without opening the menu. **[Read them all here](docs/Shortcut-Keys.md)** ([in italiano](docs/Tasti-rapidi.md)).

## Highlights

- **Cycle-exact C64 by default.** It uses VICE's most accurate C64 (x64sc). The standard C64 is one menu away.
- **C128 on two monitors.**
  - The 40-column VIC-II screen goes on one HDMI port and the 80-column VDC screen on the other (Video > Active Display > Two HDMI).
  - Video > Main HDMI Output swaps which port is the main one.
- **SID Player for .SID tunes, stereo, up to 4 SIDs.**
  - There's a VU bar for every voice: 12 bars with a four-SID tune.
  - You can mute single voices (keys 1-9, 0, - and +) or whole SIDs (ALT + 1-4).
  - F1-F4 place each SID on the left, in the centre or on the right.
  - The arrow keys jump 10 seconds back and forward, and set the volume.
  - Made for anyone who enjoys SID music or works with it.
- **reSIDfp**, the more accurate SID engine, next to reSID (Sound > SID Engine).
- **Main output on HDMI 0 or HDMI 1**, whichever you prefer.
- **Audio everywhere.** HDMI works on every model and the 3.5 mm jack on the Pi 4. USB DACs work on all models and can be plugged in while running.
- **A two-line status bar.** The second line shows temperature, CPU clock and emulation speed (ALT + SHIFT + O).
- **About 60 ALT shortcuts** for the things you do every day: the full list is in [Shortcut Keys](docs/Shortcut-Keys.md).
- **4:3 / 16:9 with one key** (ALT + \\).

## Machines

| Machine | Notes |
|---|---|
| Commodore 64 | cycle exact (default) or standard; models include C64C, SX-64 and C64 GS |
| Commodore 64 with SuperCPU | the CMD accelerator at 20 MHz, at full speed on the Pi 4 too |
| Commodore 128 | one or two monitors |
| VIC-20 | |
| Plus/4 | two emulators: VICE and Plus4Emu |
| PET | |

## Hardware

- **Raspberry Pi 4, 400, 5 and 500.**
- **Keyboards.**
  - The built-in keyboards of the Pi 400 and Pi 500 work, and so do ordinary USB PC keyboards.
  - Keyboard > Layout offers US, Italian, British and German (with Mapping set to Symbolic).
  - On the C128, the PC numeric keypad is the C128 keypad.
- **Monitors.** HDMI, or a PC VGA monitor through an HDMI-to-VGA adapter. Every machine has 1024x768 modes at 50 and 60 Hz.
- **Two USB floppy drives.** They appear in the file browser as FLP1 and FLP2. Put files or disk images on a floppy with your PC and use them from BMC64-NG.
- **Real Commodore drives** through an XUM1541 adapter. Use your own 1541, 1571 or 1581 with your own 5.25" and 3.5" disks (ALT + 7 for drive 8, ALT + SHIFT + 7 for drive 9).

## Getting started

1. Take a microSD card from a well-known, reliable brand.
2. Format it as **FAT32** with a normal cluster (allocation unit) size: **32 KB**, as the SD Association's SD Card Formatter does, or the Windows default.
   - **Do not use 512-byte clusters.** The Raspberry Pi then reads its own start-up files and the kernel in tiny pieces: a Pi 4 or 400 waits 9-10 seconds before BMC64-NG even begins, and with a 32 GB card it took 45 seconds. With normal clusters, power-on to the BASIC prompt takes about 11 seconds. Changing machine restarts the Pi, so it gets the same benefit.
   - For cards bigger than 32 GB use a tool such as FAT32Format.
3. Download the `.7z` archive of the latest release from the [Releases](https://github.com/lroby74/BMC64-NG/releases) page and copy its **whole content** onto the card.
4. Put the card in the Pi and switch it on. It starts as a PAL Commodore 64 (cycle exact) on HDMI, 720p at 50 Hz; Machine > Switch changes machine and video mode.

**The safety timer.** After you change machine or resolution (Machine > Switch), press a key within 15 seconds to keep the new setting. If you don't, BMC64-NG goes back to the previous one by itself. Leave *Safety timer (15s)* on until your screen and resolution setup is stable.

## Finding files

In any file list, type the first letters of a name and press ENTER. The ? and * wildcards work too.

## Updating

Copy the new kernel files over the old ones:
- `kernel8.img*` for the Pi 4 and 400;
- `kernel_2712.img*` for the Pi 5 and 500.

Replace the `.txt` configuration files only when the release notes say so. `config.txt`, `cmdline.txt` and the `settings*.txt` files hold your own settings.

## Guides

The PDF guides in [docs/](docs/) describe every menu and every shortcut, in English and Italian. Reading them is highly recommended. The shortcuts alone are also in [Shortcut Keys](docs/Shortcut-Keys.md) and [Tasti rapidi](docs/Tasti-rapidi.md).

## All features

BMC64-NG runs the whole of VICE 3.10, not a simplified rewrite, and it puts fidelity first: if the real Commodore did something strange, BMC64-NG does it too. Keys are given where they help; the full list is in [Shortcut Keys](docs/Shortcut-Keys.md).

### Machines

- **Cycle-exact or standard C64.** The cycle-exact C64 reproduces the VIC-II cycle by cycle, as demos and games that push the chip need, and keeps up even on a Pi 4 at stock frequency. In the Switch menu the two C64s are called X64SC (cycle exact) and Std (the standard VICE C64, lighter).
- **C64 GS console.** Pick the cartridge-only console in the C64 Model... menu. As on the real console, only the menu, reset, restart, shutdown, cartridge and status bar keys work, and the others say NOT ON C64 GS.
- **C64 with SuperCPU.** It emulates the CMD accelerator at 20 MHz, with the free ROM that comes with VICE.
- **C128 with both screens.** It has the 40-column VIC-II screen and the 80-column VDC screen, on one monitor or two.
- **Plus/4 with two engines.** Choose the VICE Plus/4 or Plus4Emu, which is considered more faithful.
- **The right VIC-20 KERNAL.** The VIC-20 NTSC modes start the American KERNAL (901486-06), which centres the screen as on a real NTSC VIC-20, and the PAL modes start the European one (901486-07). If you choose a different ROM in the menu, it stays.

### Video and screens

- **Adjustable scanlines.** Set them from 0 to 100 for the look of a tube monitor; ALT + SHIFT + "+" and ALT + SHIFT + "−" change them on the fly.
- **Aspect ratio on custom modes.** On the 768x545 and 768x525 modes, which run at the machine's exact frequency, BMC64-NG reads the TV's real shape at power-on and assumes 16:9 if the TV doesn't say. 16:9 then fills the screen and 4:3 comes out really 4:3, as at 720p.
- **Picture adjustments.** Set brightness, contrast, colour and gamma, and the geometry: width, height and centring.
- **No slowdown past the edges.** When H Center, V Center or a wider picture push the image a little past the screen edges, the part outside is simply cut and the emulation doesn't slow down.
- **C128 40/80 columns.** The VIC-II and VDC screens are two separate outputs, each with its own adjustments. ALT + PRTSCN switches between them like the little switch on the monitors, and ALT + SHIFT + PRTSCN or ALT + F7 is the 40/80 DISPLAY key of the real keyboard.
- **C128 on two monitors.** Set Second Display Present to On before choosing Video > Active Display > Two HDMI; the first time, the menu asks to restart. Each screen then fills its own TV in 4:3 or 16:9, and ALT + SHIFT + \\ switches the shape of the other screen.
- **Main HDMI Output.** The HDMI audio follows the main screen to the port you choose in Video > Main HDMI Output; the menu asks to restart, and the choice holds from then on. On the C128 with two monitors it swaps the two screens.
- **Screenshots to the card.** BMC64-NG saves screenshots on the microSD card.

### Sound and SID

- **reSID and reSIDfp.** Sound > SID Engine offers ReSIDfp next to ReSid, which stays the default, and both emulate the 6581 of the first C64s and the 8580 of the later ones. ReSIDfp is VICE's newest and most faithful SID engine: its combined waveforms come from measurements of real chips, its 6581 filter has the chip's distortion, and it needs a little more computing power.
- **reSIDfp settings.** The reSIDfp Settings folder sets Combined Waveforms (Weak, Average, Strong), Filter Curve, Filter Range and Old Caps (2200pF) for the 6581, and Filter Curve for the 8580. The sound changes when you close the menu, and Save settings keeps it all.
- **Optional background hiss.** reSIDfp Settings > Background Noise adds the faint hiss heard in recordings of real C64s, from OFF (the default) to 100% in steps of 10%. You hear it mostly when nothing is playing, for example at the BASIC prompt. Each SID has its own hiss, independent of the others, so with two, three or four SIDs they add up as they would with that many real chips.
- **reSID filter settings.** Sound > reSID Filter Settings has Passband % (default 90), Gain % (default 97) and Filter Bias (mV) for the 6581 and for the 8580, applied to every SID of that model. Passband and Gain count only when SID Resampling is set to Resampling or Fast Resampling.
- **SID resampling.** Sound > SID Resampling defaults to Resampling, the most faithful mode, without the noise the other modes add to high notes. Fast Resampling sounds the same but uses more memory and takes longer to get ready; Interpolation and Fast cost the Pi less and are less faithful.
- **Plus/4 and PET SID Card.** These machines have no SID, and as back then you add one on a cartridge. Sound > SID Card (off by default) sets the model (6581 or 8580), the address (`$FD40` or `$FE80` on the Plus/4, `$8F00` or `$E900` on the PET), the filter and the SID Card Engine (ReSid or ReSIDfp).
- **Stereo with multiple SIDs.** With two SIDs the first plays on the left and the second on the right; with three, the first plays left, the third right and the second in the centre. Sound > Channels switches between Stereo, the default, and Mono on the C64, C128 and SuperCPU.
- **Second SID on another core.** With two or three SIDs, and with four-SID tunes in the player in stereo, the second SID runs on another of the Pi's four cores. The sound is the same, and the emulation has more headroom.
- **USB DAC at any time.** With Sound > Audio out set to USB DAC, the sound moves to the DAC as soon as you plug it in and back to HDMI when you unplug it, and DACs that only work at 48 kHz work too, on every machine including Plus4Emu. If a DAC drops out for a moment, as some cheap ones do, the sound waits three seconds before going back to HDMI and carries on if the DAC returns.
- **Volume without distortion.** ALT + + and ALT + − set the volume and ALT + M mutes, with + and − on the number row or the keypad. 100% is the sound as it is, with no amplification, and at the limits the status bar says VOLUME 100% MAX or MUTE.

### SID player

- **Built-in .SID player.** Copy your .SID files into the `/sids` folder on the card, then press ALT + S to browse and play them, including tunes for two and three SIDs. ALT + SHIFT + S stops the tune and resets.
- **Four-SID tunes.** The player also plays 4-SID files (PSID v4E, the SID-Wizard ones) with 12 VU bars. Under each group it shows the SID number, model and side, for example "1 8580 L".
- **Tune controls.** The , and . keys go to the previous and next tune, P pauses and resumes, and M mutes, with or without ALT. The up and down arrows, or ALT + + and ALT + −, set the volume. The right and left arrows jump 10 seconds forward or back; press them again to jump further (three presses make 30 seconds).
- **Mute voices or whole SIDs.** Keys 1 to 3 turn the voices of the first SID off and on, 4 to 6 those of the second, 7 to 9 the third, and 0, − and + the fourth; ALT + 1 to ALT + 4 do the same for whole SIDs. Leaving the player turns every voice back on.
- **Left, centre or right.** F1 to F4 move the first to fourth SID to the next side with each press (L, C, R, then L again), and a notice shows the change. The sides hold for all tunes with the same number of SIDs until you leave the player; with Sound > Channels set to Mono there are no sides, and F1-F4 say CHANNELS: MONO.
- **Time, rate and volume.** The third line shows the music time, which stops on pause and restarts at zero with each tune, and the play rate: PAL 50Hz, NTSC 60Hz, or CIA with the value the tune put in the timer. When you change the volume, a horizontal segmented bar shows it for a second under the SID labels, with the percentage on its right and MUTE in red at zero.
- **Show or hide.** V, T and N show and hide the VU meters, the tune time and the lines with the tune name.
- **Aspect ratio in the player.** The \\ key switches between 4:3 and 16:9 without ALT; on an Italian keyboard both the \\ key next to 1 and the < > key next to the left SHIFT do it. Outside the player, \\ without ALT stays a C64 key.
- **Player screen and keys.** Inside the player the status bar stays hidden so it doesn't cover the bars, and ALT + W does nothing, since a tune can't be heard at double speed. Other ALT keys that don't apply show NOT IN SID PLAYER.

### Disks, tapes and cartridges

- **Up to four drives.** Drives 8, 9, 10 and 11 work at the same time, with fifteen disk image formats: `.d64` `.d67` `.d71` `.d80` `.d81` `.d82` `.d1m` `.d2m` `.d4m` `.g64` `.g71` `.g41` `.p64` `.x64` `.dhd`. ALT + 8 and ALT + 9 attach a disk to drive 8 or 9, and with SHIFT they detach it.
- **A model for each drive.** Each drive can be a 1541, 1541-II, 1570, 1571, 1581 or none, and the Plus/4 adds the 1551. ALT and a number key set the model of drive 8, and ALT + SHIFT and a number key that of drive 9.
- **True 1541 emulation.** The drive runs its own processor and program, like the original, so fast loaders and the copy protections of vintage games work.
- **Card folders in Plus4Emu.** In Plus4Emu, turn on IEC FileSystem for Drive 8 or Drive 9 to read the files in the card's folder; the choice is saved.
- **Blank disks.** Create new, empty disk images from the menu.
- **Tapes with recorder controls.** `.tap` and `.t64` tapes come with a tape counter and recorder keys: ALT + ↑ is PLAY, ALT + ↓ is STOP, ALT + → fast-forwards and ALT + ← rewinds, twice as fast with SHIFT. ALT + DEL resets the counter.
- **Cartridges at power-on too.** Insert `.crt` and `.bin` cartridges with ALT + C and remove them with ALT + SHIFT + C. On the C64 and C128, Cartridge > Set current cart default makes the inserted cartridge start at every power-on and saves by itself; choose it again after removing the cartridge to undo it.
- **EasyFlash saves.** What a game writes into the flash goes back into its `.crt` file, with Cartridge > Save EasyFlash Now or by itself when you remove or change the cartridge. It also saves when you power off, restart or switch machine, if the game wrote to it.
- **Programs and autostart.** Load `.prg` programs directly, or pick a PRG or disk file with ALT + A and it starts by itself. With true drive emulation, VICE puts the `.prg` on a disk it writes in the card's root, such as `autostart-PLUS4.d64`; you can delete it, and it comes back next time.
- **Long file names.** Disks, tapes, cartridges and programs with names up to 255 characters, the most the card allows, attach and start from the menu like any other file.
- **Search in file lists.** Type the first letters and press ENTER: case doesn't matter, ? stands for any single character, and `*lair` finds names that contain "lair". If no name starts that way, the search looks inside the names, so `12` finds `DrL-12-850.reu`.
- **REU memory expansion.** The REU goes from 512 KB to 16 MB: ALT + E turns it on and off and ALT + SHIFT + E changes its size. Games and demos that use it load their .REU image from the `/REU` folder with ALT + 0, and ALT + SHIFT + 0 detaches the file while the REU stays on.
- **REU contents saved.** With Ram Expansion Unit (REU) > Ram Image (optional) > Auto-save image turned on (it is off by default), BMC64-NG rewrites the .REU file when you power off, restart or switch machine. Pulling the plug doesn't save, and for game images such as Dragon's Lair it's best left off.

### Real hardware and USB

- **Real Commodore drives.** An XUM1541 adapter connects a real 1541, 1571 or 1581 so you can read your original disks (ALT + 7 for drive 8, ALT + SHIFT + 7 for drive 9). Several drives can be daisy-chained on different device numbers, and a Pi1541, which emulates a 1541 and a 1581, works too.
- **USB and GPIO joysticks.** USB joysticks and gamepads work on up to four ports, and so do vintage joysticks through an adapter on the Pi's pins. ALT + J swaps the two joystick ports.
- **1351 mouse.** A mouse works as the Commodore 1351.

### Keyboard and controls

- **Keyboard maps.** There are positional and symbolic keyboard maps, and US, IT, UK and DE layouts.
- **The C128's extra keys.** ESC, TAB, ALT, CAPS LOCK, HELP, LINE FEED, 40/80 DISPLAY and NO SCROLL are on ALT + F1 to ALT + F8, in the order of the real machine.
- **RESTORE key.** The RESTORE key is there, the one that triggers the non-maskable interrupt on the real C64.
- **Keyboard as joystick.** Keyset turns keys you choose into a joystick.

### Menu and usability

- **One-key menu.** F12 opens and closes the menu; the arrow keys move, ENTER chooses, and a letter jumps to the next entry that starts with it (S for Sound). Save settings keeps your setup for the next start.
- **Shortcut list in the menu.** The Shortcut Keys entry shows the list of shortcuts, always up to date.
- **Status bar.** It shows drives 8 and 9 with a green power LED and a red activity LED, the current track including half tracks, and the tape counter; ALT + O hides it and brings it back. The CPU temperature and frequency show at the top of the menu as well as on the second line (ALT + SHIFT + O).
- **Warp speed.** ALT + W runs the machine as fast as the Pi allows, to skip long loading times.
- **Machine snapshots.** Save the exact state of the machine and pick it up later.
- **Hard and soft reset.** ALT + R is a hard reset that clears memory, the one that restarts a game; ALT + SHIFT + R is a soft reset that keeps memory, handy for changing or studying code in memory.
- **Pi restart and shutdown.** ALT + ESC restarts the Pi and ALT + SHIFT + ESC shuts it down, each after a second press to confirm; ESC alone cancels. On the Pi 500 and 500+ the power button works too.
- **Overclock and underclock.** The menu guides you through both, with frequencies that have already been tested.

### Safety and diagnostics

- **15-second safety timer.** After you change machine, resolution or refresh rate in the Switch menu, press any key within 15 seconds to keep it, or the previous one comes back by itself. Safety timer (15s), at the bottom of the Switch menu, turns this off after a confirmation, but then a mode your monitor can't show leaves the screen black until you fix `config.txt` on a PC.
- **HDMI fallback at power-on.** If the port chosen in Video > Main HDMI Output has no monitor or doesn't respond at power-on, the picture goes to the other port.
- **NOT SAVED warning.** If the .REU file can't be written, for example because it's read-only, the Pi stays on at power-off and shows NOT SAVED; asking again powers it off anyway.
- **NO XUM FOUND warning.** Without the XUM1541 adapter, ALT + 7 shows NO XUM FOUND and the drive stays as it was.

## To do

- 15 kHz CRT monitors through VGA666, Pi2SCART, RGB-Pi 1 and RGB-Pi 2.
- The C64 DTV.
- Managing BMC64-NG over Wi-Fi from a web page, for copying files and updated kernels.

## Credits and license

BMC64-NG builds on the work of:
- BMC64 by Randy Rossi;
- VICE by the VICE Team;
- Circle by Rene Stange, and circle-stdlib by Stephan Mühlstrasser;
- Plus4Emu by Istvan Varga;
- reSID by Dag Lem, and reSIDfp by Antti S. Lankila and Leandro Nini.

BMC64-NG is licensed under the GNU General Public License v3 ([LICENSE](LICENSE)). VICE, in `third_party/vice-3.10`, is licensed under the GNU GPL v2 or later.
