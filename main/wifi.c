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


static const char *TAG = "WiFi";


extern AirCube_t AirCube;


#ifdef qcfgPROJ_CUBE_MASTER

	static void WiFi_AP_EventHandler (void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
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
	
#endif


#ifdef qcfgPROJ_CUBE_SLAVE

	static void WiFi_StationEventHandler (void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
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
	                if (AirCube.retryNumber < qcfgWIFI_MAX_RETRY)
	                {
	                    esp_wifi_connect();
	                    AirCube.retryNumber++;
	                    
	                    ESP_LOGW(TAG, "retry to connect to the AP (%d/%d)", AirCube.retryNumber, qcfgWIFI_MAX_RETRY);
	                }
	
	                ESP_LOGW(TAG, "connect to the AP fail");
	            }
	
	            break;
	
	        case IP_EVENT_STA_GOT_IP :
	            if (event_base == IP_EVENT)
	            {
	                ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
	                ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
	                
	                AirCube.retryNumber = 0;
	                
					#ifdef qcfgPROJ_CUBE_SLAVE
					
	                    if (!AirCube.connected)
	                    {
	                        xTaskCreate(TaskTCP_AudioClient, "TCP Audio Client", 8192, NULL, 3, NULL);
	                        xTaskCreate(TaskTCP_ServiceClient,  "TCP Service Server",  8192, NULL, 4, NULL);
	                        AirCube.connected = true;
	                    }
	                    
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
	
#endif


#ifdef qcfgPROJ_CUBE_MASTER

	void WiFi_InitAP (void)
	{
	    ESP_ERROR_CHECK(esp_netif_init());
	    ESP_ERROR_CHECK(esp_event_loop_create_default());
	    esp_netif_create_default_wifi_ap();
	
	    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	    esp_wifi_init(&cfg);
	
	    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
	                                                          ESP_EVENT_ANY_ID,
	                                                     &WiFi_AP_EventHandler,
	                                                 NULL,
	                                                          NULL));
	
	    wifi_config_t wifi_config = {
	        .ap = {
	            .ssid = qcfgDP_WIFI_SSID,
	            .ssid_len = strlen(qcfgDP_WIFI_SSID),
	            .password = qcfgDP_WIFI_PASS,
	            .max_connection = qcfgWIFI_MAX_CONN_AP,
	            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
	            .channel = 6
	        },
	    };
	    
	    if (strlen(qcfgDP_WIFI_PASS) == 0) {
	        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
	    }
	
	    esp_wifi_set_mode(WIFI_MODE_AP);
	    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
	
		esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW40);
	    esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11N);
	
	    esp_wifi_set_ps(WIFI_PS_NONE);
	
	    esp_wifi_start();
	    
	    esp_wifi_config_80211_tx_rate(WIFI_IF_AP, WIFI_PHY_RATE_MCS6_SGI);
	
	    ESP_LOGI(TAG, "AP started, SSID:%s password:%s", qcfgDP_WIFI_SSID, qcfgDP_WIFI_PASS);
	}

#endif


#ifdef qcfgPROJ_CUBE_SLAVE

	void WiFi_InitStation (void)
	{
	    ESP_ERROR_CHECK(esp_netif_init());
	
	    ESP_ERROR_CHECK(esp_event_loop_create_default());
	    esp_netif_create_default_wifi_sta();
	
	    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	
	    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
															  ESP_EVENT_ANY_ID,
	    												 &WiFi_StationEventHandler,
													 NULL,
															  NULL));
															  
	    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
															  IP_EVENT_STA_GOT_IP,
														 &WiFi_StationEventHandler,
													 NULL,
															  NULL));
	
	    wifi_config_t wifi_config = { 0 };
	    snprintf((char *)wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid), "%s", qcfgDP_WIFI_SSID);
	    snprintf((char *)wifi_config.sta.password, sizeof(wifi_config.sta.password), "%s", qcfgDP_WIFI_PASS);
	
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
	
	    ESP_LOGI(TAG, "Wi-Fi Station is ready. Connecting to SSID:%s", qcfgDP_WIFI_SSID);
	}
	
#endif
