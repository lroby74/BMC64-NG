# BMC64-NG — Differences from BMX and the original BMC64

### The differences, one by one

Up to date as of 11 November 2026, on the published code of all three: BMC64-NG V1.4 (11 November 2026), Randy Rossi's BMC64 5.5.1 (10 November 2026) and BMX 2026.10.09.1, the latest released.

---

## The three systems

- **BMC64** is Randy Rossi's project: a Commodore emulator that runs on the Raspberry Pi by itself, with no operating system (Circle), with VICE 3.3. It runs on Raspberry Pi Zero, Zero 2 W, 2 and 3 and, since version 5.4.0 (21 October 2026, still in beta), also on Pi 4, 400, 5, 500 and 500+.
- **BMC64-NG** starts from BMC64, but uses VICE 3.10 and runs on Raspberry Pi 4, 400, 5, 500 and 500+. The older Raspberry Pi models are gone.
- **BMX** is another fork of BMC64, also with VICE 3.10, for Raspberry Pi 4 and 5 (and their variants). It dropped the Raspberry Pi models up to the 3.

BMC64-NG shares with BMC64 the menu, the `machines.txt` file format, the REU images saved and loaded from the menu, the writing of disks back to the card (Prefs > Flush disk writes), file names up to 255 characters, the network on all machines with WiFi from the menu and, since BMC64 5.4.0 (in beta), the Raspberry Pi 4, 400, 5, 500 and 500+, with the cycle-exact C64 (X64SC) and Plus4Emu. It shares only two things with BMX: VICE 3.10 and the CRT shader.

---

## BMC64-NG compared with the original BMC64, Randy's

**Machines and Raspberry Pi models**

- **Raspberry Pi**: BMC64-NG runs on Pi 4, 400, 5, 500 and 500+. BMC64 runs on Pi Zero, Zero 2 W, 2 and 3 and, since 5.4.0, also on Pi 4, 400, 5, 500 and 500+, in beta and with some limits (HDMI only, at 720p or 1080p).
- **Emulator**: BMC64-NG has VICE 3.10, BMC64 has the 2018 VICE 3.3.
- **Cycle-exact C64**: in BMC64-NG the C64 is cycle exact (X64SC), with the lighter Std version selectable in the Machine > Switch menu. BMC64 has only X64SC on the Pi 4 and Pi 5, and only the normal C64 on the Raspberry Pi models up to the 3.
- **SuperCPU**: BMC64-NG has the C64 with a 20 MHz SuperCPU. BMC64 does not.
- **C64 models**: from the Model... menu the C64 can be a C64C, an old C64, an SX-64 or a C64 GS (the cartridge-only console). BMC64 does not offer them: its VICE knows them, but the menu does not offer them.

**Restart, power off, keys**

- **Power off**: both power off and restart from the menu, with confirmation: BMC64-NG from the Reset menu, BMC64 from the **Power** menu (**Reboot BMC64**, **Shut down**) and from the Web UI. BMC64-NG answers **Fn + F10** on the Pi 400 and the power key of the Pi 500 and 500+; BMC64 answers **Fn + F10** on the Pi 400, the Pi 5 button and the Pi 500 key. BMC64-NG also powers off and restarts from the keyboard (**ALT + ESC**, **ALT + SHIFT + ESC**, with confirmation) and from the assignable hotkeys; from the keyboard BMC64 only has the machine resets.
- **Shortcuts**: BMC64-NG has about thirty shortcuts with ALT (disks, tape, cartridges, REU, drive model, volume, SID player, scanlines, warp, status bar). BMC64 has the C= and CTRL combinations with F1, F3, F5, F7 and the buttons to which a function can be assigned, which BMC64-NG kept.
- **Keyboards**: BMC64-NG has American, Italian, British and German layouts (Keyboard > Layout menu), also for typing in the menu fields. BMC64 (**Keyboard** > **Keyboard Layout** menu, also for the menu fields) has American, British, German, French, Norwegian and Latin American on the C64; American on the C128, VIC-20, Plus/4 and PET, and German too on the C128. No Italian.
- **RESTORE on two keys**: in the Positional mappings of the C64, C128, VIC-20 and SuperCPU RESTORE is **PrtScn** (the Pi 400 and 500 keyboards) and also **Page Up** (a real C64 keyboard on a Keyrah V2). BMC64 has only Page Up.

