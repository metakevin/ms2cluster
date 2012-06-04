/******************************************************************************
* File:              avrms2.c
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

#include <stdio.h>
#include <stddef.h>
#include "platform.h"
#include "comms_generic.h"
#include "tasks.h"
#include "avrsys.h"
#include "timers.h"
#include "bufferpool.h"
#include "avrcan.h"
#include "hud.h"
#include "miscgpio.h"

/******************************************************************************
* Copied from ms2_extra.h
*******************************************************************************/

// rs232 Outputs to pc
typedef struct {
unsigned int seconds,pw1,pw2,rpm;           // pw in usec
int adv_deg;                                // adv in deg x 10
unsigned char squirt,engine,afrtgt1,afrtgt2;    // afrtgt in afr x 10
/*
; Squirt Event Scheduling Variables - bit fields for "squirt" variable above
inj1:    equ    0       ; 0 = no squirt; 1 = inj squirting
inj2:    equ    1

; Engine Operating/Status variables - bit fields for "engine" variable above
ready:  equ     0       ; 0 = engine not ready; 1 = ready to run
                                               (fuel pump on or ign plse)
crank:  equ     1       ; 0 = engine not cranking; 1 = engine cranking
startw: equ     2       ; 0 = not in startup warmup; 1 = in startw enrichment
warmup: equ     3       ; 0 = not in warmup; 1 = in warmup
tpsaen: equ     4       ; 0 = not in TPS acceleration mode; 1 = TPS acceleration mode
tpsden: equ     5       ; 0 = not in deacceleration mode; 1 = in deacceleration mode
mapaen: equ     6
mapden: equ     7
*/

unsigned char wbo2_en1,wbo2_en2; // from wbo2 - indicates whether wb afr valid
int baro,map,mat,clt,tps,batt,ego1,ego2,knock,   // baro - kpa x 10
                                                 // map - kpa x 10
                                                 // mat, clt deg(C/F)x 10
                                                 // tps - % x 10
                                                 // batt - vlts x 10
                                                 // ego1,2 - afr x 10
                                                 // knock - volts x 100
 egocor1,egocor2,aircor,warmcor,                 // all in %
 tpsaccel,tpsfuelcut,barocor,gammae,             // tpsaccel - acc enrich(.1 ms units)
                                                 // tpsfuelcut - %
                                                 // barocor,gammae - %
 vecurr1,vecurr2,iacstep,cold_adv_deg,           // vecurr - %
                                                 // iacstep - steps
                                                 // cold_adv_deg - deg x 10
tpsdot,mapdot;                                   // tps, map rate of change - %x10/.1 sec,
                                                 // kPax10 / .1 sec
unsigned int coil_dur;                           // msx10 coil chge set by ecu
int maf, fuelload,                                    // maf for future; kpa (=map or tps)
fuelcor;                                         // fuel composition correction - %
unsigned char port_status,                       // Bits indicating spare port status.
  knk_rtd;               // amount of ign retard (degx10) subtracted from normal advance.
unsigned int EAEfcor1;
int egoV1,egoV2;                                                         // ego sensor readbacks in Vx100
unsigned char status1, status2, status3, status4;   // added ms2extra
unsigned int looptime;
unsigned int istatus5;
unsigned int tpsadc; //lets do a real calibration
int fuelload2;
int ignload;
int ignload2;
//int spare[5]; - deleted (base.ini updated)
unsigned char synccnt;
char timing_err;
unsigned long dt3;                               // delta t bet. rpm pulses (us)
unsigned long wallfuel1;
unsigned int gpioadc[8]; // capture values of gpioadc ports
unsigned int gpiopwmin[4];
unsigned char gpioport[3]; // capture values from gpio digi ports
unsigned int adc6, adc7; // capture values of adc6,7
unsigned long wallfuel2;
unsigned int EAEfcor2;
unsigned char boostduty;
unsigned char syncreason;
/* this variable will hold reasons that trigger wheels lost sync
e.g.
0 = no problem
1 = init error
2 = missing tooth at wrong time
3 = too many teeth before missing tooth (last)
4 = too few teeth before missing tooth (last)
5 = 1st tooth failed test
6 = nonsense input (last)
7 = nonsense input (mid)
8 = too many teeth before missing tooth (mid)
9 = too few teeth before missing tooth (mid)
10 = too many teeth before end of sequence
11 = too few teeth before second trigger
12 = too many sync errors
13 = dizzy wrong edge
14 = trigger return vane size
15 = EDIS
16 = EDIS

space for more common reasons
plus other special reasons for the custom wheels
20 = subaru 6/7 tooth 6 error
21 = subaru 6/7 tooth 3 error
22 = Rover #2 missing tooth error
23 = 420A long tooth not found
24 = 420A cam phase wrong
25 = 420A 
26 = 420A
27 = 420A
28 = 36-1+1
29 = 36-2-2-2
30 = 36-2-2-2
31 = Miata 99-00 - 2 cams not seen
32 = Miata 99-00 - 0 cams seen
33 = 6G72 - tooth 2 error
34 = 6G72 - tooth 4 error
35 = Weber-Marelli
36 = CAS 4/1
37 = 4G63
38 = 4G63
39 = 4G63
40 = Twin trigger
41 = Twin trigger
42 = Chrysler 2.2/2.5
43 = Renix
44 = Suzuki Swift
45 = Vitara
46 = Vitara
47 = Daihatsu 3
48 = Daihatsu 4
49 = VTR1000
50 = Rover #3
51 = GM 7X
*/
unsigned int user0; // spare value for the 'user defined'
} outpc;

