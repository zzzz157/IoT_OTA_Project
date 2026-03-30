#ifndef __DATACENTER__H
#define __DATACENTER__H

uint8_t DataCenter_ReadBit(uint16_t addr);
uint8_t DataCenter_ReadInputBit(uint16_t addr);
uint16_t DataCenter_ReadRegister(uint16_t addr);
uint16_t DataCenter_ReadInputRegister(uint16_t addr);
void DataCenter_WriteBit(uint16_t addr,uint8_t data);
void DataCenter_WriteRegister(uint16_t addr,uint16_t data);

//app
void DataCenter_UpdateHealth(uint16_t hr, uint16_t spo2);
void DataCenter_UpdateSensorError(bool sensor_state);
void DataCenter_UpdateDataLegal(bool is_legal);


#endif