**Video**

- **Main screen**: Video > Main HDMI Output chooses which HDMI port carries the picture, and the audio with it. BMC64 does not: it has only one screen.
- **C128 on two monitors**: Video > Active Display set to Two HDMI sends the VIC-II to one monitor and the VDC to the other. BMC64 does not: VIC-II and VDC go to a single output, with Active Display and PIP.
- **Composite output**: BMC64-NG has **PAL Composite 576p@50Hz** and **NTSC Composite 480p@60Hz** in every machine's Switch menu, only on the Pi 4 B (3.5 mm jack, 4-pole AV cable), with the borders kept apart from the HDMI ones. BMC64 has the same two composite modes only on the Raspberry Pi models up to the 3: on the Pi 4 and Pi 5 it has HDMI only.
- **Resolution**: both have 720p and 1080p for every machine in the Switch menu. BMC64-NG also changes it from Video > HDMI Resolution; on the Pi 4 and Pi 5 BMC64 chooses it only with the Switch entry.
- **VGA**: BMC64-NG has 1024x768 VGA modes at 50 and 60 Hz for every machine (HDMI with a VGA adapter). BMC64 takes another route: DPI with a VGA666 (only on the Raspberry Pi models up to the 3) or 1920x1080 with `raster_skip`.
- **VGA and HDMI monitors together**: Video > VGA Monitor On tells which HDMI port the VGA monitor is on, and Switch sends the screen to the VGA with the VGA entries and to the other monitor with the others, with a single restart. BMC64 does not: it has only one screen.
- **CRT shader**: BMC64-NG has BMX's shader, with geometry, scanlines, phosphor mask, bloom, reflections and the `.crt` presets. BMC64 has davej's CRT shader: on the Pi 4 and Pi 5 (crt-pi, drawn by the GPU) only with **Enable**, **Curvature** and **Reset**; scanlines, mask, bloom and gamma can be adjusted only on the Raspberry Pi models up to the 3.
- **Video mode confirmation**: after a machine or mode change there are 15 seconds to confirm with a key, otherwise it goes back to what it was (Safety timer (15s) at the bottom of the Switch menu). BMC64 does not: it only asks whether to restart, and to go back it has CTRL + F7 held for 5 seconds after start-up.
- **Screenshots**: Prefs > Screenshots and **SUPER + PRTSCN**, in PNG or PPM, in the `/screenshots` folder. BMC64 does not have them.

**Sound**

- **SID engines**: besides ReSid there is **ReSIDfp** (Sound > SID Engine), with its own settings, the 6581 composer profiles (Composer Profile) and the background noise (Background Noise). BMC64 does not: its SID Engine has only Fast and ReSid.
- **reSID filters**: in the Sound menu the reSID Filter Settings folder (Passband %, Gain %, Filter Bias) for the 6581 and the 8580. In BMC64's guide Passband and Gain must stay at the factory values.
- **Stereo or mono** (Sound > Channels) with more than one SID. BMC64 does not: with two SIDs it always plays in stereo.
- **8580 + Digi Boost** (Sound > SID Model, SID2 Model and SID Card Model): the 8580 with the modification that makes digitized samples audible. BMC64 does not have it: only 6581 and 8580.
- **SID Card on PET and Plus/4**, off at the factory. BMC64 does not have it: the second SID exists only on the C64 and the C128.
- **Autostart on the PET**, with `.prg` files only. BMC64 does not have it: there is no Autostart on the PET.
- **PetSpeed key of the PET**: Machine > PetSpeed dongle, on the tape port. BMC64 does not have it.
- **.SID file player**: ALT + S opens the `/sids` folder; it also plays files with two, three, four and up to eight SIDs (PSID v4E; with eight-SID tunes, such as The Tuneful Eight, 24 bars and reSID to start with; S switches between reSID and reSIDfp live), with VU meters, song time, 10-second jumps (back at once, from a snapshot of the machine kept in memory), voices one by one and the side of each SID (F1-F4, F1-F8 with eight-SID tunes). BMC64 does not have it.
- **SIDs on more cores**: with 2 or 3 SIDs, and in the player up to 8, the ones after the first are computed by core 2 and, when the screen leaves it free, by core 3. BMC64 has at most two SIDs: the second is computed by core 2, and core 3 only computes the filter tables at startup, then stays idle.
- **USB DAC**: Sound > Audio out chooses between Auto, Jack, HDMI and USB DAC from the menu. The DAC can be plugged and unplugged while the machine is on, and 48 kHz-only DACs work too. BMC64 does not: the audio output (HDMI, jack or automatic) is chosen only in the files, with `audio_out` in `cmdline.txt` or in `machines.txt`, with no USB DAC.
- **Volume**: the on-screen bar, **ALT + "+"**, **ALT + "−"**, **ALT + M**. BMC64 only has the **Volume** entry in the Sound menu.

