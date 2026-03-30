#include "main.h"
#include "SPI_OOP.h"
#include "ST7789V.h"
#define LCD_DC_CMD()    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET) // 拉低表示发命令
#define LCD_DC_DATA()   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_SET)   // 拉高表示发数据
#define LCD_RST_LOW()   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET) // 复位引脚拉低
#define LCD_RST_HIGH()  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET)	 //复位引脚拉高

static const SPI_Device* LCD_SPI=NULL;
/**
 * @brief 向屏幕发送8位命令
 */
static void ST7789V_WriteCommand(uint8_t cmd)
{
    LCD_DC_CMD();
	LCD_SPI->SetCS(LCD_SPI,GPIO_PIN_RESET);
    LCD_SPI->Transmit(LCD_SPI, &cmd, 1, 10000); 
	LCD_SPI->SetCS(LCD_SPI,GPIO_PIN_SET);
}
/**
 * @brief 向屏幕发送8位数据
 */
static void ST7789V_WriteData(uint8_t data)
{
    LCD_DC_DATA();
	LCD_SPI->SetCS(LCD_SPI,GPIO_PIN_RESET);
    LCD_SPI->Transmit(LCD_SPI, &data, 1, 100);
	LCD_SPI->SetCS(LCD_SPI,GPIO_PIN_SET);
}
static void ST7789V_WriteDatas(uint8_t *data,uint8_t Size)
{
    LCD_DC_DATA();
	LCD_SPI->SetCS(LCD_SPI,GPIO_PIN_RESET);
    LCD_SPI->Transmit(LCD_SPI, data, Size, 100);
	LCD_SPI->SetCS(LCD_SPI,GPIO_PIN_SET);
}
void ST7789V_Init(const SPI_Device* spi_bus)
{
	if(spi_bus==NULL) return ;
	LCD_SPI=spi_bus;
	LCD_SPI->Init(LCD_SPI);
	//硬件复位
    LCD_RST_LOW();
    HAL_Delay(50);
    LCD_RST_HIGH();
    HAL_Delay(50);
	//软件复位
	ST7789V_WriteCommand(0x01);
    HAL_Delay(150);
	//退出睡眠模式
    ST7789V_WriteCommand(0x11);
    HAL_Delay(120);
	//颜色模式设置
    ST7789V_WriteCommand(0x3A); 
    ST7789V_WriteData(0x05);
	//显存数据访问控制-控制屏幕方向
    ST7789V_WriteCommand(0x36); 
    ST7789V_WriteData(0x40);
	//基础寄存器配置
	ST7789V_WriteCommand(0xB2); // Porch Setting
    ST7789V_WriteData(0x0C);
    ST7789V_WriteData(0x0C);
    ST7789V_WriteData(0x00);
    ST7789V_WriteData(0x33);
    ST7789V_WriteData(0x33);
    
    ST7789V_WriteCommand(0xB7); // Gate Control
    ST7789V_WriteData(0x35);
    
    ST7789V_WriteCommand(0xBB); // VCOM Setting
    ST7789V_WriteData(0x3E);
    
    ST7789V_WriteCommand(0xC0); // LCM Control
    ST7789V_WriteData(0xC5);
    
    ST7789V_WriteCommand(0xC2); // VDV and VRH Command Enable
    ST7789V_WriteData(0x01);
    
    ST7789V_WriteCommand(0xC3); // VRH Set
    ST7789V_WriteData(0x19);
    
    ST7789V_WriteCommand(0xC4); // VDV Set
    ST7789V_WriteData(0x20);
    
    ST7789V_WriteCommand(0xC6); // Frame Rate Control
    ST7789V_WriteData(0x0F);    // 60Hz
    
    ST7789V_WriteCommand(0xD0); // Power Control 1
    ST7789V_WriteData(0xA4);
    ST7789V_WriteData(0xA1);
	
    ST7789V_WriteCommand(0x21); // Display Inversion ON
    //开启显示
    ST7789V_WriteCommand(0x29);
	
	ST7789V_FillColor(0xFFFF);
}
/**
 * @brief 设置显示窗口
 */
