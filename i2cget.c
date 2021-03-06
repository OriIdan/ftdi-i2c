/*
 | I2C get using libftdi and FT4232 chip connected to USB.
 | Note that this utility will open the first FT4232 chip found.
 |
 | Written by Ori Idan, Helicon technologies LTD. (ori@helicontech.co.il)
 |
 | i2cget is free software: you can redistribute it and/or modify it
 | under the terms of the GNU General Public License as published by the
 | Free Software Foundation, either version 3 of the License, or
 | (at your option) any later version.
 | 
 | i2cget is distributed in the hope that it will be useful, but
 | WITHOUT ANY WARRANTY; without even the implied warranty of
 | MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 | See the GNU General Public License for more details.
 | 
 | You should have received a copy of the GNU General Public License along
 | with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <ctype.h>
#include <ftdi.h>

/*
 | Globals and constants
 */
const unsigned char MSB_FALLING_EDGE_CLOCK_BYTE_IN = '\x24';
const unsigned char MSB_FALLING_EDGE_CLOCK_BYTE_OUT = '\x11';
const unsigned char MSB_RISING_EDGE_CLOCK_BIT_IN = '\x22';
struct ftdi_context ftdic;
unsigned char OutputBuffer[1024]; // Buffer to hold MPSSE commands and data to be sent to FT4232H
unsigned char InputBuffer[1024];  // Buffer to hold Data unsigned chars to be read from FT4232H
unsigned int dwClockDivisor = 0x0095; // Value of clock divisor, SCL Frequency = 60/((1+0x0095)*2) (MHz) = 200khz
unsigned int dwNumBytesToSend = 0; // Index of output buffer
unsigned int dwNumBytesSent = 0, dwNumBytesRead = 0, dwNumInputBuffer = 0;
int chan;
unsigned char gpio;
int debug = 0;	// Debug mode

/*
 | HighSpeedSetI2CStart:
 | Generate start condition for I2C bus.
 | Set SDA and SCL high.
 | Set SDA low (while SCL remains high)
 | Set SCL low
 */
void HighSpeedSetI2CStart(void) {
	unsigned int dwCount;

	// Repeat commands to ensure the minimum period of the start hold time ie 600ns is achieved
	for(dwCount=0; dwCount < 4; dwCount++)  {		
		//Command to set directions of lower 8 pins and force value on bits set as output
		OutputBuffer[dwNumBytesToSend++] = '\x80'; 
		//Set SDA, SCL high, GPIOL0 low
		OutputBuffer[dwNumBytesToSend++] = '\x03' || (gpio << 4); 
		//Set SK,DO,GPIOL0 pins as output 
		OutputBuffer[dwNumBytesToSend++] = '\xF3';
	}
	
	// Repeat commands to ensure the minimum period of the start setup time ie 600ns is achieved
	for(dwCount=0; dwCount < 4; dwCount++) {
		//Command to set directions of lower 8 pins and force value on bits set as output
		OutputBuffer[dwNumBytesToSend++] = '\x80'; 
		//Set SDA low, SCL high, GPIOL0 low
		OutputBuffer[dwNumBytesToSend++] = '\x01' | (gpio << 4); 
		//Set SK,DO,GPIOL0 pins as output 
		OutputBuffer[dwNumBytesToSend++] = '\xF3'; 
	}
	//Command to set directions of lower 8 pins and force value on bits set as output
	OutputBuffer[dwNumBytesToSend++] = '\x80';
	//Set SDA, SCL low, GPIOL0 low
	OutputBuffer[dwNumBytesToSend++] = '\x00' | (gpio << 4); 
	//Set SK,DO,GPIOL0 pins as output with bit „1‟, other pins as input with bit „0‟
	OutputBuffer[dwNumBytesToSend++] = '\xF3'; 
}

/*
 | HighSpeedSetI2CStop:
 | Generate stop condition for I2C bus.
 | Set SDA low, SCL high.
 | Set SDA high (while SCL remains high)
 | Release both pins by setting them to input mode so they are in tristate (high impidance)
 */
