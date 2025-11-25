/**
  ******************************************************************************
  * @file    bsp_ili9341_lcd.c
  * @version V1.0
  * @date    2013-xx-xx
  * @brief   ili9341液晶屏驱动
  ******************************************************************************
  * @attention
  *
  * 实验平台:野火 F103 STM32 开发板
  * 论坛    :http://www.firebbs.cn
  * 淘宝    :https://fire-stm32.taobao.com
  *
  ******************************************************************************
  */

#include "bsp_ili9341_lcd.h"

static SRAM_HandleTypeDef SRAM_Handler;
static FSMC_NORSRAM_TimingTypeDef Timing;

//根据液晶扫描方向而变化的XY像素宽度
//调用ILI9341_GramScan函数设置方向时会自动更改
uint16_t LCD_X_LENGTH = ILI9341_LESS_PIXEL;
uint16_t LCD_Y_LENGTH = ILI9341_MORE_PIXEL;

//液晶屏扫描模式，本变量主要用于方便选择触摸屏的计算参数
//参数可选值为0-7
//调用ILI9341_GramScan函数设置方向时会自动更改
//LCD刚初始化完成时会使用本默认值
uint8_t LCD_SCAN_MODE = 5;

//保存液晶屏驱动ic的 ID
uint16_t lcdid = LCDID_UNKNOWN;

__inline void ILI9341_Write_Cmd(uint16_t usCmd);

__inline void ILI9341_Write_Data(uint16_t usData);

__inline uint16_t ILI9341_Read_Data(void);

static void ILI9341_Delay(__IO uint32_t nCount);

static void ILI9341_GPIO_Config(void);

static void ILI9341_FSMC_Config(void);

static void ILI9341_REG_Config(void);

static void ILI9341_SetCursor(uint16_t usX, uint16_t usY);

static __inline void ILI9341_FillColor(uint32_t ulAmout_Point, uint16_t usColor);

static uint16_t ILI9341_Read_PixelData(void);

/**
  * @brief  向ILI9341写入命令
  * @param  usCmd :要写入的命令（表寄存器地址）
  * @retval 无
  */
void ILI9341_Write_Cmd(uint16_t usCmd) {
	*(__IO uint16_t *) (FSMC_Addr_ILI9341_CMD) = usCmd;
}


/**
  * @brief  向ILI9341写入数据
  * @param  usData :要写入的数据
  * @retval 无
  */
void ILI9341_Write_Data(uint16_t usData) {
	*(__IO uint16_t *) (FSMC_Addr_ILI9341_DATA) = usData;
}


/**
  * @brief  从ILI9341读取数据
  * @param  无
  * @retval 读取到的数据
  */
uint16_t ILI9341_Read_Data(void) {
	return (*(__IO uint16_t *) (FSMC_Addr_ILI9341_DATA));
}


/**
  * @brief  用于 ILI9341 简单延时函数
  * @param  nCount ：延时计数值
  * @retval 无
  */
static void ILI9341_Delay(__IO uint32_t nCount) {
	for (; nCount != 0; nCount--);
}


/**
  * @brief  初始化ILI9341的IO引脚
  * @param  无
  * @retval 无
  */
