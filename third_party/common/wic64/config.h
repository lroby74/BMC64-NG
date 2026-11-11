#ifndef BMX_WIC64_CONFIG_H
#define BMX_WIC64_CONFIG_H
#include <stdlib.h>
#include <string.h>
#define BMX_WIC64_SERVER_SIZE 192
#define BMX_WIC64_REPORTING_DEFAULT {0, "bmx-wic64", -65}
#define BMX_WIC64_CONFIG_DEFAULT {0, "http://x.wic64.net/", 2, 30, 0, BMX_WIC64_REPORTING_DEFAULT, "", "0123456789ab", ""}
typedef struct bmx_wic64_reporting {
    int real;
    char ssid[33];
    int rssi;
} bmx_wic64_reporting;
static inline int bmx_wic64_reporting_valid(const bmx_wic64_reporting *r) {
    if (!r || (r->real != 0 && r->real != 1) || r->rssi < -127 || r->rssi > 0 ||
        !memchr(r->ssid, 0, sizeof r->ssid) || !r->ssid[0]) return 0;
    for (const unsigned char *p = (const unsigned char *)r->ssid; *p; ++p)
        if (*p < 32 || *p > 126) return 0;
    return 1;
}
typedef struct bmx_wic64_config {
    int enabled;
    char server[BMX_WIC64_SERVER_SIZE];
    int timezone;
    int timeout;
    int loglevel;
    bmx_wic64_reporting reporting;
    char mac[18], token[65], token_name[32];
} bmx_wic64_config;

static inline int bmx_wic64_server_valid(const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    size_t prefix;
    if (!s || strlen(s) >= BMX_WIC64_SERVER_SIZE) return 0;
    prefix = strncmp(s, "http://", 7) == 0 ? 7 :
             strncmp(s, "https://", 8) == 0 ? 8 : 0;
    if (!prefix || !s[prefix] || s[prefix] == '/') return 0;
    for (; *p; ++p) if (*p <= 32 || *p >= 127 || *p == '@' || *p == '#') return 0;
    return 1;
}
static inline int bmx_wic64_config_valid(const bmx_wic64_config *c) {
    return c && (c->enabled == 0 || c->enabled == 1) &&
           memchr(c->server, 0, sizeof c->server) &&
           bmx_wic64_server_valid(c->server) && c->timezone >= 0 &&
           c->timezone <= 31 && c->timeout >= 1 && c->timeout <= 255 &&
           c->loglevel >= 0 && c->loglevel <= 1 && bmx_wic64_reporting_valid(&c->reporting);
}


static inline int bmx_wic64_config_parse(bmx_wic64_config *c,
                                        const char *key, const char *value) {
    bmx_wic64_config next = *c;
    char *end;
    long number;
    if (strncmp(key, "wic64", 5) != 0) return 0;
    if (strcmp(key, "wic64_server") == 0) {
        if (!bmx_wic64_server_valid(value)) return -1;
        strcpy(next.server, value);
    } else if (strcmp(key, "wic64_ssid") == 0 || strcmp(key, "wic64_mac") == 0 ||
               strcmp(key, "wic64_token") == 0 || strcmp(key, "wic64_token_name") == 0) {
        char *out; size_t size;
        if (!strcmp(key, "wic64_ssid")) { out = next.reporting.ssid; size = sizeof next.reporting.ssid; }
        else if (!strcmp(key, "wic64_mac")) { out = next.mac; size = sizeof next.mac; }
        else if (!strcmp(key, "wic64_token")) { out = next.token; size = sizeof next.token; }
        else { out = next.token_name; size = sizeof next.token_name; }
        if (strlen(value) >= size) return -1;
        strcpy(out, value);

        if (out != next.reporting.ssid && !strcmp(out, "~")) out[0] = 0;
    } else {
        if (!value[0]) return -1;
        number = strtol(value, &end, 10);
        if (*end || number < -127 || number > 255) return -1;
        if (strcmp(key, "wic64") == 0) next.enabled = (int)number;
        else if (strcmp(key, "wic64_timezone") == 0) next.timezone = (int)number;
        else if (strcmp(key, "wic64_timeout") == 0) next.timeout = (int)number;
        else if (strcmp(key, "wic64_log") == 0) next.loglevel = (int)number;
        else if (!strcmp(key, "wic64_reporting")) next.reporting.real = (int)number;
        else if (!strcmp(key, "wic64_rssi")) next.reporting.rssi = (int)number;
        else return 0;
    }
    if (!bmx_wic64_config_valid(&next)) return -1;
    *c = next;
    return 1;
}
#endif