static void ST7789V_SetWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	x2 += ST7789V_X_OFFSET; x1 += ST7789V_X_OFFSET;
    y2 += ST7789V_Y_OFFSET; y1 += ST7789V_Y_OFFSET;
	
	uint8_t data[4];
    ST7789V_WriteCommand(0x2A); // Column Address Set
    data[0] = (x1 >> 8) & 0xFF; // 高8位
    data[1] = x1 & 0xFF;        // 低8位
    data[2] = (x2 >> 8) & 0xFF; // 高8位
    data[3] = x2 & 0xFF;        // 低8位
    ST7789V_WriteDatas(data, 4);

    ST7789V_WriteCommand(0x2B); // Row Address Set
    data[0] = (y1 >> 8) & 0xFF;
    data[1] = y1 & 0xFF;
    data[2] = (y2 >> 8) & 0xFF;
    data[3] = y2 & 0xFF;
    ST7789V_WriteDatas(data, 4);

    ST7789V_WriteCommand(0x2C); // 准备写入显存
}
static void ST7789V_WriteColor(uint16_t color565,volatile uint32_t pixels)
{
    uint8_t hi = (uint8_t)(color565 >> 8);
    uint8_t lo = (uint8_t)(color565 & 0xFF);
    // 256像素=512字节
    uint8_t buf[512];
    for (int i = 0; i < (int)sizeof(buf); i += 2) 
	{
        buf[i] = hi;
        buf[i+1] = lo;
    }
    LCD_DC_DATA();
	LCD_SPI->SetCS(LCD_SPI,GPIO_PIN_RESET);
    while (pixels) {
        volatile uint32_t chunk = pixels;
        if (chunk > (sizeof(buf)/2)) chunk = (sizeof(buf)/2);
        LCD_SPI->Transmit(LCD_SPI, buf, (uint16_t)(chunk * 2), 10000);
        pixels -= chunk;
    }
	LCD_SPI->SetCS(LCD_SPI,GPIO_PIN_SET);
}
void ST7789V_DrawBitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *pixels)
{
    if (!LCD_SPI) return;
    // 设置写入窗口
    ST7789V_SetWindow(x, y, x + w - 1, y + h - 1); 
    
    LCD_DC_DATA();
    LCD_SPI->SetCS(LCD_SPI, GPIO_PIN_RESET);
    // 传输像素数组，每个 16-bit 像素占 2 字节
    LCD_SPI->Transmit(LCD_SPI, (uint8_t*)pixels, w * h * 2, 1000); 
    LCD_SPI->SetCS(LCD_SPI, GPIO_PIN_SET);
}
void ST7789V_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color565)
{
    if (!LCD_SPI) return;
    if (x >= ST7789V_WIDTH || y >= ST7789V_HEIGHT) return;
    if (x + w > ST7789V_WIDTH)  w = ST7789V_WIDTH  - x;
    if (y + h > ST7789V_HEIGHT) h = ST7789V_HEIGHT - y;

    ST7789V_SetWindow(x, y, x + w - 1, y + h - 1);
    ST7789V_WriteColor(color565, (uint32_t)w * (uint32_t)h);
}

