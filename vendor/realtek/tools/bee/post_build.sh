#!/bin/sh


WORKDIR=$(cd $(dirname $0); pwd)
OUTDIR=`readlink -f $1`
echo ${WORKDIR}
echo ${OUTDIR}
cd ${WORKDIR}

mkdir -p application_is/Debug/bin/
cp $OUTDIR/nuttx application_is/Debug/bin/application_is.elf
TARGET=application_is/Debug/bin/application_is
arm-none-eabi-objcopy -O ihex ${TARGET}.elf ${TARGET}.hex

./Hex2Bin ${TARGET}.hex ${TARGET}.bin bee3
./prepend_header -t app_code -p ${TARGET}.bin -m 1 -i "mp.ini" -c crc -a ../../../tool/key.json
./MD5 ${TARGET}_MP.bin
arm-none-eabi-objdump -D -S ${TARGET}.elf > ${TARGET}.dis


