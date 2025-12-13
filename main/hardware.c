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
#include "adjunct.h"
#include "hardware.h"


static const char *TAG = "Hardware";

static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t tas2563_handle = NULL;
static i2s_chan_handle_t tx_chan;


extern AirCube_t AirCube;


static esp_err_t TAS5825_WriteReg (ui8 reg, ui8 value)
{
    ui8 data [2] = { reg, value };
    
    return i2c_master_transmit(tas2563_handle, data, sizeof(data), 100);
}

#if (qcfgPROJ_DEBUG)

	static esp_err_t TAS5825_ReadReg (ui8 reg, ui8 *value)
	{
	    esp_err_t ret = i2c_master_transmit(tas2563_handle, &reg, 1, 100);
	    
	    if (ret != ESP_OK)
	    	return ret;
	    
	    return i2c_master_receive(tas2563_handle, value, 1, 100);
	}
	
#endif


void GPIO_Init (void)
{
	static const gpio_num_t pins [] =
	{
	    LED_GPIO, CHIP_SELECT
	};
	
    for (int i = 0; i < 2; i++)
    {
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = (1ULL << pins[i]);
        io_conf.pull_down_en = 0;
        io_conf.pull_up_en = 0;
        gpio_config(&io_conf);
    }
	
    gpio_set_level(pins[0], 1);
	vTaskDelay(10);

    gpio_set_level(pins[1], 0);
    vTaskDelay(500);
    gpio_set_level(pins[1], 1);
    vTaskDelay(500);
}


esp_err_t I2C_Init (void)
{
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_PIN_SDA,
        .scl_io_num = I2C_PIN_SCL,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus));

    i2c_device_config_t dev_cfg = {
        .device_address = TAS5825_I2C_ADDR,
        .scl_speed_hz = I2C_FREQ_HZ,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus, &dev_cfg, &tas2563_handle));

    ESP_LOGI(TAG, "I2C init done");
    
    return ESP_OK;
}


void I2S_Init (void)
{
    i2s_chan_config_t tx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&tx_chan_cfg, &tx_chan, NULL));

    i2s_std_config_t tx_std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(I2S_INIT_SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,   
            .bclk = I2S_PIN_BCLK,
            .ws   = I2S_PIN_LRCLK,
            .dout = I2S_PIN_DOUT,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };
    
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &tx_std_cfg));
    
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));
}


#if  (qcfgPROJ_CUBE_ROLE == DT_MASTER)

	void SPI_SlaveInit (void)
	{
	    spi_bus_config_t buscfg = {
	        .mosi_io_num = SPI_PIN_MOSI,
	        .miso_io_num = SPI_PIN_MISO,
	        .sclk_io_num = SPI_PIN_SCLK,
	        .quadwp_io_num = -1,
	        .quadhd_io_num = -1,
	        .max_transfer_sz = SPI_BUF_SIZE,
	    };
	
	    spi_slave_interface_config_t slvcfg = {
	        .spics_io_num = SPI_PIN_CS,
	        .flags = 0,
	        .queue_size = 4,
	        .mode = 0,
	        .post_setup_cb = NULL,
	        .post_trans_cb = NULL
	    };
	
	    ESP_ERROR_CHECK(spi_slave_initialize(SPI2_HOST, &buscfg, &slvcfg, SPI_DMA_CH_AUTO));
	    ESP_LOGI(TAG, "SPI Slave initialized");
	}

#endif


void TAS5825_Init (void)
{
    ESP_LOGI(TAG, "Init start TAS5825");

    tas5825zeropage_;
    ESP_ERROR_CHECK(TAS5825_WriteReg(0x01, 0x11)); 
    vTaskDelay(10);
    
    tas5825zeropage_;
    ESP_ERROR_CHECK(TAS5825_WriteReg(0x03, 0x03)); 
 	
 	tas5825zeropage_;
    ESP_ERROR_CHECK(TAS5825_WriteReg(0x4C, 0xFF));  // Volume mute
    
    tas5825zeropage_;
    ESP_ERROR_CHECK(TAS5825_WriteReg(0x33, 0x20));  
	
	#if (qcfgPROJ_DEBUG)
	
		ui8 val = 125;
		TAS5825_ReadReg(0x39, &val);
		ESP_LOGE(TAG, "0x39 %d", val);
		
		TAS5825_ReadReg(0x5E, &val);
		ESP_LOGE(TAG, "0x5E %d", val);
		vTaskDelay(10);
		
		TAS5825_ReadReg(0x68, &val);
		ESP_LOGE(TAG, "0x68 %d", val);
		vTaskDelay(10);
		
	#endif
	
    ESP_LOGI(TAG, "Init end TAS5825");
}


