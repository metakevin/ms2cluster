/******************************************************************************
* File:              gpsavr.c
* Author:            Kevin Day
* Date:              December, 2008
* Description:       
*                    
*                    
* Copyright (c) 2008 Kevin Day
* 
*     This program is free software: you can redistribute it and/or modify
*     it under the terms of the GNU General Public License as published by
*     the Free Software Foundation, either version 3 of the License, or
*     (at your option) any later version.
*
*     This program is distributed in the hope that it will be useful,
*     but WITHOUT ANY WARRANTY; without even the implied warranty of
*     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*     GNU General Public License for more details.
*
*     You should have received a copy of the GNU General Public License
*     along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*******************************************************************************/

#include "comms_generic.h"
#include "tasks.h"
#include "avrsys.h"
#include "bufferpool.h"
#include "gpsavr.h"
#include "hud.h"
#include "platform.h"

#include <stdio.h>
#include <string.h>

//#define GPS_BAUD_RATE 115200UL
#define GPS_BAUD_RATE 38400UL

static task_t gps_taskinfo;
static u8 gps_mailbox_buf[20];
void gps_rx_notify(u8 data);
void handle_gps_field(u8 fid, char *fstr, u8 flen);


void gpstime_draw(u8 *dbuf, u8 update_only, u16 arg);
void gpsspeed_draw(u8 *dbuf, u8 update_only, u16 arg);
task_t *gps_task_create()
{
    /* Configure USART 1 for 115k */
    UBRR1L = (uint8_t)(CPU_FREQ/(GPS_BAUD_RATE*16L));
    UBRR1H = (CPU_FREQ/(GPS_BAUD_RATE*16L)) >> 8;
    UCSR1A = 0x0;
    UCSR1B = _BV(TXEN1)|_BV(RXEN1);//|_BV(RXCIE0);
       UCSR1C = _BV(UCSZ11)|_BV(UCSZ10);
 
    /* Enable pullup on RXD line for level shifter */
    PORTD |= (1<<2);

    //register_display_area(gpstime_draw, 0, 8, 8);
    register_display_area(gpsspeed_draw, 0, 7*8, 8);

    /* Enable RX interrupt */
    UCSR1B |= _BV(RXCIE1);

    return setup_task(&gps_taskinfo, TASK_ID_GPS, gps_task, 
                      gps_mailbox_buf, sizeof(gps_mailbox_buf));
}

#define RCVBUFSZ 256 /* must be power of 2 */
static u8 rcvbuf[RCVBUFSZ];
static u8 rcvhead, rcvtail;
static u8 rcv_overflow;
static u8 adv_rcvbuf(u8 ptr)
{
    return ((ptr+1)&(RCVBUFSZ-1));
}

SIGNAL(SIG_UART1_RECV)
{
    u8 nh = adv_rcvbuf(rcvhead);
    if (nh == rcvtail)
    {
        ++rcv_overflow;
        UDR1;
    }
    else
    {   
        rcvbuf[rcvhead] = UDR1;
        rcvhead = nh;
    }    
}    


void gps_valid();

u8 gps_task()
{
    u8 rc = 0;
    PORTG &= ~(2);
    while (rcvtail != rcvhead)
    {
        u8 x = rcvbuf[rcvtail];
        rcvtail = adv_rcvbuf(rcvtail);
#if 1
        gps_rx_notify(x);
#else
        static u8 lbuf[80];
        static u8 li;        
        if (x == '$') {
            li = 0;
        }
        if (li < sizeof(lbuf)) {
            lbuf[li++] = x;
        }
        if (x == '\r') {
            u8 i;
            for(i=0; i<li; )
            {
                u8 l = li-i;
                if (l > 10) {
                    l = 10;
                }

                send_msg(0xF, 
                     TASK_ID_GPS<<4|COMMS_MSG_GPS_RAW,
                     l, lbuf+i, 1);
                i += l;
            }
        } 
#endif
//        if (x == '$')
//        {
 //           PORTG ^= 2;
  //      }
        rc = 1;
    }
    PORTG |= (2);
    return rc;
}

#if 1
#define MAXNMEA 83
static char line[MAXNMEA];
static u8 i;
static u8 field;
static u8 field_start;
static u8 csum_start;
void gps_rx_notify(u8 c)
{
    if (c == '$')
    {
    field = 0;
        i = 0;
        field_start = 1;
    }
    if (i >= sizeof(line)) 
    {
    return;
    }
    line[i] = c;
    if (c == ',' || c == '*')
    {
        if (c == '*')
        {
            csum_start = i;
        }
        line[i] = 0; // make nice string
    if (i-field_start) {
        /* don't send empty fields */
            handle_gps_field(field, line + field_start, i-field_start);  
    }
        line[i] = ','; // put back for checksum
        ++field;
        field_start = i+1;
    }
    else if (c == '\r')
    {
        /* verify checksum */
        u8 rcs = 0;
        sscanf(line+csum_start+1, "%02hhX", &rcs);
        u8 cs = 0;
        u8 j;
        for(j=1; j<csum_start; j++)
        {
            cs ^= line[j];
        }
        if (cs == rcs)
        {
            gps_valid();
        }
    else
    {
        u8 xx[] = {1, cs, rcs, rcv_overflow};
        send_msg(0xF, TASK_ID_GPS<<4|2, sizeof(xx), xx, 1);
        rcv_overflow = 0;
    }
    }
    else if (c == '\n')
    {
        /* ignore */
    }
    ++i;
}

