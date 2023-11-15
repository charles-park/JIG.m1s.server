//------------------------------------------------------------------------------
/**
 * @file protocol.c
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <getopt.h>

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
/* protocol control 함수 */
#include "protocol.h"

//------------------------------------------------------------------------------
//
// https://docs.google.com/spreadsheets/d/1Of7im-2I5m_M-YKswsubrzQAXEGy-japYeH8h_754WA/edit#gid=0
//
//------------------------------------------------------------------------------
int protocol_check (ptc_var_t *var)
{
    /* head & tail check with protocol size */
    if(var->buf[(var->p_sp + var->size -1) % var->size] != '#')	return 0;
    if(var->buf[(var->p_sp               ) % var->size] != '@')	return 0;
    return 1;
}

//------------------------------------------------------------------------------
int protocol_catch (ptc_var_t *var)
{
    char cmd = var->buf[(var->p_sp + 1) % var->size];

    switch (cmd) {
        case 'R':   case 'O':   case 'E':   case 'B':
            return 1;
        default :
            printf ("unknown command %c\n", cmd);
            return 0;
    }
}

//------------------------------------------------------------------------------
void protocol_msg_tx (uart_t *puart, char cmd, int ui_id, int grp_id, int dev_id, char act, int r_delay)
{
    char send_data [PROTOCOL_TX_BYTES], str_id[5];

    /* uart null point error fix */
    if (puart == NULL)  return;

    send_data [0]  = '@';
    send_data [1]  = cmd;
    send_data [11] = act;
    // ui_id convert to str
    memset  (str_id, 0, sizeof(str_id));
    sprintf (str_id, "%04d", ui_id);
    memcpy (&send_data [2], &str_id[0], 4);

    // grp_id convert to str
    memset  (str_id, 0, sizeof(str_id));
    sprintf (str_id, "%02d", grp_id);
    memcpy (&send_data [6], &str_id[0], 2);

    // dev_id convert to str
    memset  (str_id, 0, sizeof(str_id));
    sprintf (str_id, "%03d", dev_id);
    memcpy (&send_data [8], &str_id[0], 3);

    // r_delay
    sprintf (&send_data [12], "%06d", r_delay);
    send_data [18] = '#';

    uart_write (puart, (unsigned char *)&send_data [0], PROTOCOL_TX_BYTES);
    // for console display
    {
        char LF = '\n', CR = '\r';
        uart_write (puart, (unsigned char *)&LF, 1);
        uart_write (puart, (unsigned char *)&CR, 1);
    }
}

//------------------------------------------------------------------------------
int protocol_msg_rx (uart_t *puart, char *rx_msg)
{
    unsigned char idata, p_cnt;

    /* uart null point error fix */
    if (puart == NULL)  return 0;

    /* uart data processing */
    if (uart_read (puart, &idata, 1)) {
        ptc_event (puart, idata);
        for (p_cnt = 0; p_cnt < puart->pcnt; p_cnt++) {
            if (puart->p[p_cnt].var.pass) {
                ptc_var_t *var = &puart->p[p_cnt].var;
                int i;

                puart->p[p_cnt].var.pass = 0;
                puart->p[p_cnt].var.open = 1;

                /* start(1), cmd(1), ui_ui(4), grp_id(2), dev_id(3), action(1), extra(6), end(1) = 19 bytes */
                for (i = 0; i < PROTOCOL_RX_BYTES; i++)
                    // uuid start position is 2
                    rx_msg [i] = var->buf[(var->p_sp + i) % var->size];
                return 1;
            }
        }
    }
    return 0;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------