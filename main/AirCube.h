/** ____________________________________________________________________
 *
 * 	AirCube project
 *
 *	GitHub:		qsivey, Nik125Y
 *	Telegram:	@qsivey, @Nik125Y
 *	Email:		qsivey@gmail.com, topnikm@gmail.com
 *	____________________________________________________________________
 */
 /** ____________________________________________________________________
 *
 * 	TODO List
 *
 *	Надо в ближайшее время:
 *	1) Убрать лишние хрипы
 *	2) Засинхронизировать 4 колонки
 *
 *	На перспективу
 *	1) USB-вход
 *	2) AirPlay и прочие протоколы
 *
 *	____________________________________________________________________
 */

#ifndef		MAIN_AIRCUBE_H_
#define		MAIN_AIRCUBE_H_

#ifdef		__cplusplus
extern		"C" {
#endif
/* = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = */


#include	"projectConfig.h"

#include	"hardware.h"
#include	"common.h"
#include	"wifi.h"
#include	"tcp.h"
#include	"cubeAPI.h"


/* ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
 *													   Project Constants
 */
 

typedef enum
{
	CUBE_ERROR_NONE					= 0,
	
	
}	cubeError_t;


typedef enum
{
	CUBE_STATE_IDLE					= 0,
	
	CUBE_STATE_PLAY					= 1,
	CUBE_STATE_PAUSE				= 2,
	
	CUBE_STATE_ERROR				= 255,
	
}	cubeState_t;


typedef enum
{
	LEDM_IDLE						= 0,
	
	LEDM_WAITING_CONNECTION			= 1,
	LEDM_CONNECTED					= 2,
	LEDM_LOW_BATTERY				= 3,
	LEDM_CHARGING					= 4
	
}	cubeLED_Mode_t;


typedef enum
{
	DT_UNDEFINED					= 0,
	
	/* Role: bits 0..1 */
	DT_SLAVE						= BIT0,
	DT_MASTER						= BIT1,
	
	/* Output configuration: bits 2..3 */
	DT_CUBE_MONO					= BIT2,
	DT_CUBE_STEREO					= BIT3,
	
	/* Speaker size: bits 4..6 */
	DT_CUBE_2_INCH					= BIT4,
	DT_CUBE_2_5_INCH				= BIT5,
	DT_CUBE_3_INCH					= BIT5 | BIT4,
	DT_CUBE_3_5_INCH				= BIT6,
	DT_CUBE_4_INCH					= BIT6 | BIT4
	
}	cubeDeviceType_t;


typedef enum
{
	BF_MASTER						= BIT0,
	BF_STEREO						= BIT1,
	BF_SPEAKER_8_OHM				= BIT2,
	BF_USB_PD_SUPPORT				= BIT3,
		
}	cubeBoardFeatures_t;


typedef enum
{
	DP_UNKNOWN						= 0,
	
	/* bits 0..1 */
	DP_LEFT							= BIT0,
	DP_RIGHT						= BIT1,
	DP_CENTER						= BIT1 | BIT0,
	
	/* bits 2..3 */
	DP_FRONT						= BIT2,
	DP_BACK							= BIT3,
	
}	cubeDevicePosition_t;


#ifdef qcfgPROJ_CUBE_MASTER

	typedef struct
	{
		cubeAPI_CommandHeader_t
							header;
		
		ui8					payload [API_MAX_SERVICE_SIZE];
		
	}	cubeServiceChannelTransport_t;

#endif


typedef struct
{
	/* System parameters */
	ui16					firmwareVersion,
							boardVersion;
							
	cubeDeviceType_t		type;					// Constant information
	cubeDevicePosition_t	position;				// Defined by master
	
	/* Objects */
	cubeRingBuffer_t		RingBuff1,
							RingBuff2;

	#ifdef qcfgPROJ_CUBE_MASTER
		ui8					SPI_Buff [SPI_BUF_SIZE];			
		cubeBasketBuffer_t	SPI_Basket;
	#endif

	cubeBasketBuffer_t		TCP_Basket;
	
	ui8						PCM_Buffer [PCM_BUFFER_SIZE];
	
	/* FreeRTOS */
	QueueHandle_t			ServiceChTransQueue;
	
	/* Run-time parameters */
	ui8						deviceID;				// 1 --> 255
	
	cubeError_t				lastError;
	cubeState_t				state;
	cubeLED_Mode_t			LED_Mode;
	
	ui8						volume;					// 0 --> 127
	
	ui16					batteryVoltage;
	
	#ifdef qcfgPROJ_CUBE_SLAVE
	
		ui8					retryNumber;
		bool				connected;
		
	#endif
	
}	AirCube_t;


/* ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
 *													 Function prototypes
 */
void AirCubeInit (void);


/* = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = */
#ifdef		__cplusplus
			}
#endif

#endif		/* MAIN_AIRCUBE_H_ */
