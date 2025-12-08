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
#include "lwipopts.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "esp_mac.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "hal/i2s_types.h"
#include "driver/i2s_tdm.h"
#include "adjunct.h"
#include "esp_intr_alloc.h"
#include "portmacro.h"
#include "soc/i2s_struct.h"
#include "soc/i2s_reg.h"
#include "soc/dport_reg.h"
#include "esp_attr.h"

#include <math.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include "esp_crc.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "sys/time.h"

//#define S3_MASTER

/* Wi-Fi */
#define WIFI_SSID                  "AirCube"
#define WIFI_PASS                  "12348765"
#define MAX_STA_CONN               4

#define MAXIMUM_RETRY              10

#define WIFI_CONNECTED_BIT         BIT0
#define WIFI_FAIL_BIT              BIT1

#define SERVER_IP                  "192.168.4.1"
#define SERVER_PORT                3333

/* Pins */
#define PIN_MISO                   37
#define PIN_MOSI                   38
#define PIN_SCLK                   36
#define PIN_CS                     40

#define I2S_BCLK                   12
#define I2S_LRCLK                  13
#define I2S_DOUT                   11

#define TAS5825_I2C_ADDR           0x4C

#define I2C_MASTER_SDA             17
#define I2C_MASTER_SCL             18
#define I2C_MASTER_FREQ_HZ         400000

static const gpio_num_t pins[] =
{
    //   LED_PIN    //    CHIP_SELECT
    GPIO_NUM_6,  GPIO_NUM_21
};

#define NUM_PINS (sizeof(pins)/sizeof(pins[0]))

/* Ring and Basket Buffer */
#define RING_BUF_POW               13
#define BASCKET_BUF_POW            15

#define SPI_BUF_SIZE               (1024 * 4)
#define RINGBUF_SIZE               (1 << RING_BUF_POW)
#define BASKET_SIZE                (1 << BASCKET_BUF_POW)

typedef enum
{
    RBS_READY_FOR_WRITE           = 0,
    RBS_WRITING                   = 1,
    RBS_READY_FOR_BROADCAST       = 2,
    RBS_ON_AIR                    = 3,
    RBS_READY_FOR_READ            = 4,
    RBS_READING                   = 5

} rbState;


typedef struct
{
    volatile rbState state;
    uint8_t          ringBuf[RINGBUF_SIZE];
    uint8_t          send_id;

} rb;

typedef struct
{
    volatile size_t tail;
    volatile size_t head;
    uint8_t         buf[BASKET_SIZE];

} b;

rb ringBuf1, ringBuf2;
b basket, basket_udp;


/* Handles */
static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t tas2563_handle = NULL;
static i2s_chan_handle_t tx_chan;

/* TCP unicast */
#define PACKET_SIZE	4096

uint8_t PCM_Buffer [PACKET_SIZE];

/* TAG */
#ifdef S3_MASTER
	static const char *TAG = "Master";
#else
	static const char *TAG = "Slave";
#endif


static esp_err_t tas2563_write_reg (uint8_t reg, uint8_t value)
{
    uint8_t data[2] = { reg, value };
    return i2c_master_transmit(tas2563_handle, data, sizeof(data), 100);
}

static esp_err_t tas2563_read_reg (uint8_t reg, uint8_t *value)
{
    esp_err_t ret = i2c_master_transmit(tas2563_handle, &reg, 1, 100);
    if (ret != ESP_OK) return ret;
    return i2c_master_receive(tas2563_handle, value, 1, 100);
}


static inline void cbuf_write_u8_p2 (uint8_t *buf, size_t size, volatile size_t *wr,
                                     const uint8_t *src, size_t n)
{
    const size_t mask = size - 1;
    size_t p = (*wr) & mask;

    size_t first = size - p;
    if (first > n) first = n;

    memcpy(buf + p, src, first);
    if (n > first) memcpy(buf, src + first, n - first);

    *wr = (*wr + n) & mask;
}


