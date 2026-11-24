#!/bin/bash
#
# Builds everything BMC64-NG needs below the kernels, for one 64-bit board:
# Circle and newlib (circle-stdlib), the Circle add-ons, the common library,
# Plus4Emu, mbedtls, libsmb2 and VICE 3.10.
#
#   ./make_all.sh pi4-64     Raspberry Pi 4 and 400
#   ./make_all.sh pi5        Raspberry Pi 5 and 500
#
# Then run ./make_machines.sh <board> to link the kernels (see BUILDING.md).
# The script always starts from scratch, so building for the other board means
# running it again with the other argument.

SRC_DIR=$(cd "$(dirname "$0")" && pwd)
CIRCLE_HOME="$SRC_DIR/third_party/circle-stdlib"
COMMON_HOME="$SRC_DIR/third_party/common"
VICE_HOME="$SRC_DIR/third_party/vice-3.10"
LOG_DIR="$SRC_DIR/build/logs"
CIRCLE_PUBLIC_INCLUDES="-I$CIRCLE_HOME/include -I$CIRCLE_HOME/libs/circle/include -I$CIRCLE_HOME/libs/circle/addon"
JOBS=$(nproc 2>/dev/null || echo 4)

fail()
{
   echo "ERROR: $*" >&2
   exit 1
}

BOARD=""

for arg in "$@"
do
case "$arg" in
   pi4-64|pi5)
      BOARD="$arg"
      ;;
   pi0|pi2|pi3|pi4)
      echo "$arg is a 32-bit target of the original BMC64: it cannot be built from this" >&2
      echo "repository. BMC64-NG builds only pi4-64 (Pi 4, 400) and pi5 (Pi 5, 500)." >&2
      exit 1
      ;;
   --skip-patches)
      # Old option, kept so that old command lines still work: the Circle
      # patches are already part of the sources in third_party/circle-stdlib.
      ;;
   *)
      echo "Need arg [pi4-64|pi5]" >&2
      exit 1
      ;;
esac
done

if [ -z "$BOARD" ]
then
   echo "Need arg [pi4-64|pi5]" >&2
   exit 1
fi

if [ ! -f "$SRC_DIR/sdcard/config.txt" ]
then
   fail "this does not look like the BMC64-NG repository (sdcard/config.txt is missing)."
fi

case "$SRC_DIR" in
   *" "*) fail "the path of the repository must not contain spaces: $SRC_DIR" ;;
esac

case "$BOARD" in
   pi4-64)
      RASPBERRYPI=4
      VICE_CPU=cortex-a72
      ;;
   pi5)
      RASPBERRYPI=5
      VICE_CPU=cortex-a76
      ;;
esac

# Build tools
missing=""
for tool in make gcc g++ tar xz flex bison autoconf automake
do
   command -v "$tool" >/dev/null 2>&1 || missing="$missing $tool"
done
if ! command -v curl >/dev/null 2>&1 && ! command -v wget >/dev/null 2>&1
then
   missing="$missing curl"
fi
if [ -n "$missing" ]
then
   echo "Missing required build tools:$missing" >&2
   echo "On Debian or Ubuntu install them with:" >&2
   echo "  sudo apt-get install build-essential git curl xz-utils flex bison autoconf automake libtool m4 texinfo" >&2
   exit 1
fi

# Arm GNU Toolchain for AArch64 (downloaded once into ~/.cache/bmc64/toolchains)
if ! source "$SRC_DIR/get_gnu_toolchain64.sh"
then
   fail "AArch64 Arm GNU Toolchain setup failed."
fi

# VICE's nested reSID configure scripts otherwise select the obsolete
# automake-1.15 binary instead of the installed compatible Automake.
export AUTOMAKE=automake

# AC_PROG_LEX probes libfl by linking an executable. That cannot work with the
# bare-metal toolchain, even though flex itself is available for code generation.
configure_vice()
{
   LEX=flex LEXLIB= ac_cv_lib_lex='none needed' ac_cv_search_yywrap='none required' "$@"
}

mkdir -p "$LOG_DIR" || fail "cannot create $LOG_DIR"
rm -f "$LOG_DIR/board-built"

echo "Making for $BOARD"

echo ==============================================================
echo BUILD CIRCLE-STDLIB
echo ==============================================================

cd "$CIRCLE_HOME" || fail "third_party/circle-stdlib is missing"

find . -name 'config.cache' -exec rm {} \;