/******************************************************************************
* End 'outpc' structure from ms2_extra.h
* Note: only used for 'offsetof'; not instantiated.
*******************************************************************************/
#define OP_BLK 7
#define OP_OFS(v) offsetof(outpc, v)

static const u8 ms2_can_id = 0;
static const u8 my_can_id  = 3;
#define    MSG_CMD    0
#define    MSG_REQ    1
#define    MSG_RSP    2
#define    MSG_XSUB    3
#define    MSG_BURN    4

u8 ms2_send_cmd(u8 var_blk, u16 var_offset,
                u8 *buf, u8 length)
{
    // attempt to catch stray writes
    if (var_offset < OP_OFS(gpioadc))
    {
	u16 dbg[] = {var_offset, (u16) buf};
	ms2_send_cmd(OP_BLK, OP_OFS(gpiopwmin), (u8 *)dbg, sizeof(dbg));
	return 0;
    }
    
    u8 msg[4];
    u8 msg_type = MSG_CMD;
    u8 src_id   = my_can_id;
    u8 dst_id   = ms2_can_id;

    msg[0] = var_offset>>3;
    msg[1] = ((var_offset<<5) | ((msg_type&7)<<2) | ((src_id>>2)&3));
    msg[2] = ((src_id<<6) | ((dst_id&0xF)<<2) | ((var_blk>>2)&3));
    msg[3] = var_blk<<6;
    
    return send_can_msg(msg, buf, length);
}

/* returns 0 on success */
u8 ms2_send_req(u8 var_blk, u16 var_offset, 
                  void *localbuf, u8 length)
{
    u8 msg[7];
    u8 msg_type = MSG_REQ;
    u8 src_id   = my_can_id;
    u8 dst_id   = ms2_can_id;
    msg[0] = var_offset>>3;
    msg[1] = ((var_offset<<5) | ((msg_type&7)<<2) | ((src_id>>2)&3));
    msg[2] = ((src_id<<6) | ((dst_id&0xF)<<2) | ((var_blk>>2)&3));
    msg[3] = var_blk<<6;
    msg[4] = (u16)localbuf>>8;  /* dst var blk */
    msg[5] = (u16)localbuf&0xFF; /* upper 8 bits of dst var off */
    msg[6] = length;             /* upper 3 bits treated as var off in ms2;
                                    zero here. */
    return send_can_msg(msg, msg+4, 3);
}

struct ms2_var_s;
typedef void (*ms2render_t)(struct ms2_var_s *mv, char *buf);

typedef struct ms2_var_s {
    u16  is16  : 1;
    u16  msblk : 4;
    u16  msoff : 11;
    s16  value;
    u8   dwidth;
    const char *fmt;
    //const char *name;
    ms2render_t render;
    enum {VAR_NONE, VAR_TACH, VAR_CLT, VAR_MAP} var;
} ms2_var_t;

