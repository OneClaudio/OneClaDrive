#!/bin/bash

BIN=./BIN			#MODIFY THIS if you are MOVING the TEST SCRIPTS
FILES=$BIN/FILES
READ=$BIN/READ
TRASH=$BIN/TRASH

F=($FILES/*)
F=( $(shuf -e "${F[@]}") )

CLIENT="$BIN/client -f $BIN/serversocket"

sleep 1 #Just to be sure that the served is ready

END=$((SECONDS + 30))

while [ $SECONDS -lt $END ]; do

	$CLIENT -D $TRASH -W ${F[13]},${F[22]},${F[5]},${F[7]},${F[4]},${F[13]},${F[2]} -l ${F[13]},${F[22]} -c ${F[13]},${F[5]} -u ${F[22]} &	

	$CLIENT -D $TRASH -W ${F[10]},${F[7]},${F[3]},${F[4]},${F[16]},${F[17]},${F[18]} -r ${F[10]},${F[7]},${F[3]},${F[4]},${F[16]},${F[17]},${F[18]} &

	$CLIENT -D $TRASH -w $FILES,15 -R7 &

	$CLIENT -d $READ  -W ${F[9]},${F[3]},${F[5]},${F[1]},${F[11]} l- ${F[3]},${F[4]} -c ${F[3]} -r ${F[4]} -u ${F[4]} &

	$CLIENT -d $READ  -R &
	
	sleep 0.5

	done

#N=$(ls $FILES | wc -l)	#Could use something like this
#printf "%s\n" "$N"		#To completely randomize each chosen file
#R=$(($RANDOM % $N))
#printf "%s\n" "$R"