# ./configure does "cd build/circle-newlib" without creating it, and build/ is
# not in the repository (.gitignore). Start from an empty one: nothing built
# for another board must be reused.
rm -rf "$CIRCLE_HOME/build"
mkdir -p "$CIRCLE_HOME/build/circle-newlib" || fail "cannot create third_party/circle-stdlib/build/circle-newlib"

./configure --raspberrypi="$RASPBERRYPI" --kernel-max-size 40 -p aarch64-none-elf- || fail "circle-stdlib configure failed"

cd "$CIRCLE_HOME/libs/circle" || fail "third_party/circle-stdlib/libs/circle is missing"
./makeall --nosample clean || fail "cleaning Circle failed"

cd "$CIRCLE_HOME" || exit 1
make -j4 || fail "circle-stdlib build failed"

# The Pi 4 boots through armstub8-rpi4.bin (config.txt: armstub=armstub8-rpi4.bin);
# make_sdcard_pi4_64.sh takes it from Circle's boot/armstub.
if [ "$BOARD" = "pi4-64" ]
then
   make -C "$CIRCLE_HOME/libs/circle/boot" armstub64 || fail "building armstub8-rpi4.bin failed"
fi

echo ==============================================================
echo BUILD ADDONS
echo ==============================================================

build_addon()
{
   cd "$1" || fail "$1 is missing"
   shift
   make "$@" clean
   make "$@" || fail "build failed in $(pwd)"
}

ADDON="$CIRCLE_HOME/libs/circle/addon"

build_addon "$ADDON/fatfs"
build_addon "$ADDON/linux"
build_addon "$ADDON/wlan"
build_addon "$ADDON/wlan/hostap/wpa_supplicant" -f Makefile.circle

# The Pi 5 has no VCHIQ: its audio goes through HDMI
if [ "$BOARD" != "pi5" ]
then
   build_addon "$ADDON/vc4/vchiq"
fi

echo ==============================================================
echo BUILD COMMON
echo ==============================================================

cd "$COMMON_HOME" || fail "third_party/common is missing"
make clean
BOARD=$BOARD make || fail "build of third_party/common failed"

echo ==============================================================
echo BUILD PLUS4EMU
echo ==============================================================

cd "$SRC_DIR/third_party/plus4emu" || fail "third_party/plus4emu is missing"
make clean
make || fail "build of Plus4Emu failed"

echo ==============================================================
echo BUILD MBEDTLS
echo ==============================================================

# TLS for the WiC64 of the C64SC and C64 kernels. Same recipe as the "mbedtls"
# target of circle-stdlib/Makefile, with CC and ARCH taken from the Config.mk
# that ./configure has just written.
cd "$CIRCLE_HOME" || exit 1
MBED_CC=$(sed -n 's/^CC = //p' Config.mk)
MBED_ARCH=$(sed -n 's/^ARCH = //p' Config.mk)
if [ -z "$MBED_CC" ] || [ -z "$MBED_ARCH" ]
then
   fail "cannot read CC and ARCH from third_party/circle-stdlib/Config.mk"
