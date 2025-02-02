#include "ssd1357.h"
//#include "stm32l0xx_ll_gpio.h"
//#include "stm32l0xx_ll_spi.h"
#include "stm32f4xx_hal.h"
#include "main.h"
// Configuration
#define SSD1357_DC_GPIO_Port	GPIOA
#define SSD1357_DC_LL_Pin	LL_GPIO_PIN_6
#define SSD1357_NSS_GPIO_Port	GPIOA
#define SSD1357_NSS_LL_Pin	LL_GPIO_PIN_4
#define SSD1357_OLED_64x64

#ifdef SSD1357_OLED_64x64
#define SSD1357_START_ROW	0x00
#define SSD1357_START_COL	0x20
#define SSD1357_STOP_ROW 	0x3F //START_ROW + 64 - 1
#define SSD1357_STOP_COL	0x5F //START_COL + 64 - 1
#else // assume 128x128
// Not tested
#define SSD1357_START_ROW	0x00
#define SSD1357_START_COL	0x00
#define SSD1357_STOP_ROW 	0x7F
#define SSD1357_STOP_COL	0x7F
#endif

typedef enum{
	SSD1357_CMD_SetColumnAddress = 0x15,
	// Discontinuous
	SSD1357_CMD_SetRowAddress = 0x75,
	// Discontinuous
	SSD1357_CMD_WriteRAM = 0x5C,
	SSD1357_CMD_ReadRAM,
	// Discontinuous
	SSD1357_CMD_SetRemapColorDepth = 0xA0,
	SSD1357_CMD_SetDisplayStartLine,
	SSD1357_CMD_SetDisplayOffset,
	// Discontinuous
	SSD1357_CMD_SDM_ALL_OFF = 0xA4,
	SSD1357_CMD_SDM_ALL_ON,
	SSD1357_CMD_SDM_RESET,
	SSD1357_CMD_SDM_INVERSE,
	// Discontinuous
	SSD1357_CMD_SetSleepMode_ON = 0xAE,
	SSD1357_CMD_SetSleepMode_OFF,
	// Discontinuous
	SSD1357_CMD_SetResetPrechargePeriod = 0xB1,
	// Discontinuous
	SSD1357_CMD_SetClkDiv = 0xB3,
	// Discontinuous
	SSD1357_CMD_SetSecondPrechargePeriod = 0xB6,
	// Discontinuous
	SSD1357_CMD_MLUTGrayscale = 0xB8, // master LUT (for A,B,C)
	SSD1357_CMD_UseDefMLUT,
	// Discontinuous
	SSD1357_CMD_SetPrechargeVoltage = 0xBB,
	SSD1357_CMD_SetILUTColorA, // individual LUT
	SSD1357_CMD_SetILUTColorC,
	SSD1357_CMD_SetVCOMH,
	// Discontinuous
	SSD1357_CMD_SetContrastCurrentABC = 0xC1,
	// Discontinuous
	SSD1357_CMD_SetMasterContrastCurrent = 0xC7,
	// Discontinuous
	SSD1357_CMD_SetMuxRatio = 0xCA,
	// Discontinuous
	SSD1357_CMD_NOP = 0xE3,
	// Discontinuous
	SSD1357_CMD_SetCommandLock = 0xFD,
	// Discontinuous
	SSD1357_CMD_Setup_Scrolling = 0x96,
	// Discontinuous
	SSD1357_SCROLL_STOP = 0x9E,
	SSD1357_SCROLL_START
}SSD1357_CMD_TypeDef;

#define SSD1357_MAX_WIDTH 128
#define SSD1357_MAX_HEIGHT 128

#define SSD1357_WORKING_BUFF_NUM_PIXELS	128
#define SSD1357_BYTES_PER_PIXEL 2

uint8_t working_buff[SSD1357_WORKING_BUFF_NUM_PIXELS * SSD1357_BYTES_PER_PIXEL];


