# Makefile for ftdi i2c driver

ALL: i2csend i2cget

i2csend: i2csend.c
	gcc -o i2csend i2csend.c -lftdi

i2cget: i2cget.c
	gcc -o i2cget i2cget.c -lftdi



