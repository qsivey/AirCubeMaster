/** ____________________________________________________________________
 *
 * 	AirCube project
 *
 *	GitHub:		qsivey, Nik125Y
 *	Telegram:	@qsivey, @Nik125Y
 *	Email:		qsivey@gmail.com, topnikm@gmail.com
 *	____________________________________________________________________
 */

#ifndef		MAIN_TCP_H_
#define		MAIN_TCP_H_

#ifdef		__cplusplus
extern		"C" {
#endif
/* = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = */


#include	"adjunct.h"
#include	"projectConfig.h"

#include	"lwipopts.h"
#include	"lwip/err.h"
#include	"lwip/sys.h"
#include	"lwip/sockets.h"

#include	"netinet/tcp.h"
#include	<netdb.h>


/* ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
 *															   Constants
 */
#define		TCP_PACKET_SIZE			4096

#define		TCP_PORT_AUDIO			(qcfgSERVER_PORT)
#define		TCP_PORT_CTRL			(qcfgSERVER_PORT + 1)


/* ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
 *													 Function prototypes
 */
#ifdef qcfgPROJ_CUBE_MASTER
	void TaskTCP_AudioServer (void *params);
	void TaskTCP_ServiceServer (void *params);
#endif

#ifdef qcfgPROJ_CUBE_SLAVE
	void TaskTCP_AudioClient (void *params);
	void TaskTCP_ServiceClient (void *params);
#endif


/* = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = */
#ifdef		__cplusplus
			}
#endif

#endif /* MAIN_TCP_H_ */
