#!/bin/sh
ifconfig eth0 193.169.4.151
route add default gw 193.169.4.1

cp /system/init/setir  /tmp/

echo 1 > /proc/sys/vm/overcommit_memory

cd /system/init/
/system/init/start.sh start_param


