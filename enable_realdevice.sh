#!/bin/bash
















set -e

V="$1"
if [ -z "$V" ] || [ ! -d "$V/src" ]; then
  echo "enable_realdevice: '$V' is not a VICE tree"
  exit 1
fi


C="$V/src/config.h"
if grep -q '^/\* #undef HAVE_REALDEVICE \*/' "$C"; then
  sed -i 's|^/\* #undef HAVE_REALDEVICE \*/|#define HAVE_REALDEVICE /**/|' "$C"
  echo "   config.h: HAVE_REALDEVICE on"
elif grep -q '^#define HAVE_REALDEVICE' "$C"; then
  echo "   config.h: already on"
else
  echo "   config.h: HAVE_REALDEVICE not found - tree not configured?"
  exit 1
fi




aggiungi() {
  dir="$1"; lib="$2"; obj="$3"
  m="$V/src/$dir/Makefile"
  if [ ! -f "$m" ]; then
    echo "   $dir/Makefile missing - skipped"
    return
  fi
  if grep -q "^${lib}_a_LIBADD = ${obj}.o" "$m"; then
    echo "   $dir: already on"
    return
  fi
  sed -i "s|^${lib}_a_DEPENDENCIES = *\$|${lib}_a_DEPENDENCIES = ${obj}.o|" "$m"
  sed -i "s|^${lib}_a_LIBADD = *\$|${lib}_a_LIBADD = ${obj}.o|" "$m"
  if grep -q "^${lib}_a_LIBADD = ${obj}.o" "$m"; then
    echo "   $dir: ${obj}.o added to ${lib}.a"
  else
    echo "   $dir: COULD NOT add ${obj}.o"
    exit 1
  fi
}

aggiungi serial    libserial    realdevice
aggiungi diskimage libdiskimage realimage
