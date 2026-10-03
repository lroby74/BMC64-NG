# BMC64-NG — Differences from BMX and BMC64

### The differences, one by one

Up to date as of 3 October 2026. For BMC64 this means version 5.1.1 (the guide and the sources we have), with the news from 5.2 where they are mentioned. For BMX this means version 2026.08.19, the last one in its changelog.

---

## The three systems

- **BMC64** is Randy Rossi's project: a Commodore emulator that runs on the Raspberry Pi by itself, with no operating system (Circle), with VICE 3.3. It runs on Raspberry Pi Zero, Zero 2 W, 2 and 3. The Pi 4 and later are not supported.
- **BMC64-NG** starts from BMC64, but uses VICE 3.10 and runs on Raspberry Pi 4, 400, 5, 500 and 500+. The older Raspberry Pi models are gone.
- **BMX** is another fork of BMC64, also with VICE 3.10, for Raspberry Pi 4 and 5 (and their variants). It dropped the Raspberry Pi models up to the 3.

BMC64-NG shares with BMC64 the menu, the `machines.txt` file format, the REU images saved and loaded from the menu, and the writing of disks back to the card (Prefs > Flush disk writes). It shares only two things with BMX: VICE 3.10 and the CRT shader.

---

## Compared with Randy's BMC64

**Machines and Raspberry Pi models**

- **Raspberry Pi**: BMC64-NG runs on Pi 4, 400, 5, 500 and 500+. BMC64 runs on Pi Zero, Zero 2 W, 2 and 3, and not on the Pi 4 or later.
- **Emulator**: BMC64-NG has VICE 3.10, BMC64 has the 2018 VICE 3.3.
- **Cycle-exact C64**: in BMC64-NG the C64 is cycle exact (X64SC), with the lighter Std version selectable in the Machine > Switch menu. BMC64 only has the normal C64.
- **SuperCPU**: BMC64-NG has the C64 with a 20 MHz SuperCPU. BMC64 does not.
- **C64 models**: from the Model... menu the C64 can be a C64C, an old C64, an SX-64 or a C64 GS (the cartridge-only console). BMC64 does not offer them: its VICE knows them, but the menu does not offer them.
- **Plus4Emu**: in BMC64-NG it runs on all the supported Raspberry Pi models. In BMC64 it is Pi 3 only.

**Restart, power off, keys**

- **Power off**: BMC64 is switched off by pulling the power. BMC64-NG also powers off and restarts from the Reset menu and from the keyboard (**ALT + ESC**, **ALT + SHIFT + ESC**, with confirmation), with **Fn + F10** on the Pi 400 and with the power button of the Pi 500 and 500+.
- **Shortcuts**: BMC64-NG has about thirty shortcuts with ALT (disks, tape, cartridges, REU, drive model, volume, SID player, scanlines, warp, status bar). BMC64 has the C= and CTRL combinations with F1, F3, F5, F7 and the buttons to which a function can be assigned, which BMC64-NG kept.
- **Keyboards**: BMC64-NG has American, Italian, British and German layouts (Keyboard > Layout menu), also for typing in the menu fields. BMC64 has the American one, the positional one and the Maxi one.

**Video**

- **Main screen**: Video > Main HDMI Output chooses which HDMI port carries the picture, and the audio with it. BMC64 does not: it has only one screen.
- **C128 on two monitors**: Video > Active Display set to Two HDMI sends the VIC-II to one monitor and the VDC to the other. BMC64 does not: VIC-II and VDC go to a single output, with Active Display and PIP.
- **Composite output**: BMC64-NG has **AV Composite OUT** only on the Pi 4 B, from the Video menu (3.5 mm jack, 4-pole AV cable). BMC64 has 480p and 576p composite modes for its Raspberry Pi models.
- **VGA**: BMC64-NG has 1024x768 VGA modes at 50 and 60 Hz for every machine (HDMI with a VGA adapter). BMC64 takes another route (DPI with a VGA666, or 1920x1080 with `raster_skip`).
- **CRT shader**: BMC64-NG has BMX's shader, with geometry, scanlines, phosphor mask, bloom, reflections and the `.crt` presets. BMC64 has davej's CRT filter.
- **Video mode confirmation**: after a machine or mode change there are 15 seconds to confirm with a key, otherwise it goes back to what it was (Safety timer (15s) at the bottom of the Switch menu). BMC64 does not: it asks for confirmation only for resets, and to go back it has CTRL + F7 held for 5 seconds.

**Sound**

