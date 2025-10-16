#include "esp_err.h"
#include "esp_event_base.h"
#include "esp_netif.h"
#include "esp_wifi_default.h"
#include "esp_wifi_types_generic.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "driver/spi_slave.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"gpt
#include "driver/gpio.h"
#include "lwip/sockets.h"
#include "esp_mac.h"   
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "hal/i2s_types.h"
#include "driver/i2s_tdm.h"
#include "math.h"

#define S3_MASTER

/* Wi-Fi */
#define WIFI_SSID      	"AirCube"
#define WIFI_PASS      	"1kT45678ex"
#define MAX_STA_CONN   	4

#define SERVER_IP      	"192.168.4.1"   
#define SERVER_PORT    	3333   
        
/* Pins */   
#define PIN_MISO  		37
#define PIN_MOSI  		38
#define PIN_SCLK		36
#define PIN_CS    		40

#define I2S_BCLK 12
#define I2S_LRCLK 13
#define I2S_DOUT 11

#define TAS2563_I2C_ADDR 0x4C

#define I2C_MASTER_SDA 17
#define I2C_MASTER_SCL 18
#define I2C_MASTER_FREQ_HZ 400000

/* Size of buffer */
#define BUF_SIZE       	256
#define RX_BUF_SIZE		1024*4



static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t tas2563_handle = NULL;
static i2s_chan_handle_t tx_chan; 

static const gpio_num_t pins[] = 
{
	//	 LED_PIN		   
    GPIO_NUM_6,  GPIO_NUM_21 
};

#define NUM_PINS (sizeof(pins)/sizeof(pins[0]))

#define RINGBUF_SIZE   (8*1024)
static volatile size_t rb_head = 0;
static volatile size_t rb_tail = 0;
static volatile bool rb_full = false;
static volatile bool rb_active = false;   
static volatile bool rb_new_song = false;

#ifdef S3_MASTER
	static const char *TAG = "Master";
	
	static uint8_t ringBuf[RINGBUF_SIZE];
	int16_t i2s_buf[1024]; 
	WORD_ALIGNED_ATTR uint8_t rx_buf[RX_BUF_SIZE]; 
	
#else
	static const char *TAG = "Slave";
#endif


static esp_err_t tas2563_write_reg (uint8_t reg, uint8_t value)
{
    uint8_t data [2] = { reg, value };
    return i2c_master_transmit(tas2563_handle, data, sizeof(data), 100);
}


static esp_err_t tas2563_read_reg (uint8_t reg, uint8_t *value)
{
    esp_err_t ret = i2c_master_transmit(tas2563_handle, &reg, 1, 100);
    if (ret != ESP_OK) return ret;
    return i2c_master_receive(tas2563_handle, value, 1, 100);
}


void rb_reset (void)
{
    rb_head = 0;
    rb_tail = 0;
    rb_full = false;
    rb_active = false;
    rb_new_song = false;
    memset(ringBuf, 0, sizeof(ringBuf));
}


static void rb_write (const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        ringBuf[rb_head] = data[i];
        rb_head = (rb_head + 1) % RINGBUF_SIZE;

        if (rb_full) 
        {
            rb_tail = (rb_tail + 1) % RINGBUF_SIZE; 
        }

        if (rb_head == rb_tail)
            rb_full = true;

        if (rb_full && !rb_active)
            rb_active = true; 
    }
}

static size_t rb_read (uint8_t *dst, size_t len)
{
    if (!rb_active) return 0; 

    size_t bytes_read = 0;
    for (size_t i = 0; i < len; i++)
    {
        if (rb_head == rb_tail && !rb_full) break; 

        dst[i] = ringBuf[rb_tail];
        rb_tail = (rb_tail + 1) % RINGBUF_SIZE;
        rb_full = false;
        bytes_read++;
    }

    return bytes_read;
}


