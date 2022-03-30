#!/bin/bash

BIN=./BIN			#MODIFY THIS if you are MOVING the TEST SCRIPTS
FILES=$BIN/FILES
READ=$BIN/READ
TRASH=$BIN/TRASH

F=($FILES/*)
F=( $(shuf -e "${F[@]}") )

CLIENT="$BIN/client -p -t 200 -f $BIN/serversocket"

sleep 1 #Just to be sure that the served is ready

$CLIENT -D "$TRASH" -h -W ${F[1]},${F[2]},${F[3]},${F[4]} -l ${F[1]},${F[2]} -a ${F[9]},${F[1]} -c ${F[2]} -u ${F[1]},${F[2]}

$CLIENT -w $FILES,5 -c ${F[7]},${F[1]},${F[4]} -l ${F[4]} -c ${F[4]}

$CLIENT -d $READ -w 0 -R4 -l ${F[1]} -u ${F[8]} -R4