void HighSpeedSetI2CStop(void) {
	int dwCount;
	
	// Repeat commands to ensure the minimum period of the stop setup time ie 600ns is achieved
	for(dwCount=0; dwCount<4; dwCount++) {
		//Command to set directions of lower 8 pins and force value on bits set as output
		OutputBuffer[dwNumBytesToSend++] = '\x80';
		//Set SDA low, SCL high, GPIOL0 low
		OutputBuffer[dwNumBytesToSend++] = '\x01' | (gpio << 4); 
		//Set SK,DO,GPIOL0 pins as output
		OutputBuffer[dwNumBytesToSend++] = '\xF3'; 
	}

	// Repeat commands to ensure the minimum period of the stop hold time ie 600ns is achieved
	for(dwCount=0; dwCount<4; dwCount++) {
		//Command to set directions of lower 8 pins and force value on bits set as output
		OutputBuffer[dwNumBytesToSend++] = '\x80'; 
		//Set SDA, SCL high, GPIOL0 low
		OutputBuffer[dwNumBytesToSend++] = '\x03' | (gpio << 4); 
		//Set SK,DO,GPIOL0 pins as output
		OutputBuffer[dwNumBytesToSend++] = '\xF3'; 
	}

	//Tristate the SCL, SDA pins
	//Command to set directions of lower 8 pins and force value on bits set as output
	OutputBuffer[dwNumBytesToSend++] = '\x80'; 
	OutputBuffer[dwNumBytesToSend++] = '\x00' | (gpio << 4); 
	OutputBuffer[dwNumBytesToSend++] = '\xF0'; 
}

/*
 | SendByteAndCheckACK:
 | Send byte and check ACK
 */
int SendByteAndCheckACK(unsigned char DataSend) {
	int ftStatus = 0;
	int r;
	
	// Clock data byte out on –ve Clock Edge MSB first
	OutputBuffer[dwNumBytesToSend++] = MSB_FALLING_EDGE_CLOCK_BYTE_OUT; 
	OutputBuffer[dwNumBytesToSend++] = '\x00';
	OutputBuffer[dwNumBytesToSend++] = '\x00'; //Data length of 0x0000 means 1 byte data to clock out
	OutputBuffer[dwNumBytesToSend++] = DataSend; //Add data to be send
	// Get Acknowledge bit
	// Command to set directions of lower 8 pins and force value on bits set as output
	OutputBuffer[dwNumBytesToSend++] = '\x80'; 
	OutputBuffer[dwNumBytesToSend++] = '\x00' | (gpio << 4); // Set SCL low, 
	//Set SK, GPIOL0 pins as output 
	OutputBuffer[dwNumBytesToSend++] = '\xF1';
	//Command to scan in ACK bit , -ve clock Edge MSB first
	OutputBuffer[dwNumBytesToSend++] = MSB_RISING_EDGE_CLOCK_BIT_IN;
	OutputBuffer[dwNumBytesToSend++] = '\x0';  //Length of 0x0 means to scan in 1 bit
	OutputBuffer[dwNumBytesToSend++] = '\x87'; //Send answer back immediate command
	dwNumBytesSent = ftdi_write_data(&ftdic, OutputBuffer, dwNumBytesToSend);
	dwNumBytesToSend = 0;
	// Send off the commands
	// Clear output buffer
	// Check if ACK bit received, may need to read more times to get ACK bit or fail if timeout
	dwNumBytesRead = ftdi_read_data(&ftdic, InputBuffer, 1);
	// Read one byte from device receive buffer
	if(dwNumBytesRead == 0) {
		return 0; /* Error reading bit, should not happened if we are connected to FTDI */
	}
	else if(!(InputBuffer[0] & 0x01)) {
		r = 1;
		// Check ACK bit 0 on data byte read out
	}
	else
		r = 0;
	if(debug)
		printf("Received: %d, %02X\n", dwNumBytesRead, InputBuffer[0]);
	//Command to set directions of lower 8 pins and force value on bits set as output
	OutputBuffer[dwNumBytesToSend++] = '\x80'; 
	// Set SDA high, SCL low
	OutputBuffer[dwNumBytesToSend++] = '\x02' | (gpio << 4); 
	//Set SK,DO,GPIOL0 pins as output
	OutputBuffer[dwNumBytesToSend++] = '\xF3'; 
	return r;
}

/*
 | ReadByte:
 | Read I2C byte.
 | Note that read address must be sent beforehand
 */
