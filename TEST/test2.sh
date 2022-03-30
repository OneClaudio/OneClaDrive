#!/bin/bash

BIN=./BIN			#MODIFY THIS if you are MOVING the TEST SCRIPTS
FILES=$BIN/FILES
READ=$BIN/READ
TRASH=$BIN/TRASH

F=($FILES/*)
F=( $(shuf -e "${F[@]}") )

CLIENT="$BIN/client -p -f $BIN/serversocket"

sleep 1 #Just to be sure that the served is ready

$CLIENT -D $TRASH -W ${F[1]},${F[2]},${F[3]},${F[4]} -l ${F[1]},${F[2]} -a ${F[9]},${F[1]} -c ${F[2]} -u ${F[1]},${F[2]} &

$CLIENT -D $TRASH -W ${F[10]},${F[7]},${F[3]},${F[4]} -l ${F[4]},${F[13]},${F[2]} -w -a ${F[13]},${F[4]} -c ${F[2]} &

$CLIENT -D $TRASH -w $FILES,5 -R7 -w $FILES &

$CLIENT -D $TRASH -d $READ -l ${F[10]},${F[3]},${F[4]},${F[7]},${F[1]} -w -u ${F[10]},${F[3]},${F[4]},${F[7]},${F[1]} -R

