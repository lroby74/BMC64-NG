#!/bin/bash
#
# Links the BMC64-NG kernels for one 64-bit board: the VICE machines (C64SC,
# C64, SCPU64, C128, VIC20, Plus4, PET) and the Plus4Emu one.
#
#   ./make_machines.sh pi4-64     Raspberry Pi 4 and 400
#   ./make_machines.sh pi5        Raspberry Pi 5 and 500
#
# Run ./make_all.sh <board> first. The kernels are left in the repository root
# as kernel8-rpi4.img.<machine> (pi4-64) or kernel_2712.img.<machine> (pi5);
# BUILDING.md has the names they get on the microSD card.

SRC_DIR=$(cd "$(dirname "$0")" && pwd)
cd "$SRC_DIR" || exit 1

BOARD=$1

case "$BOARD" in
pi4-64)
   KERNEL=kernel8-rpi4.img
   RASPBERRYPI=4
   ;;
pi5)
   KERNEL=kernel_2712.img
   RASPBERRYPI=5
   ;;
pi0|pi2|pi3|pi4)
   echo "$BOARD is a 32-bit target of the original BMC64: it cannot be built from this" >&2
   echo "repository. BMC64-NG builds only pi4-64 (Pi 4, 400) and pi5 (Pi 5, 500)." >&2
   exit 1
   ;;
*)
   echo "Need arg [pi4-64|pi5]" >&2
   exit 1
   ;;
esac

if ! source "$SRC_DIR/get_gnu_toolchain64.sh"
then
   echo "AArch64 Arm GNU Toolchain setup failed." >&2
   exit 1
fi

# make_all.sh must have been run, for this very board
if [ "$(cat "$SRC_DIR/build/logs/board-built" 2>/dev/null)" != "$BOARD" ] ||
   ! grep -q -E -- "-DRASPPI=$RASPBERRYPI"'( |$)' third_party/circle-stdlib/Config.mk
then
   echo "Run ./make_all.sh $BOARD first: the libraries are not built for $BOARD." >&2
   exit 1
fi

# Objects built for the other board must not be linked into these kernels.
# The "make clean" below removes only the root objects and the EXTRACLEAN list
# of the Makefile, not the subdirectories (v3dcrt, pi4v3d, pi5v3d, kms, pi4kms,
# pi5kms): they do not depend on the machine, so they go once, here.
find . \( -path ./third_party -o -path ./.git \) -prune -o -type f \( -name '*.o' -o -name '*.d' \) -exec rm -f {} +

cd third_party/common || exit 1
make clean
if ! BOARD=$BOARD make
then
   echo "Build failed for third_party/common" >&2
   exit 1
fi
cd ../..

MACHINES="C64SC:c64sc C64:c64 SCPU64:scpu64 C128:c128 VIC20:vic20 Plus4:plus4 PET:pet Plus4Emu:plus4emu"

for m in $MACHINES
do
   P1=`echo $m | sed 's/:.*$//'`
   P2=`echo $m | sed 's/^.*://'`
   echo "Building $P1 -> ${KERNEL}.$P2"
   if ! make clean
   then
      echo "make clean failed" >&2
      exit 1
   fi

   rm -f "$KERNEL"
   if ! BOARD=$BOARD make -f Makefile-$P1
   then
      echo "Build failed for $P1" >&2
      exit 1
   fi

   if [ ! -f "$KERNEL" ]
   then
      echo "Expected output $KERNEL was not created for $P1" >&2
      exit 1
   fi

   if ! cp "$KERNEL" "${KERNEL}.$P2"
   then
      echo "Copy failed for ${KERNEL}.$P2" >&2
      exit 1
   fi
done

echo
echo "Kernels built for $BOARD:"
ls -l "$KERNEL".*
