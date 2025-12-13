/** ____________________________________________________________________
 *
 * 	AirCube project
 *
 *	GitHub:		qsivey, Nik125Y
 *	Telegram:	@qsivey, @Nik125Y
 *	Email:		qsivey@gmail.com, topnikm@gmail.com
 *	____________________________________________________________________
 */


#ifndef		MAIN_COMMON_H_
#define		MAIN_COMMON_H_

#ifdef		__cplusplus
extern		"C" {
#endif
/* = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = */


#include	"adjunct.h"

#include	<math.h>
#include	<stdint.h>
#include	<sys/socket.h>
#include	<string.h>
#include	<errno.h>
#include	"esp_crc.h"
#include	"sys/time.h"

#include	"nvs_flash.h"
#include	"portmacro.h"
#include	"esp_err.h"
#include	"esp_event_base.h"
#include	"esp_system.h"
#include	"esp_event.h"
#include	"esp_log.h"
#include	"esp_intr_alloc.h"
#include	"esp_attr.h"
#include	"esp_bit_defs.h"
#include	"esp_log.h"

#include	"freertos/FreeRTOS.h"
#include	"freertos/task.h"
#include	"freertos/event_groups.h"
#include	"freertos/queue.h"
#include	"freertos/semphr.h"

#define		RING_BUF_POW			13
#define		BASCKET_BUF_POW			15

#define		RINGBUF_SIZE			(1 << RING_BUF_POW)
#define		BASKET_SIZE				(1 << BASCKET_BUF_POW)


typedef enum
{
    RBS_READY_FOR_WRITE           = 0,
    RBS_WRITING                   = 1,
    RBS_READY_FOR_BROADCAST       = 2,
    RBS_ON_AIR                    = 3,
    RBS_READY_FOR_READ            = 4,
    RBS_READING                   = 5

}	cubeRingBufferState_t;


typedef struct
{
    volatile cubeRingBufferState_t
    						state;
    						
    ui8						buff [RINGBUF_SIZE];

}	cubeRingBuffer_t;


typedef struct
{
    volatile size_t			tail,
							head;
							
	ui8						buff [BASKET_SIZE];

}	cubeBasketBuffer_t;


/* ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
 *													 Function prototypes
 */
 ui8 VolumeGammaConvert (ui8 vol);
void BasketBufferWrite (ui8 *buf, size_t size, volatile size_t *wr, const ui8 *src, size_t n);
void BasketBufferRead (const ui8 *buf, size_t size, volatile size_t *tail, ui8 *dst, size_t n);


/* = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = */
#ifdef		__cplusplus
			}
#endif

#endif /* MAIN_COMMON_H_ */
