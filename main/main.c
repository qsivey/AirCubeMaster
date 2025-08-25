#include "link.h"

static const char *TAG = "MASTER";

// ---- Slave table ----
typedef struct {
    struct sockaddr_in addr; // unicast address/port of the slave control socket
    uint32_t seq;            // control seq we send to this slave
    int battery_pct;         // last known
    int64_t last_seen_ms;
} slave_t;

static slave_t g_slaves[MAX_SLAVES];
static int g_ctrl_sock = -1;    // UDP server (listen) for control
static int g_mcast_sock = -1;   // UDP sender for audio multicast
static struct sockaddr_in g_mcast_addr;

// ---- Utility time ----
static inline int64_t now_us(void){ return esp_timer_get_time(); }
static inline int64_t now_ms(void){ return now_us() / 1000; }

// ---- Sine buffer demo ----
static uint8_t sine_buf[BUF_LEN];
static void fill_sine_wave(void){
    for (int i = 0; i < BUF_LEN; i++) {
        float s = sinf(2 * M_PI * TONE_FREQ * (float)i / (float)SAMPLE_RATE);
        sine_buf[i] = (uint8_t)((s + 1.f) * 127.5f);
    }
}

// ---- Networking ----
static int create_mcast_sender(void){
    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (s < 0) { ESP_LOGE(TAG, "mcast socket(): %d", errno); return -1; }

    // Set outgoing interface to AP IP for multicast
    esp_netif_ip_info_t ip; memset(&ip,0,sizeof(ip));
    esp_netif_t* ap = esp_netif_get_handle_from_ifkey(AP_IFKEY);
    if (ap && esp_netif_get_ip_info(ap, &ip) == ESP_OK) {
        struct in_addr ifaddr = { .s_addr = ip.ip.addr };
        if (setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF, &ifaddr, sizeof(ifaddr)) < 0)
            ESP_LOGW(TAG, "IP_MULTICAST_IF failed: %d", errno);
    } else {
        ESP_LOGW(TAG, "Failed to get AP IP, using default iface");
    }

    uint8_t ttl = 1; // stay within AP
    setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));

    uint8_t loop = 0; // don't receive our own multicasts
    setsockopt(s, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));

    // Pre-build mcast addr
    memset(&g_mcast_addr, 0, sizeof(g_mcast_addr));
    g_mcast_addr.sin_family = AF_INET;
    g_mcast_addr.sin_port = htons(AUDIO_PORT);
    inet_aton(AUDIO_MCAST_GRP, &g_mcast_addr.sin_addr);

    return s;
}

static int create_ctrl_server(void){
    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (s < 0) { ESP_LOGE(TAG, "ctrl socket(): %d", errno); return -1; }

    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in a; memset(&a,0,sizeof(a));
    a.sin_family = AF_INET; a.sin_port = htons(CTRL_PORT); a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (struct sockaddr*)&a, sizeof(a)) < 0) {
        ESP_LOGE(TAG, "ctrl bind(): %d", errno); close(s); return -1; }

    // non-blocking
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
    return s;
}

// ---- Slave table helpers ----
static int find_or_add_slave(struct sockaddr_in *from){
    // match by IP/port
    for (int i=0;i<MAX_SLAVES;i++){
        if (g_slaves[i].addr.sin_family==AF_INET &&
            g_slaves[i].addr.sin_addr.s_addr == from->sin_addr.s_addr){
            // allow port to vary; update latest port seen
            g_slaves[i].addr.sin_port = from->sin_port;
            g_slaves[i].last_seen_ms = now_ms();
            return i;
        }
    }
    for (int i=0;i<MAX_SLAVES;i++){
        if (g_slaves[i].addr.sin_family==0){
            g_slaves[i].addr = *from;
            g_slaves[i].seq = 1;
            g_slaves[i].battery_pct = -1;
            g_slaves[i].last_seen_ms = now_ms();
            ESP_LOGI(TAG, "New slave %s:%u (slot %d)", inet_ntoa(from->sin_addr), ntohs(from->sin_port), i);
            return i;
        }
    }
    return -1; // full
}

static void prune_slaves(void){
    int64_t t = now_ms();
    for (int i=0;i<MAX_SLAVES;i++){
        if (g_slaves[i].addr.sin_family){
            if (t - g_slaves[i].last_seen_ms > SLAVE_TIMEOUT_MS){
                ESP_LOGW(TAG, "Slave %s timed out", inet_ntoa(g_slaves[i].addr.sin_addr));
                memset(&g_slaves[i], 0, sizeof(g_slaves[i]));
            }
        }
    }
}

// ---- Control send ----
static void ctrl_send(int idx, enum ctrl_type type, uint64_t t1, uint64_t t3, uint64_t t4, int8_t batt){
    if (idx<0 || idx>=MAX_SLAVES || g_slaves[idx].addr.sin_family==0) return;
    ctrl_msg_t m = {0};
    m.magic = htonl(CTRL_MAGIC);
    m.type  = htons(type);
    m.ver   = htons(1);
    m.seq   = htonl(g_slaves[idx].seq++);
    m.t1_ns = htonll(t1);
    m.t3_ns = htonll(t3);
    m.t4_ns = htonll(t4);
    m.battery_pct = batt;

    sendto(g_ctrl_sock, &m, sizeof(m), 0,
           (struct sockaddr*)&g_slaves[idx].addr, sizeof(g_slaves[idx].addr));
}

