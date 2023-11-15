//------------------------------------------------------------------------------
/**
 * @file server.h
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
#ifndef __SERVER_H__
#define __SERVER_H__

//------------------------------------------------------------------------------
#include "lib_fbui/lib_fb.h"
#include "lib_fbui/lib_ui.h"
#include "lib_nlp/lib_nlp.h"
#include "lib_uart/lib_uart.h"
#include "lib_i2cadc/lib_i2cadc.h"
#include "protocol.h"
#include "header_check.h"

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//
// JIG Protocol(V2.0)
// https://docs.google.com/spreadsheets/d/1Of7im-2I5m_M-YKswsubrzQAXEGy-japYeH8h_754WA/edit#gid=0
//
//------------------------------------------------------------------------------
#define SERVER_FB       "/dev/fb0"
#define SERVER_UART     "/dev/ttyUSB0"
#define SERVER_UI       "ui.cfg"

#define UI_ID_ALIVE         0
#define UI_ID_SERVER_IP     2
#define UI_ID_PRINTER_IP    7
#define UI_ID_PORT_STATUS_L 22
#define UI_ID_PORT_STATUS_R 26

#define APP_LOOP_DELAY      500
#define STR_PATH_BYTES      128
#define STR_NAME_BYTES      20
#define SERVER_UART_BAUD    1500000

#define ALIVE_CHECK_INTERVAL    1000

// server cmd control (server_cmd.cfg)
#define MAX_CMD_COUNT   64
#define MAX_POWER_COUNT 10

// led check time (APP_LOOP_DELAY = 500uS),
#define LED_CHECK_SEC(x)  (APP_LOOP_DELAY * 2 * x)

//------------------------------------------------------------------------------
enum {
    eCH_L = 0,
    eCH_R,
    eCH_END
};

const char SERVER_I2C_PATH[eCH_END][STR_PATH_BYTES] = {
    // CH-L
    { "/dev/i2c-0" },
    // CH-R
    { "/dev/i2c-1" },
};

const char SERVER_UART_PATH[eCH_END][STR_PATH_BYTES] = {
    // CH-L
    { "/sys/devices/platform/fd8c0000.usb/usb4/4-1/4-1:1.0/" },
    // CH-R
    { "/sys/devices/platform/usbhost/fd000000.dwc3/xhci-hcd.0.auto/usb5/5-1/5-1:1.0/" },
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
typedef struct adc_pin__t {
    // port name
    char    port [STR_NAME_BYTES +1];
    // adc check range
    int     max, min;
}   adc_pin_t;

typedef struct cmd__t {
    // check device name
    char    dev_name [STR_NAME_BYTES +1];
    // left, right check data display ui_id
    int     ui_id;
    // command group (0.system, 1.storage...)
    int     grp_id;
    // device id of the group
    int     dev_id;
    // check action
    char    action;
    // response delay
    int     r_delay;
    // display info type (0 : color & data, 1 : data only, 2 : color)
    int     d_info;
    // port type (0 : normal, 1 : adc pin)
    int     p_type;
    // result check max, min for adc pin
    adc_pin_t   adc_info;
    // dev check result (0 : fail, 1 : pass)
    int     result;
}   cmd_t;

typedef struct channel_t {
    // status (0 : i2c or serial fail, 1 : ok)
    int     status;

    int     state;
    // server connect status (1 = connect, 0 = disconnect)
    int     connect;
    // I2C ADC fd
    int     i2c_fd;
    // Control server message
    int     cmd_pos;
    // command wait flag (command send후 resp를 기다린다는 flag)
    int     cmd_wait;

    // server cmds
    int     cmd_count;
    cmd_t   cmds [MAX_CMD_COUNT];
    // power pin
    adc_pin_t   power_info [MAX_POWER_COUNT];

    // uart
    uart_t  *puart;
    // ttyUSB number (0,1,2...)
    int     uart_port;
    int     power_count;
    // protocol control
    char    rx_msg [PROTOCOL_RX_BYTES +1];
    char    tx_msg [PROTOCOL_TX_BYTES +1];
    // watchdog count (1 min > watchdog count = system fail)
    int     watchdog;

    // led check time (2 sec),
    int     led_check_time;

}   channel_t;

typedef struct server__t {
    // HDMI UI
    fb_info_t   *pfb;
    ui_grp_t    *pui;

    // label printer
    struct nlp_info nlp;

    // server ip address
    char    ip_str [STR_NAME_BYTES +1];

    // check port
    channel_t   ch [eCH_END];

}   server_t;

//------------------------------------------------------------------------------
enum {
    eSTATE_WAIT = 0,
    eSTATE_RUNNING,
    eSTATE_STOP,
    eSTATE_FINISH,
    eSTATE_ERR_POWER,   /* force power stop */
    eSTATE_ERR_UART,    /* watchdog (uart) */
    eSTATE_END
};

const char STR_STATE[eSTATE_END][STR_NAME_BYTES] = {
    { "WAIT"     },
    { "RUNNING"  },
    { "STOP"     },
    { "FINISH"   },
    { "ERR_POWER"},
    { "ERR_UART" },
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif  // #define __SERVER_H__

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
