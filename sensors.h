/******************************************************************************
* File:              sensors.h
* Author:            Kevin Day
* Date:              March, 2009
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

#ifndef SENSORS_H 
#define SENSORS_H

#include "types.h"
#include "tasks.h"
#include "timers.h"
#include "adc.h"

extern u16 oil_pressure;
extern u16 coolant_pressure;
extern u16 oil_temperature;

#define OILP 0
#define CLTP 1
#define OILT 2
/* 1<<OILP set when sensor value changes 
 * there is a read-modify-write in the sensors ADC callback which is interrupt
 * context.  so disable interrupts when modifying this. */
extern u8 sensor_update;


task_t *sensors_task_create();

u8 sensors_init(adc_context_t *adc_context, u8 num_adc);

#endif

