# BMC64-NG

**A bare-metal Commodore emulator for the Raspberry Pi 4, 400, 5 and 500, built on VICE 3.10.**

There's no Linux underneath: the Pi boots straight into the Commodore.

**Community:** BMC64-NG has its own section on the [BMC64 Discord server](https://discord.gg/hKvxqMj9J6), for discussion and help.

> **Learn the shortcut keys: they make a huge difference.** About 60 ALT key combinations handle tapes, disks, cartridges, resets, video, sound and the SID player without opening the menu. **[Read them all here](docs/Shortcut-Keys.md)** ([in italiano](docs/Tasti-rapidi.md)).

## Highlights

- **Cycle-exact C64 by default.** It uses VICE's most accurate C64 (x64sc). The standard C64 is one menu away.
- **C128 on two monitors.**
  - The 40-column VIC-II screen goes on one HDMI port and the 80-column VDC screen on the other (Video > Active Display > Two HDMI).
  - Video > Main HDMI Output swaps which port is the main one.
- **SID Player for .SID tunes, stereo, up to 8 SIDs.**
  - There's a VU bar for every voice: 12 bars with a four-SID tune, 24 narrower ones with eight SIDs.
  - You can mute single voices (keys 1-9, 0, - and +) or whole SIDs (ALT + 1-4, ALT + 1-8 with eight SIDs).
  - F1-F4 (F1-F8 with eight SIDs) place each SID on the left, in the centre or on the right.
  - The arrow keys jump 10 seconds back and forward, and set the volume.
  - Made for anyone who enjoys SID music or works with it.
- **reSIDfp**, the more accurate SID engine, next to reSID (Sound > SID Engine), and in the player the S key switches between the two while the tune plays.
- **SIDs on more cores.** With two or more SIDs the Pi's other cores compute part of them, so reSIDfp keeps up with four-SID tunes even at the Pi 4's stock clock.
- **Composite video on the Pi 4 B**, PAL and NTSC, scaled and composed in hardware by the GPU.
- **Main output on HDMI 0 or HDMI 1**, whichever you prefer.
- **720p or 1080p**, one entry of the Video menu, and **CRT shaders** drawn by the Pi's GPU, with presets of your own.
- **Audio everywhere.** HDMI works from the Pi 4 on and the 3.5 mm jack on the Pi 4. USB DACs work from the Pi 4 on too and can be plugged in while running.
- **A two-line status bar.** The second line shows temperature, CPU and GPU clocks and emulation speed (ALT + SHIFT + O).
- **About 60 ALT shortcuts** for the things you do every day: the full list is in [Shortcut Keys](docs/Shortcut-Keys.md).
- **4:3 / 16:9 with one key** (ALT + \\).
- **Network on every machine.** Wi-Fi chosen from the menu, a static address if you want one, the card opened to your PC under a name that tells the model, such as `\\BMC64-NG-400\sdcard`, your games from a NAS as on the MiSTer, and a modem for BBSes on the C64, C128, Plus/4 and VIC-20.

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
  - The built-in keyboards of the Pi 400 and Pi 500 work, and so do ordinary USB PC keyboards, Logitech keyboards with the Bolt receiver included.
  - Keyboard > Layout offers US, Italian, British and German (with Mapping set to Symbolic).
  - On the C128, the PC numeric keypad is the C128 keypad.
- **Monitors.** HDMI, or a PC VGA monitor through an HDMI-to-VGA adapter, on either HDMI port (Video > VGA Monitor On). Every machine has 1024x768 modes at 50 and 60 Hz.
- **Two USB floppy drives.** They appear in the file browser as FLP1 and FLP2. Put files or disk images on a floppy with your PC and use them from BMC64-NG.
- **Real Commodore drives** through an XUM1541 adapter. Use your own 1541, 1571 or 1581 with your own 5.25" and 3.5" disks (ALT + 7 for drive 8, ALT + SHIFT + 7 for drive 9). An XUM1541 built on an Arduino Pro Micro may not be compatible: with it the real Commodore drive works only after the first power-on from cold, and after a machine change or a restart of the Raspberry it stops answering. BMC64-NG is tested with a ZoomFloppy.

## Getting started

1. Take a microSD card from a well-known, reliable brand.
2. Format it as **FAT32** with a normal cluster (allocation unit) size: **32 KB**, as the SD Association's SD Card Formatter does, or the Windows default.
   - **Do not use 512-byte clusters.** The Raspberry Pi then reads its own start-up files and the kernel in tiny pieces: a Pi 4 or 400 waits 9-10 seconds before BMC64-NG even begins, and with a 32 GB card it took 45 seconds. With normal clusters, power-on to the BASIC prompt takes about 11 seconds. Changing machine restarts the Pi, so it gets the same benefit.
   - For cards bigger than 32 GB use a tool such as FAT32Format.
3. Download the full archive of the latest release, such as `BMC64-NG V1.4.7z`, from the [Releases](https://github.com/lroby74/BMC64-NG/releases) page and copy its **whole content** onto the card.
4. Copy the Commodore ROMs into the machine folders of the card: see [ROM files](#rom-files) below.
5. Put the card in the Pi and switch it on. It starts as a PAL Commodore 64 (cycle exact) on HDMI, 1080p at 50 Hz; Machine > Switch changes machine and video mode.

**The safety timer.** After you change machine or resolution (Machine > Switch), press a key within 15 seconds to keep the new setting. If you don't, BMC64-NG goes back to the previous one by itself. Leave *Safety timer (15s)* on until your screen and resolution setup is stable.

## ROM files

BMC64-NG does not include the Commodore ROMs. You must supply them from a legal source, for example the official VICE 3.10 (its machine folders: `data/C64`, `data/DRIVES` and so on in the source package) or C64 Forever. Every machine folder of the card has a `PUT_ROMS_HERE.txt` file with the names it needs. The names must be exactly these, as in VICE 3.10, and the simplest way is to copy each VICE folder whole into the folder of the same name.

```text
C64/
    kernal-901227-03.bin           KERNAL
    basic-901226-01.bin            BASIC
    chargen-901225-01.bin          character generator
    (the other C64 models use the other KERNAL files of the VICE folder)
C128/
    kernal-318020-05.bin           KERNAL
    basiclo-318018-04.bin          BASIC low
    basichi-318019-04.bin          BASIC high
    chargen-390059-01.bin          character generator
    kernal64-901227-03.bin         KERNAL of the C64 mode
    basic64-901226-01.bin          BASIC of the C64 mode
VIC20/
    kernal.901486-07.bin           KERNAL, PAL
    kernal.901486-06.bin           KERNAL, NTSC
    basic-901486-01.bin            BASIC
    chargen-901460-03.bin          character generator
PET/
    kernal-4.901465-22.bin         KERNAL
    basic-4.901465-23-20-21.bin    BASIC 4
    edit-4-80-b-50Hz.901474-04.bin editor
    characters-2.901447-10.bin     character generator
    (the other PET models use the other files of the VICE folder)
PLUS4/
    kernal-318004-05.bin           KERNAL, PAL
    kernal-318005-05.bin           KERNAL, NTSC
    basic-318006-01.bin            BASIC
    3plus1-317053-01.bin           3-plus-1 low
    3plus1-317054-01.bin           3-plus-1 high
SCPU64/
    scpu64                         the SuperCPU ROM that comes with VICE
    chargen-901225-01.bin          character generator
DRIVES/
    dos1541-325302-01+901229-05.bin  1541
    dos1541ii-251968-03.bin        1541-II
    dos1570-315090-01.bin          1570
    dos1571-310654-05.bin          1571
    dos1581-318045-02.bin          1581
    dos1551-318008-01.bin          1551 (Plus/4)
    (the PET drives and the others: the rest of the VICE folder)
PLUS4EMU/                          (from the roms folder of Plus4Emu)
    p4kernal.rom                   KERNAL, PAL
    p4_ntsc.rom                    KERNAL, NTSC
    p4_basic.rom                   BASIC
    3plus1.rom                     3-plus-1
    dos1541.rom, dos1551.rom, dos1581.rom   drives
```

## Finding files

In any file list, type the first letters of a name: the list narrows as you type, and ENTER takes you to the first one. The ? and * wildcards work too.

## Updating

Every release has two archives:
- the full one, such as `BMC64-NG V1.4.7z`, for a new card;
- the update, such as `BMC64-NG Update V1.4.7z`, with only the files that changed since the previous release. Copy its whole content onto your card and replace the files that are already there. If you skipped a release, apply its update first, or start again from the full archive.

The update never contains `config.txt`, `cmdline.txt`, `machines.txt`, `wpa_supplicant.conf` or the `settings*.txt` files: they hold your own settings. Replace one of them only when the release notes say so. Neither archive contains ROMs, and the ones already on your card stay where they are.

The first line of About tells which version is on the card.

## Guides

The PDF guides in [docs/](docs/) describe every menu and every shortcut, in English and Italian. Reading them is highly recommended. The shortcuts alone are also in [Shortcut Keys](docs/Shortcut-Keys.md) and [Tasti rapidi](docs/Tasti-rapidi.md).

## Community

BMC64-NG has its own section on the BMC64 Discord server, for discussion and help: [join it here](https://discord.gg/hKvxqMj9J6).

Report bugs and request features on the [issues page](https://github.com/lroby74/BMC64-NG/issues) of this repository, not on the tracker of the original BMC64.

## All features

BMC64-NG runs the whole of VICE 3.10, not a simplified rewrite, and it puts fidelity first: if the real Commodore did something strange, BMC64-NG does it too. Keys are given where they help; the full list is in [Shortcut Keys](docs/Shortcut-Keys.md).

### Machines

- **Cycle-exact or standard C64.** The cycle-exact C64 reproduces the VIC-II cycle by cycle, as demos and games that push the chip need, and keeps up even on a Pi 4 at stock frequency. In the Switch menu the two C64s are called X64SC (cycle exact) and Std (the standard VICE C64, lighter). The Engine line of About tells which one is running, VICE 3.10 x64sc or VICE 3.10 x64: it comes from the emulator inside the kernel that started, not from the menu.
- **C64 GS console.** Pick the cartridge-only console in the C64 Model... menu. As on the real console, only the menu, reset, restart, shutdown, cartridge and status bar keys work, and the others say NOT ON C64 GS.
- **C64 with SuperCPU.** It emulates the CMD accelerator at 20 MHz, with the free ROM that comes with VICE.
- **C128 with both screens.** It has the 40-column VIC-II screen and the 80-column VDC screen, on one monitor or two.
- **Plus/4 with two engines.** Choose the VICE Plus/4 or Plus4Emu, which is considered more faithful.
- **The right NTSC KERNAL.** The VIC-20 NTSC modes start the American KERNAL (901486-06), which centres the screen as on a real NTSC VIC-20, and the PAL modes start the European one (901486-07). The Plus/4 and the C16 in NTSC start the NTSC KERNAL (318005-05) too. If you choose a different ROM in the menu, it stays.
- **The model you are running.** On the PET and the Plus/4 the model entry of the menu says what is running, for example Model: 8032 or Model: Plus/4 (PAL).
- **The PetSpeed key.** In the PET Machine menu, PetSpeed dongle puts on the tape port the key that the PetSpeed compiler wants in order to start. As on the real thing the key takes the port: while it is there, the tape recorder is not.
- **PET demos that reprogram the screen.** A `.prg` started with Autostart Prg... finds memory as after a real LOAD from the drive, so the *No Pets Allowed* demo on the 4032 runs to the end. If a program leaves the CRTC without vertical sync the machine does not stop: like the real monitor the screen keeps redrawing and the menu answers. If the frame gets shorter the picture keeps its size and the emulation its full speed, and if a program reprograms the CRTC the picture stays in the middle of the screen, also after a reset.
- **The later VICE fixes.** Besides VICE 3.10, BMC64-NG has the fidelity fixes the VICE developers made after it, up to 9 November 2026: among them the CIA serial register, the VIA lines of the drives, the VIC-20 and the PET after Load Snapshot, the unused port pins of the 1571 that the Super Snapshot 5 and Action Replay 5 fast loaders look at, the C128 VDC and its custom C64 KERNAL, two TED registers, the VIC-20 joystick and keyboard, the PET's original 80-column editor, a drive ROM chosen from the menu that reaches the running drive at once, and, on the C64C models of the cycle-exact C64, the right dots on a sprite that changes from multicolor to high resolution in the middle of a line.

### Video and screens

- **Adjustable scanlines.** Set them from 0 to 100 for the look of a tube monitor; ALT + SHIFT + "+" and ALT + SHIFT + "−" change them on the fly.
- **Aspect ratio on custom modes.** On the 768x545 and 768x525 modes, which run at the machine's exact frequency, BMC64-NG reads the TV's real shape at power-on and assumes 16:9 if the TV doesn't say. 16:9 then fills the screen and 4:3 comes out really 4:3, as at 720p.
- **Picture adjustments.** Set brightness, contrast, colour and gamma, and the geometry: width, height and centring.
- **720p or 1080p.** Video > HDMI Resolution chooses between the two for the running machine, PAL or NTSC as it is now, and the menu asks to restart. In the Switch menu every machine has both entries next to each other. A new card made with the full archive starts at 1080p.
- **CRT shader presets.** The CRT Shader menu adjusts the shader, and Save as preset... keeps the adjustments in a `.crt` file of the `/crt` folder, with the name you type. If you choose another preset with changes not saved, the menu asks what to do with them. Save settings remembers the preset, or the adjustments changed by hand.
- **No slowdown past the edges.** When H Center, V Center or a wider picture push the image a little past the screen edges, the part outside is simply cut and the emulation doesn't slow down.
- **The CRT shader at full speed.** On the Pi 4 and 400 at 1080p the GPU draws a 720p frame and the scaler brings it to 1080p with a soft filter, so the shader effects no longer halve the speed. From the Pi 4 on, if the GPU cannot draw a frame in time (many effects on together), it draws a smaller one, just small enough to fit, and the machine stays at 100% with a slightly softer picture; on the Pi 5 and 500 that frame is only narrower.
- **The CRT shader with the picture moved.** If H Center or V Center take part of the picture off the screen (on the VIC-20, to hide a border), the shader stays on and works on the part you see.
- **C128 40/80 columns.** The VIC-II and VDC screens are two separate outputs, each with its own adjustments. ALT + PRTSCN switches between them like the little switch on the monitors, and ALT + SHIFT + PRTSCN or ALT + F7 is the 40/80 DISPLAY key of the real keyboard.
- **C128 on two monitors.** Set Second Display Present to On before choosing Video > Active Display > Two HDMI; the first time, the menu asks to restart. Each screen then fills its own TV in 4:3 or 16:9, and ALT + SHIFT + \\ switches the shape of the other screen.
- **Main HDMI Output.** The HDMI audio follows the main screen to the port you choose in Video > Main HDMI Output; the menu asks to restart, and the choice holds from then on. On the C128 with two monitors it swaps the two screens.
- **VGA Monitor On.** Video > VGA Monitor On tells which port the VGA monitor and its adapter are on: None (the default), HDMI 0 or HDMI 1. From then on the Switch entries with VGA in their name send the picture to the VGA monitor and the others to the HDMI monitor, in one step and one restart, and C= + F7 held for 5 seconds goes back to the HDMI monitor. With None the Switch leaves the port alone. On the C128 with two monitors the Switch entry decides where the VIC-II goes, and the VDC goes on the other.
- **Screenshots to the card.** SUPER + PRTSCN (the Windows or raspberry logo key, together with PRTSCN), or Take screenshot in Prefs > Screenshots, saves a picture in the `/screenshots` folder of the card. Capture point says which image is taken: Emulator, the Commodore screen as it is; Scaled, already enlarged; CRT, the default, with the CRT shader; Screen, what reaches the monitor. Size and Format (PNG or PPM) are in the same menu. The machine stops for the time it takes to write to the card, then it goes on from where it was.
- **Composite video on the Pi 4 B.** Every machine's Switch menu also has PAL Composite 576p@50Hz and NTSC Composite 480p@60Hz: video and sound come out of the 3.5 mm jack (4-pole AV cable) with the real machine's signal, 50 or 60 Hz, and the GPU's scaler composes the picture in hardware, interpolation included. The status bars are drawn for the composite lines, and C= + F7 held for 5 seconds goes back to HDMI.

### Sound and SID

- **reSID and reSIDfp.** Sound > SID Engine offers ReSIDfp next to ReSid, which stays the default, and both emulate the 6581 of the first C64s and the 8580 of the later ones. ReSIDfp is VICE's newest and most faithful SID engine: its combined waveforms come from measurements of real chips, its 6581 filter has the chip's distortion, and it needs a little more computing power.
- **reSIDfp settings.** The reSIDfp Settings folder sets Combined Waveforms (Weak, Average, Strong), Filter Curve, Filter Range and Old Caps (2200pF) for the 6581, and Filter Curve for the 8580. The sound changes when you close the menu, and Save settings keeps it all.
- **6581 profiles by composer.** reSIDfp Settings > 6581 > Composer Profile... gives the 6581 filter the Filter Range of the chip a composer wrote on: Rob Hubbard, Martin Galway, Jeroen Tel and others. The profile in use has (*) next to it, None puts back the default values, and Save settings keeps it.
- **Optional background hiss.** reSIDfp Settings > Background Noise adds the faint hiss heard in recordings of real C64s, from OFF (the default) to 100% in steps of 10%. You hear it mostly when nothing is playing, for example at the BASIC prompt. Each SID has its own hiss, independent of the others, so with two, three or four SIDs they add up as they would with that many real chips.
- **reSID filter settings.** Sound > reSID Filter Settings has Passband % (default 90), Gain % (default 97) and Filter Bias (mV) for the 6581 and for the 8580, applied to every SID of that model. Passband and Gain count only when SID Resampling is set to Resampling or Fast Resampling.
- **SID resampling.** Sound > SID Resampling defaults to Resampling, the most faithful mode, without the noise the other modes add to high notes. Fast Resampling sounds the same but uses more memory and takes longer to get ready; Interpolation and Fast cost the Pi less and are less faithful.
- **Plus/4 and PET SID Card.** These machines have no SID, and as back then you add one on a cartridge. Sound > SID Card (off by default) sets the model (6581 or 8580), the address (`$FD40` or `$FE80` on the Plus/4, `$8F00` or `$E900` on the PET), the filter and the SID Card Engine (ReSid or ReSIDfp).
- **8580 + Digi Boost.** SID Model, SID2 Model and SID Card Model also offer 8580 + Digi Boost: the 8580 with the modification that makes digitized samples audible, which on a plain 8580 can barely be heard. It works with reSID and reSIDfp.
- **Stereo with multiple SIDs.** With two SIDs the first plays on the left and the second on the right; with three, the first plays left, the third right and the second in the centre. Sound > Channels switches between Stereo, the default, and Mono on the C64, C128 and SuperCPU.
- **SIDs on more cores.** With two or three SIDs, and in the player up to eight, core 2 always helps the emulator's core, and core 3 does too whenever the screen leaves it free; with four SIDs core 2 computes the second and the fourth. The sound is identical, and reSIDfp, which needs more computing, has the headroom it needs.
- **USB DAC at any time.** With Sound > Audio out set to USB DAC, the sound moves to the DAC as soon as you plug it in and back to HDMI when you unplug it, and DACs that only work at 48 kHz work too, on every machine including Plus4Emu. If a DAC drops out for a moment, as some cheap ones do, the sound waits three seconds before going back to HDMI and carries on if the DAC returns.
- **DACs and headphones with more channels.** Mono sound comes out of the two front channels on devices with 4, 6 or 8 channels too, like 7.1 headphones.
- **Volume without distortion.** ALT + + and ALT + − set the volume and ALT + M mutes, with + and − on the number row or the keypad. 100% is the sound as it is, with no amplification, and at the limits the status bar says VOLUME 100% MAX or MUTE.

### SID player

- **Built-in .SID player.** Copy your .SID files into the `/sids` folder on the card, then press ALT + S to browse and play them, including tunes for two and three SIDs. ALT + SHIFT + S stops the tune and resets.
- **Four-SID tunes.** The player also plays 4-SID files (PSID v4E, the SID-Wizard ones) with 12 VU bars. Under each group it shows the SID number, model and side, for example "1 8580 L".
- **Eight-SID tunes.** Files with up to eight SIDs play too, such as The Tuneful Eight, four 2-SID tunes written to play together on eight SIDs: 24 VU bars half as wide, with the SID number, side and model under each group, odd SIDs on the left and even ones on the right at the start. F1 to F8 change the side of each SID and ALT + 1 to ALT + 8 turn whole SIDs off and on; single voices can't be muted. These tunes start on reSID, in Fast mode on the Pi 4 and 400, with the SIDs split among the cores; S switches them to reSIDfp, which with eight SIDs on the Pi 4 needs CPU Clock at +20% (at the stock clock stay on reSID). Leaving the player brings back the engine you chose.
- **The model of each SID.** With tunes for more SIDs each SID plays with the model written in the file, up to the eighth; if the file does not say, with the model of the first SID.
- **reSID or reSIDfp while the tune plays.** In the player the S key (without ALT) switches between reSID and reSIDfp, and the engine name shows for a second in the volume line. The choice stays for the other tunes until you leave the player.
- **Tune controls.** The , and . keys go to the previous and next tune, P pauses and resumes, and M mutes, with or without ALT. The up and down arrows, or ALT + + and ALT + −, set the volume. The right and left arrows jump 10 seconds forward or back; press them again to jump further (three presses make 30 seconds).
- **Mute voices or whole SIDs.** Keys 1 to 3 turn the voices of the first SID off and on, 4 to 6 those of the second, 7 to 9 the third, and 0, − and + the fourth; ALT + 1 to ALT + 4 do the same for whole SIDs. Leaving the player turns every voice back on.
- **Left, centre or right.** F1 to F4 move the first to fourth SID to the next side with each press (L, C, R, then L again), and a notice shows the change. The sides hold for all tunes with the same number of SIDs until you leave the player; with Sound > Channels set to Mono there are no sides, and F1-F4 say CHANNELS: MONO.
- **Time, rate and volume.** The third line shows the music time, which stops on pause and restarts at zero with each tune, and the play rate: PAL 50Hz, NTSC 60Hz, or CIA with the value the tune put in the timer. When you change the volume, a horizontal segmented bar shows it for a second under the SID labels, with the percentage on its right and MUTE in red at zero.
- **Show or hide.** V, T and N show and hide the VU meters, the tune time and the lines with the tune name.
- **Aspect ratio in the player.** The \\ key switches between 4:3 and 16:9 without ALT; on an Italian keyboard both the \\ key next to 1 and the < > key next to the left SHIFT do it. Outside the player, \\ without ALT stays a C64 key.
- **Player screen and keys.** Inside the player the status bar stays hidden so it doesn't cover the bars, and ALT + W does nothing, since a tune can't be heard at double speed. Other ALT keys that don't apply show NOT IN SID PLAYER.

### Disks, tapes and cartridges

- **Up to four drives.** Drives 8, 9, 10 and 11 work at the same time, with fifteen disk image formats: `.d64` `.d67` `.d71` `.d80` `.d81` `.d82` `.d1m` `.d2m` `.d4m` `.g64` `.g71` `.g41` `.p64` `.x64` `.dhd`. `.nib` images (the low-level copies made with nibtools, protected disks included) are attached and started like the others: BMC64-NG converts them to `.g64` in the `/nib-g64` folder with the nibconv code of nibtools (not in Plus4Emu, which does not read `.g64` files). ALT + 8 and ALT + 9 attach a disk to drive 8 or 9, and with SHIFT they detach it.
- **A model for each drive.** Each drive can be a 1541, 1541-II, 1570, 1571, 1581 or none, and the Plus/4 adds the 1551. ALT and a number key set the model of drive 8, and ALT + SHIFT and a number key that of drive 9.
- **The sound of the disk going in and out.** With Prefs > Drive sound emulation on, attaching and detaching an image makes the sound of the disk going into and out of the real drive, along with the motor and the head. These sounds come from the Ultimate-II+; the 1581 has its own set, quieter, as a 3.5" drive is.
- **True 1541 emulation.** The drive runs its own processor and program, like the original, so fast loaders and the copy protections of vintage games work.
- **Drives that talk to each other.** With two or more drives on, programs that make two drives talk on the serial cable without going through the computer work: before touching the cable a drive brings the others up to its time, as the computer does with the drives. With one drive nothing changes; with two or more drives on, the Raspberry does a little more computing.
- **Card folders in Plus4Emu.** In Plus4Emu, turn on IEC FileSystem for Drive 8 or Drive 9 to read the files in the card's folder; the choice is saved.
- **Blank disks.** Create new, empty disk images from the menu.
- **The disk at every power-on.** Drives > Default disk chooses an image and a unit (8, 9, 10 or 11). At every power-on that image is attached by itself, if the unit is empty, and if the unit is off it is switched on by itself. By default there is no image.
- **Tapes with recorder controls.** `.tap` and `.t64` tapes come with a tape counter and recorder keys: ALT + ↑ is PLAY, ALT + ↓ is STOP, ALT + → fast-forwards and ALT + ← rewinds, twice as fast with SHIFT. ALT + DEL resets the counter.
- **Cartridges at power-on too.** Insert `.crt` and `.bin` cartridges with ALT + C and remove them with ALT + SHIFT + C. On the C64 and C128, Cartridge > Set current cart default makes the inserted cartridge start at every power-on and saves by itself; choose it again after removing the cartridge to undo it.
- **EasyFlash saves.** What a game writes into the flash goes back into its `.crt` file, with Cartridge > Save EasyFlash Now or by itself when you remove or change the cartridge. It also saves when you power off, restart or switch machine, if the game wrote to it.
- **Magic Desk Plus saves.** On the C64 and C128, what the program writes to the cartridge's SRAM and EEPROM stays in two files next to the `.crt`, with the same name and the extensions `.srm` and `.eep`. Cartridge > Save Magic Desk Plus Now writes them at once, and Auto-save Magic Desk Plus, on by default, writes them by itself when the cartridge is detached or another one is attached, and also when you power off, restart or switch machine.
- **Programs and autostart.** Load `.prg` programs directly, or pick a PRG or disk file with ALT + A and it starts by itself. With true drive emulation, VICE puts the `.prg` on a disk it writes in the card's root, such as `autostart-PLUS4.d64`; you can delete it, and it comes back next time. On the PET, Autostart Prg... starts `.prg` files.
- **PRG loading.** The PRG loading item of the main menu says how a `.prg` starts with autostart: Via disk image, the default, or Direct to RAM, which always puts it straight into RAM, even with the drive on.
- **Long file names.** Disks, tapes, cartridges and programs with names up to 255 characters, the most the card allows, attach and start from the menu like any other file.
- **Search in file lists.** Type the first letters: already as you type, with no ENTER, only the files and folders that start that way remain, and ENTER takes you to the first one. Case doesn't matter, ? stands for any single character, and `*lair` finds names that contain "lair". If no name starts that way, the search looks inside the names, so `12` finds `DrL-12-850.reu`.
- **REU memory expansion.** The REU goes from 512 KB to 16 MB: ALT + E turns it on and off and ALT + SHIFT + E changes its size. Games and demos that use it load their .REU image from the `/REU` folder with ALT + 0, and ALT + SHIFT + 0 detaches the file while the REU stays on.
- **REU contents saved.** With Ram Expansion Unit (REU) > Ram Image (optional) > Auto-save image turned on (it is off by default), BMC64-NG rewrites the .REU file when you power off, restart or switch machine. Pulling the plug doesn't save, and for game images such as Dragon's Lair it's best left off.

### Real hardware and USB

- **Real Commodore drives.** An XUM1541 adapter connects a real 1541, 1571 or 1581 so you can read your original disks (ALT + 7 for drive 8, ALT + SHIFT + 7 for drive 9). Several drives can be daisy-chained on different device numbers, and a Pi1541, which emulates a 1541 and a 1581, works too.
- **XUM1541 on an Arduino Pro Micro.** An XUM1541 built on an Arduino Pro Micro may not be compatible: with it the real Commodore drive works only after the first power-on from cold, and after a machine change or a restart of the Raspberry it stops answering. BMC64-NG is tested with a ZoomFloppy.
- **Programs with their own fast loader on a real drive.** Many games and copiers put a loader of their own into the drive, which talks to the computer on wires the XUM1541 adapter does not carry: through the adapter they cannot work, as with VICE on a PC. The machine does not stop: the drive finishes the file by itself (FAST STOP on the status bar) and Reset puts everything back, with the Pi1541 disk still attached.
- **Drive on XUM1541: Pi1541, Commodore or both.** In the Drives menu you say what is on the XUM1541 adapter. With Commodore, for a real 1541 or another Commodore drive, the drive gets the line reset as on a real Commodore: at power-on, at every machine switch and at every reset, so a drive left stuck by the previous machine starts again by itself. With Pi1541 + Commodore you keep a Commodore drive and a Pi1541 on the same cable, each with its own number: resets work as with the Pi1541, and at power-on the line is pulled only if a drive was left stuck. With Pi1541, the default, resets do not touch the line, so the Pi1541 keeps its disk. It is one choice for all machines, kept by itself without Save settings.
- **Directory of Drive on XUM1541.** In the Drives menu it shows the directory of the disk in each drive on the XUM1541 adapter, one drive after the other, read straight from the disk sectors as copy programs do: every entry, also the ones the BASIC LOAD"$" does not show (some disks, like the Super Kit 1541, have characters in their names that stop LIST halfway, on a real C64 too). It reads 1541, 1570, 1571 and 1581 disks.
- **Reset Drive on XUM1541.** In the Drives menu it puts the adapter back on its feet and restarts every drive on it, and the page says how it went, drive by drive. A Pi1541 restarts with the UJ command and keeps its image. A Commodore drive gets the line reset, held until the drive takes it (up to 3 seconds): Bus reset taken, N ms says it restarted; Bus reset not taken, 3 s that the reset wire does not reach it (the cable, or the reset input of the drive), and the UJ command is tried; does not answer that it must be switched off and on.
- **The fast loader with two drives on the cable.** The BMC64-NG fast loader uses a wire that every drive switched on hears, so it works only with one drive alone on the cable. With another drive switched on the file loads at normal speed, and the status bar says so, for example FAST OFF: DRIVE 9 ON BUS TOO.
- **Real Drive: Try the fast loader.** In the Drives menu it loads the first file of the disk twice, with the BMC64-NG fast loader and the usual way, and says how fast each one is and whether the bytes are the same (a 1541 alone on the cable).
- **The real drive without the drive ROMs.** Change Model... of a unit has the Real drive entry, like ALT + 7, even when the card has no drive ROMs.
- **The XUM that arrives late.** With the real drive saved, at power-on and after a machine change the XUM1541 is waited for up to 12 seconds, then the unit goes back to the emulated drive and NO XUM FOUND appears. An XUM built on a Pro Micro takes about 10 seconds to show up, a ZoomFloppy 3.
- **USB devices after a restart.** A device that does not make itself known after a restart or a machine switch is tried again by itself: up to three times on the Pi 5 and 500, up to four on the Pi 4 and 400. At startup the USB waits for the devices as long as BMC64 5.4.5 does, a little less than before.
- **USB mice that declare the buttons before the pointer** (for example the Microsoft 045e:0737): the buttons work and the pointer moves right, and a mouse with an unusual descriptor no longer stops the Raspberry.
- **A USB device unplugged while the Raspberry was sending it data** (for example the Caps Lock LED to a keyboard) no longer stops the Raspberry.
- **USB Off at Restart.** On the Pi 4 and 400, if a USB device does not answer after a restart or a machine switch until you unplug it, this entry of the main menu switches all the USB sockets off before every restart made from the menu, for 1.0 to 5.0 seconds, like unplugging the devices. Off by default; ENTER asks to restart, and that restart already uses the new choice.
- **USB and GPIO joysticks.** USB joysticks and gamepads work on up to four ports, ten with the V8 adapter, and so do vintage joysticks through an adapter on the Pi's pins. ALT + J swaps the two joystick ports.
- **Gamepad buttons.** Joyports > Configure USB Gamepad 1... (and 2, 3, 4): Mapping Mode on Automatic, the default, uses the right profile for the pads BMC64-NG knows and a generic one for the others. On Custom the assignments are yours and they stay: Assignments by Button... gives a function to each button, Assignments by Function... a button to each function, and Clear a Button... removes an assignment.
- **Gamepad POTs.** On the same page POT X and POT Y have their Up and Down values, and the computer really reads these numbers: the Up ones at rest, the Down ones with the button pressed. With Hat+POTX/Y Paddles the analog axes of the gamepad reach the SID as paddles.
- **Autofire.** Right below Joyports, the Autofire menu has Port 1 and Port 2 (one port on the VIC-20): Off, the default, Slow or Fast. With Slow and Fast, while you hold the fire button down, the port fires by itself about 7 or 14 times a second, whatever joystick is on it. It is kept with Save settings.
- **Userport joystick adapters.** Joyports > Userport Joystick Adapter adds joystick ports on the userport, with the adapter types VICE 3.10 has for each machine: CGA, PET, OEM, HIT, Kingsoft, Starbyte and WOJ on the C64 and C128; CGA, PET, OEM and WOJ on the VIC-20 and the PET; Synergy and WOJ on the Plus/4 with VICE. The V8 adapter (C64, SuperCPU, C128, VIC-20 and PET) has eight joysticks, for the programs written for it: Port 3 and Port 4 are the first two, and the V8 Ports 5-10 folder has the others. The type you save comes back at the next start.
- **Mouse.** In Joyports, set Port 1 or Port 2 to USB Mouse. Mouse > Type says which mouse it is: the Commodore 1351 by default, then NEOS, Amiga, Atari CX-22, Atari ST, SmartMouse, Micromys and the Commodore 1350, which the computer reads as a joystick. Sensitivity (%) sets how much it moves, from 10 to 200.
- **Spinner as a paddle.** On the C64, C128 and VIC-20, Joyports > Port 1 or Port 2 > USB Spinner (Paddle) turns the wheel of a USB mouse into a paddle, as the MiSTer does. A spinner that the Pi sees as a mouse with a wheel works too, such as the one on the TAITO EGRET II mini: each click moves the paddle one step, from 0 to 255, and any mouse button is the paddle button. Spinner Sensitivity, in the same menu, sets how far each click moves it, from 25% to 400%.

### Keyboard and controls

- **Keyboard maps.** There are positional and symbolic keyboard maps, and US, IT, UK and DE layouts.
- **Your own keyboard map.** In Keyboard, set Mapping to Custom and open Mapping Editor...: one row for every key of the machine (C64, C128, VIC-20, SuperCPU, PET and Plus/4 with VICE), and ENTER and then the PC key to give it. Save & Apply saves it and uses it at once, and Restore Defaults... goes back to the Positional one.
- **Caps Lock.** The Caps Lock of a USB keyboard is SHIFT LOCK on every machine and with every mapping, as in VICE, also together with a real C64 keyboard on a Keyrah. On the C128 the CAPS LOCK key stays on ALT + F4.
- **The C128's extra keys.** ESC, TAB, ALT, CAPS LOCK, HELP, LINE FEED, 40/80 DISPLAY and NO SCROLL are on ALT + F1 to ALT + F8, in the order of the real machine. Seven also have a key of their own: F9 ESC, F10 TAB, F11 40/80 DISPLAY, End HELP, Page Down LINE FEED, Scroll Lock NO SCROLL, Pause CAPS LOCK.
- **RESTORE key.** The RESTORE key is there, the one that triggers the non-maskable interrupt on the real C64.
- **Keyboard as joystick.** Keyset turns keys you choose into a joystick. PotX and PotY are the second and third button, for the games that use them, as on USB gamepads.
- **Restart and shutdown from the real C64 keyboard.** In the Keyboard menu, the C= + F1…F7 and CTRL + F1…F7 hotkeys can be Reboot BMC64 or Power Off BMC64, for the C64 keyboard on a Keyrah, which has neither ALT nor a power key. The hotkey asks REBOOT? or POWER OFF? at the bottom: the same hotkey again within five seconds confirms, ESC (RUN/STOP on the C64 keyboard) cancels.
- **More hotkey choices.** The same hotkeys can also be Attach Tape, Attach Cart, Attach Drive 8, 9, 10 or 11 (the file list while the machine runs), Screenshot, or SID OSD: a box with the SID entries of the Sound menu, changed with the arrow keys while the music plays.

### Menu and usability

- **One-key menu.** F12 opens and closes the menu; the arrow keys move, ENTER chooses, and a letter jumps to the next entry that starts with it (S for Sound). ALT + ↑ and ALT + ↓ jump by blocks, like PgUp and PgDn, on the Pi 400, 500 and 500+ keyboards and on USB keyboards. Save settings keeps your setup for the next start.
- **Back where you left it.** F12 opens the menu again on the last item you touched, with the folders open as you left them.
- **Quick Access.** SPACE on a switch, a choice, a button or a number of the main menu puts it in one of the five slots of Quick Access..., at the top of the menu, and ENTER on a slot takes you to its entry, opening the folders. ESC in the main menu goes to Quick Access..., and a second ESC closes the menu.
- **Mouse in the menu.** Inside the menu the pointer picks the row, the left button is ENTER and the right one goes back; the wheel scrolls the list and changes the numbers. With the menu closed the mouse belongs to the Commodore. Prefs > Mouse in Menu turns it off.
- **Menu size.** Prefs > Menu Scale % sets the size of the menu, from 75 to 115%, and Row Gap px the space between the rows.
- **Inside images and text files.** In a file list, ← goes to the folder above and → enters the one under the cursor; → on a disk image shows its programs, and ENTER starts one. ENTER on a text file shows it, up to 8 KB.
- **Long names scroll.** A name too long for its line scrolls while the cursor is on it, in the menus and in the file lists.
- **Shortcut list in the menu.** The Shortcut Keys entry shows the list of shortcuts, always up to date.
- **Status bar.** It shows drives 8 and 9 with a green power LED and a red activity LED, the current track including half tracks, and the tape counter; ALT + O hides it and brings it back. The CPU temperature and the CPU and GPU clocks show at the top of the menu as well as on the second line (ALT + SHIFT + O).
- **The resolution at the top of the menu.** The first line of the main menu ends with the resolution going out at that moment, for example C64 PAL 50Hz HDMI 1080p.
- **Date and time.** The top of the main menu shows the date and time from the network (NTP), with the time zone and daylight saving of the Network menu; files seen through SD Card Sharing get the same time.
- **The time server from the menu.** Network > NTP has Enable NTP, on by default, Server 1 (pool.ntp.org), Server 2, the spare one, and Sync interval. Save & Apply works at once, without a restart, and Sync now asks for the time now.
- **Files without a network.** Until the time arrives, the files the Pi writes on the card take the most recent date among the files in the root of the card. Before, Windows showed them as 1601.
- **The lights of each drive model.** Each drive shows the lights of its real model: two round lights on the 1541, 1541-II and 1570 (green power on the left, red activity on the right), two wide bars on the 1571 with the red power light on the left, and two bars on the 1581 with the red one on the left, switched by the drive's DOS. The track shows with its half track (18.0, 18.5).
- **Warp speed.** ALT + W runs the machine as fast as the Pi allows, to skip long loading times.
- **Machine snapshots.** Save the exact state of the machine and pick it up later.
- **Hard and soft reset.** ALT + R is a hard reset that clears memory, the one that restarts a game; ALT + SHIFT + R is a soft reset that keeps memory, handy for changing or studying code in memory.
- **Pi restart and shutdown.** ALT + ESC restarts the Pi and ALT + SHIFT + ESC shuts it down, each after a second press to confirm; ESC alone cancels. On the Pi 500 and 500+ the power button works too.
- **Overclock and underclock.** CPU Clock... sets the CPU from -20% to +20% of the stock frequency of that Raspberry, and GPU Clock... raises the GPU by 10% or 20%, with frequencies that have already been tested. The choice belongs to the Pi it was made on: the same card in another Pi starts at that Pi's frequencies.
- **Overclocking...** For those who want to go beyond CPU Clock and GPU Clock, this item of the main menu has Voltage Offset, Temperature Limit, Core Clock and V3D Clock. Apply & Reboot writes them to config.txt and restarts, and Restore Defaults sets Auto again. `force_turbo` is never written from here.
- **The warranty bit.** The firmware sets it for good when it finds `force_turbo=1` together with a raised voltage, and BMC64-NG never writes the two together.
- **The fan.** A fan on the Pi 5 connector, or on pin 8 (GPIO 14) of the Pi 4, is set with the Fan item of the main menu: Auto, the default, works like Raspberry Pi OS, or by hand from 0% to 100%. With `gpiofanpin=` in cmdline.txt, a fan on another pin switches on and off by itself with the temperature.
- **The version.** The first line of About tells which version is on the card.

### Network

- **Every machine, from the Pi 4 on.** Cable or Wi-Fi on all machines, on the Pi 4, 400, 5 and 500 (Network > Network Device). On the Pi 5 and 500 the Wi-Fi uses the three `brcmfmac43455-sdio.raspberrypi,5-model-b` files in `/firmware`, already on the card. The Pi 400 has a different Wi-Fi chip and uses the three `brcmfmac43456-sdio` files, also in `/firmware`; if the chip cannot start, the WiFi SSID window says why and the machine keeps running.
- **Wired network on the Pi 5 of revision 1.7.** These boards have a different Ethernet chip (MaxLinear MXL86110 instead of the Broadcom BCM54213PE): BMC64-NG sets up each one in its own way, and the PHY ID line of NET-LOG.TXT tells which chip is there.
- **Wi-Fi from the menu.** Network > WiFi Settings > WiFi SSID scans without restarting and lists the networks with band (MHz), channel (CH) and signal (RSSI). Set WiFi Country Code to your country (IT for Italy), then Enter Password & Reboot: Save & Reboot saves, switches to Wi-Fi and restarts.
- **DHCP or a static address.** IP Address Mode (reboot) is DHCP by default, and the Static IP, Netmask, Gateway and DNS fields show what the router gave; set it to Static to type your own. The Pi shows up on the router with a name that tells the model: BMC64-NG-4, BMC64-NG-400, BMC64-NG-5, BMC64-NG-500, BMC64-NG-500P (Pi 500+), BMC64-NG-CM4 or BMC64-NG-CM5, so two of them on the same network each have their own.
- **Daylight saving time.** Daylight Saving (reboot), under Timezone (reboot), is Off, On or Auto (EU), which follows the European rules by itself.
- **The card from your PC.** SD Card Sharing... in the main menu opens the microSD to your home PC without taking it out of the Pi. In Windows File Explorer type `\\BMC64-NG-400\sdcard`, with the name of your Pi, or `\\` and the address shown in the box; the user is bmc64 and the password bmc64 (Network > Sharing Password changes it). The emulation pauses while the card is shared, attached disks are saved first, and the files the machine is using can't be deleted or overwritten. While sharing is on, the Pi also shows up with its name in the Network folder of File Explorer. If a read or a write from the PC fails, the exact reason goes into SMB-LOG.TXT on the card. If a transfer from the card stops half-way, the card reader puts itself back in order and does it again, as Linux does, and SMB-LOG.TXT says so. Stop Sharing ends it.
- **A modem like the Ultimate II+.** On the C64 and C128, Network > Modem (SwiftLink), off by default because it takes `$DE00` (leave it off with an EasyFlash), is a SwiftLink at `DE00`, where CCGMS expects it. Dial a BBS with `ATDT` and the address with its port, for example `ATDT bbs.retrocampus.com:6510`; without a port, port 80 is called. Incoming calls arrive on port 3000: the modem rings every 3 seconds, up to 10 times, ATA answers, and ATS0=1 answers by itself. Commands, responses and registers are those of the Ultimate II+, and so is the line: DSR is on while a call is connected, and DCD is always on.
- **A modem on the Plus/4 and the VIC-20.** On the Plus/4 the modem is the built-in ACIA at `$FD00`, as in the original BMC64: always there with VICE, and with Plus4Emu when Network is chosen, only on the models that have the ACIA (Plus/4 and V364). On the VIC-20 it sits on the userport, as device 2 of the KERNAL: Network > Modem (Userport), off by default, and Modem Baud (300, 1200 or 2400; 300 by default), the same as in the terminal program. The userport holds one device, as on the real machine: switching the modem on switches the userport joystick adapter off, and the other way round.
- **The WiC64.** On the C64, with HTTPS too: the WiFi module on the userport, for the programs written for it. Network > WiC64 has Enabled, Default server, Time zone, Remote timeout and Network reporting. It uses the Pi's network, which must be on. The userport holds one thing only, as on the real machine.
- **Games from a NAS, as on the MiSTer.** The NAS item of the main menu takes the NAS address, user name and password (empty for a guest) and the SMB version (Auto, or 2.0 to 3.1.1). NAS Path holds the share and the folder the browser starts from, one for each machine; like the other fields it is empty until you type it, for example `Mister\games\C64` on a MiSTer NAS. In the file browser the first line chooses the volume: SD, USB or NAS. Disks, tapes, cartridges and programs work as from the card, up to 32 MB; what a game saves goes back to the NAS when the drive light goes off, or to the card's `NAS-NON-SALVATI` folder if the NAS doesn't answer.
- **The network log.** Each time you open the menu, if something new happened, the card writes in NET-LOG.TXT what the network did: when the cable lost the link and when it came back, how many seconds it stayed down, the address from the router and its renewals, the Wi-Fi. Nothing is written while you play.

### Safety and diagnostics

- **15-second safety timer.** After you change machine, resolution or refresh rate in the Switch menu, press any key within 15 seconds to keep it, or the previous one comes back by itself. Safety timer (15s), at the bottom of the Switch menu, turns this off after a confirmation, but then a mode your monitor can't show leaves the screen black until you fix `config.txt` on a PC.
- **HDMI fallback at power-on.** If the port chosen in Video > Main HDMI Output has no monitor or doesn't respond at power-on, the picture goes to the other port.
- **NOT SAVED warning.** If the .REU file can't be written, for example because it's read-only, the Pi stays on at power-off and shows NOT SAVED; asking again powers it off anyway.
- **NO XUM FOUND warning.** Without the XUM1541 adapter, ALT + 7 shows NO XUM FOUND and the drive stays as it was.
- **PASSI.TXT.** In this version the watchdog is off. PASSI.TXT on the card keeps the last things the machine started and finished (the start-up, the menu entries, the hotkeys). If the Raspberry freezes, after the restart that run is called PASSI-PRIMA.TXT: open an issue on [GitHub](https://github.com/lroby74/BMC64-NG/issues) and attach it with BMC64-DIAG.TXT.
- **Power in the report.** BMC64-DIAG.TXT also says whether the firmware has seen an under-voltage and which firmware is on the card. If the alimentazione line says sottotensione (under-voltage), the first suspect is the power supply or the cable.
- **USB-LOG.TXT on the Pi 4 and 400.** Every start also says whether it comes from a power-on or from a restart without switching off, and the version of the firmware of the USB chip (VIA VL805), so the logs of two Raspberry Pis can be compared.

## To do

- 15 kHz CRT monitors through VGA666, Pi2SCART, RGB-Pi 1 and RGB-Pi 2.
- The C64 DTV.

## Credits and license

BMC64-NG builds on the work of:
- BMC64 by Randy Rossi;
- BMX by kdre76 (baremetalx.de), for the KMS video, V3D and the CRT shaders, audio, the SuperCPU, the Wi-Fi fixes, the keyboard map editor, the mouse, the menu features, Quick Access, screenshots, the WiC64, Mapping Mode, Overclocking and the attach, screenshot and SID OSD hotkey choices;
- VICE by the VICE Team;
- Circle by Rene Stange, and circle-stdlib by Stephan Mühlstrasser;
- Plus4Emu by Istvan Varga;
- reSID by Dag Lem, and reSIDfp by Antti S. Lankila and Leandro Nini;
- **[Claude Code](https://claude.com/claude-code) (Max plan), used inside VS Code**, which wrote the code of BMC64-NG.

### On the use of AI

The code of BMC64-NG was written with Claude Code, and it could not have been done without it. That is stated here first, so that nobody has to go looking for it.

What matters is not who typed it, but what was checked. Every change was tested before it reached a release: the real kernels running in QEMU, the emulator code built and run on a PC, Windows as the real client of SD Card Sharing, and then real Raspberry Pi 4, 400, 5 and 500 hardware with real monitors, drives and joysticks. Ideas that looked right on paper were measured and dropped: the GPU underclock was removed after it halved the speed, so GPU Clock now only goes up; and where the hardware sets a limit, the guides say so, such as reSIDfp with eight SIDs on a Pi 4 at the stock clock.

The two objections this kind of note usually gets deserve an answer.

*"We tried AI and the results were a disaster."* — The difference is not the model, it is the environment. In a chat window you paste a file in and get a file out that nobody runs. Here the assistant had a shell: it built the kernels, ran them on test benches and got contradicted by them, again and again, before anything went on a card.

*"AI code is unmaintainable."* — Judge it by what it does: it runs the whole of VICE 3.10 on bare metal, and every release is tested on the hardware before it is published. The [issues page](https://github.com/lroby74/BMC64-NG/issues) is open.

BMC64-NG is licensed under the GNU General Public License v3 ([LICENSE](LICENSE)). VICE, in `third_party/vice-3.10`, is licensed under the GNU GPL v2 or later. Inside there are also miniz (MIT license), mbedtls (Apache 2.0) and the Mozilla certificates (MPL 2.0).