static void ms2render_fmt(ms2_var_t *mv, char *buf)
{
    /* note: assume buf is at least 9 characters */
    snprintf(buf, mv->dwidth+1, mv->fmt, mv->value);
}
static void ms2render_batt(ms2_var_t *mv, char *buf)
{
    /* allow for diode drop */
    u16 v = mv->value + 6;
    snprintf(buf, mv->dwidth+1, "Bat:%02u.%u", v/10, v%10);
}
static void ms2render_x10(ms2_var_t *mv, char *buf)
{
    snprintf(buf, mv->dwidth+1, mv->fmt, mv->value/10, mv->value%10);
}
static void ms2render_time(ms2_var_t *mv, char *buf)
{
    snprintf(buf, mv->dwidth+1, mv->fmt, mv->value/3600, (mv->value/60)%60, mv->value%60);
}

/* NOTE: some globals will not be updated if the relevant data is not displayed.
 * This is not ideal... */
u8 engine;
static void ms2render_eng(ms2_var_t *mv, char *buf)
{
    // MSB is 'engine', LSB is afr target (afr x 10)
    u8 e = mv->value>>8;
    u8 afrtgt = (mv->value&0xFF);
    u8 ready = (e&1);
    u8 crank = (e&2);
    u8 startw = (e&4);
    u8 wu     = (e&8);
    u8 decel  = (e&(16|32));
 
    engine = e;

    sprintf(buf, "%c%c%c%c%c%c%c%c", 
	    startw   ?'S':' ',
	    wu       ?'W':' ',
	    decel    ?'D':' ',
	    hfan     ?'H':' ',
	    acreq    ?'A':' ',
	    clutchin ?'C':' ',
	    crank    ?'!':' ',
	    ready    ?' ':'X');
}

ms2_var_t ms2vars[] = {
  {1, OP_BLK, OP_OFS(engine),  0, 8, "",                ms2render_eng, VAR_NONE},
  //{1, OP_BLK, OP_OFS(egoV1),   0, 8, "EgoV %3u",       "egoVx100", ms2render_fmt, VAR_NONE},
  //{1, OP_BLK, OP_OFS(pw1),     0, 8, "PW1%5u",         "inj PW 1", ms2render_fmt,  VAR_NONE},
//  {1, OP_BLK, OP_OFS(vecurr1), 0, 8, "VE%% %2d.%d",         "VE1 cur", ms2render_x10,  VAR_NONE},
  {1, OP_BLK, OP_OFS(ego1),    0, 8, "AFR%3u.%u",      ms2render_x10,  VAR_NONE},
//  {1, OP_BLK, OP_OFS(adv_deg), 0, 8, "%2u.%u ADV",      "ign adv",  ms2render_x10,  VAR_NONE},
  {1, OP_BLK, OP_OFS(mat),     0, 8, "%3dF IAT",       ms2render_x10,  VAR_NONE},
  {1, OP_BLK, OP_OFS(clt),     0, 8, "C T %3dF",       ms2render_x10,  VAR_CLT},
  ////{1, OP_BLK, OP_OFS(user0),     0, 8, "USR %04X",       ms2render_fmt,  VAR_NONE},
//  {1, OP_BLK, OP_OFS(synccnt), 0, 8, "Sync%4d",        "Sync Cnt", ms2render_fmt, VAR_NONE},
  {1, OP_BLK, OP_OFS(rpm),     0, 8, "RPM%5u",         ms2render_fmt,  VAR_TACH},  
  {1, OP_BLK, OP_OFS(map),     0, 8, "MAP%3d.%u",      ms2render_x10,  VAR_MAP}, 
//  {1, OP_BLK, OP_OFS(batt),    0, 8, "",               "BatVolts", ms2render_batt, VAR_NONE},
//  {1, OP_BLK, OP_OFS(seconds), 0, 8, "%02d:%02d:%02d", "uptime",   ms2render_time, VAR_NONE},
//  {1, OP_BLK, OP_OFS(tps),     0, 8, "TPS%3u.%u",      ms2render_x10,  VAR_NONE},
//  {1, OP_BLK, OP_OFS(mapdot),  0, 8, "M.%4d.%u",       "MAPdot",   ms2render_x10, VAR_NONE},
};
#define NUM_MS2VARS (sizeof(ms2vars)/sizeof(ms2vars[0]))