static inline void cbuf_read_u8_p2 (const uint8_t *buf, size_t size, volatile size_t *tail,
                                    uint8_t *dst, size_t n)
{
    const size_t mask = size - 1;
    size_t p = (*tail) & mask;

    size_t first = size - p;
    if (first > n) first = n;

    memcpy(dst, buf + p, first);

    if (n > first)
    {
        memcpy(dst + first, buf, n - first);
    }

    *tail = (*tail + n) & mask;
}


static esp_err_t i2c_init (void)
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
        .device_address = TAS5825_I2C_ADDR,
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
        .max_transfer_sz = SPI_BUF_SIZE,
    };

    spi_slave_interface_config_t slvcfg = {

        .spics_io_num = PIN_CS,
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


/* I2S */
static void i2s_init (void)
{
    i2s_chan_config_t tx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&tx_chan_cfg, &tx_chan, NULL));

    i2s_std_config_t tx_std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(44100),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,   
            .bclk = I2S_BCLK,
            .ws   = I2S_LRCLK,
            .dout = I2S_DOUT,
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


#define ping false
#define pong true

#ifdef S3_MASTER

bool pingpong = ping;
uint8_t spiBuf [SPI_BUF_SIZE];

void spi_slave_task (void* arg)
{
    esp_err_t ret;
    spi_slave_transaction_t t;

    memset(&t, 0, sizeof(t));
    t.length = SPI_BUF_SIZE * 8;
    t.tx_buffer = NULL;
    t.rx_buffer = spiBuf;

    size_t recLength;
    bool order = ping;

    while (1)
    {
        if (calcFreeSpaceFIFO(basket.tail, basket.head, BASKET_SIZE) >= SPI_BUF_SIZE)
        {
            ret = spi_slave_transmit(SPI2_HOST, &t, portMAX_DELAY);
            if (ret != ESP_OK)
            {
                ESP_LOGW(TAG, "spi_slave_transmit err=%s", esp_err_to_name(ret));
                continue;
            }

            recLength = t.length / 8;

            if ((ret == ESP_OK) && recLength)
            {
                cbuf_write_u8_p2(basket.buf, BASKET_SIZE, &basket.head, t.rx_buffer, recLength);

                cbuf_write_u8_p2(basket_udp.buf, BASKET_SIZE, &basket_udp.head, t.rx_buffer, recLength);

                if (BASKET_SIZE - calcFreeSpaceFIFO(basket.tail, basket.head, BASKET_SIZE) >= RINGBUF_SIZE)
                {
                    if ((ringBuf1.state == RBS_READY_FOR_WRITE) && (order == ping))
                    {
                        ringBuf1.state = RBS_WRITING;

                        cbuf_read_u8_p2(basket.buf, BASKET_SIZE, &basket.tail, ringBuf1.ringBuf, RINGBUF_SIZE);

                        ringBuf1.state = RBS_READY_FOR_READ;

                        order = pong;
                    }

                    else if ((ringBuf2.state == RBS_READY_FOR_WRITE) && (order == pong))
                    {
                        ringBuf2.state = RBS_WRITING;

                        cbuf_read_u8_p2(basket.buf, BASKET_SIZE, &basket.tail, ringBuf2.ringBuf, RINGBUF_SIZE);

                        ringBuf2.state = RBS_READY_FOR_READ;

                        order = ping;
                    }
                }
            }
        }
        vPortYield();
    }
}
#endif


static void i2s_task (void *arg)
{
    ESP_LOGI(TAG, "i2s transmit start");

    size_t written;

    bool order = ping;

    while (!((ringBuf1.state == RBS_READY_FOR_READ) && (ringBuf2.state == RBS_READY_FOR_READ)))
        vTaskDelay(1);

    ESP_LOGI(TAG, "i2s Buffers ready");

    while (1)
    {
        if ((ringBuf1.state == RBS_READY_FOR_READ) && (order == ping))
        {
            ringBuf1.state = RBS_READING;

            esp_err_t ret =  i2s_channel_write(tx_chan, ringBuf1.ringBuf, RINGBUF_SIZE, &written, portMAX_DELAY);

			if (ret != ESP_OK)
            {
                ESP_LOGW(TAG, "i2s_channel_write(1) err=%s", esp_err_to_name(ret));
            }
//			ESP_LOGE(TAG, "B 1");
            ringBuf1.state = RBS_READY_FOR_WRITE;

            order = pong;
        }

        else if ((ringBuf2.state == RBS_READY_FOR_READ) && (order == pong))
        {
            ringBuf2.state = RBS_READING;

            esp_err_t ret = i2s_channel_write(tx_chan, ringBuf2.ringBuf, RINGBUF_SIZE, &written, portMAX_DELAY);

            if (ret != ESP_OK)
            {
                ESP_LOGW(TAG, "i2s_channel_write(2) err=%s", esp_err_to_name(ret));
            }
            
            ringBuf2.state = RBS_READY_FOR_WRITE;

            order = ping;
        }
        
        vTaskDelay(1);
    }
}


