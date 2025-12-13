/** ____________________________________________________________________
 *
 * 	AirCube project
 *
 *	GitHub:		qsivey, Nik125Y
 *	Telegram:	@qsivey, @Nik125Y
 *	Email:		qsivey@gmail.com, topnikm@gmail.com
 *	____________________________________________________________________
 */
 
#include "AirCube.h"


void app_main (void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    AirCubeInit();
}