static void ILI9341_GPIO_Config(void) {
	GPIO_InitTypeDef GPIO_Initure;

	/* Enable GPIOs clock */
	__HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_GPIOE_CLK_ENABLE();
	__HAL_RCC_GPIOF_CLK_ENABLE();
	__HAL_RCC_GPIOG_CLK_ENABLE();
	__HAL_RCC_FSMC_CLK_ENABLE(); //使能FSMC时钟

	/* Common GPIO configuration */
	GPIO_Initure.Mode = GPIO_MODE_OUTPUT_PP; //推挽输出
	GPIO_Initure.Pull = GPIO_PULLUP;
	GPIO_Initure.Speed = GPIO_SPEED_FREQ_HIGH;


	GPIO_Initure.Pin = GPIO_PIN_12;
	HAL_GPIO_Init(GPIOD, &GPIO_Initure);

	//初始化复位引脚G11
	GPIO_Initure.Pin = GPIO_PIN_1;
	HAL_GPIO_Init(GPIOE, &GPIO_Initure);

	GPIO_Initure.Mode = GPIO_MODE_AF_PP;
	//  GPIO_Initure.Alternate=GPIO_AF12_FSMC;	//复用为FSMC

	//初始化PD0,1,4,5,8,9,10,14,15
	GPIO_Initure.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_7 | GPIO_PIN_8 |\
	                   GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_14 | GPIO_PIN_15;
	HAL_GPIO_Init(GPIOD, &GPIO_Initure);

	//初始化PE2,7,8,9,10,11,12,13,14,15
	GPIO_Initure.Pin = GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 |\
	                   GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
	HAL_GPIO_Init(GPIOE, &GPIO_Initure);
}


/**
 * @brief  LCD  FSMC 模式配置
 * @param  无
 * @retval 无
 */
static void ILI9341_FSMC_Config(void) {
	SRAM_Handler.Instance = FSMC_NORSRAM_DEVICE;
	SRAM_Handler.Extended = FSMC_NORSRAM_EXTENDED_DEVICE;

	/* SRAM device configuration */
	Timing.AddressSetupTime = 0x00;
	Timing.AddressHoldTime = 0x00;
	Timing.DataSetupTime = 0x08;
	Timing.BusTurnAroundDuration = 0x00;
	Timing.CLKDivision = 0x00;
	Timing.DataLatency = 0x00;
	Timing.AccessMode = FSMC_ACCESS_MODE_B;

	SRAM_Handler.Init.NSBank = FSMC_Bank1_NORSRAMx; //使用NE4
	SRAM_Handler.Init.DataAddressMux = FSMC_DATA_ADDRESS_MUX_DISABLE; //地址/数据线不复用
	SRAM_Handler.Init.MemoryType = FSMC_MEMORY_TYPE_NOR; //NOR
	SRAM_Handler.Init.MemoryDataWidth = FSMC_NORSRAM_MEM_BUS_WIDTH_16; //16位数据宽度
	SRAM_Handler.Init.BurstAccessMode = FSMC_BURST_ACCESS_MODE_DISABLE; //是否使能突发访问,仅对同步突发存储器有效,此处未用到
	SRAM_Handler.Init.WaitSignalPolarity = FSMC_WAIT_SIGNAL_POLARITY_LOW; //等待信号的极性,仅在突发模式访问下有用
	SRAM_Handler.Init.WaitSignalActive = FSMC_WAIT_TIMING_BEFORE_WS; //存储器是在等待周期之前的一个时钟周期还是等待周期期间使能NWAIT
	SRAM_Handler.Init.WriteOperation = FSMC_WRITE_OPERATION_ENABLE; //存储器写使能
	SRAM_Handler.Init.WaitSignal = FSMC_WAIT_SIGNAL_DISABLE; //等待使能位,此处未用到
	SRAM_Handler.Init.ExtendedMode = FSMC_EXTENDED_MODE_DISABLE; //读写使用相同的时序
	SRAM_Handler.Init.AsynchronousWait = FSMC_ASYNCHRONOUS_WAIT_DISABLE; //是否使能同步传输模式下的等待信号,此处未用到
	SRAM_Handler.Init.WriteBurst = FSMC_WRITE_BURST_DISABLE; //禁止突发写

	/* SRAM controller initialization */
	ILI9341_GPIO_Config();
	HAL_SRAM_Init(&SRAM_Handler, &Timing, &Timing);
}


/**
 * @brief  初始化ILI9341寄存器
 * @param  无
 * @retval 无
 */
