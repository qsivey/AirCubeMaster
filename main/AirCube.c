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


AirCube_t AirCube = { 0 };


void AirCubeInit (void)
{
	GPIO_Init();
	I2C_Init();
	I2S_Init();
    TAS5825_Init();
    
    #ifdef qcfgPROJ_CUBE_MASTER
    
	    WiFi_InitAP();
	    SPI_SlaveInit();
	    
	#else
	    WiFi_InitStation();
	    
	#endif
	
	/* Software objects init */
	#ifdef qcfgPROJ_CUBE_MASTER
		AirCube.ServiceChTransQueue = xQueueCreate(1, sizeof(cubeServiceChannelTransport_t));
	#endif
	
	AirCube.firmwareVersion = qcfgPROJ_VERSION;
	AirCube.boardVersion = qcfgBOARD_VERSION;
	
	AirCube.RingBuff1.state = RBS_READY_FOR_WRITE;
	AirCube.RingBuff2.state = RBS_READY_FOR_WRITE;
	
	/* Start tasks */
	#ifdef qcfgPROJ_CUBE_MASTER
	
	    xTaskCreate(TaskSPI_Slave, "SPI Slave", 8192, NULL, 2, NULL);
	    xTaskCreate(TaskTCP_AudioServer, "TCP Audio Server", 8192, NULL, 3, NULL);
	    xTaskCreate(TaskTCP_ServiceServer,  "TCP Service Server",  8192, NULL, 4, NULL);
	    
	#endif

    xTaskCreate(TaskDAC_Play, "DAC Play", 8192, NULL, 3, NULL);
}
