/******************************************************************************
* File:              hwi2c.c
* Author:            Kevin Day
* Date:              January, 2009
* Description:       
*                    
*                    
* Copyright (c) 2009 Kevin Day
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
#include "timers.h"
#include "avrsys.h"
#include "bufferpool.h"
#include "hwi2c.h"

#define MAX_I2C_BYTES 8

typedef struct {
    u8 addr;    /* including read/write flag in lsb */
    u8 length;  /* bytes to read or write */
    u8 rspnode; /* node to send response to */
    u8 rspcode; /* message code for response */
    u8 data[MAX_I2C_BYTES]; /* data to write or buffer to read into */
} i2c_req_t;


static task_t i2c_taskinfo;
static u8     i2c_mailbox_buf[2*(sizeof(i2c_req_t)+2)];

static i2c_req_t creq;
static u8 i2cidx; /* rx/tx byte index */

typedef enum {
    I2C_START       = 0x08,
    I2C_REPSTART    = 0x10,
    I2C_SLA_W_ACK   = 0x18,
    I2C_SLA_W_NACK  = 0x20,
    I2C_WDATA_ACK   = 0x28,
    I2C_WDATA_NACK  = 0x30,
    I2C_ARB_LOST    = 0x38,
    I2C_SLA_R_ACK   = 0x40,
    I2C_SLA_R_NACK  = 0x48,
    I2C_RDATA_ACK   = 0x50,
    I2C_RDATA_NACK  = 0x58,
} twsr_status_t; /* low 2 bits treated as zero */

enum {
    ST_IDLE         = 0, 
    ST_START        = 1, 
    ST_START_WAIT   = 2, 
    ST_RD_ADDR_WAIT = 3,
    ST_WR_ADDR_WAIT = 4,
    ST_RD_DATA      = 5,
    ST_WR_DATA      = 6,
    ST_RD_DATA_WAIT = 7,
    ST_WR_DATA_WAIT = 8,
    ST_STOP         = 9,
    ST_COMPLETE     = 10,
    ST_ERROR        = 0x80 /* ORed with non-error state value */
} i2cstate;

static inline u8 is_write(u8 addr)
{
    return (addr&1);
}

void i2c_sm()
{
i2csm_top:
    switch(i2cstate)
    {
        case ST_START:
            i2cstate = ST_START_WAIT;
            TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);
            break;
        case ST_START_WAIT:
            if ((TWSR&0xF8) != I2C_START)
                goto i2csm_error;

            TWDR = creq.addr;
            if (is_write(creq.addr))
                i2cstate = ST_WR_ADDR_WAIT;
            else
                i2cstate = ST_RD_ADDR_WAIT;
            
            TWCR = (1<<TWINT)|(1<<TWEN);
            break;

        case ST_WR_ADDR_WAIT:
            if ((TWSR&0xF8) != I2C_SLA_W_ACK)
                goto i2csm_error;
            
            i2cstate = ST_WR_DATA;
            i2cidx   = 0;
            /* fall-through */

        case ST_WR_DATA:
            if (i2cidx >= creq.length)
            {
                /* done */
                i2cstate = ST_STOP;
                goto i2csm_top;
            }
            TWDR = creq.data[i2cidx];
            i2cstate = ST_WR_DATA_WAIT;
            TWCR = (1<<TWINT)|(1<<TWEN);
            break;
        case ST_WR_DATA_WAIT:
            if ((TWSR&0xF8) != I2C_RDATA_ACK)
                goto i2csm_error;
            i2cstate = ST_WR_DATA;
            ++i2cidx;
            goto i2csm_top;
        
        case ST_STOP:
            TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWSTO);
            i2cstate = ST_COMPLETE;
            break;

        case ST_RD_ADDR_WAIT:
            if ((TWSR&0xF8) != I2C_SLA_R_ACK)
                goto i2csm_error;
            
            i2cstate = ST_RD_DATA;
            i2cidx   = 0;
            /* fall-through */

        case ST_RD_DATA:
            if (i2cidx >= creq.length)
            {
                /* done */
                i2cstate = ST_STOP;
                goto i2csm_top;
            }
            i2cstate = ST_RD_DATA_WAIT;
            /* if this is the last byte to read, send NACK
             * after it is received.  ??? */
            if (i2cidx == creq.length - 1)
                TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWEA);
            else
                TWCR = (1<<TWINT)|(1<<TWEN);
            break;

        case ST_RD_DATA_WAIT:
            if ((TWSR&0xF8) != I2C_RDATA_ACK)
                goto i2csm_error;
            creq.data[i2cidx] = TWDR;
            ++i2cidx;
            i2cstate = ST_RD_DATA;
            goto i2csm_top;
        default:
            goto i2csm_error;
    }

    return;

i2csm_error:
    TWCR = (1<<TWINT); /* clear interrupt if any, disable TWI */
    i2cstate |= ST_ERROR;
}
            
SIGNAL(SIG_2WIRE_SERIAL)
{
    i2c_sm();
}

u8 i2c_task();

task_t *i2c_task_create()
{
    /* Enable internal pullups for SDA and SCL */
    DDRD  &= (~3); /* D0/D1 inputs (this is overriddedn with TWEN) */
    PORTD |= 3;    /* D0/D1 pulled up */

    TWSR = 0;  /* no prescaler */
    TWBR = 72; /* = 100kHz @ 16MHz core */
    return setup_task(&i2c_taskinfo, TASK_ID_I2C, i2c_task, 
                      i2c_mailbox_buf, sizeof(i2c_mailbox_buf));
}

u8 i2c_task()
{
    if (i2cstate == ST_COMPLETE)
    {
        if (creq.rspcode)
        {
            if (creq.rspnode == 0)
            {
                /* local */
                send_to_task(creq.rspcode>>4, creq.rspcode&0xF, 
                             creq.length, creq.data);
            }
            else
            {
                /* remote */
                send_msg(creq.rspnode, creq.rspcode, creq.length, 
                         creq.data, 0);
            }
        }
        i2cstate = ST_IDLE;
        return 1;        
    }
    else if (i2cstate & ST_ERROR)
    {
        u8 err[] = {i2cstate, TWSR, TWCR};
        send_msg(0, TASK_ID_I2C<<4|I2C_ERROR, sizeof(err), err, 0);
        i2cstate = ST_IDLE;
        return 1;
    }
    else if (i2cstate == ST_IDLE)
    {
        u8 code, payload_len;
        if (mailbox_head(&i2c_taskinfo.mailbox, &code, &payload_len))
        {
            switch(code)
            {
                case I2C_READ:
                case I2C_WRITE:
                {
                    /* Payload format:
                     *   Slave address (shifted, lsb=1 for read, 0 for write)
                     *   Number of bytes to read/write (max 8)
                     *   destination node to send response to (0=local)
                     *   destination message code for response (0=none)
                     *   0-8 bytes to write (if write)
                     */
                    mailbox_copy_payload(&i2c_taskinfo.mailbox, &creq.addr, 
                                         payload_len, 0);
                    i2cstate = ST_START;
                    i2c_sm();
                    break;
                }
                default:
                    break;
            }
            mailbox_advance(&i2c_taskinfo.mailbox);
            return 1;
        }
    }
    return 0;
}
    