static void ILI9341_REG_Config(void) {
	lcdid = ILI9341_ReadID();

	if (lcdid == LCDID_ILI9341) {
		/*  Power control B (CFh)  */
		DEBUG_DELAY();
		ILI9341_Write_Cmd(0xCF);
		ILI9341_Write_Data(0x00);
		ILI9341_Write_Data(0x81);
		ILI9341_Write_Data(0x30);

		/*  Power on sequence control (EDh) */
		DEBUG_DELAY();
		ILI9341_Write_Cmd(0xED);
		ILI9341_Write_Data(0x64);
		ILI9341_Write_Data(0x03);
		ILI9341_Write_Data(0x12);
		ILI9341_Write_Data(0x81);

		/*  Driver timing control A (E8h) */
		DEBUG_DELAY();
		ILI9341_Write_Cmd(0xE8);
		ILI9341_Write_Data(0x85);
		ILI9341_Write_Data(0x10);
		ILI9341_Write_Data(0x78);

		/*  Power control A (CBh) */
		DEBUG_DELAY();
		ILI9341_Write_Cmd(0xCB);
		ILI9341_Write_Data(0x39);
		ILI9341_Write_Data(0x2C);
		ILI9341_Write_Data(0x00);
		ILI9341_Write_Data(0x34);
		ILI9341_Write_Data(0x02);

		/* Pump ratio control (F7h) */
		DEBUG_DELAY();
		ILI9341_Write_Cmd(0xF7);
		ILI9341_Write_Data(0x20);

		/* Driver timing control B */
		DEBUG_DELAY();
		ILI9341_Write_Cmd(0xEA);
		ILI9341_Write_Data(0x00);
		ILI9341_Write_Data(0x00);

		/* Frame Rate Control (In Normal Mode/Full Colors) (B1h) */
		DEBUG_DELAY();
		ILI9341_Write_Cmd(0xB1);
		ILI9341_Write_Data(0x00);
		ILI9341_Write_Data(0x1B);

		/*  Display Function Control (B6h) */
		DEBUG_DELAY();
		ILI9341_Write_Cmd(0xB6);
		ILI9341_Write_Data(0x0A);
		ILI9341_Write_Data(0xA2);

		/* Power Control 1 (C0h) */
		DEBUG_DELAY();
		ILI9341_Write_Cmd(0xC0);
		ILI9341_Write_Data(0x35);

		/* Power Control 2 (C1h) */
		DEBUG_DELAY();
		ILI9341_Write_Cmd(0xC1);
		ILI9341_Write_Data(0x11);

		/* VCOM Control 1 (C5h) */
		ILI9341_Write_Cmd(0xC5);
		ILI9341_Write_Data(0x45);
		ILI9341_Write_Data(0x45);

		/*  VCOM Control 2 (C7h)  */
		ILI9341_Write_Cmd(0xC7);
		ILI9341_Write_Data(0xA2);

		/* Enable 3G (F2h) */
		ILI9341_Write_Cmd(0xF2);
		ILI9341_Write_Data(0x00);

		/* Gamma Set (26h) */
		ILI9341_Write_Cmd(0x26);
		ILI9341_Write_Data(0x01);
		DEBUG_DELAY();

		/* Positive Gamma Correction */
		ILI9341_Write_Cmd(0xE0); //Set Gamma
		ILI9341_Write_Data(0x0F);
		ILI9341_Write_Data(0x26);
		ILI9341_Write_Data(0x24);
		ILI9341_Write_Data(0x0B);
		ILI9341_Write_Data(0x0E);
		ILI9341_Write_Data(0x09);
		ILI9341_Write_Data(0x54);
		ILI9341_Write_Data(0xA8);
		ILI9341_Write_Data(0x46);
		ILI9341_Write_Data(0x0C);
		ILI9341_Write_Data(0x17);
		ILI9341_Write_Data(0x09);
		ILI9341_Write_Data(0x0F);
		ILI9341_Write_Data(0x07);
		ILI9341_Write_Data(0x00);

		/* Negative Gamma Correction (E1h) */
		ILI9341_Write_Cmd(0XE1); //Set Gamma
		ILI9341_Write_Data(0x00);
		ILI9341_Write_Data(0x19);
		ILI9341_Write_Data(0x1B);
		ILI9341_Write_Data(0x04);
		ILI9341_Write_Data(0x10);
		ILI9341_Write_Data(0x07);
		ILI9341_Write_Data(0x2A);
		ILI9341_Write_Data(0x47);
		ILI9341_Write_Data(0x39);
		ILI9341_Write_Data(0x03);
		ILI9341_Write_Data(0x06);
		ILI9341_Write_Data(0x06);
		ILI9341_Write_Data(0x30);
		ILI9341_Write_Data(0x38);
		ILI9341_Write_Data(0x0F);

		/* memory access control set */
		DEBUG_DELAY();
		// ILI9341_Write_Cmd(0x36);
		// ILI9341_Write_Data(0xC8); /*竖屏  左上角到 (起点)到右下角 (终点)扫描方式*/
		// DEBUG_DELAY();

		/* column address control set */
		ILI9341_Write_Cmd(CMD_SetCoordinateX);
		ILI9341_Write_Data(0x00);
		ILI9341_Write_Data(0x00);
		ILI9341_Write_Data(0x00);
		ILI9341_Write_Data(0xEF);

		/* page address control set */
		DEBUG_DELAY();
		ILI9341_Write_Cmd(CMD_SetCoordinateY);
		ILI9341_Write_Data(0x00);
		ILI9341_Write_Data(0x00);
		ILI9341_Write_Data(0x01);
		ILI9341_Write_Data(0x3F);

		/*  Pixel Format Set (3Ah)  */
		DEBUG_DELAY();
		ILI9341_Write_Cmd(0x3a);
		ILI9341_Write_Data(0x55);

		/* Sleep Out (11h)  */
		ILI9341_Write_Cmd(0x11);
		ILI9341_Delay(0xAFFf << 2);
		DEBUG_DELAY();

		/* Display ON (29h) */
		ILI9341_Write_Cmd(0x29);
	} else if (lcdid == LCDID_ST7789V) {
		/*  Power control B (CFh)  */
		DEBUG_DELAY();
		ILI9341_Write_Cmd(0xCF);
		ILI9341_Write_Data(0x00);
		ILI9341_Write_Data(0xC1);
		ILI9341_Write_Data(0x30);

		/*  Power on sequence control (EDh) */
		DEBUG_DELAY();
		ILI9341_Write_Cmd(0xED);
		ILI9341_Write_Data(0x64);
		ILI9341_Write_Data(0x03);
		ILI9341_Write_Data(0x12);
		ILI9341_Write_Data(0x81);

		/*  Driver timing control A (E8h) */
		DEBUG_DELAY();
		ILI9341_Write_Cmd(0xE8);
		ILI9341_Write_Data(0x85);
		ILI9341_Write_Data(0x10);
		ILI9341_Write_Data(0x78);

		/*  Power control A (CBh) */
		DEBUG_DELAY();
		ILI9341_Write_Cmd(0xCB);
		ILI9341_Write_Data(0x39);
		ILI9341_Write_Data(0x2C);
		ILI9341_Write_Data(0x00);
		ILI9341_Write_Data(0x34);
		ILI9341_Write_Data(0x02);

		/* Pump ratio control (F7h) */
		DEBUG_DELAY();
		ILI9341_Write_Cmd(0xF7);
		ILI9341_Write_Data(0x20);

		/* Driver timing control B */
		DEBUG_DELAY();
		ILI9341_Write_Cmd(0xEA);
		ILI9341_Write_Data(0x00);
		ILI9341_Write_Data(0x00);


		/* Power Control 1 (C0h) */
		DEBUG_DELAY();
		ILI9341_Write_Cmd(0xC0); //Power control
		ILI9341_Write_Data(0x21); //VRH[5:0]

		/* Power Control 2 (C1h) */
		DEBUG_DELAY();
		ILI9341_Write_Cmd(0xC1); //Power control
		ILI9341_Write_Data(0x11); //SAP[2:0];BT[3:0]

		/* VCOM Control 1 (C5h) */
		ILI9341_Write_Cmd(0xC5);
		ILI9341_Write_Data(0x2D);
		ILI9341_Write_Data(0x33);

		/*  VCOM Control 2 (C7h)  */
		//	ILI9341_Write_Cmd ( 0xC7 );
		//	ILI9341_Write_Data ( 0XC0 );

		/* memory access control set */
		/* 註解：此處不設定 0x36，由 ILI9341_GramScan() 統一管理 */
		// DEBUG_DELAY();
		// ILI9341_Write_Cmd(0x36); //Memory Access Control
		// ILI9341_Write_Data(0x00 | (1 << 3)); /*竖屏  左上角到 (起点)到右下角 (终点)扫描方式*/
		// DEBUG_DELAY();

		ILI9341_Write_Cmd(0x3A);
		ILI9341_Write_Data(0x55);

		/* Frame Rate Control (In Normal Mode/Full Colors) (B1h) */
		DEBUG_DELAY();
		ILI9341_Write_Cmd(0xB1);
		ILI9341_Write_Data(0x00);
		ILI9341_Write_Data(0x17);

		/*  Display Function Control (B6h) */
		DEBUG_DELAY();
		ILI9341_Write_Cmd(0xB6);
		ILI9341_Write_Data(0x0A);
		ILI9341_Write_Data(0xA2);

		ILI9341_Write_Cmd(0xF6);
		ILI9341_Write_Data(0x01);
		ILI9341_Write_Data(0x30);

		/* Enable 3G (F2h) */
		ILI9341_Write_Cmd(0xF2);
		ILI9341_Write_Data(0x00);

		/* Gamma Set (26h) */
		ILI9341_Write_Cmd(0x26);
		ILI9341_Write_Data(0x01);
		DEBUG_DELAY();

		/* Positive Gamma Correction */
		ILI9341_Write_Cmd(0xe0); //Positive gamma
		ILI9341_Write_Data(0xd0);
		ILI9341_Write_Data(0x00);
		ILI9341_Write_Data(0x02);
		ILI9341_Write_Data(0x07);
		ILI9341_Write_Data(0x0b);
		ILI9341_Write_Data(0x1a);
		ILI9341_Write_Data(0x31);
		ILI9341_Write_Data(0x54);
		ILI9341_Write_Data(0x40);
		ILI9341_Write_Data(0x29);
		ILI9341_Write_Data(0x12);
		ILI9341_Write_Data(0x12);
		ILI9341_Write_Data(0x12);
		ILI9341_Write_Data(0x17);

		/* Negative Gamma Correction (E1h) */
		ILI9341_Write_Cmd(0xe1); //Negative gamma
		ILI9341_Write_Data(0xd0);
		ILI9341_Write_Data(0x00);
		ILI9341_Write_Data(0x02);
		ILI9341_Write_Data(0x07);
		ILI9341_Write_Data(0x05);
		ILI9341_Write_Data(0x25);
		ILI9341_Write_Data(0x2d);
		ILI9341_Write_Data(0x44);
		ILI9341_Write_Data(0x45);
		ILI9341_Write_Data(0x1c);
		ILI9341_Write_Data(0x18);
		ILI9341_Write_Data(0x16);
		ILI9341_Write_Data(0x1c);
		ILI9341_Write_Data(0x1d);


		//	/* column address control set */
		//	ILI9341_Write_Cmd ( CMD_SetCoordinateX );
		//	ILI9341_Write_Data ( 0x00 );
		//	ILI9341_Write_Data ( 0x00 );
		//	ILI9341_Write_Data ( 0x00 );
		//	ILI9341_Write_Data ( 0xEF );
		//
		//	/* page address control set */
		//	DEBUG_DELAY ();
		//	ILI9341_Write_Cmd ( CMD_SetCoordinateY );
		//	ILI9341_Write_Data ( 0x00 );
		//	ILI9341_Write_Data ( 0x00 );
		//	ILI9341_Write_Data ( 0x01 );
		//	ILI9341_Write_Data ( 0x3F );


		/* Sleep Out (11h)  */
		ILI9341_Write_Cmd(0x11); //Exit Sleep
		ILI9341_Delay(0xAFFf << 2);
		DEBUG_DELAY();

		/* Display ON (29h) */
		ILI9341_Write_Cmd(0x29); //Display on

		ILI9341_Write_Cmd(0x2c);
	}
}


