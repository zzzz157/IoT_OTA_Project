#ifndef _MCU_FLASH__H
#define _MCU_FLASH__H

#define FLASH_START_ADDR 	0x08000000
#define FLASH_END_ADDR 		0x0807FFFF

void MCU_Flash_Erase(uint32_t start_addr, uint32_t end_size);
uint8_t MCU_Flash_Write(uint32_t addr, uint8_t *data, uint32_t len);
int MCU_Flash_Read(uint32_t start_addr, uint8_t *data, uint32_t len);

#endif