**Clock**

- **CPU and GPU clock from the menu** (CPU Clock and GPU Clock): the CPU -20%, -10%, stock, +10%, +20%, the GPU stock, +10%, +20%, on every Pi from the Pi 4 on; the GPU MHz at the top of the menu and on the status bar, next to the CPU ones. In BMC64 the clock is changed by hand in `config.txt` (the `arm_freq` and `over_voltage` lines, commented out in the sample file).
- **Overclocking...** (main menu): voltage, temperature limit, Core Clock and V3D Clock, and never `force_turbo` from there. BMC64 does not have it.

**Disks, tapes, files**

- **Real Commodore drive**: a 1541, 1571 or 1581 on an XUM1541 or ZoomFloppy adapter (ALT + 7). BMC64 does not have it.
- **XUM1541 built on an Arduino Pro Micro**: it may not be compatible with BMC64-NG: the real Commodore drive works only after the first power-on from cold, and after a machine change or a restart of the Raspberry it stops answering. BMC64-NG is tested with a ZoomFloppy.
- **Directory of Drive on XUM1541** (Drives): the directory of the real drive read from the disk sectors, with every entry, also the ones the BASIC LOAD"$" does not show. BMC64 does not have it.
- **Drives that talk to each other**: with two or more drives on, before touching the serial cable a drive brings the others up to its time, and two drives can talk without going through the computer. BMC64 does not have it.
- **USB floppy drives from a PC** read as FLP1 and FLP2. BMC64 does not have them.
- **Search in the file list**: you type the first letters and the list narrows already as you type, with wildcards (the question mark and the asterisk), and if nothing is found it searches inside the name. BMC64 only jumps to the first letter typed.
- **Long names in the menu**: a name that does not fit in the line scrolls when the cursor is on it, so you can read it all (it stops for a moment, scrolls, then starts again). BMC64 cuts it.
- **.REU images**: **ALT + 0** loads a file from the `/REU` folder, **ALT + SHIFT + 0** detaches it.
- **EasyFlash cartridges save** (Cartridge > Save EasyFlash Now, and by themselves when the cartridge is detached). BMC64 has Save EasyFlash Now, but does not save by itself when the cartridge is detached.
- **Magic Desk Plus**: the cartridge with SRAM and EEPROM, which saves in two files next to the `.crt` (`.srm` and `.eep`). BMC64 does not have it.
- **Drive without a disk in Plus4Emu**: ALT + 2 and ALT + 3 set the 1541 or the 1551 and open the list of `.D64` files.
- **.NIB images** (the low-level copies made with nibtools, protected disks included): attached and started like the other disks, converted to `.g64` with the nibconv code of nibtools. BMC64 does not have them.

**Network**