void TAS5825_SetVolume (ui8 newVolume)
{
	ui8 res = VolumeGammaConvert(newVolume);
	
	tas5825zeropage_;
	ESP_ERROR_CHECK(TAS5825_WriteReg(0x4C, res));
}


#ifdef qcfgPROJ_CUBE_MASTER

	void TaskSPI_Slave (void *params)
	{
		bool order = ping_;
		
	    esp_err_t ret;
	    spi_slave_transaction_t t;
	
	    memset(&t, 0, sizeof(t));
	    t.length = SPI_BUF_SIZE * 8;
	    t.tx_buffer = NULL;
	    t.rx_buffer = AirCube.SPI_Buff;
	
	    while (1)
	    {
			cubeAPI_CommandHeader_t cmdHeader;
			
	        ret = spi_slave_transmit(SPI2_HOST, &t, portMAX_DELAY);
	        if (ret != ESP_OK)
	        {
	            ESP_LOGW(TAG, "spi_slave_transmit err=%s", esp_err_to_name(ret));
	            continue;
	        }
	
			memcpy(&cmdHeader, t.rx_buffer, 4);
			
			ui8 *cmdData = ((ui8*)t.rx_buffer) + 4;
			
	//		ESP_LOGI(TAG, "cmd %d, payload %d, __ %d bytes", cmdHeader.commandID, cmdHeader.payloadSize, t.trans_len / 8);
			
			switch (cmdHeader.commandID)
			{
				case CUBE_CMD_SEND_MUSIC_CHUNK:
				{
					if (calcFreeSpaceFIFO(AirCube.SPI_Basket.tail, AirCube.SPI_Basket.head, BASKET_SIZE) >= SPI_BUF_SIZE)
			        {
		                BasketBufferWrite(AirCube.SPI_Basket.buff, BASKET_SIZE, &AirCube.SPI_Basket.head, cmdData, cmdHeader.payloadSize);
		                BasketBufferWrite(AirCube.TCP_Basket.buff, BASKET_SIZE, &AirCube.TCP_Basket.head, cmdData, cmdHeader.payloadSize);
		
		                if (BASKET_SIZE - calcFreeSpaceFIFO(AirCube.SPI_Basket.tail, AirCube.SPI_Basket.head, BASKET_SIZE) >= RINGBUF_SIZE)
		                {
		                    if ((AirCube.RingBuff1.state == RBS_READY_FOR_WRITE) && (order == ping_))
		                    {
		                        AirCube.RingBuff1.state = RBS_WRITING;
		
		                        BasketBufferRead(AirCube.SPI_Basket.buff, BASKET_SIZE,
		                        &AirCube.SPI_Basket.tail, AirCube.RingBuff1.buff, RINGBUF_SIZE);
		
		                        AirCube.RingBuff1.state = RBS_READY_FOR_READ;
		
		                        order = pong_;
		                    }
		
		                    else if ((AirCube.RingBuff2.state == RBS_READY_FOR_WRITE) && (order == pong_))
		                    {
		                        AirCube.RingBuff2.state = RBS_WRITING;
		
		                        BasketBufferRead(AirCube.SPI_Basket.buff, BASKET_SIZE,
		                        &AirCube.SPI_Basket.tail, AirCube.RingBuff2.buff, RINGBUF_SIZE);
		
		                        AirCube.RingBuff2.state = RBS_READY_FOR_READ;
		
		                        order = ping_;
		                    }
		                }
			        }
					
					break;
				}
				
				case CUBE_CMD_SET_VOLUME:
				{
					TAS5825_SetVolume(*cmdData);
	    			
	                if (AirCube.ServiceChTransQueue)
	                {
	                    cubeServiceChannelTransport_t cmd = { 0 };
	                    cmd.header.deviceID = 1;
	                    cmd.header.commandID = CUBE_CMD_SET_VOLUME;
	                    cmd.header.payloadSize = 1;
	                    cmd.payload[0] = *cmdData;
	
	                    (void)xQueueOverwrite(AirCube.ServiceChTransQueue, &cmd);
	                }
	
	                break;
				}
				
				case CUBE_CMD_PAUSE:
				{
					tas5825zeropage_;
	    			ESP_ERROR_CHECK(TAS5825_WriteReg(0x35, 0x00));
	    			
					break;
				}
				
				case CUBE_CMD_PLAY:
				{
					tas5825zeropage_;
	    			ESP_ERROR_CHECK(TAS5825_WriteReg(0x35, 0x11));
	    			
					break;
				}
				
				default :
					break;
			}
	        
	        vPortYield();
	    }
	}

