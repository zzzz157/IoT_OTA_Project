#include "usart.h"

void put_char(char c)
{
	HAL_UART_Transmit(&huart6,(uint8_t*)&c,1,10);
}
void put_str(char* str,uint8_t len)
{
	HAL_UART_Transmit(&huart6,(uint8_t*)str,len,10);
}
void put_hex(uint32_t val,uint8_t len)
{
	char buf[12];
    uint8_t i;
    len = (len > 4) ? 4 : len;
    if (len == 0) return;
    buf[0] = '0';
    buf[1] = 'x';
    for (i = 0; i < len * 2; i++) 
	{
        uint8_t nibble = (val >> ((len * 2 - 1 - i) * 4)) & 0x0F;
        buf[2 + i] = (nibble < 10) ? (nibble + '0') : (nibble - 10 + 'A');
    }
    buf[2 + len * 2] = '\0';
    HAL_UART_Transmit(&huart6, (uint8_t*)buf, 2 + len * 2, 100);
}
