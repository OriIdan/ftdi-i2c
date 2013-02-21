# Makefile for ftdi i2c driver

ALL: i2csend i2cget

i2csend: i2csend.c
	gcc `pkg-config --cflags libftdi`  -o i2csend  i2csend.c  `pkg-config --libs libftdi`

i2cget: i2cget.c
	gcc `pkg-config --cflags libftdi`  -o i2cget  i2cget.c  `pkg-config --libs libftdi`



