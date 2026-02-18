#!/bin/bash

set -euo pipefail

if ! command -v modetest >/dev/null; then
  echo "FAIL: modetest not found (install libdrm-tests)"
  exit 1
fi

plat=$(cat /proc/device-tree/model)
echo "Running in $plat - will check if sdtv is configured correctly"

CFG=/boot/firmware/config.txt

if [ ! -f "$CFG" ]; then
  echo "FAIL: $CFG not found - is this a raspberry pi?"
  exit 1
fi

ok=true

check() {
  if grep -q "^$1" "$CFG"; then
    echo "OK:   $(grep "^$1" "$CFG")"
  else
    echo "MISS: $1 not found"
    ok=false
  fi
}

# sdtv_mode: 0=NTSC, 1=Japanese NTSC, 2=PAL, 3=Brazilian PAL
check "sdtv_mode="
# sdtv_aspect: 1=4:3, 2=14:9, 3=16:9
check "sdtv_aspect="
check "enable_tvout=1"
check "dtoverlay=vc4-fkms-v3d"

if $ok; then
  echo "$CFG looks fine."
else
  echo "Some settings are missing. Edit $CFG and reboot."
  exit 0
fi

conn_id=$(modetest -M vc4 -c 2>/dev/null | grep -i "tv\|composite\|sdtv" | awk '{print $1}')
if [ -n "$conn_id" ]; then
  echo "OK:   modetest shows a TV/composite connector (id=$conn_id)"
  echo "Test signal with: modetest -M vc4 -s ${conn_id}:#0"
else
  echo "MISS: no TV/composite connector found in modetest output"
fi

