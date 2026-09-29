#!/bin/bash
# set -e
# set -x

../../testdata/setup-sd.sh
make

ulimit -t 120

$CFG_QEMU_BIN -M $CFG_QEMU_MACHINE $CFG_QEMU_OPT $CFG_QEMU_IMG \
    -display none -nographic -semihosting -sd sdcard.img \
    -netdev user,id=net0 \
    -device usb-net,netdev=net0
exit_code=$?

exit $exit_code