static esp_err_t i2c_init(void)
{
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_MASTER_SDA,
        .scl_io_num = I2C_MASTER_SCL,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus));

    i2c_device_config_t dev_cfg = {
        .device_address = TAS2563_I2C_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus, &dev_cfg, &tas2563_handle));

    ESP_LOGI(TAG, "I2C init done");
    return ESP_OK;
}

/* SPI */
#ifdef S3_MASTER

void spi_slave_init (void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .sclk_io_num = PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = RX_BUF_SIZE,
    };

    spi_slave_interface_config_t slvcfg = {
		
        .spics_io_num = PIN_CS,
        .flags = 0,
        .queue_size = 4,
        .mode = 0,   // SPI mode 0
        .post_setup_cb = NULL,
        .post_trans_cb = NULL
    };

    
    ESP_ERROR_CHECK(spi_slave_initialize(SPI2_HOST, &buscfg, &slvcfg, SPI_DMA_CH_AUTO));
    ESP_LOGI(TAG, "SPI Slave initialized");
}


void spi_slave_task (void* arg)
{
	esp_err_t ret;
	 
    spi_slave_transaction_t t;
    memset(&t, 0, sizeof(t));
	t.length = sizeof(rx_buf) * 8;
    t.tx_buffer = NULL;
    t.rx_buffer = rx_buf;
    
    while (1)
     {
        ret = spi_slave_transmit(SPI2_HOST, &t, portMAX_DELAY);
        if (ret == ESP_OK) 
        {
			rb_write(rx_buf, sizeof(rx_buf));
//			ESP_LOGW(TAG, "SPI recv %d", rx_buf[5]);
		}
		vTaskDelay(1);
    }
}

#endif


static void TAS2563_Init (void)
{
	ESP_LOGI(TAG, "Init start TAS2563");
	
	ESP_ERROR_CHECK(tas2563_write_reg(0, 0));
	
	ESP_ERROR_CHECK(tas2563_write_reg(0x01, 0x01)); // reset
	vTaskDelay(100);
	
	ESP_ERROR_CHECK(tas2563_write_reg(0x02, 0x0E)); // Software shutdown
	vTaskDelay(10);
	
	ESP_ERROR_CHECK(tas2563_write_reg(0x07, 0x00));
	
	ESP_ERROR_CHECK(tas2563_write_reg(0x08, 0x18)); 
	
 	ESP_ERROR_CHECK(tas2563_write_reg(0x02, 0x00)); // Turn ON

	
    ESP_LOGI(TAG, "Init end TAS2563");
}

