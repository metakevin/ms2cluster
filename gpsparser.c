/******************************************************************************
* File:              gpsparser.c
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

void gps_rx_notify(u8 data);

#ifdef HOST
int main()
{
    char ch;
    while (ch=fgetc(stdin))
    {
        gps_rx_notify(ch);
    }
}
#endif

static char rxbuf[83];
u8 rxi;
enum {WAIT, SID, FIELDS, CKSUM} state;

typedef struct {
    union {
        struct {



void gps_rx_notify(u8 data)
{
    switch (data)
    {
        case '$':
            rxi = 0;
            state = SID;
            break;
        case '*':
            state = CKSUM;
            break;
        case '\n':
            /* verify checksum here -
             * rxbuf[rxi-3], rxbuf[rxi-2] */
            state = WAIT;
            if (!strncmp(rxbuf, "$GPRMC", 6))
            {
                send_msg(2, 
                     TASK_ID_GPS<<4|COMMS_MSG_GPS_RAW,
                     15, rxbuf, 1);
            }
            break;
        default:
            break;
    }
    if (rxi >= sizeof(rxbuf))
    {
        rxi = 0;
    }
    rxbuf[rxi++] = data;
 
}