static void TAS5825_Init (void)
{
    ESP_LOGI(TAG, "Init start TAS5825");

    ESP_ERROR_CHECK(tas2563_write_reg(0, 0));
    ESP_ERROR_CHECK(tas2563_write_reg(0x01, 0x11)); 
    vTaskDelay(10);
    
    ESP_ERROR_CHECK(tas2563_write_reg(0, 0));
    ESP_ERROR_CHECK(tas2563_write_reg(0x03, 0x03)); 
 	
 	ESP_ERROR_CHECK(tas2563_write_reg(0, 0));
    ESP_ERROR_CHECK(tas2563_write_reg(0x4C, 0x70));  // Volume 
    
    ESP_ERROR_CHECK(tas2563_write_reg(0, 0));
    ESP_ERROR_CHECK(tas2563_write_reg(0x33, 0x20));  
	
	uint8_t val = 125;
	tas2563_read_reg(0x39, &val);
	ESP_LOGE(TAG, "0x39 %d", val);
	
	tas2563_read_reg(0x5E, &val);
	ESP_LOGE(TAG, "0x5E %d", val);
	vTaskDelay(10);
	
	tas2563_read_reg(0x68, &val);
	ESP_LOGE(TAG, "0x68 %d", val);
	vTaskDelay(10);
	
    ESP_LOGI(TAG, "Init end TAS5825");
}


static esp_err_t send_all (int sock, const uint8_t *data, size_t len)
{
    size_t total_sent = 0;

    while (total_sent < len)
    {
        int sent = send(sock, data + total_sent, len - total_sent, 0);
        if (sent < 0)
        {
            if (errno == EINTR)
                continue;

            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                vTaskDelay(1);
                continue;
            }

            ESP_LOGW(TAG, "send error: errno=%d", errno);
            return ESP_FAIL;
        }
        else if (sent == 0)
        {
            ESP_LOGW(TAG, "send returned 0, connection closed?");
            return ESP_FAIL;
        }

        total_sent += sent;
    }

    return ESP_OK;
}