unsigned char ReadByte(void) {
	// Command to set directions of lower 8 pins and force value on bits set as output
	OutputBuffer[dwNumBytesToSend++] = '\x80'; 
	// Set SCL low
	OutputBuffer[dwNumBytesToSend++] = '\x00' | (gpio << 4); 
	// Set SK, GPIOL0 pins as output
	OutputBuffer[dwNumBytesToSend++] = '\xF1'; 
	// Command to clock data byte in on –ve Clock Edge MSB first
	OutputBuffer[dwNumBytesToSend++] = MSB_FALLING_EDGE_CLOCK_BYTE_IN; 
	OutputBuffer[dwNumBytesToSend++] = '\x00';
	OutputBuffer[dwNumBytesToSend++] = '\x00';
	// Data length of 0x0000 means 1 byte data to clock in
	// Command to scan in acknowledge bit , -ve clock Edge MSB first
	OutputBuffer[dwNumBytesToSend++] = MSB_RISING_EDGE_CLOCK_BIT_IN; 
	OutputBuffer[dwNumBytesToSend++] = '\x0';  // Length of 0 means to scan in 1 bit
	OutputBuffer[dwNumBytesToSend++] = '\x87'; // Send answer back immediate command
	dwNumBytesSent = ftdi_write_data(&ftdic, OutputBuffer, dwNumBytesToSend); // Send off the commands
	dwNumBytesToSend = 0;	// Clear output buffer
	// Read two bytes from device receive buffer, first byte is data read, second byte is ACK bit
	dwNumBytesRead = ftdi_read_data(&ftdic, InputBuffer, 2);
	if(dwNumBytesRead < 2) {
		printf("Error reading i2c\n");
		return 0xFF;
	}
	if(debug)
		printf("Data read: %02X\n", InputBuffer[0]);	
	return InputBuffer[0];
}

/*
 | ReadBytes:
 | Read I2C bytes.
 | Note that read address must be sent beforehand
 */
void ReadBytes(char * readBuffer, unsigned int readLength) {
    unsigned int clock = 60 * 1000/(1+dwClockDivisor)/2; // K Hz
    const int loopCount = (int)(10 * ((float)200/clock));
    unsigned int readCount = 0;
    int i = 0;  // Used only for loop
    if (!readBuffer || !readLength) {
        return;
    }
    
    while(readCount != (readLength -1))
    {
        // Command of read one byte
        OutputBuffer[dwNumBytesToSend++] = '\x80'; //Command to set directions of lower 8 pins and force value on bits set as output
        OutputBuffer[dwNumBytesToSend++] = '\x00'; //Set SCL low, WP disabled by SK, GPIOL0 at bit „‟
        OutputBuffer[dwNumBytesToSend++] = '\x11'; //Set SK, GPIOL0 pins as output with bit ‟‟, DO and other pins as input with bit „‟
        OutputBuffer[dwNumBytesToSend++] = MSB_FALLING_EDGE_CLOCK_BYTE_IN; //Command to clock data byte in on –ve Clock Edge MSB first
        OutputBuffer[dwNumBytesToSend++] = '\x00';
        OutputBuffer[dwNumBytesToSend++] = '\x00'; //Data length of 0x0000 means 1 byte data to clock in
        
        // Set ACK
        for (i=0; i != loopCount; ++i)
        {
            OutputBuffer[dwNumBytesToSend++] = '\x80';
            OutputBuffer[dwNumBytesToSend++] = '\x00';  // SDA and SCL Low
            OutputBuffer[dwNumBytesToSend++] = '\x13';
        }

        for (i=0; i != loopCount; ++i)
        {
            OutputBuffer[dwNumBytesToSend++] = '\x80';
            OutputBuffer[dwNumBytesToSend++] = '\x01';  // SDA Low, SCL High
            OutputBuffer[dwNumBytesToSend++] = '\x13';
        }

        for (i=0; i != loopCount; ++i)
        {
            OutputBuffer[dwNumBytesToSend++] = '\x80';
            OutputBuffer[dwNumBytesToSend++] = '\x02';  // SDA High, SCL Low
            OutputBuffer[dwNumBytesToSend++] = '\x13';
        }
        ftdi_write_data(&ftdic, OutputBuffer, dwNumBytesToSend);
        dwNumBytesToSend = 0;
        ++readCount;
    }
    
    // The last byte is read with NO ACK.
    // Command of read one byte
    OutputBuffer[dwNumBytesToSend++] = '\x80'; //Command to set directions of lower 8 pins and force value on bits set as output
    OutputBuffer[dwNumBytesToSend++] = '\x00'; //Set SCL low, WP disabled by SK, GPIOL0 at bit „‟
    OutputBuffer[dwNumBytesToSend++] = '\x11'; //Set SK, GPIOL0 pins as output with bit ‟‟, DO and other pins as input with bit „‟
    OutputBuffer[dwNumBytesToSend++] = MSB_FALLING_EDGE_CLOCK_BYTE_IN; //Command to clock data byte in on –ve Clock Edge MSB first
    OutputBuffer[dwNumBytesToSend++] = '\x00';
    OutputBuffer[dwNumBytesToSend++] = '\x00'; //Data length of 0x0000 means 1 byte data to clock in
    
    // Set NO ACK
    for (i=0; i != loopCount; ++i)
    {
        OutputBuffer[dwNumBytesToSend++] = '\x80';
        OutputBuffer[dwNumBytesToSend++] = '\x02'; // SDA High SCL Low
        OutputBuffer[dwNumBytesToSend++] = '\x13';
    }

    for (i=0; i != loopCount; ++i)
    {
        OutputBuffer[dwNumBytesToSend++] = '\x80';
        OutputBuffer[dwNumBytesToSend++] = '\x03'; // SDA High, SCL High
        OutputBuffer[dwNumBytesToSend++] = '\x13';
    }

    for (i=0; i != loopCount; ++i)
    {
        OutputBuffer[dwNumBytesToSend++] = '\x80';
        OutputBuffer[dwNumBytesToSend++] = '\x02'; // SDA High, SCL Low
        OutputBuffer[dwNumBytesToSend++] = '\x13';
    }
    ftdi_write_data(&ftdic, OutputBuffer, dwNumBytesToSend);
    dwNumBytesToSend = 0;
        
    // Read bytes from device receive buffer, first byte is data read, second byte is ACK bit
	dwNumBytesRead = ftdi_read_data(&ftdic, readBuffer, readLength);
    
    if(dwNumBytesRead != readLength) {
		printf("Error reading i2c\n");
		return;
	}
    
    if(debug) {
        for(i=0; i != readLength; ++i) {
            printf("Data read: %02X\n", readBuffer[i]);
        }
    }
		
    return;
}

