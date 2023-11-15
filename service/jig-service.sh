#!/bin/bash

#--------------------------
# delay for system stability
#--------------------------
sleep 10 && sync

#--------------------------
# ODROID-M1S Server enable (iperf3 odroid iperf3 demon mode)
#--------------------------
/usr/bin/iperf3_odroid -s -p 8000 &

#--------------------------
# ODROID-M1S Server app
#--------------------------
/root/JIG.m1s.server/JIG.m1s.server > /dev/null 2>&1
