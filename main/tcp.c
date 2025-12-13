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


static const char *TAG = "TCP";


extern AirCube_t AirCube;


#ifdef qcfgPROJ_CUBE_MASTER

	static esp_err_t send_all (int sock, const ui8 *data, size_t len)
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
	
	
	static esp_err_t send_cmd (int sock, ui8 dev, ui8 cmd, const void *payload, ui16 payload_len)
	{
	    cubeAPI_CommandHeader_t h = {
	        .deviceID = dev,
	        .commandID = cmd,
	        .payloadSize = payload_len,
	    };
	
	    if (send_all(sock, (const ui8 *)&h, sizeof(h)) != ESP_OK)
	    	return ESP_FAIL;
	    	
	    if (payload_len && payload)
	        if (send_all(sock, (const ui8 *)payload, payload_len) != ESP_OK)
	        	return ESP_FAIL;

	    return ESP_OK;
	}
	
	
	void TaskTCP_AudioServer (void *params)
	{
	    (void)params;
	
	    while (1)
	    {
	        AirCube.TCP_Basket.tail = AirCube.TCP_Basket.head;
	
	        struct sockaddr_in listen_addr = {
	            .sin_family = AF_INET,
	            .sin_port   = htons(TCP_PORT_AUDIO),
	            .sin_addr.s_addr = htonl(INADDR_ANY),
	        };
	
	        int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	        
	        if (listen_sock < 0)
	        {
	            ESP_LOGE(TAG, "AUDIO: socket() failed: errno=%d", errno);
	            vTaskDelay(pdMS_TO_TICKS(1000));
	            continue;
	        }
	
	        int opt = 1;
	        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	
	        int sndbuf = 1024 * 128;
	        setsockopt(listen_sock, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
	
	        if (bind(listen_sock, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0)
	        {
	            ESP_LOGE(TAG, "AUDIO: bind() failed: errno=%d", errno);
	            close(listen_sock);
	            vTaskDelay(pdMS_TO_TICKS(1000));
	            continue;
	        }
	
	        if (listen(listen_sock, 1) < 0)
	        {
	            ESP_LOGE(TAG, "AUDIO: listen() failed: errno=%d", errno);
	            close(listen_sock);
	            vTaskDelay(pdMS_TO_TICKS(1000));
	            continue;
	        }
	
	        ESP_LOGI(TAG, "TCP AUDIO listening on %d", TCP_PORT_AUDIO);
	
	        struct sockaddr_in client_addr;
	        socklen_t addr_len = sizeof(client_addr);
	        int sock = accept(listen_sock, (struct sockaddr *)&client_addr, &addr_len);
	        close(listen_sock);
	
	        if (sock < 0)
	        {
	            ESP_LOGE(TAG, "AUDIO: accept() failed: errno=%d", errno);
	            vTaskDelay(pdMS_TO_TICKS(200));
	            continue;
	        }
	
	        ESP_LOGI(TAG, "AUDIO client connected");
	
	        while (1)
	        {
	            if ((BASKET_SIZE - calcFreeSpaceFIFO(AirCube.TCP_Basket.tail, AirCube.TCP_Basket.head, BASKET_SIZE)) >= TCP_PACKET_SIZE)
	            {
	                BasketBufferRead(AirCube.TCP_Basket.buff, BASKET_SIZE, &AirCube.TCP_Basket.tail, AirCube.PCM_Buffer, TCP_PACKET_SIZE);
	
	                for (int i = 0; i < 4; i++)
	                {
	                    if (send_cmd(sock, API_MASTER_ID, CUBE_CMD_SEND_MUSIC_CHUNK,
	                    AirCube.PCM_Buffer + (i * API_MUSIC_CHUNK_SIZE), API_MUSIC_CHUNK_SIZE) != ESP_OK)
	                    {
	                        ESP_LOGE(TAG, "AUDIO: send failed, closing socket");
	                        goto audio_disconnect;
	                    }
	                }
	            
	                vTaskDelay(1);
				}
				
	            else
	                vTaskDelay(1);
	        }
	
			audio_disconnect:
		        shutdown(sock, 0);
		        close(sock);
		        ESP_LOGW(TAG, "AUDIO client disconnected");
	    }
	}
	
	
	void TaskTCP_ServiceServer (void *pvParameters)
	{
	    (void)pvParameters;
	
	    while (1)
	    {
	        struct sockaddr_in listen_addr = {
	            .sin_family = AF_INET,
	            .sin_port   = htons(TCP_PORT_CTRL),
	            .sin_addr.s_addr = htonl(INADDR_ANY),
	        };
	
	        int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	        
	        if (listen_sock < 0)
	        {
	            ESP_LOGE(TAG, "CTRL: socket() failed: errno=%d", errno);
	            vTaskDelay(pdMS_TO_TICKS(1000));
	            continue;
	        }
	
	        int opt = 1;
	        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	
	        if (bind(listen_sock, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0)
	        {
	            ESP_LOGE(TAG, "CTRL: bind() failed: errno=%d", errno);
	            close(listen_sock);
	            vTaskDelay(pdMS_TO_TICKS(1000));
	            continue;
	        }
	
	        if (listen(listen_sock, 1) < 0)
	        {
	            ESP_LOGE(TAG, "CTRL: listen() failed: errno=%d", errno);
	            close(listen_sock);
	            vTaskDelay(pdMS_TO_TICKS(1000));
	            continue;
	        }
	
	        ESP_LOGI(TAG, "TCP CTRL listening on %d", TCP_PORT_CTRL);
	
	        struct sockaddr_in client_addr;
	        socklen_t addr_len = sizeof(client_addr);
	        int sock = accept(listen_sock, (struct sockaddr *)&client_addr, &addr_len);
	        close(listen_sock);
	
	        if (sock < 0)
	        {
	            ESP_LOGE(TAG, "CTRL: accept() failed: errno=%d", errno);
	            vTaskDelay(pdMS_TO_TICKS(200));
	            continue;
	        }
	
	        int one = 1;
	        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
	
	        ESP_LOGI(TAG, "CTRL client connected");
	
	        (void)send_cmd(sock, 1, CUBE_CMD_SET_VOLUME, &AirCube.volume, 1);
	
	        while (1)
	        {
	            cubeServiceChannelTransport_t cmd;
	            
	            if (xQueueReceive(AirCube.ServiceChTransQueue, &cmd, portMAX_DELAY) == pdTRUE)
	            {
	                if (send_cmd(sock, cmd.header.deviceID, cmd.header.commandID, cmd.payload, cmd.header.payloadSize) != ESP_OK)
	                {
	                    ESP_LOGE(TAG, "CTRL: send failed, closing socket");
	                    break;
	                }
	            }
	        }
	
	        shutdown(sock, 0);
	        close(sock);
	        ESP_LOGW(TAG, "CTRL client disconnected");
	    }
	}

#endif


#ifdef qcfgPROJ_CUBE_SLAVE

	static bool recv_exact (int sock, ui8 *dst, size_t len)
	{
	    size_t got = 0;
	    
	    while (got < len)
	    {
	        int r = recv(sock, dst + got, len - got, 0);
	        
	        if (r > 0)
	        {
	            got += (size_t)r;
	            continue;
	        }
	
	        if (r == 0)
	            return false;
	
	        if (errno == EINTR)
	        	continue;
	        	
	        if (errno == EAGAIN || errno == EWOULDBLOCK)
	        {
	            vTaskDelay(1);
	            continue;
	        }
	
	        return false;
	    }
	    
	    return true;
	}
	
	
	void TaskTCP_AudioClient (void *params)
	{
	    (void)params;
	
	    ESP_LOGI(TAG, "TCP Audio client task started");
	
	    bool order = ping_;
	    size_t received = 0;
	
	    while (1)
	    {
	        struct sockaddr_in dest_addr = {
	            .sin_family = AF_INET,
	            .sin_port   = htons(TCP_PORT_AUDIO),
	            .sin_addr.s_addr = inet_addr(qcfgSERVER_IP),
	        };
	
	        int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	        
	        if (sock < 0)
	        {
	            ESP_LOGE(TAG, "Audio: socket() failed: errno=%d", errno);
	            vTaskDelay(pdMS_TO_TICKS(1000));
	            continue;
	        }
	
	        int rcvbuf = 1024 * 128;
	        setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
	
	        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
	        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	
	        ESP_LOGI(TAG, "Audio: connecting to %s:%d", qcfgSERVER_IP, TCP_PORT_AUDIO);
	        if (connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0)
	        {
	            ESP_LOGW(TAG, "Audio: connect() failed: errno=%d", errno);
	            close(sock);
	            vTaskDelay(pdMS_TO_TICKS(500));
	            continue;
	        }
	
	        ESP_LOGI(TAG, "Audio: connected");
	
	        AirCube.TCP_Basket.tail = AirCube.TCP_Basket.head;
	        received = 0;
	
	        while (1)
	        {
	            cubeAPI_CommandHeader_t header;
	            
	            if (!recv_exact(sock, (ui8 *)&header, sizeof(header)))
	                break;
	
	            ui8 payload [API_MUSIC_CHUNK_SIZE];
	            
	            if (header.payloadSize)
	            {
	                if (!recv_exact(sock, payload, header.payloadSize))
	                    break;
	            }
	
	            if (header.commandID == CUBE_CMD_SEND_MUSIC_CHUNK)
	            {
	                if ((received + header.payloadSize) > TCP_PACKET_SIZE)
	                    received = 0;
	
	                memcpy(AirCube.PCM_Buffer + received, payload, header.payloadSize);
	                received += header.payloadSize;
	
	                if (received >= TCP_PACKET_SIZE)
	                {
	                    while (calcFreeSpaceFIFO(AirCube.TCP_Basket.tail, AirCube.TCP_Basket.head, BASKET_SIZE) < TCP_PACKET_SIZE)
	                        vTaskDelay(1);
	
	                    BasketBufferWrite(AirCube.TCP_Basket.buff, BASKET_SIZE, &AirCube.TCP_Basket.head, AirCube.PCM_Buffer, TCP_PACKET_SIZE);
	
	                    if (BASKET_SIZE - calcFreeSpaceFIFO(AirCube.TCP_Basket.tail, AirCube.TCP_Basket.head, BASKET_SIZE) >= RINGBUF_SIZE)
	                    {
	                        if ((AirCube.RingBuff1.state == RBS_READY_FOR_WRITE) && (order == ping_))
	                        {
	                            AirCube.RingBuff1.state = RBS_WRITING;
	                            BasketBufferRead(AirCube.TCP_Basket.buff, BASKET_SIZE, &AirCube.TCP_Basket.tail, AirCube.RingBuff1.buff, RINGBUF_SIZE);
	                            AirCube.RingBuff1.state = RBS_READY_FOR_READ;
	                            
	                            order = pong_;
	                        }
	                        else if ((AirCube.RingBuff2.state == RBS_READY_FOR_WRITE) && (order == pong_))
	                        {
	                            AirCube.RingBuff2.state = RBS_WRITING;
	                            BasketBufferRead(AirCube.TCP_Basket.buff, BASKET_SIZE, &AirCube.TCP_Basket.tail, AirCube.TCP_Basket.buff, RINGBUF_SIZE);
	                            AirCube.RingBuff2.state = RBS_READY_FOR_READ;
	                            
	                            order = ping_;
	                        }
	                    }
	
	                    received = 0;
	                }
	            }
	            
	            else
	                ESP_LOGW(TAG, "Audio: unexpected cmd=%u len=%u", (unsigned)header.commandID, (unsigned)header.payloadSize);
	        }
	
	        shutdown(sock, 0);
	        close(sock);
	        ESP_LOGW(TAG, "Audio: disconnected, retry in 300ms");
	        
	        vTaskDelay(300);
	    }
	}
	
	void TaskTCP_ServiceClient (void *params)
	{
	    (void)params;
	
	    ESP_LOGI(TAG, "TCP Ctrl client task started");
	
	    while (1)
	    {
	        struct sockaddr_in dest_addr = {
	            .sin_family = AF_INET,
	            .sin_port   = htons(TCP_PORT_CTRL),
	            .sin_addr.s_addr = inet_addr(qcfgSERVER_IP),
	        };
	
	        int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	        if (sock < 0)
	        {
	            ESP_LOGE(TAG, "Ctrl: socket() failed: errno=%d", errno);
	            vTaskDelay(pdMS_TO_TICKS(1000));
	            continue;
	        }
	
	        int one = 1;
	        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
	
	        struct timeval tv = { .tv_sec = 0, .tv_usec = 10000 };
	        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	
	        ESP_LOGI(TAG, "Ctrl: connecting to %s:%d", qcfgSERVER_IP, TCP_PORT_CTRL);
	        if (connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0)
	        {
	            ESP_LOGW(TAG, "Ctrl: connect() failed: errno=%d", errno);
	            close(sock);
	            vTaskDelay(pdMS_TO_TICKS(500));
	            continue;
	        }
	
	        ESP_LOGI(TAG, "Ctrl: connected");
	
	        while (1)
	        {
	            cubeAPI_CommandHeader_t header;
	            
	            if (!recv_exact(sock, (ui8 *)&header, sizeof(header)))
	                break;
	
	            if (header.payloadSize > API_MAX_SERVICE_SIZE)
	            {
	                ESP_LOGW(TAG, "Ctrl: payload too big (%u), drop & reconnect", (unsigned)header.payloadSize);
	                break;
	            }
	
	            ui8 payload [API_MAX_SERVICE_SIZE];
	            
	            if (header.payloadSize)
	            {
	                if (!recv_exact(sock, payload, header.payloadSize))
	                    break;
	            }
	
	            switch (header.commandID)
	            {
	                case CUBE_CMD_SET_VOLUME :
	                {
	                    if (header.payloadSize >= 1)
	                    {
							AirCube.volume = payload[0];
							TAS5825_SetVolume(AirCube.volume);
						}
	                    
	                    break;
	                }
	
	                case CUBE_CMD_PAUSE :
	                    break;
	
	                case CUBE_CMD_PLAY :
	                    break;
	
	                case CUBE_CMD_SYNC_TIME :
	                    break;
	
	                default:
	                    break;
	            }
	        }
	
	        shutdown(sock, 0);
	        close(sock);
	        ESP_LOGW(TAG, "Ctrl: disconnected, retry in 500ms");
	        
	        vTaskDelay(500);
	    }
	}
	
#endif
