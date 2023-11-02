#!/bin/sh
#Lamarr I2c Test
source ./utils
ti() {
        BUS=$1
        DEV=$2
        REG=$3
        DEVICENAME=$4
        EXPECTEDVALUE=$5

        VAL=$(i2cget -y $BUS $DEV $REG 2>&1)
        RC=$?
        echo $VAL val
        echo $EXPECTEDVALUE
        if test -n "$EXPECTEDVALUE"; then test $VAL = $EXPECTEDVALUE; RC=$?; fi
        if test $RC = 0; then
                log $DEVICENAME okay
        else
                log $DEVICENAME failed
        fi
}

test_all() {
        while read BUS DEV REG DEVICENAME EXPECTEDVALUE; do
                ti $BUS $DEV $REG $DEVICENAME $EXPECTEDVALUE;
        done
}

show_help() {
        echo " USAGE: lamar-i2c [-n <no of iterations to run>]
Make sure devices are setup under i2cdevs
All Logs stored in log file"
}

test -e i2cdevs || fatal "Missing i2cdevs file"
iterations=1
while getopts n:h flag
do
        case "${flag}" in
                n) iterations=${OPTARG};;
                h|\?) show_help
                exit 0
                ;;
        esac
done

seq 1 $iterations | while read x; do
        test_all < i2cdevs
done
