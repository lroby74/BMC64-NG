#ifndef BMX_WIC64_TRANSPORT_H
#define BMX_WIC64_TRANSPORT_H
#include <stddef.h>
#include <stdint.h>
#include "wic64/config.h"
#ifdef __cplusplus
extern "C" {
#endif
#define BMX_WIC64_MAX_REQUEST (1024U * 1024U)
typedef struct {
    uint64_t device, connection, request;
} bmx_wic64_token;
enum bmx_wic64_result {
    BMX_WIC64_PENDING, BMX_WIC64_UNSUPPORTED, BMX_WIC64_UNAVAILABLE,
    BMX_WIC64_STALE, BMX_WIC64_BUSY, BMX_WIC64_NO_MEMORY,
    BMX_WIC64_INVALID, BMX_WIC64_REPLY
};
#define BMX_WIC64_REPLY_PAGE 65536U
typedef struct {
    const uint8_t *const *pages;
    uint32_t offset, length;
    uint8_t code;
    int8_t legacy_length_adjust;
    char message[96], legacy[4];
} bmx_wic64_reply;
typedef struct {
    uint64_t resets, step_max_us;
    uint32_t request_bytes, reply_bytes;
    uint8_t tcp_open, http_open;
} bmx_wic64_stats;
void bmx_wic64_read_stats(bmx_wic64_stats *stats);



int bmx_wic64_submit(uint8_t command, uint8_t protocol, const void *payload,
                     size_t length, unsigned timeout, bmx_wic64_token *token);
int bmx_wic64_poll(bmx_wic64_token token);
int bmx_wic64_submit_with_server(uint8_t command, uint8_t protocol,
    const void *payload, size_t length, unsigned timeout, const char *server, const bmx_wic64_reporting *reporting,
    bmx_wic64_token *token);
int bmx_wic64_poll_reply(bmx_wic64_token token, bmx_wic64_reply *reply);
void bmx_wic64_cancel(bmx_wic64_token token);
void bmx_wic64_release(bmx_wic64_token token);
void bmx_wic64_reset(int enabled);
#ifdef __cplusplus
}
#endif
#endif
