#ifndef _MCU_FLASH__H
#define _MCU_FLASH__H


void MCU_Flash_Erase(uint32_t start_addr, uint32_t end_size);
uint8_t MCU_Flash_Write(uint32_t addr, uint8_t *data, uint32_t len);

#endif