- **SID engines**: besides ReSid there is **ReSIDfp** (Sound > SID Engine), with its own settings and the background noise (Background Noise). BMC64 does not: its SID Engine has only Fast and ReSid.
- **Resampling**: SID Resampling is set at the factory to Resampling, the most faithful. In BMC64 on the Pi 3 you choose between Fast, Interpolation and Fast Resampling, and on the Pi Zero and 2 there is only Fast.
- **reSID filters**: in the Sound menu the reSID Filter Settings folder (Passband %, Gain %, Filter Bias) for the 6581 and the 8580. In BMC64's guide Passband and Gain must stay at the factory values.
- **Stereo or mono** (Sound > Channels) with more than one SID.
- **SID Card on PET and Plus/4**, off at the factory. BMC64 does not have it: the second SID exists only on the C64 and the C128.
- **.SID file player**: ALT + S opens the `/sids` folder; it also plays files with two, three, four and up to eight SIDs (PSID v4E; with eight-SID tunes, such as The Tuneful Eight, 24 bars and reSID), with VU meters, song time, 10-second jumps, voices one by one and the side of each SID (F1-F4, F1-F8 with eight-SID tunes). BMC64 does not have it.
- **One SID on a separate core**: with more than one SID, the second one runs on another core. BMC64 does not: its extra cores only compute the filter tables at startup, then stay idle.
- **USB DAC**: Sound > Audio out chooses between Auto, Jack, HDMI and USB DAC from the menu. The DAC can be plugged and unplugged while the machine is on, and 48 kHz-only DACs work too. BMC64 does not: the audio output (HDMI, jack or automatic) is chosen only in `machines.txt`, with no USB DAC.
- **Volume**: the on-screen bar, **ALT + "+"**, **ALT + "−"**, **ALT + M**.

**Clock**

- **CPU and GPU clock from the menu** (CPU Clock and GPU Clock): -20%, -10%, stock, +10%, +20%, for every model. In BMC64 the clock is changed by hand in `config.txt` (the `arm_freq` and `over_voltage` lines, commented out in the sample file).

**Disks, tapes, files**

- **Real Commodore drive**: a 1541, 1571 or 1581 on an XUM1541 or ZoomFloppy adapter (ALT + 7). BMC64 does not have it.
- **USB floppy drives from a PC** read as FLP1 and FLP2. BMC64 does not have them.
- **Search in the file list**: you type the first letters, with wildcards (the question mark and the asterisk), and if nothing is found it searches inside the name. BMC64 only jumps to the first letter typed.
- **.REU images**: **ALT + 0** loads a file from the `/REU` folder, **ALT + SHIFT + 0** detaches it.
- **EasyFlash cartridges save** (Cartridge > Save EasyFlash Now, and by themselves when the cartridge is detached). BMC64 has Save EasyFlash Now, but does not save by itself when the cartridge is detached.
- **Drive without a disk in Plus4Emu**: ALT + 2 and ALT + 3 set the 1541 or the 1551 and open the list of `.D64` files.

**Network**

- **Network on all machines**, also on the Pi 5 and the Pi 500, from the Network menu. BMC64 has the network only on C64 and C128.
- **WiFi from the menu**: WiFi Settings > WiFi SSID searches for networks and lists them with band, channel and signal; WiFi Country Code is typed in; the password is typed in clear text in the box.
- **Fixed address**: IP Address Mode set to DHCP or Static (Static IP, Netmask, Gateway, DNS). The Raspberry Pi introduces itself to the router as BMC64-NG. BMC64 does not: only the automatic address (DHCP).
- **Daylight saving time**: Daylight Saving (Off, On, Auto (EU)) under Timezone. BMC64 takes the time from the network and has a time zone in minutes in the settings file (`timezone_offset_minutes`); with no daylight saving time.
- **SD Card Sharing...**: the microSD seen from the PC (`\\BMC64-NG` or `\\address`, user bmc64, password bmc64, changeable with Sharing Password). BMC64 does not have it.
- **NAS**: the NAS entry (NAS IP Address, NAS User Name, NAS Password, SMB Protocol, Test NAS Connection) opens a NAS the way the MiSTer does, with the `Mister` share and its folders. BMC64 does not have it.
- **The modem like the Ultimate II+** (C64 and C128 only): Modem (SwiftLink) off at the factory, Modem Address `DE00` at the factory. It also receives calls, on port 3000 (RING, **ATA**, **ATS0=1**). BMC64's modem only dials out; its factory address is `DE00` in 5.1.1 and `D700` from 5.2.

**Controls**

- **Spinner as a paddle** (Joyports > USB Spinner (Paddle), with Spinner Sensitivity) on C64, C128 and VIC-20. BMC64 does not: as a paddle it uses only the analog axes of gamepads.
- **Tape controls with no tape**: with no tape mounted the recorder controls do nothing and **NO TAPE** appears at the bottom.
- **Status bar**: the real lights of each drive model, the track (half-tracks too), the tape counter, and temperature and frequency on the second line (**ALT + SHIFT + O**).

**When something goes wrong**