static void ctrl_broadcast(enum ctrl_type type){
    for (int i=0;i<MAX_SLAVES;i++) if (g_slaves[i].addr.sin_family){
        ctrl_send(i, type, 0, 0, 0, -1);
    }
}

// ---- Control RX loop ----
static void ctrl_task(void *arg){
    g_ctrl_sock = create_ctrl_server();
    if (g_ctrl_sock < 0){ vTaskDelete(NULL); return; }

    uint8_t rxbuf[256];

    while (1){
        prune_slaves();

        // poll socket (non-blocking)
        struct sockaddr_in from; socklen_t flen = sizeof(from);
        int n = recvfrom(g_ctrl_sock, rxbuf, sizeof(rxbuf), 0, (struct sockaddr*)&from, &flen);
        if (n >= (int)sizeof(ctrl_msg_t)){
            ctrl_msg_t *m = (ctrl_msg_t*)rxbuf;
            if (ntohl(m->magic) == CTRL_MAGIC){
                int idx = find_or_add_slave(&from);
                enum ctrl_type t = (enum ctrl_type)ntohs(m->type);
                uint64_t t1 = ntohll(m->t1_ns);
                uint64_t t3 = ntohll(m->t3_ns);
                switch (t){
                    case CTRL_HELLO:
                        ESP_LOGI(TAG, "HELLO from %s", inet_ntoa(from.sin_addr));
                        // respond with SYNC immediately
                        ctrl_send(idx, CTRL_PTP_SYNC, (uint64_t)now_us()*1000ULL, 0, 0, -1);
                        ctrl_send(idx, CTRL_PTP_FOLLOW_UP, (uint64_t)now_us()*1000ULL, 0, 0, -1);
                        // also ask battery
                        ctrl_send(idx, CTRL_GET_BATT, 0, 0, 0, -1);
                        break;
                    case CTRL_PTP_DELAY_REQ: {
                        // Slave asks for t4; respond ASAP with current time as t4
                        uint64_t t4 = (uint64_t)now_us()*1000ULL; // ns
                        ctrl_send(idx, CTRL_PTP_DELAY_RESP, t1, 0, t4, -1);
                        break; }
                    case CTRL_BATT_REPLY:
                    case CTRL_STATUS:
                        g_slaves[idx].battery_pct = m->battery_pct;
                        g_slaves[idx].last_seen_ms = now_ms();
                        ESP_LOGI(TAG, "Battery from %s: %d%%", inet_ntoa(from.sin_addr), g_slaves[idx].battery_pct);
                        break;
                    default:
                        break;
                }
            }
        }

        // periodic SYNC + battery poll to all known slaves
        static int64_t next_sync_ms = 0, next_batt_ms = 0;
        int64_t t = now_ms();
        if (t >= next_sync_ms){
            for (int i=0;i<MAX_SLAVES;i++) if (g_slaves[i].addr.sin_family){
                uint64_t t1 = (uint64_t)now_us()*1000ULL;
                ctrl_send(i, CTRL_PTP_SYNC, t1, 0, 0, -1);
                ctrl_send(i, CTRL_PTP_FOLLOW_UP, t1, 0, 0, -1);
            }
            next_sync_ms = t + 1000; // 1 Hz
        }
        if (t >= next_batt_ms){
            for (int i=0;i<MAX_SLAVES;i++) if (g_slaves[i].addr.sin_family){
                ctrl_send(i, CTRL_GET_BATT, 0, 0, 0, -1);
            }
            next_batt_ms = t + 5000; // 5 s
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ---- Multicast audio TX ----
static void audio_task(void *arg){
    g_mcast_sock = create_mcast_sender();
    if (g_mcast_sock < 0){ vTaskDelete(NULL); return; }

    fill_sine_wave();

    uint32_t seq = 0;
    const int pkt_payload = BUF_LEN;
    const size_t hdr = sizeof(audio_pkt_t);
    const size_t total = hdr + pkt_payload;
    uint8_t *buf = (uint8_t*)malloc(total);
    if (!buf){ ESP_LOGE(TAG, "malloc failed"); vTaskDelete(NULL); return; }

    while (1){
        audio_pkt_t *p = (audio_pkt_t*)buf;
        p->magic = htonl(AUDIO_MAGIC);
        p->seq   = htonl(seq++);
        // media PTS in ns — for demo, just wallclock monotonic; align to packet boundary
        p->pts_ns = htonll((uint64_t)now_us()*1000ULL);
        p->payload_len = htonl(pkt_payload);
        p->flags = 0;
        memcpy(p->audio, sine_buf, pkt_payload);

        int sent = sendto(g_mcast_sock, buf, total, 0, (struct sockaddr*)&g_mcast_addr, sizeof(g_mcast_addr));
        if (sent < 0){ ESP_LOGW(TAG, "mcast sendto: %d", errno); }

        vTaskDelay(pdMS_TO_TICKS(AUDIO_PKT_MS));
    }
}

void app_main(void){
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init_ap();

    memset(g_slaves, 0, sizeof(g_slaves));

    xTaskCreate(ctrl_task,   "ctrl_task",   4096, NULL, 6, NULL);
    xTaskCreate(audio_task,  "audio_task",  4096, NULL, 5, NULL);
}