void tcp_server_task (void *pvParameters)
{
    (void)pvParameters;

    while (1)
    {
        struct sockaddr_in listen_addr = {
            .sin_family = AF_INET,
            .sin_port   = htons(SERVER_PORT),
            .sin_addr.s_addr = htonl(INADDR_ANY),
        };

        int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (listen_sock < 0)
        {
            ESP_LOGE(TAG, "Unable to create server socket: errno=%d", errno);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        int opt = 1;
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        int sndbuf = 1024 * 256;
        setsockopt(listen_sock, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

        if (bind(listen_sock, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0)
        {
            ESP_LOGE(TAG, "Server socket bind failed: errno=%d", errno);
            close(listen_sock);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (listen(listen_sock, MAX_STA_CONN) < 0)
        {
            ESP_LOGE(TAG, "Error during listen: errno=%d", errno);
            close(listen_sock);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        ESP_LOGI(TAG, "TCP server listening on port %d", SERVER_PORT);

        while (1)
        {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int sock = accept(listen_sock, (struct sockaddr *)&client_addr, &addr_len);
            
            if (sock < 0)
            {
                ESP_LOGE(TAG, "accept failed: errno=%d", errno);
                break;
            }

            ESP_LOGI(TAG, "Client connected");

            while (1)
            {
                while ((BASKET_SIZE - calcFreeSpaceFIFO(basket_udp.tail, basket_udp.head, BASKET_SIZE)) < PACKET_SIZE)
                    vTaskDelay(1);

                cbuf_read_u8_p2(basket_udp.buf, BASKET_SIZE, &basket_udp.tail, PCM_Buffer, PACKET_SIZE);

                if (send_all(sock, PCM_Buffer, PACKET_SIZE) != ESP_OK)
                {
                    ESP_LOGW(TAG, "send_all failed, closing client socket");
                    break;
                }
            }

            shutdown(sock, 0);
            close(sock);
            ESP_LOGI(TAG, "Client disconnected");
        }

        shutdown(listen_sock, 0);
        close(listen_sock);
        ESP_LOGW(TAG, "TCP server restart in 1s");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


void tcp_client_task (void *pvParameters)
{
    (void)pvParameters;

    ESP_LOGI(TAG, "TCP client task started");

    bool order = ping;

    while (1)
    {
        struct sockaddr_in dest_addr = {
            .sin_family = AF_INET,
            .sin_port   = htons(SERVER_PORT),
            .sin_addr.s_addr = inet_addr(SERVER_IP),
        };

        int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (sock < 0)
        {
            ESP_LOGE(TAG, "Unable to create client socket: errno=%d", errno);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        ESP_LOGI(TAG, "Connecting to %s:%d", SERVER_IP, SERVER_PORT);

        if (connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0)
        {
            ESP_LOGE(TAG, "Socket connect failed: errno=%d", errno);
            shutdown(sock, 0);
            close(sock);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        ESP_LOGI(TAG, "Connected to server");

        int rcvbuf = 1024 * 256;
        setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

        struct timeval tv = {
            .tv_sec = 1,
            .tv_usec = 0,
        };
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        while (1)
        {
            size_t received = 0;

            while (received < PACKET_SIZE)
            {
                int len = recv(sock, PCM_Buffer + received, PACKET_SIZE - received, 0);
                if (len < 0)
                {
                    if (errno == EINTR)
                    {
                        continue;
                    }
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        vTaskDelay(1);
                        continue;
                    }
                    ESP_LOGW(TAG, "recv failed: errno=%d", errno);
                    goto disconnect;
                }
                else if (len == 0)
                {
                    ESP_LOGW(TAG, "Connection closed by peer");
                    goto disconnect;
                }
                else
                {
                    received += len;
                }
            }

            while (calcFreeSpaceFIFO(basket_udp.tail, basket_udp.head, BASKET_SIZE) < PACKET_SIZE)
                vTaskDelay(1);

            cbuf_write_u8_p2(basket_udp.buf, BASKET_SIZE, &basket_udp.head, PCM_Buffer, PACKET_SIZE);

            if (BASKET_SIZE - calcFreeSpaceFIFO(basket_udp.tail, basket_udp.head, BASKET_SIZE) >= RINGBUF_SIZE)
            {
                if ((ringBuf1.state == RBS_READY_FOR_WRITE) && (order == ping))
                {
                    ringBuf1.state = RBS_WRITING;

                    cbuf_read_u8_p2(basket_udp.buf, BASKET_SIZE, &basket_udp.tail, ringBuf1.ringBuf, RINGBUF_SIZE);

                    ringBuf1.state = RBS_READY_FOR_READ;

                    order = pong;
                }

                else if ((ringBuf2.state == RBS_READY_FOR_WRITE) && (order == pong))
                {
                    ringBuf2.state = RBS_WRITING;

                    cbuf_read_u8_p2(basket_udp.buf, BASKET_SIZE, &basket_udp.tail, ringBuf2.ringBuf, RINGBUF_SIZE);

                    ringBuf2.state = RBS_READY_FOR_READ;

                    order = ping;
                }
            }
        }

		disconnect:
        shutdown(sock, 0);
        close(sock);
        ESP_LOGI(TAG, "Reconnecting to server in 1s");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


static void wifi_ap_event_handler (void* arg, esp_event_base_t event_base,
                                   int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d", MAC2STR(event->mac), event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d, reason=%d", MAC2STR(event->mac), event->aid, event->reason);
    }
}


static int s_retry_num = 0;

static void wifi_sta_event_handler (void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    switch (event_id)
    {
        case WIFI_EVENT_STA_START :
            if (event_base == WIFI_EVENT)
                esp_wifi_connect();

            break;

        case WIFI_EVENT_STA_DISCONNECTED :
            if (event_base == WIFI_EVENT)
            {
                if (s_retry_num < MAXIMUM_RETRY)
                {
                    esp_wifi_connect();
                    s_retry_num++;
                    ESP_LOGW(TAG, "retry to connect to the AP (%d/%d)", s_retry_num, MAXIMUM_RETRY);
                }

                ESP_LOGW(TAG, "connect to the AP fail");
            }

            break;

        case IP_EVENT_STA_GOT_IP :
            if (event_base == IP_EVENT)
            {
                ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
                ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
                s_retry_num = 0;
                
				#ifndef S3_MASTER
					xTaskCreate(tcp_client_task, "tcp_client_task", 4096 * 2, NULL, 2, NULL);
				#endif
            }

            break;

        case WIFI_EVENT_AP_STACONNECTED :
        {
            wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
            ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                     MAC2STR(event->mac), event->aid);
            break;
        }

        case WIFI_EVENT_AP_STADISCONNECTED :
        {
            wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
            ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d, reason=%d",
                     MAC2STR(event->mac), event->aid, event->reason);
            break;
        }
    }
}


void wifi_init_ap (void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_ap_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .password = WIFI_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .channel = 6
        },
    };
    if (strlen(WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);

	esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW40);
    esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11N);

    esp_wifi_set_ps(WIFI_PS_NONE);

    esp_wifi_start();
    
    esp_wifi_config_80211_tx_rate(WIFI_IF_AP, WIFI_PHY_RATE_MCS6_SGI);

    ESP_LOGI(TAG, "AP started, SSID:%s password:%s", WIFI_SSID, WIFI_PASS);
}


void wifi_init_sta (void)
{
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_sta_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_sta_event_handler, NULL, NULL));

    wifi_config_t wifi_config = { 0 };
    snprintf((char *)wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid), "%s", WIFI_SSID);
    snprintf((char *)wifi_config.sta.password, sizeof(wifi_config.sta.password), "%s", WIFI_PASS);

    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW40);
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11N);

    esp_wifi_set_ps(WIFI_PS_NONE);

    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi Station is ready. Connecting to SSID:%s", WIFI_SSID);
}


