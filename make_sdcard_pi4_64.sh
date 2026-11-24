#!/bin/bash














set -euo pipefail

SRC_DIR=$(cd "$(dirname "$0")" && pwd)
OUT=${1:?"Usage: $0 <output directory>"}

CIRCLE=$SRC_DIR/third_party/circle-stdlib/libs/circle

rm -rf "$OUT"
mkdir -p "$OUT"

echo "== Wi-Fi firmware"
"$SRC_DIR/make_wifi_firmware.sh" "$OUT"

echo "== support files"
cp "$SRC_DIR/sdcard/cmdline.txt" "$SRC_DIR/sdcard/machines.txt" "$OUT/"
cp "$SRC_DIR/sdcard/wpa_supplicant.conf.example" "$OUT/wpa_supplicant.conf"
cp "$SRC_DIR/LICENSE" "$SRC_DIR/README.md" "$OUT/"

echo "== boot firmware (Pi 4 only: no bootcode.bin, it lives in the EEPROM)"
for f in start4.elf fixup4.dat bcm2711-rpi-400.dtb bcm2711-rpi-4-b.dtb
do
    cp "$SRC_DIR/release/common_release_files/$f" "$OUT/"
done
cp "$CIRCLE/boot/armstub/armstub8-rpi4.bin" "$OUT/"

echo "== machine directories"
for pair in c64:C64 c128:C128 vic20:VIC20 plus4:PLUS4 plus4emu:PLUS4EMU pet:PET
do
    src=${pair%%:*}
    dst=${pair##*:}
    mkdir -p "$OUT/$dst"
    cp -a "$SRC_DIR/sdcard/$src/." "$OUT/$dst/"
done
# The SuperCPU kernel reads its keyboard maps from SCPU64/: they are the C64 ones.
# Without SCPU64/bootstat.txt it uses its built-in boot list (bootstat_scpu64.h).
mkdir -p "$OUT/SCPU64"
cp "$SRC_DIR"/sdcard/c64/*.vkm "$OUT/SCPU64/"

for machine in c64 c128 vic20 plus4 plus4emu pet
do
    release_path="$SRC_DIR/release/${machine}_release_files"
    machine_dir=${machine^^}
    for entry in "$release_path"/*
    do
        if [ "$entry" = "$release_path/carts" ]
        then
            mkdir -p "$OUT/carts/$machine_dir"
            cp -a "$entry/." "$OUT/carts/$machine_dir/"
        else
            cp -a "$entry" "$OUT/"
        fi
    done
done

for data_dir in disks tapes snapshots
do
    for machine_dir in C64 C128 VIC20 PLUS4 PET
    do
        mkdir -p "$OUT/$data_dir/$machine_dir"
    done
done
for machine_dir in C64 C128 VIC20 PLUS4
do
    mkdir -p "$OUT/carts/$machine_dir"
done
mkdir -p "$OUT/disks/SCPU"
mkdir -p "$OUT/DRIVES" "$OUT/prg" "$OUT/tmp"

echo "== kernels"




# Names as in BUILDING.md: the cycle exact C64 is the default kernel.
cp "$SRC_DIR/kernel8-rpi4.img.c64sc" "$OUT/kernel8.img"
cp "$SRC_DIR/kernel8-rpi4.img.c64" "$OUT/kernel8.img.c64-normale"
for machine in scpu64 c128 vic20 plus4 plus4emu pet
do
    cp "$SRC_DIR/kernel8-rpi4.img.$machine" "$OUT/kernel8.img.$machine"
done

missing=0
for f in start4.elf fixup4.dat armstub8-rpi4.bin kernel8.img kernel8.img.vic20 \
         kernel8.img.c128 kernel8.img.plus4 kernel8.img.plus4emu kernel8.img.pet \
         kernel8.img.c64-normale kernel8.img.scpu64 SCPU64/rpi_pos.vkm \
         cmdline.txt machines.txt C64/PUT_ROMS_HERE
do
    if [ ! -f "$OUT/$f" ]
    then
        echo "MISSING: $f" >&2
        missing=1
    fi
done
[ "$missing" -eq 0 ] || { echo "Card is incomplete." >&2; exit 1; }

echo
echo "Card staged in $OUT"
echo "Still to add by hand: config.txt, and the ROMs in C64/ etc."