/**
 * @brief  ILI9341初始化函数，如果要用到lcd，一定要调用这个函数
 * @param  无
 * @retval 无
 */
void ILI9341_Init(void) {
	ILI9341_GPIO_Config();
	ILI9341_FSMC_Config();

	ILI9341_BackLed_Control(ENABLE); //点亮LCD背光灯
	ILI9341_Rst();
	ILI9341_REG_Config();

	//设置默认扫描方向，其中 6 模式为大部分液晶例程的默认显示方向
	ILI9341_GramScan(LCD_SCAN_MODE);
}


/**
 * @brief  ILI9341G背光LED控制
 * @param  enumState ：决定是否使能背光LED
  *   该参数为以下值之一：
  *     @arg ENABLE :使能背光LED
  *     @arg DISABLE :禁用背光LED
 * @retval 无
 */
void ILI9341_BackLed_Control(FunctionalState enumState) {
	if (enumState) {
		digitalL(GPIOD, GPIO_PIN_12);
	} else {
		digitalH(GPIOD, GPIO_PIN_12);
	}
}


/**
 * @brief  读取LCD驱动芯片ID函数，可用于测试底层的读写函数
 * @param  无
 * @retval 正常时返回值为LCD驱动芯片ID: LCDID_ILI9341/LCDID_ST7789V
 *         否则返回: LCDID_UNKNOWN
 */