fi
make -C libs/mbedtls/library clean >/dev/null 2>&1
rm -f src/circle-mbedtls/*.o src/circle-mbedtls/*.d src/circle-mbedtls/*.a
CC="$MBED_CC" CFLAGS="$MBED_ARCH -fsigned-char -ffreestanding -O2 -Wno-parentheses -g -I../../../include -I../../circle/include -DMBEDTLS_CONFIG_FILE='<circle-mbedtls/config-circle-mbedtls.h>'" \
   make -C libs/mbedtls/library -j"$JOBS" || fail "build of mbedtls failed"
make -C src/circle-mbedtls || fail "build of circle-mbedtls failed"

echo ==============================================================
echo BUILD LIBSMB2
echo ==============================================================

cd "$SRC_DIR/third_party/smb" || fail "third_party/smb is missing"
make clean >/dev/null 2>&1
make -j"$JOBS" || fail "build of third_party/smb failed"
[ -f libbmcsmb.a ] || fail "third_party/smb/libbmcsmb.a was not built"

echo ==============================================================
echo BUILD VICE 3.10
echo ==============================================================

cd "$VICE_HOME" || fail "third_party/vice-3.10 is missing"

# Objects of an earlier build (for the other board, say) must not survive:
# the VICE makefiles do not notice a change of the compiler flags.
find . -type f \( -name '*.o' -o -name '*.a' -o -name '*.lo' \) -delete
find . -type d \( -name '.deps' -o -name '.libs' \) -prune -exec rm -rf {} +

# The repository has VICE's configure but not the Makefile.in files.
if [ ! -f src/Makefile.in ]
then
   echo "Generating the VICE build system (autogen.sh)..."
   if ! sh ./autogen.sh > "$LOG_DIR/vice-autogen.log" 2>&1
   then
      tail -20 "$LOG_DIR/vice-autogen.log" >&2
      fail "VICE autogen.sh failed, see $LOG_DIR/vice-autogen.log"
   fi
fi

echo "Configuring VICE for $BOARD ($VICE_CPU)..."
(
   export LDFLAGS="-nostdlib -Wl,-e,main -L$CIRCLE_HOME/install/aarch64-none-circle/lib"
   export CXXFLAGS="-I$VICE_HOME/src/arch/systemheaderoverride -O3 -std=c++11 -fno-exceptions -mcpu=$VICE_CPU -mlittle-endian -DAARCH=64 -ffreestanding -nostdlib -nostdinc++ $CIRCLE_PUBLIC_INCLUDES"
   export CFLAGS="-O3 -I$COMMON_HOME -I$CIRCLE_HOME/install/aarch64-none-circle/include/ $CIRCLE_PUBLIC_INCLUDES -I$CIRCLE_HOME/libs/circle/addon/fatfs -I$ARM_HOME/lib/gcc/aarch64-none-elf/$ARM_VERSION/include-fixed -I$ARM_HOME/lib/gcc/aarch64-none-elf/$ARM_VERSION/include -fno-exceptions -mcpu=$VICE_CPU -mlittle-endian -DAARCH=64 -ffreestanding -nostdlib"
   configure_vice ./configure --host=aarch64-none-elf --enable-raspiui \
      --without-zlib --without-libcurl --without-png --without-gif \
      --without-vorbis --without-flac --without-mpg123 --without-lame \
      --without-portaudio --without-oss --without-alsa --without-pulse \
      --without-libieee1284 --without-unzip \
      --disable-ethernet --disable-realdevice --disable-ipv6 \
      --disable-catweasel --disable-hardsid --disable-parsid \
      --disable-rs232 --disable-midi --disable-hidmgr \
      --disable-sdl1ui --disable-sdl2ui --disable-gtk3ui \
      --disable-html --disable-pdf --disable-hwscale
) > "$LOG_DIR/vice-configure.log" 2>&1
if [ $? -ne 0 ]
then
   tail -20 "$LOG_DIR/vice-configure.log" >&2
   fail "VICE configure failed, see $LOG_DIR/vice-configure.log"
fi

echo "Building VICE (this takes a while)..."
(
   cd "$VICE_HOME" || exit 1
   make -C src/arch/shared
   # -k: linenoise, zmbv and the VICE tools do not build on bare metal and the
   # kernels do not need them.
   make -C src -k -j"$JOBS"
   make -C src/arch/raspi libarch.a
   make -C src/video
   make -C src/drive
   make -C src/drive/iec
   for d in c64 c128 vic20 plus4 pet
   do
      make -C src/arch/raspi/$d
   done
) > "$LOG_DIR/vice-make.log" 2>&1

# What counts is what the kernels link: every VICE object and library named in
# the Makefile-<machine> files must exist.
missing=0
for f in $(cd "$SRC_DIR" && grep -h -o '\$(VICE)/[^ ]*\.[ao]' Makefile-C64SC Makefile-C64 Makefile-SCPU64 Makefile-C128 Makefile-VIC20 Makefile-Plus4 Makefile-PET | grep -v teensy | sort -u)
do
   f="$VICE_HOME/src/$(echo "$f" | sed 's#^\$(VICE)/##')"
   if [ ! -f "$f" ]
   then
      echo "Missing VICE output: ${f#$SRC_DIR/}" >&2
      missing=1
   fi
done
if [ $missing -ne 0 ]
then
   grep ' error:' "$LOG_DIR/vice-make.log" | grep -v -e linenoise -e ConvertUTF -e wcwidth -e termios.h -e zmbv.c -e collect2 | head -10 >&2
   fail "VICE did not build everything the kernels need, see $LOG_DIR/vice-make.log"
fi

cd "$SRC_DIR" || exit 1
echo "$BOARD" > "$LOG_DIR/board-built"

echo ==============================================================
echo "Circle, add-ons, common, Plus4Emu, mbedtls, libsmb2 and VICE 3.10 are ready for $BOARD."
echo "Next: ./make_machines.sh $BOARD"
echo ==============================================================
