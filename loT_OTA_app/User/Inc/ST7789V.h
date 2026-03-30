#ifndef _ST7789V__H
#define _ST7789V__H
#include "SPI_OOP.h"

#define ST7789V_WIDTH   240
#define ST7789V_HEIGHT  280

#define ST7789V_X_OFFSET  0
#define ST7789V_Y_OFFSET  20


void ST7789V_Init(const SPI_Device* spi_bus);
void ST7789V_FillColor(uint16_t color565);
void ST7789V_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color565);
void ST7789V_DrawBitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *pixels);

void ST7789V_ShowChar(uint16_t X,uint16_t Y,char Char,uint16_t forecolor,uint16_t backcolor);
void ST7789V_ShowString(uint16_t X, uint16_t Y, char *String,uint16_t forecolor,uint16_t backcolor);
void ST7789V_ShowNum(uint16_t X, uint16_t Y, uint32_t Number, uint8_t Length,
	uint16_t forecolor,uint16_t backcolor);
void ST7789V_ShowSignedNum(uint16_t X, uint16_t Y,int32_t Number, 
	uint8_t Length,uint16_t forecolor,uint16_t backcolor);
void ST7789V_ShowHexNum(uint16_t X, uint16_t Y,int32_t Number, 
	uint8_t Length,uint16_t forecolor,uint16_t backcolor);
void ST7789V_ShowBinNum(uint16_t X, uint16_t Y,int32_t Number, 
	uint8_t Length,uint16_t forecolor,uint16_t backcolor);


void vLCD_Test(void* arg);


extern const uint8_t OLED_F8x16[][16];
#endif