#!/bin/sh
mipsel-linux-android-objdump -D ../../spl/u-boot-spl > ../../palladium.dump
./btow ../../spl/u-boot-spl.bin ../../palladium.txt
cd ../../
ls palladium.txt palladium.dump
#myscp paladin.dump /home/user/work/paladin
#myscp paladin.txt /home/user/work/paladin
#upload paladin.dump paladin
#upload paladin.txt paladin