void tas_error (void)
{
    uint8_t val;
    esp_err_t err;

	tas2563_write_reg(0x27, 0x00);
	tas2563_write_reg(0x28, 0x00);
	tas2563_write_reg(0x29, 0x00);
	tas2563_write_reg(0x2A, 0x00);
	tas2563_write_reg(0x2B, 0x00);
	tas2563_write_reg(0x2C, 0x00);
	tas2563_write_reg(0x2D, 0x00);
	tas2563_write_reg(0x2E, 0x00);
	tas2563_write_reg(0x2F, 0x00);
	tas2563_write_reg(0x30, 0x00);
	
    ESP_LOGI(TAG, "=== TAS2563 STATUS CHECK ===");

    // 0x27 - FAULT_STATUS
    if ((err = tas2563_read_reg(0x27, &val)) == ESP_OK)
        ESP_LOGI(TAG, "FAULT_STATUS(0x27) = 0x%02X", val);
    else ESP_LOGE(TAG, "Read 0x27 failed (%s)", esp_err_to_name(err));

    // 0x28 - IRQ_STATUS_1
    if ((err = tas2563_read_reg(0x28, &val)) == ESP_OK)
        ESP_LOGI(TAG, "IRQ_STATUS_1(0x28) = 0x%02X", val);
    else ESP_LOGE(TAG, "Read 0x28 failed (%s)", esp_err_to_name(err));

    // 0x29 - IRQ_STATUS_2
    if ((err = tas2563_read_reg(0x29, &val)) == ESP_OK)
        ESP_LOGI(TAG, "IRQ_STATUS_2(0x29) = 0x%02X", val);
    else ESP_LOGE(TAG, "Read 0x29 failed (%s)", esp_err_to_name(err));

    // 0x2A - TEMP_WARNING_STATUS
    if ((err = tas2563_read_reg(0x2A, &val)) == ESP_OK)
        ESP_LOGI(TAG, "TEMP_STATUS(0x2A) = 0x%02X", val);
    else ESP_LOGE(TAG, "Read 0x2A failed (%s)", esp_err_to_name(err));

    // 0x2B - PWR_STATUS
    if ((err = tas2563_read_reg(0x2B, &val)) == ESP_OK)
        ESP_LOGI(TAG, "POWER_STATUS(0x2B) = 0x%02X", val);
    else ESP_LOGE(TAG, "Read 0x2B failed (%s)", esp_err_to_name(err));

    // 0x2C - CLOCK_STATUS
    if ((err = tas2563_read_reg(0x2C, &val)) == ESP_OK)
        ESP_LOGI(TAG, "CLOCK_STATUS(0x2C) = 0x%02X", val);
    else ESP_LOGE(TAG, "Read 0x2C failed (%s)", esp_err_to_name(err));

    // 0x2D - TDM_ERROR_STATUS
    if ((err = tas2563_read_reg(0x2D, &val)) == ESP_OK)
        ESP_LOGI(TAG, "TDM_ERROR_STATUS(0x2D) = 0x%02X", val);
    else ESP_LOGE(TAG, "Read 0x2D failed (%s)", esp_err_to_name(err));

    // 0x2E - BOOST_STATUS
    if ((err = tas2563_read_reg(0x2E, &val)) == ESP_OK)
        ESP_LOGI(TAG, "BOOST_STATUS(0x2E) = 0x%02X", val);
    else ESP_LOGE(TAG, "Read 0x2E failed (%s)", esp_err_to_name(err));

    // 0x2F - PLL_STATUS
    if ((err = tas2563_read_reg(0x2F, &val)) == ESP_OK)
        ESP_LOGI(TAG, "PLL_STATUS(0x2F) = 0x%02X", val);
    else ESP_LOGE(TAG, "Read 0x2F failed (%s)", esp_err_to_name(err));

    // 0x30 - DIE_TEMP (опционально)
    if ((err = tas2563_read_reg(0x30, &val)) == ESP_OK)
        ESP_LOGI(TAG, "DIE_TEMP(0x30) = 0x%02X (~%.1f°C)", val, (float)val * 0.75f);
    else ESP_LOGE(TAG, "Read 0x30 failed (%s)", esp_err_to_name(err));

    ESP_LOGI(TAG, "==============================");
}
/* I2S */
//static i2s_chan_handle_t tx;

static void i2s_init (void) 
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan, NULL));

    i2s_tdm_config_t tdm = {
        .clk_cfg  = I2S_TDM_CLK_DEFAULT_CONFIG(48000),   // Fs = 48 kHz
        .slot_cfg = {
            .data_bit_width   = I2S_DATA_BIT_WIDTH_16BIT,   
            .slot_bit_width   = I2S_SLOT_BIT_WIDTH_16BIT,   
            .slot_mode        = I2S_SLOT_MODE_STEREO,
            .total_slot       = 1,                          
            .slot_mask        = I2S_TDM_SLOT0,              
            .ws_width         = 16,                         // ширина FSYNC = 1 слот × 16
            .ws_pol           = false,                      
            .bit_shift        = false,                      	// MSB на первом такте слота
            .left_align       = true,             			// I2S-подобное выравнивание
            .big_endian       = false,
            .bit_order_lsb    = false,
            .skip_mask        = false,
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,                       
            .bclk = I2S_BCLK,                              
            .ws   = I2S_LRCLK,                             
            .dout = I2S_DOUT,                            
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = { 0 },
            .invert_flags.bclk_inv = true,
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_tdm_mode(tx_chan, &tdm));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));
}


