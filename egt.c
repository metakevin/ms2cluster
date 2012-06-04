/******************************************************************************
* File:              egt.c
* Author:            Kevin Day
* Date:              December, 2005
* Description:       
*                    
*                    
* Copyright (c) 2005 Kevin Day
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

#include "timers.h"
#include "persist.h"
#include "userinterface.h"
#include "egt.h"
#include "output.h"
#include "onewire.h"
#include "comms_generic.h"
#include "datalogger.h"

typedef struct {
    u8 cold_junction_temp_msb_lsb[2];
    u8 thermocouple_volts_msb_lsb[2];
} thermocouple_raw_data_t;

void egt_display_func(ui_mode_t mode, u8 mode_just_changed);
u8   egt_config_func(ui_mode_t mode, ui_config_event_t event);
void egt_update_callback(timerentry_t *ctx);
u8   ow_2760_read_thermocouple(thermocouple_raw_data_t *raw_ret);
s16  convert_thermocouple_volts_to_temp(thermocouple_raw_data_t *tc);

#define EGT_UPDATE_PERIOD MS_TO_TICK(100)
#define EGT_PEAK_AGE 100     /* 10 seconds */

    
timerentry_t            egt_update_timer;
thermocouple_raw_data_t tc_data;
u8                      tc_status;
u8                      tc_units_fahrenheit;
u8                      egt_peak_counter;
s16                     PeakTempC;

u8                      tc_module_blink_counter;

void egt_init()
{
    tc_units_fahrenheit = load_persist_data(PDATA_EGT_UNITS);
    
    register_display_mode(MODE_EGT_INSTANT, egt_display_func,
            egt_config_func);
    register_display_mode(MODE_EGT_PEAK, egt_display_func,
        egt_config_func);

    egt_update_callback(0);
}


void egt_update_callback(timerentry_t *ctx)
{
    tc_status = ow_2760_read_thermocouple(&tc_data);

    if (tc_status == 0)
    {
        /* This could be compressed significantly! */
        log_data_sample(DATA_TYPE_EGT_RAW, 4, &tc_data);
    }
    
    if (egt_peak_counter < EGT_PEAK_AGE)
    {
        ++egt_peak_counter;
    }

    if (tc_module_blink_counter < 32)
    {
        ++tc_module_blink_counter;

        u8 on = (tc_module_blink_counter&3)?0:0xFF;
        ow_2760_write_reg(8, on);
    }

    register_timer_callback(&egt_update_timer, EGT_UPDATE_PERIOD, 
            egt_update_callback, 0);
}

    
void egt_display_func(ui_mode_t mode, u8 mode_just_changed)
{
    s16 TempC;
    temp_format_mode_t dmode = TEMP_RANGE_1000;

    if (tc_status == 0)
    {
        TempC = convert_thermocouple_volts_to_temp(&tc_data);

#if 0
        send_msg(BROADCAST_NODE_ID, 0xCA, 4, &tc_data, 0); 
        send_msg(BROADCAST_NODE_ID, 0xCB, 2, &TempC, 0); 
#endif   
        if (mode == MODE_EGT_PEAK)
        {            
            if (TempC > PeakTempC)
            {
                PeakTempC = TempC;
                egt_peak_counter = 0;
            }
            
            if (egt_peak_counter < EGT_PEAK_AGE)
            {                
                dmode |= TEMP_PEAK_HOLD;
                TempC = PeakTempC;
            }
        }
    }
    else
    {
        TempC = tc_status;
        dmode |= TEMP_SHOW_ERROR;
    }

    if ((s8)tc_data.thermocouple_volts_msb_lsb[0] < 0 || tc_status != 0)
    {
        /* Restart blinking */
        if (tc_module_blink_counter >= 32)
        {
            tc_module_blink_counter = 0;
        }
    }
    if (tc_units_fahrenheit)
    {
        dmode |= TEMP_FAHRENHEIT;
    }

    output_temperature(mode, TempC, dmode);
}
    
u8  egt_config_func(ui_mode_t mode, ui_config_event_t event)
{
    if (event == CONFIG_SHORT_PRESS)
    {
        tc_units_fahrenheit = 
            !tc_units_fahrenheit;
        save_persist_data(PDATA_IAT_UNITS, tc_units_fahrenheit);
    }
    else if (event == CONFIG_LONG_PRESS)
    {
        /* done */
        return 0;
    }

    egt_display_func(mode, 0);

    /* not done */
    return 1;
}

u8 ow_2760_read_thermocouple(thermocouple_raw_data_t *raw_ret)
{
    u8 status;
    
    status = ow_2760_read_reg(OW_2760_REG_TEMP_MSB, raw_ret->cold_junction_temp_msb_lsb, 2);
    status <<= 2;
    status = ow_2760_read_reg(OW_2760_REG_CURRENT_MSB, raw_ret->thermocouple_volts_msb_lsb, 2);
    return status;
}
    
/* First degree polynomial for type K thermocouples:
 *  temp = 5 + mvolts*24
 *
 *  This is accurate to +/- 5 degrees celsius from 0-1000C
 *
 *  The voltage read from the DS2760 is in units of 15.625 microvolts per LSB.
 *  The 2760 averages 128 samples and the averaged value is available every 88ms.
 *  
 *  In this simplified compensation scheme, the cold junction temperature is 
 *  added to the computed thermocouple temperature after the thermocouple is
 *  linearized.  For maximim accuracy it should be done by converting the
 *  CJ temp to a thermocouple voltage and adjusting the TC voltage before 
 *  the linearization.  But this is good enough.  The TDS-2 should use 
 *  the 8th degree poly and the better CJ compensation using the raw data
 *  which is logged instead of the computed temperature.
 *
 *  Since the DS2760 reads in units of 1/64th of a millivolt, the
 *  actual calculation used is:
 *
 *  TempC = 5 + 3*ds2760_val/8
 */
#define THERMOCOUPLE_OFFSET 5

/******************************************************************************
* convert_thermocouple_volts_to_temp
*        This returns a positive temperature in degrees celsius.
*        Negative temperatures are returned as zero.
*        Cold junction compensation is performed.
*******************************************************************************/
s16 convert_thermocouple_volts_to_temp(thermocouple_raw_data_t *tc)
{
    s16 TempC;    
    u8 msb = tc->thermocouple_volts_msb_lsb[0];
    
    if (msb&0x80)
    {
        /* Thermocouple is colder than "cold" junction */
        TempC = 0;
    }
    else
    {
        TempC = (msb<<5) + (tc->thermocouple_volts_msb_lsb[1]>>3);

        //TempC = TempC + TempC + TempC;
        TempC *= 3;     /* Probably already using 16 bit multiply, right? */
        TempC >>= 3;
        TempC += THERMOCOUPLE_OFFSET;
    }

    
    /* Cold junction comp.
     * MSB of temperature register is in degrees C */
    TempC += (s8)tc->cold_junction_temp_msb_lsb[0];

    return TempC;
}

