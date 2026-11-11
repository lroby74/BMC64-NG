

static bmx_wic64_token bmx_token;
static int bmx_pending, bmx_reply_held;
static bmx_wic64_reply bmx_reply;
static unsigned bmx_header_length;

static void bmx_release_reply(void)
{
    if (bmx_reply_held) bmx_wic64_release(bmx_token);
    bmx_reply_held = 0;
}
static uint8_t bmx_reply_byte(uint32_t offset)
{
    if (bmx_reply_held && offset >= bmx_header_length) {
        uint32_t position = bmx_reply.offset + offset - bmx_header_length;
        return bmx_reply.pages[position / BMX_WIC64_REPLY_PAGE][position % BMX_WIC64_REPLY_PAGE];
    }
    return replybuffer[offset];
}
static void bmx_cancel_transport(void)
{
    bmx_release_reply();
    if (bmx_pending) bmx_wic64_cancel(bmx_token);
    bmx_pending = 0;
    if (http_get_alarm) alarm_unset(http_get_alarm);
}
static void bmx_transport_reply(int result)
{
    uint8_t code = CLIENT_ERROR;
    const char *message = "Transport not implemented";
    if (result == BMX_WIC64_UNAVAILABLE) {
        code = NETWORK_ERROR; message = "Network unavailable";
    } else if (result == BMX_WIC64_NO_MEMORY) {
        code = INTERNAL_ERROR; message = "Out of memory";
    } else if (result == BMX_WIC64_BUSY) {
        code = INTERNAL_ERROR; message = "Transport busy";
    }
    send_reply_revised(code, message, NULL, 0, "!E");
}
static void bmx_publish_reply(void)
{
    uint32_t length = bmx_reply.length;
    if (!length || (wic64_protocol == WIC64_PROT_LEGACY && bmx_reply.legacy[0])) {
        send_reply_revised(bmx_reply.code, bmx_reply.message, NULL, 0,
                          bmx_reply.legacy[0] ? bmx_reply.legacy : NULL);
        bmx_wic64_release(bmx_token);
        return;
    }
    wic64_set_status(bmx_reply.message);
    bmx_reply_held = 1;
    if (wic64_protocol == WIC64_PROT_LEGACY) {
        uint32_t declared = length + bmx_reply.legacy_length_adjust;
        bmx_header_length = big_load ? 4 : 2;
        if (big_load) {
            replybuffer[0] = length >> 24; replybuffer[1] = length >> 16;
            replybuffer[2] = length >> 8; replybuffer[3] = length;
        } else { replybuffer[0] = declared >> 8; replybuffer[1] = declared; }
        stage_dummy = stage_retcode = stage_length = stage_data = -1;
    } else {
        bmx_header_length = wic64_protocol == WIC64_PROT_EXTENDED ? 5 : 3;
        replybuffer[0] = bmx_reply.code; replybuffer[1] = length; replybuffer[2] = length >> 8;
        if (bmx_header_length == 5) { replybuffer[3] = length >> 16; replybuffer[4] = length >> 24; }
        cmd_timeout(1);
        stage_dummy = 1; bsr = stage_retcode = 1;
        bsl = stage_length = bmx_header_length - 1; bsd = stage_data = length;
    }
    replyptr = 0; reply_length = bmx_header_length + length;
    handshake_flag2();
}
static void bmx_transport_alarm(CLOCK offset, void *data)
{
    int result;
    alarm_unset(http_get_alarm);
    if (!bmx_pending || !userport_wic64_enabled) return;
    result = bmx_wic64_poll_reply(bmx_token, &bmx_reply);
    if (result == BMX_WIC64_PENDING) {
        alarm_set(http_get_alarm, maincpu_clk + 1000);
        return;
    }
    bmx_pending = 0;
    if (result == BMX_WIC64_REPLY) bmx_publish_reply();
    else {
        bmx_wic64_release(bmx_token);
        if (result != BMX_WIC64_STALE) bmx_transport_reply(result);
    }
}
static void bmx_submit_transport(void)
{
    int result;
    const char *server;
    bmx_wic64_reporting reporting = BMX_WIC64_REPORTING_DEFAULT;
    reporting.real = bmx_report_real; reporting.rssi = bmx_report_rssi;
    snprintf(reporting.ssid, sizeof reporting.ssid, "%s", bmx_report_ssid);
    bmx_cancel_transport();
    resources_get_string("WIC64DefaultServer", &server);
    result = bmx_wic64_submit_with_server(input_command, wic64_protocol, commandbuffer,
                                         commandptr, remote_to, server, &reporting, &bmx_token);
    if (result != BMX_WIC64_PENDING) { bmx_transport_reply(result); return; }
    bmx_pending = 1;
    if (!http_get_alarm)
        http_get_alarm = alarm_new(maincpu_alarm_context, "BMXWiC64", bmx_transport_alarm, NULL);
    alarm_set(http_get_alarm, maincpu_clk + 1000);
}
static void cmd_http_get_encoded(void) {}
static void cmd_http_get(void) { bmx_submit_transport(); }
static void cmd_http_post(int cmd) { bmx_submit_transport(); }
static void cmd_tcp_open(void) { bmx_submit_transport(); }
static void cmd_tcp_available(void) { bmx_submit_transport(); }
static void cmd_tcp_read(void) {

    big_load = 0;
    bmx_submit_transport();
}
static void cmd_tcp_write(void) { bmx_submit_transport(); }
static void cmd_tcp_close(void) { bmx_submit_transport(); }