static void i2s_task(void *arg)
{
	ESP_LOGI(TAG, "i2s transmit start");
	
    while (1) 
    {
		
		if (rb_new_song)
		{
            rb_reset();          
            rb_new_song = false;
        }

        if (rb_active) 
        {
            size_t got = rb_read((uint8_t*)i2s_buf, sizeof(i2s_buf));
            if (got > 0) 
            {
                size_t written;
                i2s_channel_write(tx_chan, i2s_buf, got, &written, 10);
            }
        }
        
		vTaskDelay(1);
    }
}


/* Wi-Fi Hendler */
static void wifi_event_handler (void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT)
     {
        switch (event_id) 
        {
            case WIFI_EVENT_AP_START:
            {
                ESP_LOGI(TAG, "SoftAP started");
                break;
			}
			
            case WIFI_EVENT_AP_STOP:
            {
                ESP_LOGI(TAG, "SoftAP stopped");
                break;
			}
			
            case WIFI_EVENT_AP_STACONNECTED: 
            {
                wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
                ESP_LOGI(TAG, "Client connected: MAC="MACSTR", AID=%d",
                         MAC2STR(event->mac), event->aid);
                break;
            }

            case WIFI_EVENT_AP_STADISCONNECTED:
             {
                wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
                ESP_LOGI(TAG, "Client disconnected: MAC="MACSTR", AID=%d",
                         MAC2STR(event->mac), event->aid);
                break;
            }
            
            case WIFI_EVENT_STA_START:
            {
				esp_wifi_connect();
				ESP_LOGI(TAG, "Station started"); 
				break;
			}
			
			case WIFI_EVENT_STA_CONNECTED:
			{
				ESP_LOGI(TAG, "Station connected to AP");
				break;
    		}
    		
            case WIFI_EVENT_STA_DISCONNECTED:
            {
				wifi_event_sta_disconnected_t* disconn = (wifi_event_sta_disconnected_t*)event_data;
    			ESP_LOGW(TAG, "Disconnected from AP, reason: %d", disconn->reason);
				esp_wifi_connect();
				break;
			}
			
			case IP_EVENT_STA_GOT_IP:
            {
				ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        		ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        		break;
			}
			
            default:
                ESP_LOGI(TAG, "Unhandled WiFi event: %ld", (long)event_id);
                break;
        }
    }
    
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}


/* Wi-Fi init */
static void wifi_init (void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	#ifdef S3_MASTER
	
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
													 ESP_EVENT_ANY_ID,
													 &wifi_event_handler,
													 NULL,
													 NULL));
													 
	#else
	
	
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
                                                        
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
                                                        
    #endif
    
    wifi_config_t wifi_config = {
		
		#ifdef S3_MASTER
		
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .channel = 1,                        
            .password = WIFI_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
//            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
            .pmf_cfg = {
                .required = false,
            },
         }, 
         
        #else
        
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
        
        #endif
    };
	
	#ifdef S3_MASTER
	
    if (strlen(WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
	
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    
    #else 
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    #endif
    
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi started. SSID:%s password:%s", WIFI_SSID, WIFI_PASS);
}

/*  UDP task */
#ifdef S3_MASTER

//static void udp_server_task (void *pvParameters)
//{
//    struct sockaddr_in dest_addr;
//    dest_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST); 
//    dest_addr.sin_family = AF_INET;
//    dest_addr.sin_port = htons(SERVER_PORT);
//
//    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
//    if (sock < 0) {
//        ESP_LOGE(TAG, "Unable to create socket");
//        vTaskDelete(NULL);
//    }
//
//    while (1)
//     {
//        sendto(sock, tx_buf, sizeof(tx_buf), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
//        ESP_LOGI(TAG, "Data sent (%d bytes)", sizeof(tx_buf));
//        
//        vTaskDelay(pdMS_TO_TICKS(1000));
//    }
//}

#else

static void udp_client_task (void *pvParameters)
{
    uint16_t rx_buffer[BUF_SIZE];
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(SERVER_PORT);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0)
    {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Socket created, waiting for data...");

    while (1) 
    {
        struct sockaddr_in source_addr;
        socklen_t socklen = sizeof(source_addr);
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0,
                           (struct sockaddr *)&source_addr, &socklen);

        if (len > 0)
        {
            rx_buffer[len] = 0; 
            ESP_LOGI(TAG, "Received: %d", rx_buffer[8]);
        } 
        else 
        {
            ESP_LOGW(TAG, "recvfrom failed: errno %d", errno);
        }
    }

    close(sock);
    vTaskDelete(NULL);
}