/*
 | Open FT4232 device and get valid handle for subsequent access.
 | Note that this function initialize the ftdic struct used by other functions.
 */
int InitializeI2C(int chan, unsigned char gpio) {
	unsigned int dwCount;
	char SerialNumBuf[64];
	int bCommandEchoed;
	int ftStatus = 0;
	int i;
	
	ftStatus = ftdi_init(&ftdic);
	if(ftStatus < 0) {
		printf("ftdi init failed\n");
		return 0;
	}
	ftStatus = ftdi_usb_open(&ftdic, 0x0403, 0x6011);
	if(ftStatus < 0) {
		printf("Error opening usb device: %s\n", ftdi_get_error_string(&ftdic));
		return 1;
	}
	
	// Port opened successfully
	if(debug)
		printf("Port opened, resetting device...\n");
	ftStatus |= ftdi_usb_reset(&ftdic); 			// Reset USB device
	ftStatus |= ftdi_usb_purge_rx_buffer(&ftdic);	// purge rx buffer
	ftStatus |= ftdi_usb_purge_tx_buffer(&ftdic);	// purge tx buffer
	/* Set MPSSE mode */
	ftdi_set_bitmode(&ftdic, 0xFF, BITMODE_RESET);
	ftdi_set_bitmode(&ftdic, 0xFF, BITMODE_MPSSE);
	/*
	 | Below code will synchronize the MPSSE interface by sending bad command 0xAA
	 | response should be echo command followed by bad command 0xAA.
	 | This will make sure the MPSSE interface enabled and synchronized successfully
	 */
	OutputBuffer[dwNumBytesToSend++] = '\xAA';	// Add BAD command 0xAA
	dwNumBytesSent = ftdi_write_data(&ftdic, OutputBuffer, dwNumBytesToSend);
	dwNumBytesToSend = 0;
	i = 0;
	do {
		dwNumBytesRead = ftdi_read_data(&ftdic, InputBuffer, 2);
		if(dwNumBytesRead < 2) {
			if(debug)
				printf("Error: %s\n", ftdi_get_error_string(&ftdic));
			break;
		}
		if(debug)
			printf("Got %d bytes %02X %02X\n", dwNumBytesRead, InputBuffer[0], InputBuffer[1]);
		if(++i > 5)	/* up to 5 times read */
			break;
	} while (dwNumBytesRead == 0);
	// Check if echo command and bad received
	for (dwCount = 0; dwCount < dwNumBytesRead; dwCount++) {
		if ((InputBuffer[dwCount] == 0xFA) && (InputBuffer[dwCount+1] == 0xAA)) {
			if(debug)
				printf("FTDI synchronized\n");
			bCommandEchoed = 1;
			break;
		}
	}
	if (bCommandEchoed == 0) {
		return 1;
		/* Error, cant receive echo command , fail to synchronize MPSSE interface. */ 
	}

	OutputBuffer[dwNumBytesToSend++] = '\x8A'; //Ensure disable clock divide by 5 for 60Mhz master clock
	OutputBuffer[dwNumBytesToSend++] = '\x97';
	// Ensure turn off adaptive clocking
	// Enable 3 phase data clock, used by I2C to allow data on both clock edges
	OutputBuffer[dwNumBytesToSend++] = '\x8D'; 
	dwNumBytesSent = ftdi_write_data(&ftdic, OutputBuffer, dwNumBytesToSend);	// Send off the commands
	dwNumBytesToSend = 0;	//Clear output buffer
	OutputBuffer[dwNumBytesToSend++] = '\x80'; // Command to set directions of lower 8 pins and force value on 	bits set as output
	OutputBuffer[dwNumBytesToSend++] = '\x03' | (gpio << 4); // Set SDA, SCL high
	OutputBuffer[dwNumBytesToSend++] = '\xF3'; // Set SK,DO DI as outputs
	// The SK clock frequency can be worked out by below algorithm with divide by 5 set as off
	// SK frequency = 60MHz /((1 + [(1 +0xValueH*256) OR 0xValueL])*2)
	OutputBuffer[dwNumBytesToSend++] = '\x86'; // Command to set clock divisor
	OutputBuffer[dwNumBytesToSend++] = dwClockDivisor & '\xFF'; //Set 0xValueL of clock divisor
	OutputBuffer[dwNumBytesToSend++] = (dwClockDivisor >> 8) & '\xFF';
	// Set ValueH of clock divisor
	dwNumBytesSent = ftdi_write_data(&ftdic, OutputBuffer, dwNumBytesToSend);
	dwNumBytesToSend = 0; //Clear output buffer
	OutputBuffer[dwNumBytesToSend++] = '\x85'; // Turn off loop back in case
	//Command to turn off loop back of TDI/TDO connection
	dwNumBytesSent = ftdi_write_data(&ftdic, OutputBuffer, dwNumBytesToSend);
	dwNumBytesToSend = 0; 
	return 0;
}

