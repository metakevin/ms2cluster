/******************************************************************************
* File:              hud.h
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

#ifndef HUD_H
#define HUD_H

#include "tasks.h"

#define CMD_ENTER_BOOTLOADER 1
#define HUD_EVT_USER_INPUT 2

typedef struct {
    u8 start;
    u8 len;
} hud_area_t;

task_t *hud_task_create();

void render_string(u8 *dbuf, u8 start, u8 len, char *string);

typedef void (*dispup_func_t)(u8 *dbuf, u8 update_only, u16 arg);

void register_display_area(dispup_func_t drawfunc, u16 arg, u8 start, u8 length);
typedef enum {
    LED_TOP_LEFT      = 0,
    LED_TOP_MIDDLE    = 1,
    LED_TOP_RIGHT     = 2,
    LED_BOTTOM_LEFT   = 3,
    LED_BOTTOM_MIDDLE = 4,
    LED_BOTTOM_RIGHT  = 5,
    LED_LEFT_SIDE     = 6,
    LED_RIGHT_SIDE    = 7,
} hud_led_t;
typedef enum {
    LED_OFF,
    LED_RED,
    LED_BLUE,
    LED_GREEN,
    LED_ORANGE
} hud_ledcolor_t;

typedef enum {
    LED_BLINK_NONE,
    LED_BLINK_FAST,
    LED_BLINK_MED,
    LED_BLINK_SLOW,
} hud_ledblink_t;

void hud_set_led(hud_led_t led, hud_ledcolor_t color, hud_ledblink_t blink);



#endif /* !HUD_H */