- **Fixed address**: IP Address Mode set to DHCP or Static (Static IP, Netmask, Gateway, DNS). The Raspberry Pi introduces itself to the router as BMC64-NG. BMC64 does not: only the automatic address (DHCP).
- **Daylight saving time**: Daylight Saving (Off, On, Auto (EU)) under Timezone. BMC64 takes the time from the network and has the time zone in the Network menu (**Timezone (reboot)**, from UTC-12:00 to UTC+14:00), with no daylight saving time.
- **Date and time**: at the top of the main menu, taken from the network with the time zone and daylight saving of the Network menu; the files seen through SD Card Sharing get the same time. BMC64 takes the time from the network but does not show it.
- **NTP from the menu**: Network > NTP (Server 1, Server 2, Sync interval, Save & Apply, Sync now). BMC64 does not: it always takes the time from `pool.ntp.org`, every 15 minutes; in the menu it only has the time zone (**Timezone (reboot)**) and, in **Prefs** > **Boot Options**, **Delay autostart for time sync**.
- **The date of files without a network**: until the time arrives, the most recent date among the files in the root of the card is used. BMC64 writes the zero date, which Windows shows as 1601.
- **SD Card Sharing...**: the microSD seen from the PC (`\\BMC64-NG-400`, with the name of the model, or `\\address`, user bmc64, password bmc64, changeable with Sharing Password). BMC64 does not: its card can be reached from the PC only in the browser, through the Web UI.
- **NAS**: the NAS entry (NAS IP Address, NAS User Name, NAS Password, SMB Protocol, NAS Path, Test NAS Connection) opens a NAS the way the MiSTer does; NAS Path is the share and the folder the browser starts from, one per machine, empty at the factory like the other fields; the password is typed in clear text and after the reboot it is shown as asterisks. BMC64 does not have it.
- **The modem like the Ultimate II+** (C64, C128, Plus/4 and VIC-20): Modem (SwiftLink) off at the factory, Modem Address `DE00` at the factory. It also receives calls, on port 3000 (RING, **ATA**, **ATS0=1**). BMC64's modem only dials out; it is also off at the factory (**Modem Address** on **Off**) and on the C64 and the C128 it is switched on by choosing `D700`, `DE00`, `DF00` or `DF80`. The modem on the Plus/4 (the built-in ACIA, `$FD00`) and on the VIC-20 (the user port) is in BMC64 too. On the C64 and the C128 the line is on DSR, as on the Ultimate II+; in BMC64 it is on DCD.
- **WiC64** (C64 and C64 Std, HTTPS too): Network > WiC64. BMC64 does not have it.

**Controls**

- **The user port joystick adapter**: in BMC64-NG the Starbyte can be chosen (in BMC64 it is in the list but the menu never reaches it) and after a snapshot the menu finds the right type again (BMC64 only looks at the first one).
- **Spinner as a paddle** (Joyports > USB Spinner (Paddle), with Spinner Sensitivity) on C64, C128 and VIC-20. BMC64 does not: as a paddle it uses only the analog axes of gamepads.
- **The gamepad buttons**: Mapping Mode, Assignments by Button, Assignments by Function and Clear a Button. BMC64 only has the choices by button.
- **1350 mouse and V8 adapter**: Mouse > Type **1350 (Joystick)** and Userport Joystick Adapter > **V8**, with ports 5 to 10. BMC64 does not have them.
- **Autofire**: the Autofire menu, right below Joyports, with Port 1 and Port 2 (one port on the VIC-20) on Off, Slow or Fast. BMC64 does not have it.
- **The menu by blocks with ALT**: in the menu **ALT** + **↑** and **↓** jump up and down by blocks like **PgUp** and **PgDn** (held down, they keep going), with any keyboard: the 500+ too, where FN does not reach the computer. BMC64 jumps by blocks with **PgUp** and **PgDn** or with **F3** and **F5**; **ALT** with the arrows does nothing.
- **The mouse in the menu**: the pointer picks the row, the left button is ENTER and the right one goes back, the wheel scrolls the list or changes the numbers of the item under the pointer; it is turned off in **Prefs** (Mouse in Menu). BMC64 does not use the mouse in the menu.
- **Tape controls with no tape**: with no tape mounted the recorder controls do nothing and **NO TAPE** appears at the bottom.
- **Status bar**: the real lights of each drive model, the track (half-tracks too), the tape counter, and temperature and frequency on the second line (**ALT + SHIFT + O**).

**When something goes wrong**

- **PASSI.TXT**: it keeps the last things the machine started and finished (the start-up, the menu entries, the hotkeys); if the Raspberry freezes, after the next power-on `PASSI-PRIMA.TXT` says where it stopped. In this version the watchdog (the black BMC64-NG HAS STOPPED box) is off.
- **Files on the card**: `BMC64-DIAG.TXT`, `PASSI.TXT` (and `PASSI-PRIMA.TXT`), `USB-LOG.TXT`, `NET-LOG.TXT`, `SMB-LOG.TXT`. BMC64 has its own log, switched on from the main menu and from the configuration file.

---

## BMC64-NG compared with BMX

**Raspberry Pi and machines**

- **Raspberry Pi**: BMC64-NG is tested on Pi 4, 400, 5, 500 and 500+. BMX prepares the card also for the CM4 and, on the Pi 4, can build its own old 32-bit version.
- **Plus/4**: BMC64-NG has two engines, VICE and Plus4Emu. BMX only VICE.
- **Changing machine**: BMC64-NG from the Machine > Switch menu with the profiles of `machines.txt`, with 15 seconds to confirm; BMX from the Machine menu with Apply & Reboot, with the profiles of `machines.ini` (it does not read `machines.txt`), with no timed confirmation.

