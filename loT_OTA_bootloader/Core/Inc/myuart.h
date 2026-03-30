#ifndef __MYUART_H
#define __MYUART_H

void put_char(char c);
void put_str(const char* str,uint8_t len);
void put_hex(uint32_t val,uint8_t len);

#endif