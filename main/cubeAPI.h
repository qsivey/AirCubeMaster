/** ____________________________________________________________________
 *
 * 	AirCube project
 *
 *	GitHub:		qsivey, Nik125Y
 *	Telegram:	@qsivey, @Nik125Y
 *	Email:		qsivey@gmail.com, topnikm@gmail.com
 *	____________________________________________________________________
 */

#ifndef		MAIN_CUBEAPI_H_
#define		MAIN_CUBEAPI_H_

#ifdef		__cplusplus
extern		"C" {
#endif
/* = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = */


#include	"adjunct.h"

#define		API_RECEIVER_ID			0
#define		API_MASTER_ID			1
#define		API_SLAVE_BASE_ID		2

#define		API_MUSIC_CHUNK_SIZE	1024
#define		API_MAX_SERVICE_SIZE	16


typedef enum
{
	CUBE_CMD_UNKNOWN				= 0,
	
	/* Receiver commands: 0 - 15 */
	CUBE_CMD_A2DP_MUSIC_STREAM		= 1,
	CUBE_CMD_RVCP_VOLUME_LEVEL		= 2,
	CUBE_CMD_RVCP_PLAY				= 3,
	CUBE_CMD_RVCP_PAUSE				= 4,
	CUBE_CMD_RVCP_STOP				= 5,
	
	/* Service commands: 16 - 63 */
	/* Master only: */
	CUBE_CMD_PING					= 16,
	CUBE_CMD_GET_DEVICE_INFO		= 17,
	CUBE_CMD_SET_DEVICE_TYPE		= 18,
	CUBE_CMD_SYNC_TIME				= 19,
	
	/* Slaves only: */
	CUBE_CMD_CONFIRM				= 50,
	CUBE_CMD_SEND_DEVICE_INFO		= 51,
	
	/* Play music commands: 64 - 255 */
	CUBE_CMD_SEND_MUSIC_CHUNK		= 64,
	CUBE_CMD_SET_VOLUME				= 65,
	CUBE_CMD_PAUSE					= 66,
	CUBE_CMD_STOP					= 67,
	CUBE_CMD_PLAY					= 68,
	CUBE_CMD_SET_BQ_FILTERS			= 69,
	
	/* Common: */
	CUBE_CMD_ERROR					= 255
	
}	cubeAPI_CommandID_t;


/* ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
 *															API commands
 */
typedef struct PACKED__
{
	uint8_t					deviceID,
							commandID;
								
	uint16_t				payloadSize;
	
}	cubeAPI_CommandHeader_t;


typedef struct PACKED__
{
	ui32					frameID;
	ui8						fragmentID;

	ui8						data [API_MUSIC_CHUNK_SIZE];
	
}	cubeAPI_MusicChunk_t;


/* = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = */
#ifdef		__cplusplus
			}
#endif

#endif		/* MAIN_CUBEAPI_H_ */