**Restart, power off, keys**

- **Restart and power off**: BMC64-NG also from the keyboard (**ALT + ESC**, **ALT + SHIFT + ESC**, **Fn + F10** on the Pi 400, the power button on the Pi 500 and 500+). BMX only from the System menu.
- **Restart and power off from the assignable hotkeys**: in BMC64-NG the choices of **C= + F1…F7** and **CTRL + F1…F7** also include **Reboot BMC64** and **Power Off BMC64**, for the real C64 keyboard on the Keyrah, which has no ALT. In BMX those choices do not include them.
- **Shortcuts**: all those with ALT belong to BMC64-NG. With the Super key BMX starts a function together with another key (W warp, S status bar, T tape, C cartridge, R reset and others); in BMC64-NG the Super key alone opens and closes the menu, like F12, and **SUPER + PRTSCN** takes a screenshot.
- **Keyboards**: BMC64-NG American, Italian, British and German; BMX American and German.
- **RESTORE on two keys**: BMC64-NG **PrtScn** (Pi 400 and 500) and **Page Up** (Keyrah V2); BMX **Page Up**, and **F12** if F12 opens menu is turned off.
- **The menu opened again**: BMC64-NG goes back to the last item touched also when you leave with **ESC**; BMX opens again on **Quick Access...** after **ESC**.

**Clock**

- **CPU clock**: BMC64-NG from the CPU Clock menu, from -20% to +20% of the stock clock of every Pi from the Pi 4 on, so also lower. BMX only upwards (Pi 4 from 1500 to 2400 MHz, Pi 5 from 2400 to 3200 MHz, in steps of 25 MHz).
- **GPU clock**: BMC64-NG has **GPU Clock** in the main menu, which raises Core and V3D together (stock, +10% or +20%), with the GPU MHz at the top of the menu and on the status bar. BMX has no such entry: it raises them one by one, with Core Clock and V3D Clock in the Expert folder (Pi 4 up to 800 MHz, Pi 5 up to 1200).
- **Fan**: BMC64-NG from the main menu (Fan), Auto with the Raspberry Pi OS curve or from 0% to 100% in steps of 10%: on the Pi 5 the fan on its connector, on the Pi 4 the one with its control wire on pin 8 (GPIO 14). BMX writes `gpiofanpin=45` on the Pi 5 and Circle regulates the fan with the temperature, with no menu item; on the Pi 4 nothing.

**Video**