uint16_t ILI9341_ReadID(void) {
	uint16_t id = 0;

	ILI9341_Write_Cmd(0x04);
	ILI9341_Read_Data();
	ILI9341_Read_Data();
	id = ILI9341_Read_Data();
	id <<= 8;
	id |= ILI9341_Read_Data();

	if (id == LCDID_ST7789V) {
		printf("%-20s lcd drv ic: %s\r\n", "[bsp_lcd.c]", "ST7789V");
		return id;
	} else {
		ILI9341_Write_Cmd(0xD3);
		ILI9341_Read_Data();
		ILI9341_Read_Data();
		id = ILI9341_Read_Data();
		id <<= 8;
		id |= ILI9341_Read_Data();
		if (id == LCDID_ILI9341) {
			printf("%-20s lcd drv ic: %s\r\n", "[bsp_lcd.c]", "ILI9341");
			return id;
		}
	}

	return LCDID_UNKNOWN;
}


/**
 * @brief  ILI9341G 软件复位
 * @param  无
 * @retval 无
 */
void ILI9341_Rst(void) {
	digitalL(GPIOE, GPIO_PIN_1); //低电平复位
	ILI9341_Delay(0xAFF);
	digitalH(GPIOE, GPIO_PIN_1);
	ILI9341_Delay(0xAFF);
}


