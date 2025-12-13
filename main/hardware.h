/** ____________________________________________________________________
 *
 * 	AirCube project
 *
 *	GitHub:		qsivey, Nik125Y
 *	Telegram:	@qsivey, @Nik125Y
 *	Email:		qsivey@gmail.com, topnikm@gmail.com
 *	____________________________________________________________________
 */

#ifndef		MAIN_HARDWARE_H_
#define		MAIN_HARDWARE_H_

#ifdef		__cplusplus
extern		"C" {
#endif
/* = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = */


#include	"adjunct.h"

#include	"driver/i2c_master.h"
#include	"driver/i2s_std.h"
#include	"driver/gpio.h"
#include	"driver/spi_slave.h"

#include	"hal/i2s_types.h"
#include	"soc/i2s_struct.h"
#include	"soc/i2s_reg.h"
#include	"soc/dport_reg.h"


/* ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
 *															   Constants
 */
/* SPI */
#define		SPI_BUF_SIZE			(1024 * 16)

#define		SPI_PIN_MISO			37
#define		SPI_PIN_MOSI			38
#define		SPI_PIN_SCLK			36
#define		SPI_PIN_CS				40

/* I2S */
#define		I2S_INIT_SAMPLE_RATE	44100

#define		I2S_PIN_BCLK			12
#define		I2S_PIN_LRCLK			13
#define		I2S_PIN_DOUT			11

/* I2C */
#define		I2C_PIN_SDA             17
#define		I2C_PIN_SCL             18

#define		I2C_FREQ_HZ				400000

#define		TAS5825_I2C_ADDR		0x4C

/* GPIO */
#define		LED_GPIO				GPIO_NUM_6
#define		CHIP_SELECT				GPIO_NUM_21

/* Other */
#define		tas5825zeropage_		ESP_ERROR_CHECK(TAS5825_WriteReg(0, 0));

#define		ping_					false
#define		pong_					true

#define		PCM_BUFFER_SIZE			(1024 * 4)


/* ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
 *													 Function prototypes
 */
void GPIO_Init (void);
esp_err_t I2C_Init (void);
void I2S_Init (void);
void SPI_SlaveInit (void);

void TAS5825_SetVolume (ui8 newVolume);
void TAS5825_Init (void);

#ifdef qcfgPROJ_CUBE_MASTER
	void TaskSPI_Slave (void *params);
#endif

void TaskDAC_Play (void *params);
void LED_BlinkTask (void *params);


/* = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = */
#ifdef		__cplusplus
			}
#endif

#endif		/* MAIN_HARDWARE_H_ */