typedef enum {DONTCARE, GPRMC} gpsmsg_t;
gpsmsg_t msgtype;
typedef struct {
    u8 utc_hr;
    u8 utc_min;
    u8 utc_sec;
//    u8 lat_deg;
//    u8 lat_min;
//    u8 lat_sec;
//    u8 lat_ns;
//    u8 lon_deg;
//    u8 lon_min;
//    u8 lon_sec;
//    u8 lon_ew;
    u8 spd_knts;
    u8 spd_knts_fract;
    u8 utc_day;
    u8 utc_month;
    u8 utc_year;
} gprmc_data_t;

gprmc_data_t gprmc[2];
u8 gprmc_i;
u8 gprmc_valid;

void gps_valid()
{
    if (msgtype == GPRMC) 
    {
    gprmc_valid = gprmc_i | 0x80;
    gprmc_i = !gprmc_i;
    }
}

void handle_gps_field(u8 fid, char *fstr, u8 flen)
{
    //u8 xx[] = {0, fid, flen};
    //send_msg(0xF, TASK_ID_GPS<<4|2, sizeof(xx), xx, 1);
    //send_msg(0xF, 
    //     TASK_ID_GPS<<4|4,
    //     flen, fstr, 1);
    if (fid == 0)
    {
        if (!strncmp(fstr, "GPRMC", 5))
        {
            msgtype = GPRMC;
        }
        else
        {
            msgtype = DONTCARE;
        }
        return;
    }

    if (msgtype == GPRMC)
    {
        switch (fid)
        {
            case 1:
            { u8 x;
                // time  hhmmss.sss
                x = sscanf(fstr, "%02hhu%02hhu%02hhu", 
                        &gprmc[gprmc_i].utc_hr,
                        &gprmc[gprmc_i].utc_min,
                        &gprmc[gprmc_i].utc_sec);
                //send_msg(0xF, 
                //     TASK_ID_GPS<<4|4,
                //     flen, fstr, 1);
        //send_msg(0xF, TASK_ID_GPS<<4|2, 1, &x, 1);
            }
                break;
            case 2:
                // status A = valid V = not valid 
                break;
            case 3:
                // lat xxxx.xx
                break;
            case 4:
                // N/S
                break;
            case 5:
                // long xxxxx.xx
                break;
            case 6:
                // E/W
                break;
            case 7:
                // speed over ground in knots
                sscanf(fstr, "%hhu.%hhu",
                        &gprmc[gprmc_i].spd_knts,
                        &gprmc[gprmc_i].spd_knts_fract);
                break;
            case 8:
                // date
                sscanf(fstr, "%02hhu%02hhu%02hhu", 
                        &gprmc[gprmc_i].utc_day,
                        &gprmc[gprmc_i].utc_month,
                        &gprmc[gprmc_i].utc_year);
                break;
            case 9:
                // magnetic variation
                break;
            case 10:
                // E/W
                break;
            default:
                break;
        }
    }
}
u16 g_knotsx100;
void gpsspeed_draw(u8 *dbuf, u8 update_only, u16 arg)
{
    if (gprmc_valid&0x80)
    {
        u8 i = (gprmc_valid&1);
        char buf[9];

        /* I think the fractional part of the knots field is only to one decimal place.  
         * But just in case it isn't... */
        while (gprmc[i].spd_knts_fract >= 10)
        {
            gprmc[i].spd_knts_fract /= 10;
        }

        g_knotsx100 = gprmc[i].spd_knts * 115 + gprmc[i].spd_knts_fract * 12;
        u16 spdmph = (g_knotsx100 + 50) / 100;
        snprintf(buf, 9, "MPH  %3u", spdmph);
        render_string(dbuf, 0, 8, buf);    
    }
    else
    {
        render_string(dbuf, 0, 8, "no speed");
    }
}

void gpstime_draw(u8 *dbuf, u8 update_only, u16 arg)
{
    if (gprmc_valid&0x80)
    {
    u8 i = (gprmc_valid&1);

    static const s8 tz_ofs = -4;
    char buf[9];
    s8 hr = gprmc[i].utc_hr + tz_ofs;
    if (hr < 0) {
        hr += 24;
    }
    snprintf(buf, 9, "%02d:%02d:%02d",
        hr,
        gprmc[i].utc_min,
        gprmc[i].utc_sec);
    render_string(dbuf, 0, 8, buf);    
    }
    else
    {
    render_string(dbuf, 0, 8, "no time ");
    }
}

#endif