/**
 * @brief  设置ILI9341的GRAM的扫描方向
 * @param  ucOption ：选择GRAM的扫描方向
 *     @arg 0-7 :参数可选值为0-7这八个方向
 *
 *	！！！其中0、3、5、6 模式适合从左至右显示文字，
 *				不推荐使用其它模式显示文字	其它模式显示文字会有镜像效果
 *
 *	其中0、2、4、6 模式的X方向像素为240，Y方向像素为320
 *	其中1、3、5、7 模式下X方向像素为320，Y方向像素为240
 *
 *	其中 6 模式为大部分液晶例程的默认显示方向
 *	其中 3 模式为摄像头例程使用的方向
 *	其中 0 模式为BMP图片显示例程使用的方向
 *
 * @retval 无
 * @note  坐标图例：A表示向上，V表示向下，<表示向左，>表示向右
					X表示X轴，Y表示Y轴

------------------------------------------------------------
模式0：				.		模式1：		.	模式2：			.	模式3：
					A		.					A		.		A					.		A
					|		.					|		.		|					.		|
					Y		.					X		.		Y					.		X
					0		.					1		.		2					.		3
	<--- X0 o		.	<----Y1	o		.		o 2X--->  .		o 3Y--->
------------------------------------------------------------
模式4：				.	模式5：			.	模式6：			.	模式7：
	<--- X4 o		.	<--- Y5 o		.		o 6X--->  .		o 7Y--->
					4		.					5		.		6					.		7
					Y		.					X		.		Y					.		X
					|		.					|		.		|					.		|
					V		.					V		.		V					.		V
---------------------------------------------------------
											 LCD屏示例
								|-----------------|
								|			野火Logo		|
								|									|
								|									|
								|									|
								|									|
								|									|
								|									|
								|									|
								|									|
								|-----------------|
								屏幕正面（宽240，高320）

 *******************************************************/
