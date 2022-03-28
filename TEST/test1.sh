#!/bin/bash

#BIN=./BIN
#TEST=./TEST
BIN=../BIN
FILES=$BIN/FILES
READ=$BIN/READ
TRASH=$BIN/TRASH

F=( $(ls /home/oneclaudio/Desktop/SOL/PROGETTO/OneClaDrive/BIN/FILES) )

#RF="$(shuf -n10 $LIST)"	#RANDOM FILE NAME
#RF="$FILES/$RFN"			#RANDOM FILE PATH
#F=( $RF )



CLIENT="$BIN/client -p -t 200"

$CLIENT -h -W ${F[1]},${F[2]},${F[3]},${F[4]} -l ${F[1]},${F[2]} -a ${F[9]},${F[1]} -c ${F[2]} -u ${F[1]},${F[2]}

$CLIENT -w $FILES,5 -c ${F[7]},${F[1]},${F[4]} -l ${F[4]} -c ${F[4]}

$CLIENT -w 0 -R4 -l ${F[1]} -u ${F[8]} -R4
