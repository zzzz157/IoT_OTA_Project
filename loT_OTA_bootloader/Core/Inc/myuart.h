#ifndef __MYUART_H
#define __MYUART_H
#include <stdint.h>

void put_char(char c);
void put_str(char* str);
void put_hex(uint32_t val,uint8_t len);

#endif