void ILI9341_GramScan(uint8_t ucOption) {
	//参数检查，只可输入0-7
	if (ucOption > 7)
		return;

	//根据模式更新LCD_SCAN_MODE的值，主要用于触摸屏选择计算参数
	LCD_SCAN_MODE = ucOption;

	//根据模式更新XY方向的像素宽度
	if (ucOption % 2 == 0) {
		//0 2 4 6模式下X方向像素宽度为240，Y方向为320
		LCD_X_LENGTH = ILI9341_LESS_PIXEL;
		LCD_Y_LENGTH = ILI9341_MORE_PIXEL;
	} else {
		//1 3 5 7模式下X方向像素宽度为320，Y方向为240
		LCD_X_LENGTH = ILI9341_MORE_PIXEL;
		LCD_Y_LENGTH = ILI9341_LESS_PIXEL;
	}

	//0x36命令参数的高3位可用于设置GRAM扫描方向
	ILI9341_Write_Cmd(0x36);
	if (lcdid == LCDID_ILI9341) {
		ILI9341_Write_Data(0x08 | (ucOption << 5)); //ILI9341: BGR=1
	} else if (lcdid == LCDID_ST7789V) {
		ILI9341_Write_Data(0x08 | (ucOption << 5)); //ST7789V: BGR=1 (與 ILI9341 相同)
	} else {
		// 未知 IC，使用預設值
		ILI9341_Write_Data(0x08 | (ucOption << 5));
	}
	ILI9341_Write_Cmd(CMD_SetCoordinateX);
	ILI9341_Write_Data(0x00); /* x 起始坐标高8位 */
	ILI9341_Write_Data(0x00); /* x 起始坐标低8位 */
	ILI9341_Write_Data(((LCD_X_LENGTH - 1) >> 8) & 0xFF); /* x 结束坐标高8位 */
	ILI9341_Write_Data((LCD_X_LENGTH - 1) & 0xFF); /* x 结束坐标低8位 */

	ILI9341_Write_Cmd(CMD_SetCoordinateY);
	ILI9341_Write_Data(0x00); /* y 起始坐标高8位 */
	ILI9341_Write_Data(0x00); /* y 起始坐标低8位 */
	ILI9341_Write_Data(((LCD_Y_LENGTH - 1) >> 8) & 0xFF); /* y 结束坐标高8位 */
	ILI9341_Write_Data((LCD_Y_LENGTH - 1) & 0xFF); /* y 结束坐标低8位 */

	/* write gram start */
	ILI9341_Write_Cmd(CMD_SetPixel);
}

/*********************end of file*************************/
