#!/bin/sh
#Lamarr I2c Test
fatal() {
	echo "$@"
	exit 1
}

ti() {
	BUS=$1
	DEV=$2
	REG=$3
	DEVICENAME=$4
	EXPECTEDVALUE=$5

	VAL=$(i2cget -y $BUS $DEV $REG 2>&1)
	if test -n "$EXPECTEDVALUE" ; then test $VAL = $EXPECTEDVAL; RC=$? ; fi
	test $RC = 0 echo $DEVICENAME okay || echo $DEVICENAME failed
}

test -e i2cdevs || fatal "Missing i2cdevs file"
while read BUS DEV REG; do ti $BUS $DEV $REG $DEVICENAME $EXPECTEDVALUE; done;<i2cdevs

while read BUS ADDR REG DEVICENAME; do if i2cget -y $BUS $ADDR $REG $DEVICENAME; then echo OK ; else echo DEVICENAME FAILED; fi; done << EOF
0 42 35 yolo
0 55 35 yolo1
EOF