- **Main screen**: in BMC64-NG Video > Main HDMI Output chooses HDMI 0 or 1 for all machines. In BMX `hdmi_output` in `cmdline.txt` says it (0, 1 or auto).
- **The first Pi 5 boards (C1 chip)**: in BMC64-NG the scaler, with the CRT shaders, gives the picture also on the first Pi 5 boards with the BCM2712 C1 chip, with the PixelValve and the plane transparency programmed the way Linux does on that chip. BMX too, since version 2026.09.18, has its own cure for the C chip.
- **The CRT shader at 1080p on the Pi 4 and the 400**: in BMC64-NG the GPU draws at most a 720p frame and the scaler brings it to 1080p, so at 1080p it does the same work as at 720p. BMX on the Pi 4 always draws as large as the screen.
- **The CRT shader with all the effects, from the Pi 4 on**: if the GPU cannot draw a frame in time, BMC64-NG draws a smaller one and the scaler brings it to the size of the screen, so the machine stays at 100%. BMX always draws the full frame, even when the GPU cannot make it in time.
- **The CRT shader with the picture moved**: in BMC64-NG, if **H Center** or **V Center** take the picture partly off the screen, the shader works on the part you see; in BMX the shader switches off and the picture stays without effects.
- **The PET with the CRTC out of sync**: in BMC64-NG, if a program leaves the CRTC without vertical sync, the screen and the menu keep going; BMX and BMC64 have VICE's CRTC as it is, which in that case no longer ends the frame.
- **Autostart on the PET like the real LOAD**: in BMC64-NG starting a `.prg` does not touch `$C7`/`$C8`, like the real LOAD of the PET; BMX and BMC64 have VICE's start as it is, which writes the program's address there.
- **The PET with a shorter frame**: in BMC64-NG, when the reprogrammed CRTC makes the frame shorter, the PET layer stays as tall as its fixed canvas and the screen stays on the video chip, at full speed; in BMX the layer follows VICE's frame and the same screen check hands every frame to the processor (read in the code, not tested).
- **The PET picture in its place**: in BMC64-NG the PET canvas stays 272 lines tall whatever the CRTC asks for and the picture stays centred in the screen you see; BMX has VICE 3.10's CRTC as it is, which makes the canvas taller and centres the picture inside it, while the screen always shows the same lines (read in the code, not tested); BMC64 has VICE 3.3, which leaves the canvas alone.
- **The microSD that recovers**: in BMC64-NG the card reader, if a transfer stops half-way, puts the lines back in order, stops or re-initialises the card and issues the command again, as Linux does; BMX and BMC64 have Circle's reader as it is, which retries without putting anything back in order and then reports an error (read in the code, not tested).
- **C128 on two monitors**: BMC64-NG yes (Two HDMI, with swapping). BMX shows the two screens on one monitor only.
- **VGA**: BMC64-NG has 1024x768 profiles at 50 and 60 Hz. BMX does not.
- **VGA and HDMI monitors together**: in BMC64-NG Video > VGA Monitor On moves the screen between the HDMI monitor and the VGA one, on the other port, with the Switch entry alone. BMX does not: it has no VGA modes, and `hdmi_output` in `cmdline.txt` says the port.
- **Composite output**: BMC64-NG has **PAL Composite 576p@50Hz** and **NTSC Composite 480p@60Hz** in the Pi 4 B Switch menu, with the borders kept apart from the HDMI ones, and you go back to HDMI with an HDMI entry of Switch or with C= + F7 held for 5 seconds. BMX has two composite modes (576p and 480p) marked as experimental.
- **Video mode confirmation**: 15 seconds to confirm, then it goes back to what it was (Safety timer (15s) removes it). BMX does not have it.
- **Video modes**: BMC64-NG has 720p and 1080p at 50 and 60 Hz for every machine (Video > HDMI Resolution, or the Switch) and VGA 1024x768, and a new card starts at 1080p. BMX has 480p and 576p over HDMI, 720p, 1080p and its own exact-frequency modes, and at the factory it starts at 720p (the PET at 1080p).
- **The resolution at the top of the menu**: in BMC64-NG the first line of the main menu ends with the resolution going out, for example *C64 PAL 50Hz HDMI 1080p*. In BMX the line only says the machine and the video (*C64 PAL 50Hz HDMI*).
- **VIC-20 NTSC** with its American KERNAL: BMC64-NG yes. BMX has NTSC timing, but does not choose an NTSC KERNAL.
- **Screenshots**: in BMC64-NG the machine stops for the time it takes to write to the card, and while playing they cost nothing. BMX writes with core 3, and also has Stop emulator.

**Sound**

- **Outputs**: BMC64-NG HDMI, USB DAC and the 3.5 mm jack of the Pi 4. BMX HDMI and USB DAC.
- **SID Card on PET and Plus/4**: BMC64-NG only ($8F00 or $E900 on the PET, $FD40 or $FE80 on the Plus/4).
- **.SID file player** (ALT + S) up to eight SIDs: BMC64-NG only. BMX does not open `.SID` files.
- **ReSIDfp** as a third SID engine (Sound > SID Engine), with Background Noise and the 6581 composer profiles: BMC64-NG. BMX does not: in its menu the engine has only FastSID and reSID.
- **reSID filters**: in BMC64-NG the reSID Filter Settings folder of the Sound menu applies to all SIDs. In BMX there is one for the first SID and one for the second, each adjusted on its own.
- **SIDs on more cores**: BMC64-NG with core 2 and, when the screen leaves it free, with core 3, with 2 or 3 SIDs and in the player up to 8; BMX only the second SID on core 2, with two SIDs.
- **Stereo or mono** (Sound > Channels): BMC64-NG. In BMX stereo goes with the second SID.

**Files, disks and cartridges**

