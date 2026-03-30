#include "main.h"
#include <string.h>

static uint32_t GetSector(uint32_t Address)
{
    if(Address < 0x08004000) return FLASH_SECTOR_0;  // 16 KB
    if(Address < 0x08008000) return FLASH_SECTOR_1;  // 16 KB
    if(Address < 0x0800C000) return FLASH_SECTOR_2;  // 16 KB
    if(Address < 0x08010000) return FLASH_SECTOR_3;  // 16 KB
    if(Address < 0x08020000) return FLASH_SECTOR_4;  // 64 KB
    if(Address < 0x08040000) return FLASH_SECTOR_5;  // 128 KB
    if(Address < 0x08060000) return FLASH_SECTOR_6;  // 128 KB
    if(Address < 0x08080000) return FLASH_SECTOR_7;  // 128 KB
    if(Address < 0x080A0000) return FLASH_SECTOR_8;  // 128 KB
    if(Address < 0x080C0000) return FLASH_SECTOR_9;  // 128 KB
    if(Address < 0x080E0000) return FLASH_SECTOR_10; // 128 KB
    return FLASH_SECTOR_11;                          // 128 KB
}
static uint32_t GetSectorIndex(uint32_t Address)
{
    if(Address < 0x08004000) return 0;
    if(Address < 0x08008000) return 1;
    if(Address < 0x0800C000) return 2;
    if(Address < 0x08010000) return 3;
    if(Address < 0x08020000) return 4;
    if(Address < 0x08040000) return 5;
    if(Address < 0x08060000) return 6;
    if(Address < 0x08080000) return 7;
    if(Address < 0x080A0000) return 8;
    if(Address < 0x080C0000) return 9;
    if(Address < 0x080E0000) return 10;
    return 11;
}
void MCU_Flash_Erase(uint32_t start_addr, uint32_t len)
{
	HAL_FLASH_Unlock();
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | 
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
	uint32_t StartSectorMacro = GetSector(start_addr);
	uint32_t start_idx = GetSectorIndex(start_addr);
	uint32_t end_idx   = GetSectorIndex(start_addr + len - 1);
	FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError = 0;
	EraseInitStruct.TypeErase    = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    EraseInitStruct.Sector       = StartSectorMacro;
    EraseInitStruct.NbSectors    = (end_idx - start_idx) + 1;
	HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError);
    HAL_FLASH_Lock();
}
uint8_t MCU_Flash_Write(uint32_t addr, uint8_t *data, uint32_t len) 
{
	HAL_StatusTypeDef status;
    HAL_FLASH_Unlock(); 
    for (uint32_t i = 0; i < len; i += 4) 
    {
        uint32_t word_data = 0xFFFFFFFF;
        uint32_t remain = len - i;
        uint32_t copy_len = (remain >= 4) ? 4 : remain;
        memcpy(&word_data, data + i, copy_len);
		__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | 
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
        status =HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i, word_data);
		if (status != HAL_OK) {
            HAL_FLASH_Lock();
            return 0;
        }
    }
    HAL_FLASH_Lock();
	return 1;
}