- **The watchdog**: if the emulator stays still for ten seconds a large black box appears, in English (BMC64-NG HAS STOPPED), with the address for reporting the defect on GitHub.
- **Files on the card**: `BMC64-DIAG.TXT`, `PASSI.TXT` (and `PASSI-PRIMA.TXT`), `BLOCCO.TXT` (and `BLOCCO-PRIMA.TXT`), `USB-LOG.TXT`. BMC64 has its own log, switched on from the main menu and from the configuration file.

---

## Compared with BMX

**Raspberry Pi and machines**

- **Raspberry Pi**: BMC64-NG is tested on Pi 4, 400, 5, 500 and 500+. BMX prepares the card also for the CM4 and, on the Pi 4, can build its own old 32-bit version.
- **Plus/4**: BMC64-NG has two engines, VICE and Plus4Emu. BMX only VICE.
- **Machines**: BMC64-NG has C64 (cycle exact and Std), SuperCPU, C128, VIC-20, Plus/4 and PET; BMX has the same, with the cycle-exact C64 and the SuperCPU marked as experimental.
- **Changing machine**: BMC64-NG from the Machine > Switch menu with the profiles of `machines.txt`, with 15 seconds to confirm; BMX from the Machine menu with Apply & Reboot, with the profiles of `machines.ini` (it does not read `machines.txt`), with no timed confirmation.

**Restart, power off, keys**

- **Restart and power off**: BMC64-NG also from the keyboard (**ALT + ESC**, **ALT + SHIFT + ESC**, **Fn + F10** on the Pi 400, the power button on the Pi 500 and 500+). BMX only from the System menu.
- **Shortcuts**: all those with ALT belong to BMC64-NG. BMX only has F12 and the eight C= and CTRL + function key combinations, which BMC64-NG has too.
- **Keyboards**: BMC64-NG American, Italian, British and German; BMX American and German.

**Clock**

- **CPU clock**: BMC64-NG from the CPU Clock menu, from -20% to +20% of each model's stock clock, so also lower. BMX only upwards (Pi 4 from 1500 to 2400 MHz, Pi 5 from 2400 to 3200 MHz, in steps of 25 MHz).
- **GPU clock**: BMC64-NG from the GPU Clock menu, Core and V3D together, from -20% to +20% of the stock values. BMX has Core Clock and V3D Clock in the Expert folder, only upwards (Pi 4 up to 800 MHz, Pi 5 up to 1200).

**Video**

- **Main screen**: in BMC64-NG Video > Main HDMI Output chooses HDMI 0 or 1 for all machines. In BMX the firmware decides.
- **C128 on two monitors**: BMC64-NG yes (Two HDMI, with swapping). BMX shows the two screens on one monitor only.
- **VGA**: BMC64-NG has 1024x768 profiles at 50 and 60 Hz. BMX does not.
- **Composite output**: BMC64-NG has **AV Composite OUT** on the Pi 4 B, from the Video menu, and you go back to HDMI from there or with C= + F7 held for 5 seconds. BMX has two composite modes (576p and 480p) marked as experimental.
- **Video mode confirmation**: 15 seconds to confirm, then it goes back to what it was (Safety timer (15s) removes it). BMX does not have it.
- **Video modes**: BMC64-NG has 720p (1080p for the PET), 768x525 and 768x545 at exact frequency, and VGA 1024x768. BMX has 480p, 576p, 720p, 1080p and its own exact-frequency modes.
- **VIC-20 NTSC** with its American KERNAL: BMC64-NG yes. BMX has NTSC timing, but does not choose an NTSC KERNAL.

**Sound**

- **Outputs**: BMC64-NG HDMI, USB DAC and the 3.5 mm jack of the Pi 4. BMX HDMI and USB DAC.
- **SID Card on PET and Plus/4**: BMC64-NG only ($8F00 or $E900 on the PET, $FD40 or $FE80 on the Plus/4).
- **.SID file player** (ALT + S) up to eight SIDs: BMC64-NG only. BMX does not open `.SID` files.
- **ReSIDfp** as a third SID engine (Sound > SID Engine), with Background Noise: BMC64-NG. BMX does not: in its menu the engine has only FastSID and reSID.
- **reSID filters**: BMC64-NG has the reSID Filter Settings folder, valid for all SIDs. BMX shows the values on screen and adjusts the second SID on its own.
- **One SID on a separate core**: BMC64-NG with two, three and four SIDs, and the four even ones with eight-SID tunes; BMX only with two.
- **Stereo or mono** (Sound > Channels): BMC64-NG. In BMX stereo goes with the second SID.

**Files, disks and cartridges**