void gpio_init (void)
{
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
	vTaskDelay(10);

    gpio_set_level(pins[1], 0);
    vTaskDelay(500);
    gpio_set_level(pins[1], 1);
    vTaskDelay(500);
}


#define SAMPLE_RATE     48000
#define TONE_FREQ_HZ    1000
#define AMPLITUDE       30000
#define BUF_LEN         256

static void test (void *pvParameters)
{
    ESP_LOGI("TEST", "Generating sine wave");

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


void app_main (void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    gpio_init();
    i2c_init();
	i2s_init();
    TAS5825_Init();

//	test(NULL);

	#ifdef S3_MASTER
	    wifi_init_ap();
	    spi_slave_init();
	#else
	    wifi_init_sta();
	#endif
	
    basket.tail = basket.head = basket_udp.tail = basket_udp.head = 0;
    memset(basket.buf, 0, BASKET_SIZE);
    memset(basket_udp.buf, 0, BASKET_SIZE);

    memset(ringBuf1.ringBuf, 0, RINGBUF_SIZE);
    memset(ringBuf2.ringBuf, 0, RINGBUF_SIZE);

    ringBuf1.state = ringBuf2.state = RBS_READY_FOR_WRITE;
    ringBuf1.send_id = ringBuf2.send_id = 0;
	
	#ifdef S3_MASTER
	    xTaskCreate(spi_slave_task, "spi_slave_task", 8192, NULL, 2, NULL);
	    xTaskCreate(tcp_server_task, "tcp_server_task", 8192, NULL, 2, NULL);
	#endif

    xTaskCreate(i2s_task, "i2s_task", 8192, NULL, 3, NULL);
    
}