u16 shadow_user0;
void send_user0(u16 u)
{
    u16 uswap = ((u<<8) | (u>>8));
    ms2_send_cmd(OP_BLK, OP_OFS(user0), (u8 *)&uswap, sizeof(u16));
    shadow_user0 = u;
}

u16 shadow_user0;
void set_user0_bit(u8 bit, u8 val)
{
    if (val)
    {
        shadow_user0 |= (1<<bit);
    }
    else
    {
        shadow_user0 &= ~(1<<bit);
    }
    send_user0(shadow_user0);
}

void send_egt(u16 val)
{
    u16 uswap = ((val<<8) | (val>>8));
    ms2_send_cmd(OP_BLK, OP_OFS(gpioadc[0]), (u8 *)&uswap, sizeof(u16));

#if 0
    // low 3 bits are junk; resolution is in 1/4 degree C
    val >>= 3; 
    val *= 9;
    val /= 5;
    val += 32;
    ms2_send_cmd(OP_BLK, OP_OFS(adc6), (u8 *)&uswap, sizeof(u16));
#endif
}

void send_stack(u16 val)
{
    u16 uswap = ((val<<8) | (val>>8));
    ms2_send_cmd(OP_BLK, OP_OFS(gpiopwmin[2]), (u8 *)&uswap, sizeof(uswap));
}
	

void send_oilp(u16 val)
{
    u16 uswap = ((val<<8) | (val>>8));
    ms2_send_cmd(OP_BLK, OP_OFS(gpioadc[1]), (u8 *)&uswap, sizeof(u16));
}
void send_oilt(u16 val)
{
    u16 uswap = ((val<<8) | (val>>8));
    ms2_send_cmd(OP_BLK, OP_OFS(gpioadc[2]), (u8 *)&uswap, sizeof(u16));
}

void send_clp(u16 val)
{
    u16 uswap = ((val<<8) | (val>>8));
    ms2_send_cmd(OP_BLK, OP_OFS(gpioadc[3]), (u8 *)&uswap, sizeof(u16));
}
void send_knots(u16 val)
{
    u16 uswap = ((val<<8) | (val>>8));
    ms2_send_cmd(OP_BLK, OP_OFS(gpioadc[4]), (u8 *)&uswap, sizeof(u16));
}


static u8 next_req;
void request_ms2_data()
{
    u8 stop_req = next_req;
    do
    {
        ms2_var_t *mv = &ms2vars[next_req];
        u8 r = ms2_send_req(mv->msblk, mv->msoff, mv, mv->is16?2:1);
//        u8 r = ms2_send_req(11, 0, mv, 1);
        if (r)
            break;
        ++next_req;
        if (next_req >= NUM_MS2VARS)
            next_req = 0;
    } while (next_req != stop_req);
}
void ms2_data_updated(ms2_var_t *mv);

void ms2_can_rx(u8 *msg, u8 length)
{
    u16 var_offset = (msg[0]<<3) | (msg[1]>>5);
    u8  msg_type   = ((msg[1]>>2)&7);
    u8  src_id     = (((msg[1]<<2)&0xF)| (msg[2]>>6));
    u8  dst_id     = ((msg[2]>>2)&0xF);
    u8  var_blk    = (((msg[2]<<2)&0xF)|(msg[3]>>6));
    
    ms2_var_t *mv = (ms2_var_t *)((var_blk<<8) | (var_offset>>3));

    if (msg_type == MSG_RSP &&
        dst_id == my_can_id && src_id == ms2_can_id &&
        mv >= &ms2vars[0] &&
        mv <  &ms2vars[NUM_MS2VARS])
    {
        /* note: no length check */
        if (mv->is16)
        {
            /* byte swap 16 bit word */
            mv->value = (msg[5] | (msg[4]<<8));
        }
        else
        {
            mv->value = msg[4];
        }
        ms2_data_updated(mv);
    }
    else
    {
        u16 p[] = {(u16)mv};
        send_msg(2,
                 TASK_ID_CAN<<4|CAN_RX_ERROR,
                 sizeof(p), (u8 *)p, 1);
    }
}

