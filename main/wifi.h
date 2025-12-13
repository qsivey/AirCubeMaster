/** ____________________________________________________________________
 *
 * 	AirCube project
 *
 *	GitHub:		qsivey, Nik125Y
 *	Telegram:	@qsivey, @Nik125Y
 *	Email:		qsivey@gmail.com, topnikm@gmail.com
 *	____________________________________________________________________
 */

#ifndef		MAIN_WIFI_H_
#define		MAIN_WIFI_H_

#ifdef		__cplusplus
extern		"C" {
#endif
/* = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = */

#include	"esp_netif.h"
#include	"esp_wifi_default.h"
#include	"esp_wifi_types_generic.h"
#include	"esp_wifi.h"
#include	"esp_mac.h"

#define		WIFI_CONNECTED_BIT		BIT0
#define		WIFI_FAIL_BIT			BIT1

/* ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
 *													 Function prototypes
 */
#ifdef qcfgPROJ_CUBE_MASTER
	void WiFi_InitAP (void);
#endif

#ifdef qcfgPROJ_CUBE_SLAVE
	void WiFi_InitStation (void);
#endif


/* = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = */
#ifdef		__cplusplus
			}
#endif

#endif		/* MAIN_WIFI_H_ */
