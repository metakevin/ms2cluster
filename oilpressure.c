/******************************************************************************
* File:              oilpres.c
* Author:            Kevin Day
* Date:              November, 2006
* Description:       
*                    
*                    
* Copyright (c) 2006 Kevin Day
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

#include <avr/io.h>
#include "types.h"
#include "timers.h"
#include "adc.h"
#include "userinterface.h"
#include "comms_generic.h"
#include "audiradio.h"
#include "persist.h"
#include "output.h"

#define OILPRES_PIN 2

void oilpres_display_func(ui_mode_t mode, u8 mode_changed);
u8 oilpres_config_func(ui_mode_t mode, ui_config_event_t event);
void oilpres_adc_callback(adc_context_t *ctx);
void oilpres_update_callback(timerentry_t *ctx);
timerentry_t oilpres_timer;

#define SAMPLES_PER_AVG 256
#define ACCUMULATOR_Q   8

#define SAMPLES_PER_AVG_INSTANT 64
#define ACCUMULATOR_Q_INSTANT   6
#define SAMPLES_PER_AVG_PEAK    64
#define ACCUMULATOR_Q_PEAK      6

#define OILPRES_UPDATE_PERIOD   MS_TO_TICK(250)
#define OILPRES_PEAK_AGE        4*5 /* 5 seconds */

#define BLINK_QUARTER_MASK  2
#define BLINK_WRAP 8    // 1000ms -- see display_update_timer

typedef struct {
    u32 sample_accumulator;
    u32 current_oilpres_accum;
    u16 sample_counter;
    u8  oilpres_accum_valid;
    u8  units_nonmetric;
} oilpres_context_t;

oilpres_context_t oilpres_ctx;

void oilpres_init(adc_context_t *adc_context)
{
    adc_context->pin      = OILPRES_PIN;
    //adc_context->ref      = ADC_INT_256;
    adc_context->ref      = ADC_AVCC;
    adc_context->enabled  = 1;
    adc_context->callback = oilpres_adc_callback;
    
    oilpres_ctx.units_nonmetric = load_persist_data(PDATA_OILPRES_UNITS);
    
    register_display_mode(MODE_OILPRES, oilpres_display_func, oilpres_config_func);   
}


void oilpres_adc_callback(adc_context_t *ctx)
{
    oilpres_ctx.sample_accumulator += ctx->sample;
    ++oilpres_ctx.sample_counter;
    if (oilpres_ctx.sample_counter == SAMPLES_PER_AVG_INSTANT)
    {
        oilpres_ctx.sample_counter = 0;
        oilpres_ctx.current_oilpres_accum = oilpres_ctx.sample_accumulator;
        oilpres_ctx.oilpres_accum_valid = 1;
        oilpres_ctx.sample_accumulator = 0;
    }
}


void oilpres_display_func(ui_mode_t mode, u8 mode_changed)
{
    const u16 r1 = 390; // value of resistor to +5
    const u16 r2_fixed = 90; // value of fixed resistor added to sensor resistance
                             // actually measured 96.5, but ADC reports 90.

    u16 adc_10q6 = oilpres_ctx.current_oilpres_accum;
 
#if 1
    u16 Vratio_10q6 = adc_10q6 / 1023;
    
    u16 r2 = (Vratio_10q6 * r1)/(64 - Vratio_10q6);
#else
    // underflow ?
    u16 r2 = (adc_10q6 * r1)/(64*1023 - adc_10q6);
#endif

    
//    r2_10q6 -= r2_fixed_10q6;
    
    if (r2 > r2_fixed)
    {
        r2 -= r2_fixed;
    }
    else
    {
        r2 = 0;
    }

    if (r2 > 999)
    {
        /* output_number uses signed int, so limit this */
        r2 = 999;
    }
    else
    {
        r2 = r2 * 10;
    }
    
    output_number(mode, r2, CENTER_D, 0);
}

u8 oilpres_config_func(ui_mode_t mode, ui_config_event_t event)
{
    if (event == CONFIG_SHORT_PRESS)
    {
        oilpres_ctx.units_nonmetric = 
            !oilpres_ctx.units_nonmetric;
        save_persist_data(PDATA_OILPRES_UNITS, oilpres_ctx.units_nonmetric);
    }
    else if (event == CONFIG_LONG_PRESS)
    {
        /* done */
        return 0;
    }

    oilpres_display_func(mode, 0);

    /* not done */
    return 1;   
}
    
