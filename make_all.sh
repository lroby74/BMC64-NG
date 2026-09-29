#!/bin/bash


# Set directory variables
SRC_DIR=`pwd`
CIRCLE_HOME="$SRC_DIR/third_party/circle-stdlib"
COMMON_HOME="$SRC_DIR/third_party/common"

# Check for the Arm GNU Toolchain and install it if necessary
if ! source "$SRC_DIR/get_gnu_toolchain.sh"
then
       echo "Arm GNU Toolchain setup failed." >&2
       exit 1
fi

CIRCLE_PUBLIC_INCLUDES="-I$CIRCLE_HOME/include -I$CIRCLE_HOME/libs/circle/include -I$CIRCLE_HOME/libs/circle/addon"

if ! command -v flex >/dev/null 2>&1
then
       echo "Missing required build dependency: flex" >&2
       echo "Install it with: sudo apt-get install flex" >&2
       exit 1
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


BOARD=""
SKIP_PATCHES=1

for arg in "$@"
do
case "$arg" in
       pi0|pi2|pi3|pi4|pi4-64|pi5)
              BOARD="$arg"
              ;;
       --skip-patches)
              SKIP_PATCHES=1
              ;;
       *)
              echo "Need arg [pi0|pi2|pi3|pi4|pi4-64|pi5] [--skip-patches]"
              exit 1
              ;;
esac
done

if [ -z "$BOARD" ]
then
echo "Need arg [pi0|pi2|pi3|pi4|pi4-64|pi5] [--skip-patches]"
exit 1
fi

echo "Making for $BOARD"




if [ "$BOARD" = "pi4-64" ] || [ "$BOARD" = "pi5" ]
then
       if ! source "$SRC_DIR/get_gnu_toolchain64.sh"
       then
              echo "AArch64 Arm GNU Toolchain setup failed." >&2
              exit 1
       fi
fi

if [ -f sdcard/config.txt ]
then
echo Making everything...
else
echo Must be run from BMC64 root dir.
exit
fi



cd $SRC_DIR/third_party/circle-stdlib

find . -name 'config.cache' -exec rm {} \;

echo ==============================================================
echo APPLY PATCHES
echo ==============================================================

apply_patch_file()
{
       patch_file="$1"
       sed_expression="$2"

       if [ ! -f "$patch_file" ]
       then
              echo "Required patch file is missing: $patch_file" >&2
              exit 1
       fi

       if [ -n "$sed_expression" ]
       then
              sed "$sed_expression" "$patch_file" | patch -p1
              patch_status=("${PIPESTATUS[@]}")
              if [ "${patch_status[0]}" != "0" ] || [ "${patch_status[1]}" != "0" ]
              then
                     exit 1
              fi
       else
              patch -p1 < "$patch_file"
              if [ "$?" != "0" ]
              then
                     exit 1
              fi
       fi
}

if [ "$SKIP_PATCHES" = "1" ]
then
echo "Skipping patches"
else
cd $SRC_DIR/third_party/circle-stdlib/libs/circle-newlib
git reset --hard HEAD
git clean -fd
apply_patch_file "$SRC_DIR/circle_newlib_patch.diff"

cd $SRC_DIR/third_party/circle-stdlib/libs/circle
git reset --hard HEAD
git clean -fd

circle_patch_file="$SRC_DIR/circle_patch.diff"
if [ "$BOARD" = "pi0" ]
then
       apply_patch_file "$circle_patch_file" 's@+#define ARM_ALLOW_MULTI_CORE@+//#define ARM_ALLOW_MULTI_CORE@'
else
       apply_patch_file "$circle_patch_file"
fi

apply_patch_file "$SRC_DIR/circle_8bitdo_keyboard_patch.diff"
apply_patch_file "$SRC_DIR/circle_8bitdo_gamepad_patch.diff"
apply_patch_file "$SRC_DIR/circle_usb_descriptor_patch.diff"
apply_patch_file "$SRC_DIR/circle_xbox360_gamepad_patch.diff"
apply_patch_file "$SRC_DIR/circle_tcpconnection_patch.diff"
apply_patch_file "$SRC_DIR/circle_ethernet_patch.diff"
apply_patch_file "$SRC_DIR/circle_usbaudio_safety_patch.diff"
fi

