/** ____________________________________________________________________
 *
 * 	AirCube project
 *
 *	GitHub:		qsivey, Nik125Y
 *	Telegram:	@qsivey, @Nik125Y
 *	Email:		qsivey@gmail.com, topnikm@gmail.com
 *	____________________________________________________________________
 */

#ifndef		MAIN_PROJECT_CONFIG_H_
#define		MAIN_PROJECT_CONFIG_H_

#ifdef		__cplusplus
extern		"C" {
#endif
/* = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = */


#define		setOFF					(0)
#define		setON					(-1)


/* ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
 *														 Project Configs
 */
#define		qcfgPROJ_VERSION		100				// x.xx
#define		qcfgBOARD_VERSION		20				// x.x

#define		qcfgPROJ_DEBUG			setOFF

/*	Choose speaker role:
									qcfgPROJ_CUBE_MASTER
									qcfgPROJ_CUBE_SLAVE
*/
#define		qcfgPROJ_CUBE_SLAVE

/*	Choose speaker type:
									qcfgPROJ_CUBE_S
									qcfgPROJ_CUBE_M
									qcfgPROJ_CUBE_L
									qcfgPROJ_CUBE_XL
									qcfgPROJ_CUBE_XXL
*/
#define		qcfgPROJ_CUBE_S

/* Common */
#define		qcfgPROJ_ERASE_NVS		setOFF

#define		qcfgSERVER_PORT			3333
#define		qcfgSERVER_IP			"192.168.4.1"

#ifdef qcfgPROJ_CUBE_MASTER
	#define	qcfgWIFI_MAX_CONN_AP	5
#endif

#ifdef qcfgPROJ_CUBE_SLAVE
	#define	qcfgWIFI_MAX_RETRY		10
#endif

/* Default parameters */
#define		qcfgDP_WIFI_SSID		"AirCube"
#define		qcfgDP_WIFI_PASS		"12348765"


/* = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = */
#ifdef		__cplusplus
			}
#endif

#endif		/* MAIN_PROJECT_CONFIG_H_ */