#if 0
uint8_t spi_buf[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
	0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10
};
bool spi_dma_finished = false;

void SSD1357_DMA_SendCommand(uint32_t len) {
	LL_DMA_DisableChannel(DMA1, SPI1_DMA_Channel);
//    spi_dma_finished = false;
    LL_DMA_ConfigAddresses(DMA1, SPI1_DMA_Channel, (uint32_t) (&spi_buf[0]),
		 (uint32_t) LL_SPI_DMA_GetRegAddr(SPI1),
		 LL_DMA_GetDataTransferDirection(DMA1, SPI1_DMA_Channel));
    LL_DMA_SetDataLength(DMA1, SPI1_DMA_Channel, len);
    LL_SPI_EnableDMAReq_TX(SPI1);

    LL_GPIO_SetOutputPin(DISP_DC_GPIO_Port, DISP_DC_Pin);
    LL_GPIO_ResetOutputPin(SPI1_NSS_GPIO_Port, SPI1_NSS_Pin);

    LL_SPI_Enable(SPI1);
    LL_DMA_EnableChannel(DMA1, SPI1_DMA_Channel);

    while (!spi_dma_finished)
  	  __WFI();
    LL_SPI_Disable(SPI1);
}
#endif

static inline void SendCommand_Sync(uint8_t cmd) {
//	LL_GPIO_ResetOutputPin(SSD1357_DC_GPIO_Port, SSD1357_DC_LL_Pin);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi1, &cmd, 1, 1000);
//	LL_SPI_TransmitData8(SPI1, cmd);
//	while (LL_SPI_IsActiveFlag_BSY(SPI1))
//		;
//	LL_GPIO_SetOutputPin(SSD1357_DC_GPIO_Port, SSD1357_DC_LL_Pin);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
}

void SSD1357_SetCommandLock(bool locked) {
//	LL_SPI_Enable(SPI1);
//	LL_GPIO_ResetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
	SendCommand_Sync(SSD1357_CMD_SetCommandLock);
	uint8_t tmp = (locked) ? 0x16 : 0x12;
	HAL_SPI_Transmit(&hspi1, &tmp, 1, 1000);
//	LL_SPI_TransmitData8(SPI1, (locked) ? 0x16 : 0x12);
//	while (LL_SPI_IsActiveFlag_BSY(SPI1))
//		;
//	LL_GPIO_SetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
//	LL_SPI_Disable(SPI1);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
}

void SSD1357_SetSleepMode(bool on) {
//	LL_SPI_Enable(SPI1);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
//	LL_GPIO_ResetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
	SendCommand_Sync(on ? SSD1357_CMD_SetSleepMode_ON : SSD1357_CMD_SetSleepMode_OFF);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
//	LL_GPIO_SetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
//	LL_SPI_Disable(SPI1);
}

void SSD1357_SetClockDivider(uint8_t divider_code) {
//	LL_SPI_Enable(SPI1);
//	LL_GPIO_ResetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
	SendCommand_Sync(SSD1357_CMD_SetClkDiv);
	HAL_SPI_Transmit(&hspi1, &divider_code, 1, 1000);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
//	LL_SPI_TransmitData8(SPI1, divider_code);
//	while (LL_SPI_IsActiveFlag_BSY(SPI1))
//		;
//	LL_GPIO_SetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
//	LL_SPI_Disable(SPI1);
}

void SSD1357_SetMuxRatio(uint8_t ratio) {
//	LL_SPI_Enable(SPI1);
//	LL_GPIO_ResetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
	SendCommand_Sync(SSD1357_CMD_SetMuxRatio);
	HAL_SPI_Transmit(&hspi1, &ratio, 1, 1000);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
//	LL_SPI_TransmitData8(SPI1, ratio);
//	while (LL_SPI_IsActiveFlag_BSY(SPI1))
//		;
//	LL_GPIO_SetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
//	LL_SPI_Disable(SPI1);
}

