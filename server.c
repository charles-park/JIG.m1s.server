//------------------------------------------------------------------------------
/**
 * @file server.c
 * @author charles-park (charles.park@hardkernel.com)
 * @brief ODROID-M1S JIG Client App.
 * @version 0.2
 * @date 2023-10-23
 *
 * @package apt install iperf3, nmap, ethtool, usbutils, alsa-utils
 *
 * @copyright Copyright (c) 2022
 *
 */
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <linux/fb.h>
#include <getopt.h>

//------------------------------------------------------------------------------
#include "server.h"

//------------------------------------------------------------------------------
//
// JIG Protocol(V2.0)
// https://docs.google.com/spreadsheets/d/1Of7im-2I5m_M-YKswsubrzQAXEGy-japYeH8h_754WA/edit#gid=0
//
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// 문자열 변경 함수. 입력 포인터는 반드시 메모리가 할당되어진 변수여야 함.
//------------------------------------------------------------------------------
static void tolowerstr (char *p)
{
    int i, c = strlen(p);

    for (i = 0; i < c; i++, p++)
        *p = tolower(*p);
}

//------------------------------------------------------------------------------
static void toupperstr (char *p)
{
    int i, c = strlen(p);

    for (i = 0; i < c; i++, p++)
        *p = toupper(*p);
}

//------------------------------------------------------------------------------
static int find_uart_port (const char *path)
{
    FILE *fp;
    char rdata[256], *ptr;

    memset  (rdata, 0x00, sizeof(rdata));
    sprintf (rdata, "find %s -name ttyUSB* 2<&1", path);

    // UART (ttyUSB) find...
    if ((fp = popen(rdata, "r")) != NULL) {
        memset (rdata, 0x00, sizeof(rdata));
        while (fgets (rdata, sizeof(rdata), fp) != NULL) {

            if ((ptr = strstr (rdata, "ttyUSB")) != NULL) {
                char c_port = *(ptr +6);
                pclose(fp);
                return (c_port - '0');
            }
            memset (rdata, 0x00, sizeof(rdata));
        }
        pclose(fp);
    }
    return -1;
}

//------------------------------------------------------------------------------
static int run_interval_check (struct timeval *t, double interval_ms)
{
    struct timeval base_time;
    double difftime;

    gettimeofday(&base_time, NULL);

    if (interval_ms) {
        /* 현재 시간이 interval시간보다 크면 양수가 나옴 */
        difftime = (base_time.tv_sec - t->tv_sec) +
                    ((base_time.tv_usec - (t->tv_usec + interval_ms * 1000)) / 1000000);

        if (difftime > 0) {
            t->tv_sec  = base_time.tv_sec;
            t->tv_usec = base_time.tv_usec;
            return 1;
        }
        return 0;
    }
    /* 현재 시간 저장 */
    t->tv_sec  = base_time.tv_sec;
    t->tv_usec = base_time.tv_usec;
    return 1;
}

//------------------------------------------------------------------------------
static int power_status (adc_pin_t *pinfo, int ch, int mv)
{
    printf ("power status = %s, [ch %d] %s : %d mV, max = %d, min = %d\n",
            (mv < pinfo->min) ? "OFF" : "ON",
            ch,
            pinfo->port,
            mv,
            pinfo->max,
            pinfo->min);

    if ((mv < pinfo->min) || (mv > pinfo->max))
        return 0;

    return 1;
}

