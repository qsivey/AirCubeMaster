#ifndef LINK_H
#define LINK_H

#pragma once

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <inttypes.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_timer.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"

// ---- Wi-Fi AP ----
#define WIFI_SSID      "AirCube"
#define WIFI_PASS      "12345678"

// ---- Audio / Multicast ----
#define AUDIO_MCAST_GRP 	"239.10.10.10"       // Organization-local scope
#define AUDIO_PORT      	5004                 // RTP-like
#define AUDIO_PKT_MS    	10                   // packetization interval (ms)
#define SAMPLE_RATE     	48000
#define TONE_FREQ       	440
#define BYTES_PER_MS    	48                   // 48 kHz, 8-bit mono demo -> 48 bytes per ms
#define BUF_LEN         	(AUDIO_PKT_MS * BYTES_PER_MS)

// ---- Control (unicast) ----
#define CTRL_PORT       	6000
#define MAX_SLAVES      	4
#define SLAVE_TIMEOUT_MS 	5000

// Optional: AP default IP (for IP_MULTICAST_IF). In IDF AP defaults to 192.168.4.1
#define AP_IFKEY        	"WIFI_AP_DEF"

// ---- Helpers ----
static inline uint64_t htonll(uint64_t v){
    uint32_t hi = htonl((uint32_t)(v >> 32));
    uint32_t lo = htonl((uint32_t)(v & 0xFFFFFFFFu));
    return (((uint64_t)lo) << 32) | hi;
}
static inline uint64_t ntohll(uint64_t v){
    uint32_t hi = ntohl((uint32_t)(v >> 32));
    uint32_t lo = ntohl((uint32_t)(v & 0xFFFFFFFFu));
    return (((uint64_t)lo) << 32) | hi;
}

// ---- Packet formats ----
#define AUDIO_MAGIC 0xA0D10A00u
#define CTRL_MAGIC  0xC7F1C7A1u

// Audio payload is raw PCM 8-bit unsigned for demo. Replace with PCM16/opus/etc.
#pragma pack(push,1)
typedef struct {
    uint32_t magic;          // AUDIO_MAGIC
    uint32_t seq;            // increasing per packet
    uint64_t pts_ns;         // media timestamp (monotonic) for packet start
    uint32_t payload_len;    // bytes of audio[]
    uint8_t  flags;          // bit0: keyframe/marker reserved
    uint8_t  audio[];        // variable
} audio_pkt_t;
#pragma pack(pop)

// Minimal PTP-like control protocol (unicast)
// Master -> Slave: PTP_SYNC (t1), PTP_FOLLOW_UP (t1 precise)
// Slave  -> Master: PTP_DELAY_REQ (t3)
// Master -> Slave: PTP_DELAY_RESP (t4)
// Also: HELLO from slave, GET_BATT from master, BATT_REPLY/STATUS from slave

enum ctrl_type : uint16_t {
    CTRL_HELLO = 1,
    CTRL_PTP_SYNC = 2,
    CTRL_PTP_FOLLOW_UP = 3,
    CTRL_PTP_DELAY_REQ = 4,
    CTRL_PTP_DELAY_RESP = 5,
    CTRL_GET_BATT = 6,
    CTRL_BATT_REPLY = 7,
    CTRL_STATUS = 8,
};

#pragma pack(push,1)
typedef struct {
    uint32_t magic;      // CTRL_MAGIC
    uint16_t type;       // ctrl_type
    uint16_t ver;        // 1
    uint32_t seq;        // per-peer sequence
    uint64_t t1_ns;      // SYNC send time (master)
    uint64_t t2_ns;      // optional (unused here)
    uint64_t t3_ns;      // DELAY_REQ send time (slave)
    uint64_t t4_ns;      // DELAY_RESP send time (master)
    int8_t   battery_pct;// -1 if N/A
    uint8_t  reserved[7];// pad to 48 bytes header
} ctrl_msg_t;
#pragma pack(pop)

void wifi_init_ap(void);
void udp_server_task(void *pvParameters);

#endif