echo ==============================================================
echo BUILD CIRCLE-STDLIB
echo $PATH
echo ==============================================================

cd $SRC_DIR/third_party/circle-stdlib

if [ "$BOARD" = "pi2" ]
then
#cat ../../circle_stdlib_patch.diff  | sed 's/-std=c++14//' | patch -p1
./configure --raspberrypi=2 --kernel-max-size 40
elif [ "$BOARD" = "pi0" ]
then
#cat ../../circle_stdlib_patch.diff | patch -p1
./configure --raspberrypi=1 --kernel-max-size 40
elif [ "$BOARD" = "pi3" ]
then
#cat ../../circle_stdlib_patch.diff  | patch -p1
./configure --raspberrypi=3 --kernel-max-size 40
elif [ "$BOARD" = "pi4" ]
then
#cat ../../circle_stdlib_patch.diff  | patch -p1
./configure --raspberrypi=4 --kernel-max-size 40
elif [ "$BOARD" = "pi4-64" ]
then
./configure --raspberrypi=4 --kernel-max-size 40 -p aarch64-none-elf-
elif [ "$BOARD" = "pi5" ]
then
./configure --raspberrypi=5 --kernel-max-size 40 -p aarch64-none-elf-
else
echo "I don't know what to do for $BOARD"
exit
fi

cd $SRC_DIR/third_party/circle-stdlib/libs/circle
./makeall --nosample clean
if [ "$?" != "0" ]
then
       exit
fi

cd $SRC_DIR/third_party/circle-stdlib

# For pi0, we turn on our HID report throttle
if [ "$BOARD" = "pi0" ]
then
CFLAGS=-DBMC64_REPORT_THROTTLE make -j4
else
make -j4
if [ "$?" != "0" ]
then
       exit
fi
fi

echo ==============================================================
echo BUILD ADDONS
echo ==============================================================

cd $SRC_DIR/third_party/circle-stdlib/libs/circle/addon/fatfs
make clean
make
if [ "$?" != "0" ]
then
       exit
fi

cd $SRC_DIR/third_party/circle-stdlib/libs/circle/addon/linux
make clean
make
if [ "$?" != "0" ]
then
       exit
fi

cd $SRC_DIR/third_party/circle-stdlib/libs/circle/addon/wlan
make clean
make
if [ "$?" != "0" ]
then
       exit
fi

cd $SRC_DIR/third_party/circle-stdlib/libs/circle/addon/wlan/hostap/wpa_supplicant
make -f Makefile.circle clean
make -f Makefile.circle
if [ "$?" != "0" ]
then
       exit
fi




if [ "$BOARD" != "pi5" ]
then
cd $SRC_DIR/third_party/circle-stdlib/libs/circle/addon/vc4/vchiq
make clean
make
if [ "$?" != "0" ]
then
       exit
fi
fi





if [ "$BOARD" != "pi4-64" ] && [ "$BOARD" != "pi5" ]
then
for vc4lib in bcm_host khronos vmcs_host vcos
do
cd $SRC_DIR/third_party/circle-stdlib/libs/circle/addon/vc4/interface/$vc4lib
make
if [ "$?" != "0" ]
then
       exit
fi
done
fi

# Common
cd $SRC_DIR/third_party/common
make clean
BOARD=$BOARD make

# Plus4Emu
cd $SRC_DIR/third_party/plus4emu
make
if [ "$?" != "0" ]
then
       exit
fi










echo ==============================================================
echo "Circle, addons, common and Plus4Emu are ready."
echo "Next: configure and build VICE 3.10 in third_party/vice-3.10, then the kernels with make -f Makefile-<machine>."
echo ==============================================================