void SSD1357_SetDisplayOffset(uint8_t offset) {
//	LL_SPI_Enable(SPI1);
//	LL_GPIO_ResetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
	SendCommand_Sync(SSD1357_CMD_SetDisplayOffset);
	HAL_SPI_Transmit(&hspi1, &offset, 1, 1000);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
//	LL_SPI_TransmitData8(SPI1, offset);
//	while (LL_SPI_IsActiveFlag_BSY(SPI1))
//		;
//	LL_GPIO_SetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
//	LL_SPI_Disable(SPI1);
}

void SSD1357_SetDisplayStartLine(uint8_t line) {
//	LL_SPI_Enable(SPI1);
//	LL_GPIO_ResetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
	SendCommand_Sync(SSD1357_CMD_SetDisplayStartLine);
	HAL_SPI_Transmit(&hspi1, &line, 1, 1000);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
//	LL_SPI_TransmitData8(SPI1, line);
//	while (LL_SPI_IsActiveFlag_BSY(SPI1))
//		;
//	LL_GPIO_SetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
//	LL_SPI_Disable(SPI1);
}

void SSD1357_SetRemapColorDepth(bool inc_Vh, bool rev_ColAddr, bool swap_ColOrder, bool rev_SCAN, bool en_SplitOddEven, uint8_t color_depth_code) {
	uint8_t m = 0;
	if (inc_Vh) {
		m |= 0x01;
	}
	if (rev_ColAddr) {
		m |= 0x02;
	}
	if (swap_ColOrder) {
		m |= 0x04;
	}
	if (rev_SCAN) {
		m |= 0x10;
	}
	if (en_SplitOddEven) {
		m |= 0x20;
	}
	m |= ((0x03 & color_depth_code) << 6);

//	LL_SPI_Enable(SPI1);
//	LL_GPIO_ResetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
	SendCommand_Sync(SSD1357_CMD_SetRemapColorDepth);
	HAL_SPI_Transmit(&hspi1, &m, 1, 1000);
	uint8_t tmp = 0;
	HAL_SPI_Transmit(&hspi1, &tmp, 1, 1000);
//	LL_SPI_TransmitData8(SPI1, m);
//	while (LL_SPI_IsActiveFlag_BSY(SPI1))
//		;
//	LL_SPI_TransmitData8(SPI1, 0);
//	while (LL_SPI_IsActiveFlag_BSY(SPI1))
//		;
//	LL_GPIO_SetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
//	LL_SPI_Disable(SPI1);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
}

void SSD1357_SetContrastCurrentABC(uint8_t ccA, uint8_t ccB, uint8_t ccC) {
//	LL_SPI_Enable(SPI1);
//	LL_GPIO_ResetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
	SendCommand_Sync(SSD1357_CMD_SetContrastCurrentABC);
	HAL_SPI_Transmit(&hspi1, &ccA, 1, 1000);
	HAL_SPI_Transmit(&hspi1, &ccB, 1, 1000);
	HAL_SPI_Transmit(&hspi1, &ccC, 1, 1000);
//	LL_SPI_TransmitData8(SPI1, ccA);
//	while (LL_SPI_IsActiveFlag_BSY(SPI1))
//		;
//	LL_SPI_TransmitData8(SPI1, ccB);
//	while (LL_SPI_IsActiveFlag_BSY(SPI1))
//		;
//	LL_SPI_TransmitData8(SPI1, ccC);
//	while (LL_SPI_IsActiveFlag_BSY(SPI1))
//		;
//	LL_GPIO_SetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
//	LL_SPI_Disable(SPI1);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
}

void SSD1357_SetMasterContrastCurrent(uint8_t ccCode) {
//	LL_SPI_Enable(SPI1);
//	LL_GPIO_ResetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
	SendCommand_Sync(SSD1357_CMD_SetMasterContrastCurrent);
	HAL_SPI_Transmit(&hspi1, &ccCode, 1, 1000);
//	LL_SPI_TransmitData8(SPI1, ccCode);
//	while (LL_SPI_IsActiveFlag_BSY(SPI1))
//		;
//	LL_GPIO_SetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
//	LL_SPI_Disable(SPI1);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
}

