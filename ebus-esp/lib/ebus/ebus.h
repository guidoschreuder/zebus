#ifndef __EBUS
#define __EBUS

unsigned char crc8_calc(unsigned char data, unsigned char crc_init);
unsigned char crc8_array(unsigned char data[], unsigned int length);

#endif
