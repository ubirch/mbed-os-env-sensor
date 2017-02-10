#! /bin/bash
BASE=`dirname $0`
if [ "$1" = "-j" ]; then
  JLINK=`which JLinkExe`
  if [ "${JLINK}"x = "x" ]; then
    echo "JLinkExe not found, please ensure its in the PATH"
    exit 1
  else
    ${JLINK} -if SWD -device MK82FN256xxx15 -speed 4000 $BASE/flash.jlink
  fi
else
  HOST=`uname`
  BLBASE=$BASE/blhost
  BLHOST=$([ "$HOST" == "Darwin" ] && echo "$BLBASE/mac/blhost" || echo "$BLBASE/linux/amd64/blhost")
  ${BLHOST} -u -- flash-erase-all
  [ $? -eq 0 ] && ${BLHOST} -u -- write-memory 0x0 ./BUILD/UBIRCH1/GCC_ARM/mbed-os-env-sensor.bin
  [ $? -eq 0 ] && ${BLHOST} -u -- reset
fi
