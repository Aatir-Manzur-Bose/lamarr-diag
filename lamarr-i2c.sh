#!/bin/sh
#Lamarr I2c Test
. uitls
ti() {
	BUS=$1
	DEV=$2
	REG=$3
	DEVICENAME=$4
	EXPECTEDVALUE=$5

	VAL=$(i2cget -y $BUS $DEV $REG 2>&1)
	if test -n "$EXPECTEDVALUE" ; then test $VAL = $EXPECTEDVAL; RC=$? ; fi
	test $RC = 0 log $DEVICENAME okay || log $DEVICENAME failed
}

test_all() {
	while read BUS DEV REG DEVICENAME EXPECTEDVALUE; do
		ti $BUS $DEV $REG $DEVICENAME $EXPECTEDVALUE;
	done
}

test -e i2cdevs || fatal "Missing i2cdevs file"
iterations=1
while getopts n: flag
do
	case "${flag}" in
		n) iterations=${OPTARG};;
	esac
done

seq 1 $iterations | while read x; do
 	test_all < i2cdevs
done