#endif

#define SAMPLE_RATE     48000
#define TONE_FREQ_HZ    1000
#define AMPLITUDE       30000
#define BUF_LEN         256

static void test (void *pvParameters)
{
    ESP_LOGI("TESTE", "Generating sine wave");

    static int16_t i2s_buf[BUF_LEN];
    float phase = 0.0f;
    const float delta = 2.0f * M_PI * TONE_FREQ_HZ / SAMPLE_RATE;

    while (1)
    {
        for (int i = 0; i < BUF_LEN; i++)
        {
            i2s_buf[i] = (int16_t)(AMPLITUDE * sinf(phase));
            phase += delta;
            if (phase > 2.0f * M_PI)
                phase -= 2.0f * M_PI;
        }

        for (int i = 0; i < BUF_LEN; i++)
        {
            uint16_t s = (uint16_t)i2s_buf[i];
            i2s_buf[i] = (s >> 8) | (s << 8);
        }

        size_t written = 0;
        esp_err_t ret = i2s_channel_write(tx_chan, i2s_buf, sizeof(i2s_buf), &written, portMAX_DELAY);
        if (ret != ESP_OK)
        {
            ESP_LOGE("TEST_TONE", "I2S write error: %s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}



// main 
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
 
//    wifi_init();
//    esp_wifi_set_max_tx_power(24);
    
	#ifdef S3_MASTER
	for (int i = 0; i < NUM_PINS; i++)
	{
		gpio_config_t io_conf = {};
	    io_conf.intr_type = GPIO_INTR_DISABLE;
	    io_conf.mode = GPIO_MODE_OUTPUT;
	    io_conf.pin_bit_mask = (1ULL<<pins[i]);
	    io_conf.pull_down_en = 0;
	    io_conf.pull_up_en = 0;
	    gpio_config(&io_conf);
	}
	
    gpio_set_level(pins[0], 1);
    
    gpio_set_level(pins[1], 0);
    vTaskDelay(100);
    gpio_set_level(pins[1], 1);
    vTaskDelay(100);
    
    i2c_init();
	spi_slave_init();
	
    i2s_init();
    
    TAS2563_Init();
	uint8_t val;
	tas2563_read_reg(0x06, &val);
    ESP_LOGI(TAG, "0x06 = 0x%02X", val);
    tas2563_read_reg(0x07, &val);
    ESP_LOGI(TAG, "0x07 = 0x%02X", val);
    tas2563_read_reg(0x08, &val);
    ESP_LOGI(TAG, "0x08 = 0x%02X", val);
    tas2563_read_reg(0x09, &val);
    ESP_LOGI(TAG, "0x09 = 0x%02X", val);
    tas2563_read_reg(0x06, &val);
    ESP_LOGI(TAG, "0x06 = 0x%02X", val);
    tas_error();
	rb_new_song = true;
	
    xTaskCreate(spi_slave_task, "spi_slave_task", 8192, NULL, 5, NULL);
	xTaskCreate(i2s_task, "i2s_task", 8192, NULL, 4, NULL);
//	xTaskCreate(test, "test", 4096, NULL, 5, NULL);
	
//    xTaskCreate(udp_server_task, "udp_server", 4096, NULL, 5, NULL);
    
    #else
    vTaskDelay(pdMS_TO_TICKS(5000));
    xTaskCreate(udp_client_task, "udp_client", 4096, NULL, 5, NULL);
	#endif
}
