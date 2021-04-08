#ifndef _SERIAL_H_
#define _SERIAL_H_

#define PORT_COM1       0x3F8

void write_serial(char c);
void init_serial();

#endif
