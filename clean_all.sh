#!/bin/bash
#
# Removes what make_all.sh and make_machines.sh built. Only build products are
# removed: no source file is touched and "git reset" is never run, so local
# changes stay.

if [ -f sdcard/config.txt ]
then
echo Cleaning...
else
echo "Must be run from the BMC64-NG repository root."
exit 1
fi

SRC_DIR=`pwd`

cd "$SRC_DIR/third_party/vice-3.10" || exit 1
if [ -f Makefile ]
then
make clean
fi
# The configured VICE clean target can stop at an unavailable optional
# subdirectory, leaving objects from a prior Pi target behind.
find . -type f \( -name '*.o' -o -name '*.a' -o -name '*.lo' \) -delete
find . -type d \( -name '.deps' -o -name '.libs' \) -prune -exec rm -rf {} +

cd "$SRC_DIR/third_party/common" || exit 1
make clean

cd "$SRC_DIR/third_party/plus4emu" || exit 1
make clean

cd "$SRC_DIR/third_party/smb" || exit 1
make clean
rm -rf obj libbmcsmb.a

# The add-ons first: their makefiles need the Config.mk that mrproper removes.
cd "$SRC_DIR/third_party/circle-stdlib/libs/circle/addon" || exit 1

for d in fatfs linux wlan vc4/vchiq
do
(cd $d && make clean)
done
(cd wlan/hostap/wpa_supplicant && make -f Makefile.circle clean)

cd "$SRC_DIR/third_party/circle-stdlib" || exit 1
find . -name 'config.cache' -exec rm -f {} \;
make clean
make mrproper
rm -rf build

cd "$SRC_DIR" || exit 1
make clean
# "make clean" leaves the objects of the subdirectories (v3dcrt, pi4v3d, ...).
find . \( -path ./third_party -o -path ./.git \) -prune -o -type f \( -name '*.o' -o -name '*.d' \) -exec rm -f {} +

rm -f kernel*img.*
rm -rf build/logs

# A "make clean" can delete generated files that the repository tracks. Put
# back those files, and only those (a reset would throw away local changes).
git ls-files -d -z -- third_party/vice-3.10 third_party/common third_party/plus4emu third_party/smb third_party/circle-stdlib 2>/dev/null | xargs -0 -r git checkout -- 2>/dev/null
