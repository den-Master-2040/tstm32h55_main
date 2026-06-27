/*
 * web.c
 *
 *  Created on: Jun 18, 2026
 *      Author: User
 */


#include "mongoose.h"
#include "ipc.h"
#include "web_content.h"     /* scope_html[], scope_html_len */
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

static struct mg_mgr mgr;
/* в web.c, рядом с pump_frames */
#include <string.h>
#define ADC_FRAME_SAMPLES  512
static uint8_t frame_wire[32 + 2 * ADC_FRAME_SAMPLES];
static uint32_t frame_seq = 0;

extern volatile int frame_ready;
extern uint16_t adc_buf[];

static void build_and_send_adc(void) {
    if (!frame_ready) return;
    frame_ready = 0;


    /* --- метрики одним проходом (дёшево) --- */
    uint16_t mn = 4095, mx = 0;
    uint32_t sum = 0;
    for (int i = 0; i < ADC_FRAME_SAMPLES; i++) {
        uint16_t s = adc_buf[i];
        if (s < mn) mn = s;
        if (s > mx) mx = s;
        sum += s;
    }
    uint16_t dc = sum / ADC_FRAME_SAMPLES;

    /* --- хедер (32 байта, little-endian) --- */
    uint8_t *p = frame_wire;
    *(uint32_t*)(p+0)  = ++frame_seq;          /* seq */
    *(uint16_t*)(p+4)  = ADC_FRAME_SAMPLES;    /* n_samples */
    *(uint16_t*)(p+6)  = 0x2;                  /* flags: signal, no trig */
    *(uint32_t*)(p+8)  = 100000;               /* sample_rate_hz */
    *(int32_t*) (p+12) = 0;                    /* trigger_offset */
    *(uint16_t*)(p+16) = dc;                   /* dc_level */
    *(uint16_t*)(p+18) = mn;                   /* v_min */
    *(uint16_t*)(p+20) = mx;                   /* v_max */
    *(uint16_t*)(p+22) = mx - mn;              /* v_pp */
    *(float*)   (p+24) = 0.0f;                 /* rms (пока 0) */
    *(uint32_t*)(p+28) = 0;                    /* crossings (пока 0) */

    /* --- сэмплы --- */
    memcpy(p + 32, adc_buf, 2 * ADC_FRAME_SAMPLES);

    /* --- отправка во все WS-соединения --- */
    for (struct mg_connection *c = mgr.conns; c; c = c->next) {
        if (!c->is_websocket) continue;
        if (c->send.len > 16 * 1024) continue;   /* backpressure: дроп */
        mg_ws_send(c, frame_wire, sizeof(frame_wire), WEBSOCKET_OP_BINARY);
    }
}
/* --- отправка команды на CM4 через мейлбокс (CM7-сторона) --- */
static void ipc_send_cmd(uint32_t id, uint32_t p0, uint32_t p1) {
    g_ipc.mbox.cmd_id = id;
    g_ipc.mbox.p0 = p0;
    g_ipc.mbox.p1 = p1;
    __DMB();
    g_ipc.mbox.cmd_seq++;
    __DSB();
    HAL_HSEM_FastTake(IPC_HSEM_CMD);
    HAL_HSEM_Release(IPC_HSEM_CMD, 0);
}

/* --- разбор JSON-команды от кнопок фронта --- */
static void handle_command(struct mg_str j) {
    char *cmd = mg_json_get_str(j, "$.cmd");
    if (!cmd) return;
    if      (!strcmp(cmd, "stream_on"))   ipc_send_cmd(IPC_CMD_STREAM_ON, 0, 0);
    else if (!strcmp(cmd, "stream_off"))  ipc_send_cmd(IPC_CMD_STREAM_OFF, 0, 0);
    else if (!strcmp(cmd, "set_points"))  ipc_send_cmd(IPC_CMD_SET_POINTS,   mg_json_get_long(j, "$.n",  512), 0);
    else if (!strcmp(cmd, "set_rate"))    ipc_send_cmd(IPC_CMD_SET_ADC_RATE, mg_json_get_long(j, "$.hz", 100000), 0);
    else if (!strcmp(cmd, "set_dac"))     ipc_send_cmd(IPC_CMD_SET_DAC_FREQ, mg_json_get_long(j, "$.hz", 1000), 0);
    else if (!strcmp(cmd, "set_pwm"))     ipc_send_cmd(IPC_CMD_SET_PWM_DUTY,
                                              mg_json_get_long(j, "$.hz", 20000),
                                              mg_json_get_long(j, "$.duty", 50));
    else if (!strcmp(cmd, "reset_state")) ipc_send_cmd(IPC_CMD_RESET_STATE, 0, 0);
    /* set_trig / reboot аналогично */
    mg_free(cmd);
}

/* --- HTTP + WebSocket события --- */
static void ev_handler(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;
        if (mg_match(hm->uri, mg_str("/scope"), NULL)) {

            mg_ws_upgrade(c, hm, NULL);          /* апгрейд в WebSocket */
        } else {

            //mg_http_reply(c, 200, "Content-Type: text/html\r\n", "%.*s", (int)scope_html_len, scope_html);
            mg_printf(c, "HTTP/1.1 200 OK\r\n"
                         "Content-Type: text/html\r\n"
                         "Content-Length: %u\r\n"
                         "\r\n", (unsigned) scope_html_len);
            mg_send(c, scope_html, scope_html_len);   /* сырые байты, без форматирования */
        }
    } else if (ev == MG_EV_WS_MSG) {
        struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;
        handle_command(wm->data);                /* JSON-команда */
    }
}

/* --- слить кадры из IPC-кольца во все WS-соединения --- */
static void pump_frames(void) {

    volatile ipc_frame_t *f;
    while ((f = ipc_ring_peek(&g_ipc.ring)) != NULL) {
        size_t wire = 32 + 2 * f->n_samples;     /* хедер + n точек */
        for (struct mg_connection *c = mgr.conns; c; c = c->next) {
            if (!c->is_websocket) continue;
            if (c->send.len > 16 * 1024) continue;   /* backpressure: дроп */
            mg_ws_send(c, (const void *) f, wire, WEBSOCKET_OP_BINARY);
        }

        ipc_ring_release(&g_ipc.ring);
    }
}

/* --- задача сервера --- */
static void web_task(void *arg) {
    (void) arg;

    mg_mgr_init(&mgr);
    struct mg_connection *l = mg_http_listen(&mgr, "http://0.0.0.0:80", ev_handler, NULL);
    if(l == NULL)__BKPT(0);
    for (;;) {
        mg_mgr_poll(&mgr, 5);    /* 5 мс → плавный поток, низкая латентность */

        //pump_frames();
        build_and_send_adc();   /* было pump_frames */

    }
}

void web_server_start(void) {


	BaseType_t ok = xTaskCreate(web_task, "web", 4096, NULL, tskIDLE_PRIORITY + 3, NULL);
	if (ok != pdPASS) {
		__BKPT(0);
	}

}