void ST7789V_FillColor(uint16_t color565)
{
    ST7789V_FillRect(0, 0, ST7789V_WIDTH, ST7789V_HEIGHT, color565);
}
static void ST7789V_ShowByte(uint16_t X, uint16_t Y, uint8_t Byte, uint16_t forecolor, uint16_t backcolor)
{
    if (!LCD_SPI) return;
    if (X >= ST7789V_WIDTH || Y >= ST7789V_HEIGHT) return;
    if (Y + 7 >= ST7789V_HEIGHT) return;

    ST7789V_SetWindow(X, Y, X, Y + 7);

    uint8_t pixel_buf[16];
    for (int i = 0; i < 8; i++) 
	{
        uint16_t color = (Byte & (1<<i)) ? forecolor : backcolor;
        pixel_buf[i * 2]     = (color >> 8) & 0xFF;
        pixel_buf[i * 2 + 1] = color & 0xFF;
    }
    LCD_DC_DATA();
    LCD_SPI->SetCS(LCD_SPI, GPIO_PIN_RESET);
    LCD_SPI->Transmit(LCD_SPI, pixel_buf, sizeof(pixel_buf), 1000);
    LCD_SPI->SetCS(LCD_SPI, GPIO_PIN_SET);
}
void ST7789V_ShowChar(uint16_t X,uint16_t Y,char Char,uint16_t forecolor,uint16_t backcolor)
{
	for(uint8_t i=0;i<8;i++)
	{
		ST7789V_ShowByte(X+i,Y,OLED_F8x16[Char-' '][i],forecolor,backcolor);
	}
	for(uint8_t i=0;i<8;i++)
	{
		ST7789V_ShowByte(X+i,Y+8,OLED_F8x16[Char-' '][i+8],forecolor,backcolor);
	}
}
void ST7789V_ShowString(uint16_t X, uint16_t Y, char *String,uint16_t forecolor,uint16_t backcolor)
{
	uint8_t i;
	for (i = 0; String[i] != '\0'; i++)
	{
		ST7789V_ShowChar(X+8*i, Y, String[i],forecolor,backcolor);
	}
}
static uint32_t ST7789V_Pow(uint32_t X, uint32_t Y)
{
	uint32_t Result = 1;
	while (Y--)
	{
		Result *= X;
	}
	return Result;
}
void ST7789V_ShowNum(uint16_t X, uint16_t Y, uint32_t Number, uint8_t Length,
	uint16_t forecolor,uint16_t backcolor)
{
	uint8_t i;
	for (i = 0; i < Length; i++)							
	{
		ST7789V_ShowChar(X+8*i,Y,Number/ST7789V_Pow(10,Length-i-1)%10+'0',forecolor,backcolor);
	}
}
void ST7789V_ShowSignedNum(uint16_t X, uint16_t Y,int32_t Number, uint8_t Length,
	uint16_t forecolor,uint16_t backcolor)
{
	uint8_t i;
	uint32_t Number1;
	if (Number >= 0)
	{
		ST7789V_ShowChar(X, Y, '+',forecolor,backcolor);
		Number1 = Number;
	}
	else
	{
		ST7789V_ShowChar(X, Y, '-',forecolor,backcolor);
		Number1 = -Number;
	}
	for (i = 0; i < Length; i++)							
	{
		ST7789V_ShowChar(X+(i+1)*8,Y,Number1/ST7789V_Pow(10,Length-i-1)%10+'0',forecolor,backcolor);
	}
}
void ST7789V_ShowHexNum(uint16_t X, uint16_t Y,int32_t Number, uint8_t Length,
	uint16_t forecolor,uint16_t backcolor)
{
	uint8_t i, SingleNumber;
	for (i = 0; i < Length; i++)							
	{
		SingleNumber = Number / ST7789V_Pow(16, Length - i - 1) % 16;
		if (SingleNumber < 10)
		{
			ST7789V_ShowChar(X+8*i, Y, SingleNumber + '0',forecolor,backcolor);
		}
		else
		{
			ST7789V_ShowChar(X+8*i, Y, SingleNumber - 10 + 'A',forecolor,backcolor);
		}
	}
}
void ST7789V_ShowBinNum(uint16_t X, uint16_t Y,int32_t Number, uint8_t Length,
	uint16_t forecolor,uint16_t backcolor)
{
	uint8_t i;
	for (i = 0; i < Length; i++)							
	{
		ST7789V_ShowChar(X+8*i,Y,Number/ST7789V_Pow(2,Length-i-1)%2+'0',forecolor,backcolor);
	}
}
void vLCD_Test(void* arg)
{
	ST7789V_Init(&HardSPI1_DMA_Obj);
	//ST7789V_FillColor(0x50AA); // 红色
	ST7789V_ShowString(1,1,"AAAAAAAAAAAAAAAAAAAA",0x50AA,0xFFFF);
	ST7789V_ShowNum(50,100,666666,10,0x50AA,0xFFFF);
	ST7789V_ShowSignedNum(20,80,7777,5,0x50AA,0xFFFF);
	ST7789V_ShowHexNum(3,20,10,2,0x50AA,0xFFFF);
	ST7789V_ShowBinNum(5,50,66,8,0x50AA,0xFFFF);
	while(1)
	{
		 
	}
}