void SSD1357_SetResetPrechargePeriod(uint8_t reset_clocks, uint8_t precharge_clocks) {
	uint8_t d = (((precharge_clocks & 0x0F) << 4) | (reset_clocks & 0x0F));

//	LL_SPI_Enable(SPI1);
//	LL_GPIO_ResetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
	SendCommand_Sync(SSD1357_CMD_SetResetPrechargePeriod);
	HAL_SPI_Transmit(&hspi1, &d, 1, 1000);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
//	LL_SPI_TransmitData8(SPI1, d);
//	while (LL_SPI_IsActiveFlag_BSY(SPI1))
//		;
//	LL_GPIO_SetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
//	LL_SPI_Disable(SPI1);
}

void SSD1357_SetPrechargeVoltage(uint8_t voltage_scale_code) {
//	LL_SPI_Enable(SPI1);
//	LL_GPIO_ResetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
	SendCommand_Sync(SSD1357_CMD_SetPrechargeVoltage);
	HAL_SPI_Transmit(&hspi1, &voltage_scale_code, 1, 1000);
//	LL_SPI_TransmitData8(SPI1, voltage_scale_code);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
//	while (LL_SPI_IsActiveFlag_BSY(SPI1))
//		;
//	LL_GPIO_SetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
//	LL_SPI_Disable(SPI1);
}

void SSD1357_SetMLUTGeryscale(uint8_t * pdata63B) {
//	LL_SPI_Enable(SPI1);
//	LL_GPIO_ResetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
	SendCommand_Sync(SSD1357_CMD_MLUTGrayscale);
	HAL_SPI_Transmit(&hspi1, pdata63B, 63, 1000);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
//	for (unsigned i = 0; i < 63; ++i) {
//		LL_SPI_TransmitData8(SPI1, pdata63B[i]);
//		while (LL_SPI_IsActiveFlag_BSY(SPI1))
//			;
//	}
//	LL_GPIO_SetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
//	LL_SPI_Disable(SPI1);
}

void SSD1357_UseDefaultLUT() {
//	LL_SPI_Enable(SPI1);
//	LL_GPIO_ResetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
	SendCommand_Sync(SSD1357_CMD_UseDefMLUT);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
//	LL_GPIO_SetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
//	LL_SPI_Disable(SPI1);
}

void SSD1357_setVCOMH(uint8_t voltage_scale_code) {
//	LL_SPI_Enable(SPI1);
//	LL_GPIO_ResetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
	SendCommand_Sync(SSD1357_CMD_SetVCOMH);
	HAL_SPI_Transmit(&hspi1, &voltage_scale_code, 1, 1000);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
//	LL_SPI_TransmitData8(SPI1, voltage_scale_code);
//	while (LL_SPI_IsActiveFlag_BSY(SPI1))
//		;
//	LL_GPIO_SetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
//	LL_SPI_Disable(SPI1);
}

void SSD1357_SetColumnAddress(uint8_t start, uint8_t stop) {
//	LL_SPI_Enable(SPI1);
//	LL_GPIO_ResetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
	SendCommand_Sync(SSD1357_CMD_SetColumnAddress);
	HAL_SPI_Transmit(&hspi1, &start, 1, 1000);
	HAL_SPI_Transmit(&hspi1, &stop, 1, 1000);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
//	LL_SPI_TransmitData8(SPI1, start);
//	while (LL_SPI_IsActiveFlag_BSY(SPI1))
//		;
//	LL_SPI_TransmitData8(SPI1, stop);
//	while (LL_SPI_IsActiveFlag_BSY(SPI1))
//		;
//	LL_GPIO_SetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
//	LL_SPI_Disable(SPI1);
}

