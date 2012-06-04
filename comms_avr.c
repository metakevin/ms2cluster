/******************************************************************************
* File:              comms_avr.c
* Author:            Kevin Day
* Date:              December, 2004
* Description:       Framed serial protocol, AVR side
*                    
*                    
* Copyright (c) 2004 Kevin Day
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

#include <avr/wdt.h>
#include "platform.h"
#include "comms_generic.h"
#include "tasks.h"
#include "avrsys.h"
#include "bufferpool.h"
#include "dataflash.h"
#include "spimaster.h"

#define COMMS_MSG_HEADER    0x10
#define COMMS_MSG_PAYLOAD   0x11

u8 get_node_id()
{
    return 1;
}

static task_t   comms_taskinfo;
static u8       comms_mailbox_buf[40];
u8 comms_task();

/* called by main */
task_t* comms_task_create()
{
    /* Configure the baud rate. */
    /* On the at90can128, the -1 that was in the baud rate expressions
     * (as used on the atmega88/168) had to be removed. */
	UBRR0L = (uint8_t)(CPU_FREQ/(UART_BAUD_RATE*16L));
	UBRR0H = (CPU_FREQ/(UART_BAUD_RATE*16L)) >> 8;
	UCSR0A = 0x0;
	UCSR0B = _BV(TXEN0)|_BV(RXEN0);
   	UCSR0C = _BV(UCSZ01)|_BV(UCSZ00);

    /* Enable RX interrupt */
    UCSR0B |= _BV(RXCIE0);
 
    return setup_task(&comms_taskinfo, TASK_ID_COMMS, comms_task, 
            comms_mailbox_buf, sizeof(comms_mailbox_buf));
}


#define RCVBUFSZ 16 /* must be power of 2 */
static u8 rcvbuf[RCVBUFSZ];
static u8 rcvhead, rcvtail;
static u8 rcv_overflow;
static u8 adv_rcvbuf(u8 ptr)
{
    return ((ptr+1)&(RCVBUFSZ-1));
}

SIGNAL(SIG_UART0_RECV)
{
    u8 nh = adv_rcvbuf(rcvhead);
    if (nh == rcvtail)
    {
        ++rcv_overflow;
        UDR0;
    }
    else
    {   
        rcvbuf[rcvhead] = UDR0;
        rcvhead = nh;
    }    
}	

/* called by task_dispatcher periodically.
 * returns 1 if work was done that may require
 * a reschedule */
u8 comms_task()
{
#ifdef PACKET_RECEIVE_SUPPORT
    u8 ret = 0;
    u8 code, payload_len;

#if 0
	while (UCSR0A & _BV(RXC0))
    {
        ret = 1;
        rx_notify(UDR0, 0);
    }
#else
    while (rcvtail != rcvhead)
    {
        rx_notify(rcvbuf[rcvtail], 0);
        rcvtail = adv_rcvbuf(rcvtail);
        ret = 1;
    }
#endif

    if (mailbox_head(&comms_taskinfo.mailbox, &code, &payload_len))
    {
        static u8 saved_code, saved_to;
        switch(code)
        {
            case COMMS_MSG_HEADER:
                if (payload_len == 2)
                {
                    mailbox_copy_payload(&comms_taskinfo.mailbox,
                            &saved_to, 1, 0);
                    mailbox_copy_payload(&comms_taskinfo.mailbox,
                            &saved_code, 1, 1);
                }
                break;
            case COMMS_MSG_PAYLOAD:
            {
                u8 msgbuf[MAX_BUFFERED_MSG_SIZE];
                u8 sz;
                sz = mailbox_copy_payload(&comms_taskinfo.mailbox,
                    msgbuf, MAX_BUFFERED_MSG_SIZE, 0);
                send_msg(saved_to, saved_code, sz, msgbuf, 0);
                break;
            }
        }
        mailbox_advance(&comms_taskinfo.mailbox);
    }
    
    return ret;
#else
    return 0;
#endif
}

#ifdef PACKET_RECEIVE_SUPPORT
u8 packet_drops_overflow;
u8 packet_drops_badcode;

void packet_received(msgaddr_t addr, u8 code, u8 length, u8 flags, u8 *payload)
{
    if (addr.to == NODE_ID_DISPAVR)
    {
        // note: from will be wrong
        send_msg(addr.to, code, length, payload, 0);
    }
    else
    {
        /* code is divided into two nibbles.
         * the first nibble indicates the task mailbox for the
         * message, and the second is determined by the task. */
        u8 taskid = code>>4;
        code &= 0xF;
        task_t *task = get_task_by_id(taskid);
        
        if (taskid == TASK_ID_COMMS)
        {

            if (code == COMMS_MSG_ECHO_REQUEST)
            {
                send_msg(addr.from, 
                        TASK_ID_COMMS<<4|COMMS_MSG_ECHO_REPLY, 
                        length, payload, 1);

#if 0
                if (length == 1)
                {
                    if (payload[0])
                    {
                        antenna_relay(1);
                    }
                    else
                    {
                        antenna_relay(0);
                    }
                }
#endif
            }
            else if (code == COMMS_MSG_RESET_BOARD)
            {
                wdt_enable(WDTO_15MS);
    
                /* Wait for the watchdog to reset the chip */
                while(1)
                    ;
            }
        }
        else if (task == NULL)
        {
            DEBUG("invalid message code");
            send_msg(addr.from, 
                    TASK_ID_COMMS<<4|COMMS_MSG_BADTASK, 
                    1, &taskid, 0);
            ++packet_drops_badcode;
        }
        else 
        {
         if (mailbox_deliver(&(task->mailbox), 
                            code & 0x0F, length, payload))
            {
                   DEBUG("mailbox full");
                ++packet_drops_overflow;
            }
        }
    }
    if(payload)
    {
        bufferpool_release(payload);
    }
}
#endif // PACKET_RECEIVE_SUPPORT

/* This should be changed to be interrupt-driven.
 * A simple Tx ring queue using the TX done interrupt
 * would suffice... */
void tx_enqueue_uart(u8 data)
{
    while (!(UCSR0A & _BV(UDRE0)))
        ;
    UDR0 = data;
}


static inline void tx_enqueue_spi(u8 cs, u8 byte)
{
    /* Sending only one byte at a time for simplicity.
     * Slave should not rely on /SS transitions for
     * framing. */
    spi_send(cs, 1, &byte);
}

void set_msg_dest(u8 to)
{
    if (to == NODE_ID_DISPAVR)
    {
        msg_dest = DEV_SPI_AVRDISP;
    }
    else
    {
        msg_dest = DEV_UART0;
    }
}

void tx_enqueue(u8 byte)
{
    if (msg_dest == DEV_SPI_AVRDISP)
    {
        tx_enqueue_spi(SLAVE_DISP_AVR, byte);
    }
    else
    {
        tx_enqueue_uart(byte);
    }
}






#if 1
int send_msg_buffered(u8 to, u8 code, u8 payload_len, u8 *payload, u8 do_crc)
{
    u8 flags;
    u8 header[2];
    u8 ret;
    header[0] = to;
    header[1] = code;

    /* Must disable interrupts to assure that both header and payload
     * arrive in comms mailbox sequentially. */
    ret = 1;
    flags = disable_interrupts();
    if (mailbox_deliver(&comms_taskinfo.mailbox, COMMS_MSG_HEADER, 2, (u8 *)header))
    {
    }
    else if (mailbox_deliver(&comms_taskinfo.mailbox, COMMS_MSG_PAYLOAD, payload_len, payload))
    {
    }
    else
    {
        ret = 0;
    }
    restore_flags(flags);
    return ret;
}
#endif