int main(int argc, char *argv[]) {
	int i, a;
	char *s;
	int b = 0;
	int addr;

	if(argc < 2) {
		printf("i2cget: get data from i2c bus using ftdi F4232H I2C\n");
		printf("Written by: Ori Idan Helicon technologies ltd. (ori@helicontech.co.il)\n\n");
		printf("usage: i2cget [-c <chan>] [-g <gpio state>] <adress> <data>\n");
		return 1;
	}
	for(a = 1; a < argc; a++) {
		s = argv[a];
		if(*s == '-') {	/* This is a command line option */
			s++;
			a++;
			if(*s == 'c')
				chan = atoi(argv[a]);
			else if(*s == 'g')
				gpio = atoi(argv[a]);
			else {
				printf("Unknown option -%c\n", *s);
				exit;
			}
		}
		else
			break;
	}
	InitializeI2C(chan, gpio);

	s = argv[1];
	b = 0;
	if(*s == '0')
		s++;
	if(*s == 'x')
		s++;
	while(*s) {	
		if(!isxdigit(*s)) {
			printf("%c Invalid hex value: %s\n", *s, argv[i]);
			break;
		}
		b *= 16;
		*s = toupper(*s);
		if(*s >= 'A')
			b += (*s - 'A' + 10);
		else
			b += (*s - '0');
		s++;
	}
	b = b << 1;	/* R/W bit should be 1 */
	b |= 0x01;
	addr = b;
	if(argv[2] != NULL) {
		i = atoi(argv[2]);
		if(i <= 0)
			i = 1;
	}
	else
		i = 1;
	for( ; i > 0; i--) {
		HighSpeedSetI2CStart();
		if(debug)
			printf("Sending %02X\n", addr);
		b = SendByteAndCheckACK((unsigned char)addr);
		if(debug) {
			if(b)
				printf("Received ACK\n");
			else
				printf("Error reading ACK\n");
		}
		b = ReadByte();
		printf("0x%02X ", b);
		HighSpeedSetI2CStop();
		ftdi_write_data(&ftdic, OutputBuffer, dwNumBytesToSend);
		dwNumBytesToSend = 0;
	}

	ftdi_usb_close(&ftdic);
    ftdi_deinit(&ftdic);
    printf("\n");
	return 0;
}

