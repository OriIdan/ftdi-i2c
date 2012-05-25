# Makefile for ftdi i2c driver

ALL: i2csend i2cget

i2csend: i2csend.c
	gcc -lftdi -o i2csend i2csend.c

i2cget: i2cget.c
	gcc -lftdi -o i2cget i2cget.c