//------------------------------------------------------------------------------
static void adc_port_check (server_t *p, int ch)
{
    int mv = 0, pin = 0;

    if (adc_board_read (p->ch[ch].i2c_fd, "P1_6.8", &mv, &pin))
        printf ("POWER_LED P1_6.8 = %d mv\n", mv);
    if (adc_board_read (p->ch[ch].i2c_fd, "P1_6.7", &mv, &pin))
        printf ("ACTIVE_LED P1_6.7 = %d mv\n", mv);
    if (adc_board_read (p->ch[ch].i2c_fd, "P1_6.6", &mv, &pin))
        printf ("ETH_G_LED P1_6.6 = %d mv\n", mv);
    if (adc_board_read (p->ch[ch].i2c_fd, "P1_6.5", &mv, &pin))
        printf ("ETH_Y_LED P1_6.5 = %d mv\n", mv);
    if (adc_board_read (p->ch[ch].i2c_fd, "P1_6.4", &mv, &pin))
        printf ("NVME_LED P1_6.4 = %d mv\n", mv);
    if (adc_board_read (p->ch[ch].i2c_fd, "P1_5.3", &mv, &pin))
        printf ("AUDIO_R P1_5.3 = %d mv\n", mv);
    if (adc_board_read (p->ch[ch].i2c_fd, "P1_5.2", &mv, &pin))
        printf ("AUDIO_L P1_5.2 = %d mv\n", mv);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#define HEADER_5V_PIN   1

const int UI_ID_POWER[eCH_END] = {35, 39};

//------------------------------------------------------------------------------
static int client_power_check (server_t *p, int ch)
{
    int mv = 0, pin = 0, i;
    channel_t *p_ch = &p->ch[ch];

    /* 5V display */
    if (adc_board_read (p_ch->i2c_fd, p_ch->power_info[HEADER_5V_PIN].port, &mv, &pin)) {
        char mv_str [STR_NAME_BYTES];
        memset  (mv_str, 0, sizeof(mv_str));
        if ((mv < p_ch->power_info[HEADER_5V_PIN].min))
            ui_set_ritem (p->pfb, p->pui, UI_ID_POWER[ch], COLOR_RED, -1);

        sprintf (mv_str, "%d", mv);
        ui_set_sitem (p->pfb, p->pui, UI_ID_POWER[ch], -1, -1, mv_str);
    }

    for (i = 0; i < p->ch[ch].power_count; i++) {
        if (!adc_board_read (p_ch->i2c_fd, p_ch->power_info[i].port, &mv, &pin))
            return 0;

        if (!power_status (&p_ch->power_info[i], ch, mv))
            return 0;
    }
    // led, audio, check (debug msg)
    adc_port_check (p, ch);

    return 1;
}

//------------------------------------------------------------------------------
static int error_check (server_t *p, int ch)
{
    int i, pos = 0;
    char err_msg [STR_PATH_BYTES];

    memset (err_msg, 0, sizeof(err_msg));

    if (p->nlp.conn) {
        for (i = 0; i < p->ch[ch].cmd_count; i++) {
            if (p->ch[ch].cmds [i].result == 0)
                pos += sprintf (&err_msg [pos], "%s,", p->ch[ch].cmds [i].dev_name);
        }
        if (pos)
            nlp_printf (&p->nlp, MSG_TYPE_ERR, err_msg, ch);
    }
    return pos ? 1 : 0;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#define STATE_CHECK_INTERVAL   500

const int UI_ID_STATUS[eCH_END] = {182, 186};
// STATE_CHECK_INTERVAL이 500ms 이므로 40 sec wait
#define UART_WATCHDOG_TIME  (40 * 2)

//------------------------------------------------------------------------------
static void server_state_control (server_t *p)
{
    static struct timeval i_time;
    channel_t   *p_ch;
    int ch;
    static int onoff = 0;

    if (run_interval_check(&i_time, STATE_CHECK_INTERVAL)) {
        int state;
        onoff = onoff ? 0 : 1;
        for (ch = 0; ch < eCH_END; ch++) {
            p_ch = &p->ch[ch];
            state = p_ch->state;

            if (client_power_check (p, ch)) {

                if ((state == eSTATE_STOP) || (state == eSTATE_ERR_POWER)) {
                    state = eSTATE_WAIT;
                    p_ch->connect  = 0;
                    p_ch->cmd_pos  = 0;
                    p_ch->cmd_wait = 0;
                    p_ch->watchdog = 0;
                    ui_update_group (p->pfb, p->pui, ch);
                }
                if ((state == eSTATE_WAIT) && p_ch->connect) {
                    state = eSTATE_RUNNING;
                    ui_set_ritem (p->pfb, p->pui, UI_ID_STATUS[ch], COLOR_YELLOW, -1);
                }
                if ((state == eSTATE_RUNNING) && (p_ch->cmd_pos == p_ch->cmd_count)) {
                    state = eSTATE_FINISH;
                    // error check & print
                    ui_set_ritem (p->pfb, p->pui, UI_ID_STATUS[ch],
                                error_check(p, ch) ? COLOR_RED : COLOR_GREEN, -1);
                }
            } else {
                if ((state == eSTATE_RUNNING)) {
                    if (p_ch->cmd_pos == p_ch->cmd_count) {
                        state = eSTATE_FINISH;
                        // error check & print
                        ui_set_ritem (p->pfb, p->pui, UI_ID_STATUS[ch],
                                    error_check(p, ch) ? COLOR_RED : COLOR_GREEN, -1);
                    } else {
                        state  = eSTATE_ERR_POWER;
                        ui_set_ritem (p->pfb, p->pui, UI_ID_STATUS[ch],
                                    (p_ch->cmd_pos == p_ch->cmd_count) ? COLOR_GREEN : COLOR_RED, -1);
                    }
                } else {
                    state  = eSTATE_STOP;
                    ui_set_ritem (p->pfb, p->pui, UI_ID_STATUS[ch], COLOR_GRAY, -1);
                }
            }
            if ((state == eSTATE_WAIT) && (p_ch->watchdog ++ > UART_WATCHDOG_TIME)) {
                state = eSTATE_ERR_UART;
                nlp_printf (&p->nlp, MSG_TYPE_ERR, "Timeout,", ch);
                ui_set_ritem (p->pfb, p->pui, UI_ID_STATUS[ch], COLOR_RED, -1);
            }
            if (state == eSTATE_RUNNING) {
                ui_set_ritem (p->pfb, p->pui, UI_ID_STATUS[ch], onoff ? COLOR_YELLOW : COLOR_BLUE, -1);
            }

            ui_set_sitem (p->pfb, p->pui, UI_ID_STATUS[ch], -1, -1, STR_STATE[state]);
            printf ("CH %d : state = %s\n", ch, STR_STATE[state]);
            p_ch->state = state;
        }
    }
}

//------------------------------------------------------------------------------
static void server_alive_display (server_t *p)
{
    static struct timeval i_time;
    static int onoff = 0;

    if (run_interval_check(&i_time, ALIVE_CHECK_INTERVAL)) {
        ui_set_ritem (p->pfb, p->pui, UI_ID_ALIVE,
                    onoff ? COLOR_GREEN : p->pui->bc.uint, -1);

        if (onoff)  ui_update (p->pfb, p->pui, -1);

        onoff = !onoff;
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#define CMD_CHECK_INTERVAL  10

//------------------------------------------------------------------------------
static void server_cmd_control (server_t *p)
{
    static struct timeval i_time;
    channel_t *p_ch;
    int ch;

    if (run_interval_check(&i_time, CMD_CHECK_INTERVAL)) {
        for (ch = 0; ch < eCH_END; ch++) {
            p_ch = &p->ch[ch];
            if (p_ch->state != eSTATE_RUNNING)
                continue;
            if ((p_ch->cmd_pos < p_ch->cmd_count) && !p_ch->cmd_wait) {
                protocol_msg_tx (p->ch[ch].puart,
                                'C',
                                p_ch->cmds[p_ch->cmd_pos].ui_id,
                                p_ch->cmds[p_ch->cmd_pos].grp_id,
                                p_ch->cmds[p_ch->cmd_pos].dev_id,
                                p_ch->cmds[p_ch->cmd_pos].action,
                                p_ch->cmds[p_ch->cmd_pos].r_delay);

                // command send, cmd wait flag set
                p_ch->cmd_wait = 1;
                ui_set_ritem (p->pfb, p->pui, p_ch->cmds[p_ch->cmd_pos].ui_id,
                            COLOR_YELLOW, -1);
            }
        }
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#define CONFIG_APP_FILE "server_cmd.cfg"

//------------------------------------------------------------------------------
static int server_cmd_load (server_t *p)
{
    FILE *fp;
    channel_t *p_ch;
    char read_line[STR_PATH_BYTES +1], *ptr;
    int appcfg = 0;

    if (access (CONFIG_APP_FILE, R_OK) == 0) {
        if ((fp = fopen (CONFIG_APP_FILE, "r")) != NULL) {
            while (1) {
                memset (read_line, 0x00, sizeof(read_line));
                if (fgets(read_line, sizeof(read_line), fp) == NULL)
                    break;

                if (!appcfg) {
                    appcfg = strncmp ("ODROID-APP-CONFIG", read_line,
                                    strlen(read_line)-1) == 0 ? 1 : 0;
                    continue;
                }
                if (read_line[0] == '#')
                    continue;

                if (!strncmp ("SERVER_CMD", read_line, sizeof("SERVER_CMD")-1)) {
                    /* struct cmd */
                    if ((ptr = strtok (read_line, ",")) != NULL) {
                        ptr = strtok (NULL, ",");   p_ch = &p->ch[atoi(ptr)];
                        ptr = strtok (NULL, ",");   strncpy (p_ch->cmds[p_ch->cmd_count].dev_name, ptr, strlen (ptr));

                        ptr = strtok (NULL, ",");   p_ch->cmds[p_ch->cmd_count].ui_id   = atoi(ptr);
                        ptr = strtok (NULL, ",");   p_ch->cmds[p_ch->cmd_count].grp_id  = atoi(ptr);
                        ptr = strtok (NULL, ",");   p_ch->cmds[p_ch->cmd_count].dev_id  = atoi(ptr);
                        ptr = strtok (NULL, ",");   p_ch->cmds[p_ch->cmd_count].action  = *ptr;
                        ptr = strtok (NULL, ",");   p_ch->cmds[p_ch->cmd_count].d_info  = atoi(ptr);
                        ptr = strtok (NULL, ",");   p_ch->cmds[p_ch->cmd_count].r_delay = atoi(ptr);
                        ptr = strtok (NULL, ",");   p_ch->cmds[p_ch->cmd_count].p_type  = atoi(ptr);

                        // adc pin info
                        if (p_ch->cmds[p_ch->cmd_count].p_type) {
                            ptr = strtok (NULL, ",");
                            strncpy (p_ch->cmds[p_ch->cmd_count].adc_info.port, ptr, strlen (ptr));
                            ptr = strtok (NULL, ",");   p_ch->cmds[p_ch->cmd_count].adc_info.max = atoi(ptr);
                            ptr = strtok (NULL, ",");   p_ch->cmds[p_ch->cmd_count].adc_info.min = atoi(ptr);
                        }
                        p_ch->cmd_count++;
                    }
                }
                if (!strncmp ("POWER_PIN", read_line, sizeof("POWER_PIN")-1)) {
                    if ((ptr = strtok (read_line, ",")) != NULL) {
                        ptr = strtok (NULL, ",");   p_ch = &p->ch[atoi(ptr)];
                        ptr = strtok (NULL, ",");   strncpy (p_ch->power_info[p_ch->power_count].port, ptr, strlen (ptr));
                        ptr = strtok (NULL, ",");   p_ch->power_info[p_ch->power_count].max = atoi(ptr);
                        ptr = strtok (NULL, ",");   p_ch->power_info[p_ch->power_count].min = atoi(ptr);
                        p_ch->power_count++;
                    }
                }
            }
            fclose (fp);
            return 1;
        }
    }
    return 0;
}

//------------------------------------------------------------------------------
static void server_setup (server_t *p)
{
    char dev_node [STR_PATH_BYTES +1];
    int ch;

    // uart, i2c init
    for (ch = 0; ch < eCH_END; ch++) {
        // ODROID-M1S (1.5M baud), I2C setup
        if ((p->ch[ch].uart_port = find_uart_port(SERVER_UART_PATH[ch])) < 0)
            printf ("%s : ttyUSB not found\n", SERVER_UART_PATH[ch]);
        else {
            if ((p->ch[ch].i2c_fd = adc_board_init (SERVER_I2C_PATH[ch])) < 0)
                printf ("%s : I2C not found\n", SERVER_I2C_PATH[ch]);
            else
                p->ch[ch].status = 1;
        }

        if (p->ch[ch].status) {
            memset (dev_node, 0, sizeof(dev_node));
            sprintf (dev_node, "/dev/ttyUSB%d", p->ch[ch].uart_port);
            if ((p->ch[ch].puart = uart_init (dev_node, SERVER_UART_BAUD)) == NULL)
                p->ch[ch].status = 0;
            else {
                if (ptc_grp_init (p->ch[ch].puart, 1)) {
                    if (!ptc_func_init (p->ch[ch].puart, 0, PROTOCOL_RX_BYTES, protocol_check, protocol_catch)) {
                        printf ("%s : protocol install error.", __func__);
                        p->ch[ch].status = 0;
                    }
                }
            }
        }
    }
}

//------------------------------------------------------------------------------
static int server_init (server_t *p)
{
    memset (p, 0, sizeof (server_t));

    // UI Init
    if ((p->pfb = fb_init (SERVER_FB)) == NULL)         exit(1);
    if ((p->pui = ui_init (p->pfb, SERVER_UI)) == NULL) exit(1);

    // server serial, i2c setup
    server_setup (p);
    ui_set_ritem (p->pfb, p->pui, UI_ID_PORT_STATUS_L, p->ch[eCH_L].status ? COLOR_GREEN : COLOR_RED, -1);
    ui_set_ritem (p->pfb, p->pui, UI_ID_PORT_STATUS_R, p->ch[eCH_R].status ? COLOR_GREEN : COLOR_RED, -1);

    // Get Server ip
    memset (&p->nlp, 0, sizeof(struct nlp_info));
    if (get_iface_info (&p->nlp, "eth0")) {
        printf ("eth0 nlp ip = %s, mac = %s\n", p->nlp.ip, p->nlp.mac);
        ui_set_sitem (p->pfb, p->pui, UI_ID_SERVER_IP, -1, -1, p->nlp.ip);
    }

    // Get Network printer info
    memset (&p->nlp, 0, sizeof(struct nlp_info));
    if (nlp_init (&p->nlp, NULL)) {
        printf ("nlp ip = %s, mac = %s\n", p->nlp.ip, p->nlp.mac);
        ui_set_sitem (p->pfb, p->pui, UI_ID_PRINTER_IP, -1, -1, p->nlp.ip);
        // printer test
        if (nlp_printf (&p->nlp, MSG_TYPE_MAC, p->nlp.mac, CH_NONE))
            printf ("NLP MAC Addr print\n");
        if (nlp_printf (&p->nlp, MSG_TYPE_ERR, "ODROID-M1S,SERVER,BOOT", CH_LEFT))
            printf ("NLP Printer Test Msg\n");
    }

    // server cmd load (app.cfg 파일에 등록되어진 client check command를 가져옴)
    server_cmd_load (p);
    return 1;
}

//------------------------------------------------------------------------------
static int check_header_pattern (server_t *p, int ch, int pattern)
{
    channel_t *p_ch = &p->ch[ch];
    int adc_h40 [40 +1], adc_h14 [14 +1], p_cnt = 0;

    int err_h40_pos = 0, err_h14_pos = 0;
    char err_h40_str [STR_PATH_BYTES *4], err_h14_str [STR_PATH_BYTES *2];

    memset (adc_h40, 0, sizeof(adc_h40));
    memset (adc_h14, 0, sizeof(adc_h14));

    memset (err_h40_str, 0, sizeof(err_h40_str));
    memset (err_h14_str, 0, sizeof(err_h14_str));

    adc_board_read (p_ch->i2c_fd, "P1_5.7", &adc_h14[11], &p_cnt);
    adc_board_read (p_ch->i2c_fd, "P1_5.6", &adc_h14[12], &p_cnt);
    adc_board_read (p_ch->i2c_fd, "P1_5.5", &adc_h14[13], &p_cnt);
    adc_board_read (p_ch->i2c_fd, "P1_5.4", &adc_h14[14], &p_cnt);

    adc_board_read (p_ch->i2c_fd, "CON1", &adc_h40[0], &p_cnt);

    if (pattern < PATTERN_COUNT) {
        int i;
        for (i = 0; i < (int)(sizeof(HEADER14)/sizeof(int)); i++) {
            if (HEADER14[i]) {
                if (H14_PATTERN[pattern][i]) {
                    // GPIO Set 전압이 설정값 보다 낮은 경우 error
                    if (adc_h14[i] < p_ch->cmds[p_ch->cmd_pos].adc_info.max) {
                        if (!err_h14_pos)
                            err_h14_pos = sprintf (err_h14_str, "H14-%d,", pattern);
                        // error pin save
                        err_h14_pos += sprintf (&err_h14_str[err_h14_pos], "%d-1,", i);
                    }
                } else {
                    // GPIO Clr 전압이 설정값 보다 높은 경우 error
                    if (adc_h14[i] > p_ch->cmds[p_ch->cmd_pos].adc_info.min) {
                        if (!err_h14_pos)
                            err_h14_pos = sprintf (err_h14_str, "H14-%d,", pattern);
                        // error pin save
                        err_h14_pos += sprintf (&err_h14_str[err_h14_pos], "%d-0,", i);
                    }
                }
            }
        }
        for (i = 0; i < (int)(sizeof(HEADER40)/sizeof(int)); i++) {
            if (HEADER40[i] && i) {
                if (H40_PATTERN[pattern][i]) {
                    // GPIO Set 전압이 설정값 보다 낮은 경우 error
                    // adc read시 0 ~ 39배열에 1 ~ 40번 핀의 값을 넣게 되므로 pin번호에서 1개를 뺀다.
                    if (adc_h40[i -1] < p_ch->cmds[p_ch->cmd_pos].adc_info.max) {
                        if (!err_h40_pos)
                            err_h40_pos = sprintf (err_h40_str, "H40-%d,", pattern);
                        // error pin save
                        err_h40_pos += sprintf (&err_h40_str[err_h40_pos], "%d-1,", i);
                    }
                } else {
                    // GPIO Clr 전압이 설정값 보다 높은 경우 error
                    if (adc_h40[i -1] > p_ch->cmds[p_ch->cmd_pos].adc_info.min) {
                        if (!err_h40_pos)
                            err_h40_pos = sprintf (err_h40_str, "H40-%d,", pattern);
                        // error pin save
                        err_h40_pos += sprintf (&err_h40_str[err_h40_pos], "%d-0,", i);
                    }
                }
            }
        }
    }
    if (err_h40_pos || err_h14_pos) {
        if (err_h14_pos)    nlp_printf (&p->nlp, MSG_TYPE_ERR, err_h14_str, ch);
        if (err_h40_pos)    nlp_printf (&p->nlp, MSG_TYPE_ERR, err_h40_str, ch);
        return 0;
    }
    return 1;
}

//------------------------------------------------------------------------------
static void channel_status_update (server_t *p, int ch, char *rx_str)
{
    channel_t *p_ch = &p->ch[ch];
    int ui_id = p_ch->cmds[p_ch->cmd_pos].ui_id;

    // hdmi의 경우만 string데이터임. (pass/fail)
    if ((p_ch->cmds[p_ch->cmd_pos].grp_id != 3) &&
      !((p_ch->cmds[p_ch->cmd_pos].grp_id == 5) && (p_ch->cmds[p_ch->cmd_pos].dev_id == 1)))
        sprintf (rx_str, "%d", atoi(rx_str));

    // Response dasta display
    switch (p_ch->cmds[p_ch->cmd_pos].d_info) {
        // info only
        case    1:
            ui_set_sitem (p->pfb, p->pui, ui_id, -1, -1, rx_str);
            break;
        // box color only
        case    2:
            ui_set_ritem (p->pfb, p->pui, ui_id,
                        p_ch->cmds[p_ch->cmd_pos].result ? COLOR_GREEN : COLOR_RED, -1);
            break;
        // info & box color
        default :
            ui_set_sitem (p->pfb, p->pui, ui_id, -1, -1, rx_str);
            ui_set_ritem (p->pfb, p->pui, ui_id,
                        p_ch->cmds[p_ch->cmd_pos].result ? COLOR_GREEN : COLOR_RED, -1);
            break;
    }
    // cmd wait flag clear, update cmd pos
    p_ch->cmd_wait = 0;
    p_ch->cmd_pos++;
}

//------------------------------------------------------------------------------
static void client_msg_parse (server_t *p, int ch)
{
    channel_t *p_ch = &p->ch[ch];

    // resp status check (R/O/E/B)
    switch (p_ch->rx_msg [1]) {
        case 'R':
            printf ("Ready message received (ch = %d).\n", ch);
            // connect가 1인 경우에 다시 'R'이 들어오는 경우 시스템이 재부팅되었음을 의미하므로
            // 테스트진행을 처음부터 다시 진행하여야 함.
            if (p_ch->connect && (p_ch->state == eSTATE_RUNNING))
                p_ch->state = eSTATE_WAIT;

            p_ch->connect = 1;
            return;
        case 'O':   case 'E':   case 'B':
            // connect처리가 되지 않은 경우에는 의미없는 데이터이므로 버림.
            if (!p_ch->connect)
                return;
            // running시 uart watchdog활성화 상태이므로 정상적인 경우 초기화 시켜야 함.
            p_ch->watchdog = 0;
            break;
        default:
            printf ("Unknown message received(%c).\n", p_ch->rx_msg[1]);
            return;
    }
    if (p_ch->cmd_wait) {
        int ui_id;
        char rx_str[10];

        memset (rx_str, 0, sizeof(rx_str));
        strncpy(rx_str, &p_ch->rx_msg[2], 4);

        ui_id = atoi(rx_str);

        // 보내진 server cmd id와 같은지 비교하여 결과값을 업데이트
        if (p_ch->cmds[p_ch->cmd_pos].ui_id == ui_id) {
            memset (rx_str, 0, sizeof(rx_str));
            strncpy(rx_str, &p->ch[ch].rx_msg[6], 6);

            p_ch->cmds[p_ch->cmd_pos].result = 0;

            // port check type이 adc인 경우 처리
            if (p_ch->cmds[p_ch->cmd_pos].p_type) {
                // GPIO_HEADER group, HEADER devid (Header pattern check)
                if ((p_ch->cmds[p_ch->cmd_pos].grp_id == 6) &&
                    (p_ch->cmds[p_ch->cmd_pos].dev_id == 0)) {
                    p_ch->cmds[p_ch->cmd_pos].result =
                        check_header_pattern (p, ch, p_ch->cmds[p_ch->cmd_pos].action - '0');
                    sprintf (rx_str, "%d", p_ch->cmds[p_ch->cmd_pos].result);
                }
                else
                    p_ch->led_check_time = LED_CHECK_SEC(2); // 500us * 4000 = 2sec
            }
            else
                p_ch->cmds[p_ch->cmd_pos].result = (p->ch[ch].rx_msg[1] == 'O') ? 1 : 0;

            // 응답받은 값이 ethernet mac address이고 정상적으로 write되어 있다면 프린트 (보드 MAC addr)
            if (p_ch->cmds[p_ch->cmd_pos].result) {
                // grp_id(5) = ethernet, dev_id(1) = mac
                if ((p_ch->cmds[p_ch->cmd_pos].grp_id == 5) && (p_ch->cmds[p_ch->cmd_pos].dev_id == 1))
                {
                    char mac [STR_NAME_BYTES +1];
                    memset (mac, 0, sizeof(mac));

                    sprintf (mac, "001E06%6s", rx_str);
                    nlp_printf (&p->nlp, MSG_TYPE_MAC, mac, ch);
                }
            }
            // update channel response data
            if (!p_ch->led_check_time)
                channel_status_update (p, ch, rx_str);
        }
    }
}

//------------------------------------------------------------------------------
static int check_led_data (channel_t *p_ch, int *resp)
{
    int pin, mv, status = 0;

    if (adc_board_read (p_ch->i2c_fd, p_ch->cmds[p_ch->cmd_pos].adc_info.port, &mv, &pin)) {
        if (p_ch->cmds[p_ch->cmd_pos].adc_info.max && p_ch->cmds[p_ch->cmd_pos].adc_info.min) {
            if ((mv < p_ch->cmds[p_ch->cmd_pos].adc_info.max) &&
                (mv > p_ch->cmds[p_ch->cmd_pos].adc_info.min))  status = 1;
        }
        if (p_ch->cmds[p_ch->cmd_pos].adc_info.max) {
            if (mv < p_ch->cmds[p_ch->cmd_pos].adc_info.max)    status = 1;
        }
        if (p_ch->cmds[p_ch->cmd_pos].adc_info.min) {
            if (mv > p_ch->cmds[p_ch->cmd_pos].adc_info.min)    status = 1;
        }
    }
    *resp = mv;
    return status ? 1 : 0;
}

//------------------------------------------------------------------------------
int main (void)
{
    int ch, resp;
    char resp_str[10];
    server_t server;
    channel_t *p_ch;

    // UI, UART, I2C
    server_init (&server);

    // Ready msg send
    protocol_msg_tx (server.ch[eCH_L].puart, 'P', 0, 0, 0, '0', 0);
    protocol_msg_tx (server.ch[eCH_R].puart, 'P', 0, 0, 0, '0', 0);

    while (1) {
        server_alive_display(&server);
        server_state_control(&server);
        server_cmd_control  (&server);

        for (ch = 0; ch < eCH_END; ch++) {
            p_ch = &server.ch[ch];
            if (protocol_msg_rx (p_ch->puart, p_ch->rx_msg))
                client_msg_parse (&server, ch);

            /* led check는 polling으로 검사함. */
            if (p_ch->led_check_time) {
                p_ch->led_check_time--;
                if (check_led_data (p_ch, &resp)) {
                    p_ch->led_check_time = 0;
                    p_ch->cmds[p_ch->cmd_pos].result = 1;
                }
                if (!p_ch->led_check_time) {
                    memset (resp_str, 0, sizeof(resp_str));
                    sprintf (resp_str, "%d", resp);
                    channel_status_update (&server, ch, resp_str);
                }
            }
        }
        usleep (APP_LOOP_DELAY);
    }
    return 0;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
