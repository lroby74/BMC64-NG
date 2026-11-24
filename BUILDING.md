# Building BMC64-NG

BMC64-NG is built on Linux (Debian, Ubuntu, or Ubuntu under WSL) for the two 64-bit boards:

| Argument | Boards | Kernels |
|---|---|---|
| `pi4-64` | Raspberry Pi 4 and 400 | `kernel8-rpi4.img.<machine>` |
| `pi5` | Raspberry Pi 5 and 500 | `kernel_2712.img.<machine>` |

The 32-bit targets of the original BMC64 (`pi0`, `pi2`, `pi3`, `pi4`) cannot be built from this repository.

## 1. Packages

```
sudo apt-get install build-essential git curl xz-utils flex bison autoconf automake libtool m4 texinfo
```

The first run also downloads the Arm GNU Toolchain 15.2.rel1 for AArch64 (`aarch64-none-elf`) into `~/.cache/bmc64/toolchains`, so it needs network access. Build in a directory whose path has no spaces.

## 2. Build

```
git clone https://github.com/lroby74/BMC64-NG.git
cd BMC64-NG
./make_all.sh pi4-64         # or: ./make_all.sh pi5
./make_machines.sh pi4-64    # or: ./make_machines.sh pi5
```

`make_all.sh` builds, in this order: Circle and newlib (circle-stdlib), the Circle add-ons, the common library, Plus4Emu, mbedtls, libsmb2, and VICE 3.10 (its build system is generated with `autogen.sh` and it is configured for bare metal in `third_party/vice-3.10`). It always starts from scratch. `make_machines.sh` then links the eight kernels, one after the other. The logs of the VICE steps are in `build/logs/`.

To build for the other board, run the two scripts again with the other argument. `./clean_all.sh` removes everything the build created; it never touches the sources.

## 3. The kernels

They are left in the repository root. Copy each one to the microSD card with the name in the last column; the other files of the card (firmware, `config.txt`, `cmdline.txt`, `machines.txt`) come from the release archive. The ROMs are not part of the repository: see the README.

| Machine | Built as (Pi 4, 400) | Name on the microSD |
|---|---|---|
| C64 cycle exact (default) | `kernel8-rpi4.img.c64sc` | `kernel8.img` |
| C64 standard | `kernel8-rpi4.img.c64` | `kernel8.img.c64-normale` |
| C64 with SuperCPU | `kernel8-rpi4.img.scpu64` | `kernel8.img.scpu64` |
| C128 | `kernel8-rpi4.img.c128` | `kernel8.img.c128` |
| VIC-20 | `kernel8-rpi4.img.vic20` | `kernel8.img.vic20` |
| Plus/4 (VICE) | `kernel8-rpi4.img.plus4` | `kernel8.img.plus4` |
| PET | `kernel8-rpi4.img.pet` | `kernel8.img.pet` |
| Plus/4 (Plus4Emu) | `kernel8-rpi4.img.plus4emu` | `kernel8.img.plus4emu` |

For the Pi 5 and 500 it is the same with `kernel_2712.img` instead of `kernel8-rpi4.img` and instead of `kernel8.img`.

For the Pi 4 and 400, `./make_sdcard_pi4_64.sh <directory>` stages a whole card in a new directory (anything already in it is deleted): the kernels with the names above, the firmware, `cmdline.txt`, `machines.txt` and the machine folders. It downloads the Wi-Fi firmware, so it needs network access. `config.txt` and the ROMs are still to be added by hand.