#endif


void TaskDAC_Play (void *params)
{
    ESP_LOGI(TAG, "I2S transmit start");

    size_t written;
    bool order = ping_;

    while (!((AirCube.RingBuff1.state == RBS_READY_FOR_READ) && (AirCube.RingBuff2.state == RBS_READY_FOR_READ)))
        vTaskDelay(1);

    ESP_LOGI(TAG, "I2S buffers ready");

    while (1)
    {
        if ((AirCube.RingBuff1.state == RBS_READY_FOR_READ) && (order == ping_))
        {
            AirCube.RingBuff1.state = RBS_READING;

            esp_err_t ret = i2s_channel_write(tx_chan, AirCube.RingBuff1.buff, RINGBUF_SIZE, &written, portMAX_DELAY);

			if (ret != ESP_OK)
                ESP_LOGE(TAG, "i2s_channel_write(1) err=%s", esp_err_to_name(ret));
            
            AirCube.RingBuff1.state = RBS_READY_FOR_WRITE;

            order = pong_;
        }

        else if ((AirCube.RingBuff2.state == RBS_READY_FOR_READ) && (order == pong_))
        {
            AirCube.RingBuff2.state = RBS_READING;

            esp_err_t ret = i2s_channel_write(tx_chan, AirCube.RingBuff2.buff, RINGBUF_SIZE, &written, portMAX_DELAY);

            if (ret != ESP_OK)
                ESP_LOGE(TAG, "i2s_channel_write(2) err=%s", esp_err_to_name(ret));
            
            AirCube.RingBuff2.state = RBS_READY_FOR_WRITE;

            order = ping_;
        }
        
        vTaskDelay(1);
    }
}


void LED_BlinkTask (void *params)
{
	while (1)
    {
		switch (AirCube.LED_Mode)
		{
			case LEDM_IDLE :
			{
				gpio_set_level(LED_GPIO, 1);
				vTaskDelay(1000);

				break;
			}

			case LEDM_WAITING_CONNECTION :
			{
				gpio_set_level(LED_GPIO, 1);
				vTaskDelay(2000);
			
				gpio_set_level(LED_GPIO, 0);
				vTaskDelay(2000);

				break;
			}

			case LEDM_CONNECTED :
			{
				gpio_set_level(LED_GPIO, 1);
				vTaskDelay(50);
				
				gpio_set_level(LED_GPIO, 0);
				vTaskDelay(1950);

				break;
			}
			
			case LEDM_LOW_BATTERY :
			{
				gpio_set_level(LED_GPIO, 1);
				vTaskDelay(200);
				
				gpio_set_level(LED_GPIO, 0);
				vTaskDelay(200);

				break;
			}
			
			case LEDM_CHARGING :
			{
				gpio_set_level(LED_GPIO, 1);
				vTaskDelay(1000);
				
				gpio_set_level(LED_GPIO, 0);
				vTaskDelay(1000);

				break;
			}
		}
	}
}