- **Real Commodore drive** (XUM1541, ZoomFloppy, ALT + 7 or **Real drive** in **Change Model...**, even without the drive ROMs), with **Reset Drive on XUM1541** that restarts the real drives: BMC64-NG only.
- **The real drive's directory read from the sectors** (Drives > Directory of Drive on XUM1541): BMC64-NG only.
- **Drives that talk to each other** on the serial cable, without going through the computer: BMC64-NG only.
- **USB floppy drives** (FLP1 and FLP2) and the CMD FD-2000 and FD-4000 drives: BMC64-NG.
- **Default disk** (Drives): both. BMX attaches its `utils.d64` by default, with programs by others; BMC64-NG none by default.
- **Search in the file list** already as you type, with wildcards (the question mark and the asterisk): BMC64-NG.
- **.REU images**: BMC64-NG with **ALT + 0** from the `/REU` folder and from the menu. BMX only from the REU menu.
- **.prg programs**: with **PRG loading** set to **Via disk image**, the default in both, in BMC64-NG they start even without a drive, straight into RAM. BMX in that case uses a temporary disk and needs a drive; with **Direct to RAM** it does not.
- **.SID files**: BMC64-NG only.
- **.NIB images**: BMC64-NG only (converted to `.g64` with the nibconv code of nibtools).
- **Saving disks**: Prefs > Flush disk writes in BMC64-NG. BMX has no such entry.
- **Magic Desk Plus**, with its memory in the `.srm` and `.eep` files next to the `.crt`: BMC64-NG.

**Network**

- **Network**: both have Wi-Fi or Ethernet, the list of networks, the automatic or fixed address and the time from the network with daylight saving time. BMC64-NG has it on all machines, also on the Pi 5 and 500, with date and time at the top of the menu. BMX takes the time from the network since 2026.10.09 (Network > NTP, off by default), with the UTC time zone, a fixed offset or Central Europe with daylight saving time, and shows it in the NTP folder.
- **The network of the Pi 5 rev 1.7**: these Pi 5 boards have a different Ethernet chip (MaxLinear MXL86110 instead of the Broadcom BCM54213PE). BMC64-NG recognises it; BMX's sources have no code for that chip.
- **The date of files without a network**: in BMC64-NG the most recent date among the files in the root of the card is used. BMX writes the zero date, which Windows shows as 1601.
- **SD Card Sharing...**: the microSD seen from the PC over SMB (`\\BMC64-NG-400`, with the name of the model), with the Raspberry Pi in the Windows Network folder. BMX has File sharing (SMB), since 2026.09.18, with the name of the server and of the share to choose.
- **NAS like the MiSTer**: BMC64-NG, with a folder for each machine and `NAS-NON-SALVATI` if the NAS does not answer. BMX has Network Drive (SMB), since 2026.09.18, with the search for servers on the network.
- **Modem like the Ultimate II+** (SwiftLink, dials and receives): BMC64-NG, on the C64, C128, Plus/4 and VIC-20. BMX has a Hayes modem with a phone book and sounds, RS-232 over the network, UP9600 and SwiftLink.
- **Web commands and update from the Internet**: BMX only. In BMC64-NG there is no web control, by choice.

**Controls**

- **The user port joystick adapter** (Joyports > Userport Joystick Adapter): in BMC64-NG the types you can choose are the ones VICE 3.10 has for the machine, Synergy and WOJ on the Plus/4. BMX keeps the VICE 3.3 list: on the Plus/4 only types that 3.10 refuses are left, and the adapter never turns on; on the C64 the Hummer is left, which in 3.10 belongs to the C64DTV only.
- **Spinner as a paddle** (Joyports > USB Spinner (Paddle)): BMC64-NG on C64, C128 and VIC-20. BMX has nothing about the spinner (searched in its sources).
- **1350 mouse, V8 adapter and PetSpeed key of the PET**: BMC64-NG.
- **Autofire** (Off, Slow, Fast, port by port): BMC64-NG. BMX does not have it in the menu (searched in its sources: it is only inside VICE).
- **The menu by blocks with ALT**: in the menu **ALT** + **↑** and **↓** jump up and down by blocks like **PgUp** and **PgDn** (held down, they keep going), with any keyboard, the 500+ too. In BMX **ALT** does nothing in the menu, and **↑** and **↓** move one row.
- **Tape controls with no tape**: BMC64-NG shows **NO TAPE** and does nothing. In BMX the tape controls reach VICE unchecked.
- **Tape sound** (Tape sound emulation) does not wipe out the SID music in BMC64-NG; in the VICE file of BMX the defect is still there (read again on 11 November in its 2026.10.09.1, in datasette-sound.c).
- **VICE fixes after 3.10**: BMX has the drive idle rework, with its selector in the Drives menu since 2026.10.04; BMC64-NG does not, so as not to slow down. BMC64-NG has, since 4 October 2026, the fidelity fixes of the development VICE: the CIA serial register, the extra RTS read, the PET's original 80-column editor, the $FFF0 register of the PET 8096/8296, the C128 VDC lock-up with R0=0, the C128's custom C64 kernal (no longer replaced by the stock one), the C128 $01 bits, two TED registers, the VIC-20 joystick and keyboard column 7, the POT glitch only when a connected POT changes, the drive ROM changed from the menu reaching the running drive at once, the virtual drive's T-W commands, SFX Sound Expander, DigiMAX, tapecart and the PET HRE board. Since 10 October 2026 also the keyboard clear that forgets the key queue (r46275 of 28 September).
- **The development VICE up to 9 November 2026**: BMC64-NG also has the fixes up to r46396, the new combined waveforms and background hiss of reSIDfp (r46370) and the C64C sprite (bug 2164): a sprite that changes from multicolor to high resolution in the middle of a line, on the 8565 and 8562 chips.