void ms2draw(u8 *dbuf, u8 update_only, u16 arg)
{
    ms2_var_t *vd = &ms2vars[arg];

    char buf[9];
    vd->render(vd, buf);
    render_string(dbuf, 0, vd->dwidth, buf);
}

void register_ms2_areas()
{
    u8 i; 
    /* note: skipping first 8 chars for fuel gauge 
     * this needs to be improved... */
    u8 w = 8;
    /* skip more for GPS for now */
    w += 8;
    for(i=0; i<8; i++)
    {
    if (i >= sizeof(ms2vars)/sizeof(ms2vars[0]))
        break;
        register_display_area(ms2draw, i, w, ms2vars[i].dwidth);
        w += ms2vars[i].dwidth;
    if (w == 56)
    {
        /* leave room for speed */
        w += 8;
    }
    }
}

timerentry_t ms2_timer;
void ms2_callback(timerentry_t *t)
{
    request_ms2_data();
    
    register_timer_callback(&ms2_timer, MS_TO_TICK(100), ms2_callback, 0);
}

void init_avrms2()
{
    register_ms2_areas();

    ms2_callback(NULL);
}

u16 g_rpm;
u16 g_mapx10;
u16 g_cltx10;

void ms2_data_updated(ms2_var_t *mv)
{
    /* set LEDs based on current values */
    if (mv->var == VAR_TACH)
    {
        g_rpm = mv->value;
        if (mv->value > 5000)
        {
            hud_set_led(LED_TOP_LEFT, LED_RED, LED_BLINK_NONE);
            if (mv->value > 5500)
            {
                hud_set_led(LED_TOP_MIDDLE, LED_RED, LED_BLINK_NONE);
                if (mv->value > 6000)
                {
                    hud_set_led(LED_TOP_RIGHT, LED_RED, LED_BLINK_NONE);

                    if (mv->value > 6500)
                    {
                        hud_set_led(LED_LEFT_SIDE, LED_BLUE, LED_BLINK_NONE);
                        hud_set_led(LED_RIGHT_SIDE, LED_BLUE, LED_BLINK_NONE);
                    }
                    else
                    {
                        hud_set_led(LED_LEFT_SIDE, LED_OFF, LED_BLINK_NONE);
                        hud_set_led(LED_RIGHT_SIDE, LED_OFF, LED_BLINK_NONE);
                    }
                        
                }
                else
                {
                    hud_set_led(LED_TOP_RIGHT, LED_OFF, LED_BLINK_NONE);
                }
            }
            else
            {
                hud_set_led(LED_TOP_MIDDLE, LED_OFF, LED_BLINK_NONE);
            }
        }
        else
        {
            hud_set_led(LED_TOP_LEFT,   LED_OFF, LED_BLINK_NONE);
            hud_set_led(LED_TOP_MIDDLE, LED_OFF, LED_BLINK_NONE);
            hud_set_led(LED_TOP_RIGHT,  LED_OFF, LED_BLINK_NONE);
            hud_set_led(LED_LEFT_SIDE,  LED_OFF, LED_BLINK_NONE);
            hud_set_led(LED_RIGHT_SIDE, LED_OFF, LED_BLINK_NONE);
        }
    }
    else if (mv->var == VAR_CLT)
    {
        if (mv->value < 1700)
        {
            hud_set_led(LED_BOTTOM_LEFT, LED_ORANGE, LED_BLINK_NONE);
        }
        else if (mv->value > 2200)
        {
            hud_set_led(LED_BOTTOM_LEFT, LED_RED, LED_BLINK_FAST);
        }
        else
        {
            hud_set_led(LED_BOTTOM_LEFT, LED_GREEN, LED_BLINK_NONE);
        }
        g_cltx10 = mv->value;
    }
    else if (mv->var == VAR_MAP)
    {
        g_mapx10 = mv->value;
    }
}

    
