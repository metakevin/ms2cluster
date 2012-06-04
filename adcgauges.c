/******************************************************************************
* File:              adcgauges.c
* Author:            Kevin Day
* Date:              February, 2009
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

#include <stdio.h>
#include <stddef.h>
#include "platform.h"
#include "comms_generic.h"
#include "tasks.h"
#include "avrsys.h"
#include "timers.h"
#include "bufferpool.h"
#include "hud.h"
#include "adc.h"


static u16 fgauge_accum;
static u8 fgauge_count;
static u16 fgauge_10q6;
/* Since the fuel gauge ADC range is within the lower 1/4 of the scale,
 * sample 256 times into a 16 bit accumulator.  the accumulator
 * is "virtually" 18 bits wide, with the lower 8 bits being fractional.
 * The upper two bits are always zero if the ADC never goes above 1/4
 * scale (1023/4*64=65472).  */
void fuel_gauge_adc_callback(adc_context_t *ctx)
{
    fgauge_accum += ctx->sample; // 10 bits
    ++fgauge_count;
    if (fgauge_count == 64)
    {
        fgauge_10q6 = fgauge_accum;
        fgauge_accum = 0;
    }
}

void fuelgauge_draw(u8 *dbuf, u8 update_only, u16 arg)
{
    char buf[9];

    /* fgauge is the smoothed ADC value for the fuel gauge sender
     * voltage divider, with 470 ohms to +5v and 3-110 ohms to
     * ground (3 ohms = full tank of 11.5 gallons) 
     *
     * ADC values (approx):
     *  6   - full
     *  194 - empty
     *
     *
     * or gallons remaining = 11.5 - (adc-6) / 16.35
     *
     */

/*
 *gas gauge at 205 miles: ~170 when engine running w/ headlights (150-175)
with engine off at 205 miles: 65 
filled up with 8.9 gal at 206 miles; engine off reads 0 (11.5g remaining; correct); engine on reads 200.  Briefly read ~10, then went wacky.  Declined to 185 after a few minutes.
at 212 miles, 186 with engine and parking lights on, 144 with pl off, 9-10 with engine and pl off, 15 with pl on eng off.
*/


    u8 fgauge = ((fgauge_10q6+32)>>6); // discard extra precision
    u16 fgauge_clamp;
    if (fgauge < 6)
    {
        fgauge_clamp = 0;
    }
    else
    {
        fgauge_clamp = fgauge - 6;
    }
    if (fgauge_clamp > 188)
    {
        fgauge_clamp = 188;
    }

    static u32 fgauge_acc;
    static u16 acc_cnt;
    fgauge_acc += fgauge_clamp;
    ++acc_cnt;
    if (acc_cnt == 0)
    {
        /* once delay line fills up, use the
         * LPF'd version 
         * (period is somewhere in the 10s of seconds) */
        fgauge_clamp = fgauge_acc>>16;
        fgauge_acc = 0;
    }


    /* Convert gauge reading to gallons left.
     * This is unlikely to be a linear relationship.
     * Will need to record the fgauge value at various
     * fillups and construct a curve.  Linear for now. */

    /* 1046 = 16.35*64 */
    u16 gleft = ((188-fgauge_clamp)<<8) / 1046;
    /* gleft/4 is gallons.  gleft&3 is 0/4, 1/4, 2/4, 3/4 */

    /* font characters 3 - 6 are 0/4-3/4 */
    snprintf(buf, 9, "%2d%cG %03u", 
             gleft>>2,
             3+(gleft&3),
             fgauge);

    render_string(dbuf, 0, 8, buf);    
}




#define FUELSENDER_PIN 3
#define OILPRESSURE_PIN 4
void fuel_gauge_init(adc_context_t *adc_context)
{
    adc_context->pin      = FUELSENDER_PIN;
    adc_context->ref      = ADC_AVCC;
    adc_context->enabled  = 1;
    adc_context->callback = fuel_gauge_adc_callback;

    register_display_area(fuelgauge_draw, 0, 0, 8);
    
}

    