**When something goes wrong**

- **BMC64-NG** writes `BMC64-DIAG.TXT`, `PASSI.TXT`, `USB-LOG.TXT`, `NET-LOG.TXT` and `SMB-LOG.TXT` to the card (in this version the watchdog is off). BMX has an on-screen diagnostics panel, with no files.

---

## What they have and we do not (or not yet)

**BMX**

- **Heavy overclocking**: CPU up to 2400 MHz on the Pi 4 and 3200 MHz on the Pi 5. BMC64-NG's CPU Clock stops at +20%; **Voltage Offset**, **Temperature Limit**, **Core Clock** and **V3D Clock** are in BMC64-NG too, in the Overclocking... item.
- **480p and 576p over HDMI** (720x480 at 60 Hz and 720x576 at 50 Hz) as video modes for all machines.
- **Card with two partitions** (SYS: and USER:) and a utility disk (with ccgms and wic64) in drive 8.
- **Menu**: the pending system changes shown before the restart (Pending system changes).
- **On-screen monitors** for keyboard, mouse and GPIO.
- **Hayes modem** with a phone book and sounds, RS-232 over the network, UP9600 and Userport.
- **Web commands** (off at the factory) and **update from the Internet**, from the BMX site.
- **On-screen diagnostics panel** with frames, memory and core load.
- **Palettes loaded from the card** (`palettes`) and **contents of disks and tapes** shown in the file list.
- **Declared 8BitDo controllers** (Ultimate 2, Ultimate C, V1 and V2 dongles, keyboards): in BMC64-NG the code is there, but they have not been tested.
- **BMX news after August**: smart file list, automatic drive handling at autostart, a keyboard map for each device, a hardware information page.

**Randy's BMC64**

- **Raspberry Pi Zero, Zero 2 W, 2 and 3**: BMC64-NG runs on none of them.
- **Web UI and updater** (BMC64 5.2, on all machines since 5.3): a web interface on the local network, off at the factory (Network > Web UI Settings), with an SD card file browser, an editor for BASIC programs and configuration files, the profiles page and an updater using `bmc64-update.zip`. BMC64-NG does not have them: the owner ruled out the Web UI.
- **DPI output** (VGA666, RGB on the comb) with its modes in `machines.txt`: BMC64 has it only on the Raspberry Pi models up to the 3, not on the Pi 4 and Pi 5. In BMC64-NG the code is there, but the modes in `machines.txt` are not.
- **Choice of the card partition** (`disk_partition=` in `cmdline.txt`): BMC64-NG has it too, but without the fix from BMC64 5.2.0 (#369).
- **BMC64 news up to 5.5.1** (10 November 2026): profiles (with **Profile Summary**), **Prefs** > **Boot Options** (**Warp after boot**, **Delay autostart for time sync**), **Auto-attach current disks at boot**, the IP address on screen when the network connects (**Show IP Address on Connect**), the GPIO machine signal (`enable_gpio_machine_signal`), the **Pimmodore** map for a real C64 keyboard through the Keymmodore-64, and the French, Norwegian and Latin American keyboards.