void SSD1357_SetRowAddress(uint8_t start, uint8_t stop) {
//	LL_SPI_Enable(SPI1);
//	LL_GPIO_ResetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
	SendCommand_Sync(SSD1357_CMD_SetRowAddress);
	HAL_SPI_Transmit(&hspi1, &start, 1, 1000);
	HAL_SPI_Transmit(&hspi1, &stop, 1, 1000);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
//	LL_SPI_TransmitData8(SPI1, start);
//	while (LL_SPI_IsActiveFlag_BSY(SPI1))
//		;
//	LL_SPI_TransmitData8(SPI1, stop);
//	while (LL_SPI_IsActiveFlag_BSY(SPI1))
		;
//	LL_GPIO_SetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
//	LL_SPI_Disable(SPI1);
}

void SSD1357_SetDisplayMode_Reset() {
//	LL_SPI_Enable(SPI1);
//	LL_GPIO_ResetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
	SendCommand_Sync(SSD1357_CMD_SDM_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
//	LL_GPIO_SetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
//	LL_SPI_Disable(SPI1);
}

void SSD1357_DefaultInit() {
	SSD1357_SetCommandLock(false);
	SSD1357_SetSleepMode(true);
	SSD1357_SetClockDivider(0xB0);
	SSD1357_SetMuxRatio(0x3F);
	SSD1357_SetDisplayOffset(0x40);
	SSD1357_SetDisplayStartLine(0x00);
	SSD1357_SetRemapColorDepth(false, true, true, true, true, SSD1357_COLOR_MODE_65k);
	SSD1357_SetContrastCurrentABC(0x88, 0x32, 0x88);
	SSD1357_SetMasterContrastCurrent(0x0F);
	SSD1357_SetResetPrechargePeriod(0x02, 0x03);

	SSD1357_UseDefaultLUT();
	SSD1357_SetPrechargeVoltage(0x17);
	SSD1357_setVCOMH(0x05);
	SSD1357_SetColumnAddress(SSD1357_START_COL, SSD1357_STOP_COL);
	SSD1357_SetRowAddress(SSD1357_START_ROW, SSD1357_STOP_ROW);
	SSD1357_SetDisplayMode_Reset();
	SSD1357_SetSleepMode(false);

	SSD1357_FillDisplay(0x0000);
}

static void SSD1357_FillWorkingBuffer(uint16_t value, uint8_t num_pixels) {
	if (num_pixels > SSD1357_WORKING_BUFF_NUM_PIXELS) {
		num_pixels = SSD1357_WORKING_BUFF_NUM_PIXELS;
	}

	for (uint8_t i = 0; i < num_pixels; ++i) {
		working_buff[2 * i + 0] = ((value & 0xFF00) >> 8);
		working_buff[2 * i + 1] = ((value & 0x00FF) >> 0);
	}
}

static void SSD1357_WriteRAM(uint8_t * pdata, uint8_t startrow, uint8_t startcol, uint8_t stoprow, uint8_t stopcol, uint16_t size) {
	SSD1357_SetRowAddress(startrow, stoprow);
	SSD1357_SetColumnAddress(startcol, stopcol);

//	LL_SPI_Enable(SPI1);
//	LL_GPIO_ResetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
	SendCommand_Sync(SSD1357_CMD_WriteRAM);

	HAL_SPI_Transmit(&hspi1, pdata, size, 1000);
//	for (unsigned i = 0; i < size; ++i) {
//		LL_SPI_TransmitData8(SPI1, pdata[i]);
//		while (LL_SPI_IsActiveFlag_BSY(SPI1))
//			;
//	}
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
//	LL_GPIO_SetOutputPin(SSD1357_NSS_GPIO_Port, SSD1357_NSS_LL_Pin);
}

static void SSD1357_SetPixelRam(uint8_t x, uint8_t y, uint16_t value)
{
	if((x >= SSD1357_MAX_WIDTH) || (y >= SSD1357_MAX_HEIGHT))	// Make sure we are within the RAM limits
	{
		return;
	}
	working_buff[0] = ((value & 0xFF00) >> 8);
	working_buff[1] = ((value & 0x00FF) >> 0);
	SSD1357_WriteRAM(working_buff, y, x, SSD1357_STOP_ROW, SSD1357_STOP_COL, 2);
}

static void SSD1357_FastFilledRectangle(int8_t x0, int8_t y0, int8_t x1, int8_t y1, int16_t value) {
	// This uses the boundaries on write_ram to quickly fill a given rectangle
	bool x0offscreen = false;
	bool x1offscreen = false;
	bool y0offscreen = false;
	bool y1offscreen = false;

	// Ensure bounds are good
	if(x0 >= SSD1357_MAX_WIDTH) {
		x0 = SSD1357_MAX_WIDTH-1;
		x0offscreen = true;
	}
	if(x1 >= SSD1357_MAX_WIDTH) {
		x1 = SSD1357_MAX_WIDTH-1;
		x1offscreen = true;
	}
	if(y0 >= SSD1357_MAX_HEIGHT) {
		y0 = SSD1357_MAX_HEIGHT-1;
		y0offscreen = true;
	}
	if(y1 >= SSD1357_MAX_HEIGHT) {
		y1 = SSD1357_MAX_HEIGHT-1;
		y1offscreen = true;
	}

	if(x0 < 0) {
		x0 = 0;
		x0offscreen = true;
	}
	if(x1 < 0) {
		x1 = 0;
		x1offscreen = true;
	}
	if(y0 < 0) {
		y0 = 0;
		y0offscreen = true;
	}
	if(y1 < 0) {
		y1 = 0;
		y1offscreen = true;
	}

	if ((x1offscreen == true) && (x0offscreen == true)) {
		return;
	}
	if ((y1offscreen == true) && (y0offscreen == true)) {
		return;
	}

	// Ensure the order is right
	if (x0 > x1) {
		uint8_t temp = x0;
		x0 = x1;
		x1 = temp;
	}

	if (y0 > y1) {
		uint8_t temp = y0;
		y0 = y1;
		y1 = temp;
	}

	uint8_t width = x1-x0+1;
	uint8_t height = y1-y0+1;

	uint8_t rows_per_block = SSD1357_WORKING_BUFF_NUM_PIXELS / width;
	uint8_t num_full_blocks = height / rows_per_block;
	uint8_t remaining_rows = height - (num_full_blocks * rows_per_block);

	uint8_t offsety = 0;

	for(uint8_t indi = 0; indi < num_full_blocks; indi++) {
		SSD1357_FillWorkingBuffer(value, rows_per_block * width);
		SSD1357_WriteRAM(working_buff, y0 + offsety, x0, y1, x1, 2 * rows_per_block * width);
		offsety += rows_per_block;
	}
	SSD1357_FillWorkingBuffer(value, remaining_rows * width);
	SSD1357_WriteRAM(working_buff, y0 + offsety, x0, y1, x1, 2 * remaining_rows * width);
}

void SSD1357_SetPixel(uint8_t x, uint8_t y, uint16_t value)
{
	SSD1357_SetPixelRam(SSD1357_START_COL + x,SSD1357_START_ROW + y, value);
}

void SSD1357_FillDisplay(uint16_t value) {
	SSD1357_FastFilledRectangle(SSD1357_START_COL, SSD1357_START_ROW, SSD1357_STOP_COL, SSD1357_STOP_ROW, value);
}

void SSD1357_FillRect(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint16_t value) {
	SSD1357_FastFilledRectangle(SSD1357_START_COL + x, SSD1357_START_ROW + y, SSD1357_START_COL + x + width, SSD1357_START_ROW + y + height, value);
}

void SSD1357_Image(uint8_t x, uint8_t y, uint8_t width, uint8_t height, const uint8_t * pixels) {
//	SSD1357_Image(0, 0, 64, 64, img1);
	SSD1357_WriteRAM(pixels, 0, 0, 64, 64, 4096);
}
