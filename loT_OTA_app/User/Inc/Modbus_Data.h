#ifndef __MODBUS_DATA_H
#define __MODBUS_DATA_H

#define NUM_BITS 			10
#define NUM_INPUT_BITS 		10
#define NUM_REGISTERS 		10
#define NUM_INPUT_REGISTERS 10

//modbus
uint16_t crc16(uint8_t *buffer, uint16_t buffer_length);;


#endif