- **Real Commodore drive** (XUM1541, ZoomFloppy, ALT + 7): BMC64-NG only.
- **USB floppy drives** (FLP1 and FLP2) and the CMD FD-2000 and FD-4000 drives: BMC64-NG.
- **Search in the file list** with wildcards (the question mark and the asterisk): BMC64-NG.
- **.REU images**: BMC64-NG with **ALT + 0** from the `/REU` folder and from the menu. BMX only from the REU menu.
- **.prg programs**: in BMC64-NG they start even without a drive. BMX uses a temporary disk and needs a drive.
- **.SID files**: BMC64-NG only.
- **Saving disks**: Prefs > Flush disk writes in BMC64-NG, and EasyFlash cartridges save.

**Network**

- **Network**: BMC64-NG has Wi-Fi or Ethernet on all machines, also on the Pi 5 and 500; WiFi from the menu with a list of networks; automatic or fixed address; daylight saving time; time taken from the network. BMX has Wi-Fi or Ethernet with a fixed address and network search.
- **SD Card Sharing...**: the microSD seen from the PC over SMB (`\\BMC64-NG`), with the Raspberry Pi in the Windows Network folder. BMX does not have it.
- **NAS like the MiSTer**: BMC64-NG only. BMX does not have it.
- **Modem like the Ultimate II+** (SwiftLink, dials and receives): BMC64-NG, C64 and C128 only. BMX has a Hayes modem with a phone book and sounds, RS-232 over the network, UP9600 and SwiftLink.
- **Web commands and update from the Internet**: BMX only. In BMC64-NG there is no web control, by choice.

**Controls**

- **Spinner as a paddle** (Joyports > USB Spinner (Paddle)): BMC64-NG on C64, C128 and VIC-20. BMX has nothing about the spinner (searched in its sources).
- **Tape controls with no tape**: BMC64-NG shows **NO TAPE** and does nothing. In BMX the tape controls reach VICE unchecked.
- **Tape sound** (Tape sound emulation) does not wipe out the SID music in BMC64-NG; in the VICE file of BMX the defect is still there (our comparison of 19 September).
- **VICE fixes after 3.10**: BMC64-NG has four taken from BMX; BMX has more (see below).

**When something goes wrong**

- **BMC64-NG** writes `BMC64-DIAG.TXT`, `PASSI.TXT`, `BLOCCO.TXT` and `USB-LOG.TXT` to the card, and has the **watchdog** with the black BMC64-NG HAS STOPPED box. BMX has an on-screen diagnostics panel, with no files and no watchdog.

---

## What they have and we do not (or not yet)

**BMX**

- **Heavy overclocking**: CPU up to 2400 MHz on the Pi 4 and 3200 MHz on the Pi 5, **Voltage Offset**, **Temperature Limit** and, in the Expert folder, **Core Clock** and **V3D Clock** (GPU clocks). BMC64-NG's CPU Clock stops at +20%. Core and V3D are also adjusted by BMC64-NG, with its GPU Clock (from -20% to +20%).
- **480p and 576p** as video modes for all machines.
- **Card with two partitions** (SYS: and USER:), a disk chosen to be mounted at every power-on and a utility disk (with ccgms) in drive 9.
- **Menu**: the five Quick Access slots, the menu used with the mouse, the enlargeable menu, the pending system changes shown before the restart (Pending system changes).
- **Keyboard map editor** and on-screen monitors for keyboard, mouse and GPIO.
- **Hayes modem** with a phone book and sounds, RS-232 over the network, UP9600 and Userport.
- **Web commands** (off at the factory) and **update from GitHub**.
- **On-screen diagnostics panel** with frames, memory and core load.
- **Palettes loaded from the card** (`palettes`) and **contents of disks and tapes** shown in the file list.
- **VICE fixes after 3.10** beyond our four, such as the REU DMA of the SuperCPU and the POT glitch of the 1351 mouse (comparison of 19 September, point 4.3).
- **Declared 8BitDo controllers** (Ultimate 2, Ultimate C, V1 and V2 dongles, keyboards): not tested in BMC64-NG.

**Randy's BMC64**

- **Raspberry Pi Zero, Zero 2 W, 2 and 3**: BMC64-NG runs on none of the three.
- **Web UI and updater** (BMC64 5.2): a web interface on the local network with an SD card file browser, a BASIC program editor and an updater using `bmc64-update.zip`. BMC64-NG does not have them: the owner ruled out the Web UI.
- **DPI output** (VGA666, RGB on the comb) with its modes in `machines.txt`: in BMC64-NG the code is there, but the modes in `machines.txt` are not.
- **Choice of the card partition** (`disk_partition=` in `cmdline.txt`): BMC64-NG has it too, but without the fix from BMC64 5.2.0 (#369).
- **Large CMD HD and IDE64 images** (over 32 MB) read from the card as needed: in BMC64 since 5.2; in BMC64-NG, as of 16 September, still to do.
