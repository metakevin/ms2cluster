/******************************************************************************
* File:              avrcan.c
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

#include "platform.h"
#include "comms_generic.h"
#include "tasks.h"
#include "avrsys.h"
#include "bufferpool.h"
#include "avrcan.h"
#include "avrms2.h"

#define CANPORT  PORTD
#define CANDIR   DDRD
#define TXCANPIN 5
#define RXCANPIN 6

#define NUM_MESSAGE_OBJECTS 15
static void clear_current_message_object()
{
    u8 volatile *r;
    for(r=&CANSTMOB; r<&CANSTML; r++)
        *r = 0;
}
#if 0
static void clear_all_message_objects()
{
    u8 page;
    for(page=0; page<NUM_MESSAGE_OBJECTS; page++)
    {
        CANPAGE = (page<<4);
        clear_current_message_object();
    }
}
#endif
static u8 find_free_message_object()
{
    u8 page;
    for(page=0; page<NUM_MESSAGE_OBJECTS; page++)
    {
        CANPAGE = (page<<4);
        if (!(CANCDMOB & ((1<<CONMOB1)|(1<<CONMOB0))))
        {
            /* Disable mode.  I.e. available. */
            return page;
        }
    }
    return 0xFF;
}

static void config_for_rx()
{
    /* Enable reception */
    CANCDMOB = ((1<<CONMOB1));     
}

static void config_for_tx(u8 len)
{
    /* Enable TX, extended frame, set length */
    CANCDMOB = ((1<<CONMOB0)|(1<<IDE)|len);
}



/* MS2 CAN message format:
 * http://www.megamanual.com/com/CAN.htm
 * Extended 29 bit header is used.
 */

    

void canbus_init()
{
    /* Turn pullups on */
    CANDIR  &= ~((1<<TXCANPIN) | (1<<RXCANPIN));
    CANPORT |=  ((1<<TXCANPIN) | (1<<RXCANPIN));
    
    /* Initialize CAN unit */
    CANGCON = (1<<SWRES);

    /* Configure bit timings for 500kbit/sec at 16 MHz */
    CANBT1 = 0x02;
    CANBT2 = 0x0C;
    CANBT3 = 0x37;

    /* Mark 8 MObs for RX */
    u8 page;
    for(page=0; page<8; page++)
    {
        CANPAGE = (page<<4);
        clear_current_message_object();
        config_for_rx();
    }
    /* And 7 for TX (empty for now) */
    for(; page<15; page++)
    {
        CANPAGE = (page<<4);
        clear_current_message_object();
    }

    /* Enable all interrupts except timer */
    CANGIE |= 0xFE;

    /* Enable all MOB interrupts */
    CANIE1 = 0x7F;
    CANIE2 = 0xFF;

    /* Enable CAN unit */
    CANGCON |= (1<<ENASTB);

}

u8 send_can_msg(u8 *idt, u8 *payload, u8 plen)
{
    if (find_free_message_object() != 0xFF)
    {
        CANIDT1 = idt[0];
        CANIDT2 = idt[1];
        CANIDT3 = idt[2];
        CANIDT4 = idt[3];

        u8 i;
        for(i=0; i<plen; i++)
        {
            CANMSG = payload[i];
        }
        config_for_tx(plen);
        return 0;
    }
    return 1;
}


static task_t can_taskinfo;
static u8 can_mailbox_buf[80];
static u8 can_task();

task_t *can_task_create()
{
    canbus_init();

    return setup_task(&can_taskinfo, TASK_ID_CAN, can_task,  
                      can_mailbox_buf, sizeof(can_mailbox_buf));
}

u8 can_task()
{
    u8 ret = 0;
    u8 code, payload_len;
    
    if (mailbox_head(&can_taskinfo.mailbox, &code, &payload_len))
    {
        switch(code)
        {
        case CAN_SEND_RAW_MSG:
            /* format: IDT1 - IDT4 followed by 0 - 8 payload bytes 
             * CANIDT1 = IDT[28:21] ... CANIDT4 = IDT[4:0] */
            if (payload_len < 4)
            {
                /* error */
                send_msg(0, 
                    TASK_ID_CAN<<4|CAN_ERROR,
                    0, NULL, 1);
            }
            else
            {
                u8 payload[12];
                mailbox_copy_payload(&can_taskinfo.mailbox, payload, 12, 0);
                send_can_msg(&payload[0], &payload[4], payload_len-4);
            }
            break;
        case CAN_MSG_RX:
            {
                /* Received message from ISR */
                u8 payload[12];
                mailbox_copy_payload(&can_taskinfo.mailbox, payload, 12, 0);
#if 0
                send_msg(2, 
                         TASK_ID_CAN<<4|COMMS_MSG_CAN_RAW,
                         payload_len, payload, 1);    
#endif
                ms2_can_rx(payload, payload_len);
                break;
            }
        default:
            break;
        }
        mailbox_advance(&can_taskinfo.mailbox);
        ret = 1;
    }

    return ret;
}                

    



SIGNAL(SIG_CAN_INTERRUPT1)
{
    u8 pagesave = CANPAGE;
    if ((CANHPMOB & 0xF0) != 0xF0)
    {
        CANPAGE = CANHPMOB;
        if (CANSTMOB & (1<<RXOK))
        {
            u8 canbuf[12];
            canbuf[0] = CANIDT1;
            canbuf[1] = CANIDT2;
            canbuf[2] = CANIDT3;
            canbuf[3] = CANIDT4;
            u8 i;
            for(i=4; i<12; i++)
            {
                canbuf[i] = CANMSG;
            }
            
            u8 len = 4 + (CANCDMOB&0xF);
#if 0
            send_msg(2, 
                     TASK_ID_CAN<<4|COMMS_MSG_CAN_RAW,
                     len, canbuf, 1);    
#else
            mailbox_deliver(&can_taskinfo.mailbox, 
                            CAN_MSG_RX, len, canbuf);
#endif
            /* Acknowledge interrupt */
            CANSTMOB &= ~(1<<RXOK);
            clear_current_message_object();
            config_for_rx();
        }
        else if (CANSTMOB & (1<<TXOK))
        {
            /* Acknowledge interrupt */
            CANSTMOB &= ~(1<<TXOK);
            clear_current_message_object();
        }
        else
        {
            /* This happens sometimes... CANSTMOB is zero.
             * Why? */
        }
    }
    CANPAGE = pagesave;
}

/* Notes on MS CAN:
 *  - ID registers on AVR and HC12 do not match up exactly
 *

avrtalk> cantxms
  cantxms <var_offset> <msg_type> <src_id> <rcv_id> <var_blk> [payload...]
avrtalk> cantxms 0 1 3 0 4 0 0 1
Sent CAN:
 var_offset: 0
 msg_type  : 1
 src_id    : 3
 rcv_id    : 0
 var_blk   : 4
 var_byt   : 3
04: 00
05: 00
06: 01
avrtalk> Received CAN:
 var_offset: 0
 msg_type  : 2
 src_id    : 0
 rcv_id    : 3
 var_blk   : 0
 var_byt   : 1
04: 04
 *
 * This sends a read request of length 1 byte from offset 0 in block 4
 * The response has the first two bytes of the read request payload
 * in var_offset and var_blk.
 */


