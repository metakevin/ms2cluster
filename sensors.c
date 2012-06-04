/******************************************************************************
* File:              sensors.c
* Author:            Kevin Day
* Date:              November, 2010
* Description:       
*                    
*                    
* Copyright (c) 2010 Kevin Day
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
#include <avr/io.h>
#include <avr/pgmspace.h>
#include "types.h"
#include "timers.h"
#include "adc.h"
#include "hud.h"

#define OILP_PIN 6
#define OILT_PIN 0
#define CLTP_PIN 5

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
    u16 sample_accumulator;
    u16 sample_counter;
} sensors_context_t;

/* oil pressure, oil temperature, coolant pressure */
#define OILP 0
#define CLTP 1
#define OILT 2
sensors_context_t sensors_ctx[3];

u8 sensor_update;
u16 oil_pressure;
u16 coolant_pressure;
u16 oil_temperature;

u16 mlh_150_psi(u16 vofs10q6);
u16 mlh_100_psi(u16 vofs10q6);
u16 mlh_50_psi(u16 vofs10q6);
u16 adc_to_oil_temp(u16 adc);

void oilp_draw(u8 *dbuf, u8 update_only, u16 arg)
{
    char buf[9];
    snprintf(buf, 9, "OP%3dC%2d", oil_pressure, coolant_pressure);
    render_string(dbuf, 0, 8, buf);
}
extern u16 egt;
void egt_draw(u8 *dbuf, u8 update_only, u16 arg)
{
    char buf[9];
    snprintf(buf, 9, "EGT %4d", egt>>2);
    render_string(dbuf, 0, 8, buf);
}
    

void sensors_adc_callback(adc_context_t *ctx)
{
    u8 c = OILP;
    if (ctx->pin == CLTP_PIN)
    {
        c = CLTP;
    }
    if (ctx->pin == OILT_PIN)
    {
        c = OILT;
    }
    
    sensors_ctx[c].sample_accumulator += ctx->sample;
    ++sensors_ctx[c].sample_counter;
    if (sensors_ctx[c].sample_counter == SAMPLES_PER_AVG_INSTANT)
    {
        if (c == OILP)
        { 
            oil_pressure = mlh_100_psi(sensors_ctx[c].sample_accumulator);
        }
        else if (c == OILT)
        {
            oil_temperature = adc_to_oil_temp(sensors_ctx[c].sample_accumulator);
        }
        else {
            coolant_pressure = mlh_50_psi(sensors_ctx[c].sample_accumulator);
        }
        sensors_ctx[c].sample_counter = 0;
        sensors_ctx[c].sample_accumulator = 0;
        sensor_update |= (1<<c);
    }
}

u8 sensors_init(adc_context_t *adc_context, u8 num_adc)
{    
    adc_context->pin      = OILP_PIN;
    adc_context->ref      = ADC_AVCC;
    adc_context->enabled  = 1;
    adc_context->callback = sensors_adc_callback;    
    ++adc_context;
    ++num_adc;

    adc_context->pin      = CLTP_PIN;
    adc_context->ref      = ADC_AVCC;
    adc_context->enabled  = 1;
    adc_context->callback = sensors_adc_callback;    
    ++adc_context;
    ++num_adc;

    adc_context->pin      = OILT_PIN;
    adc_context->ref      = ADC_AVCC;
    adc_context->enabled  = 1;
    adc_context->callback = sensors_adc_callback;    
    ++adc_context;
    ++num_adc;

    /* turn off pullups */
    PORTF &= ~((1<<OILP_PIN)|(1<<OILT_PIN)|(1<<CLTP_PIN));


    register_display_area(oilp_draw, 0, 0, 8);
    register_display_area(egt_draw, 0, 8, 8);



    return num_adc;
}



/* Honeywell ML150
 * 0.5V = 0 PSIS
 * 4.5V = 150 PSIS  
 *
 *  4 volt span.  150/4 = 37.5PSI/volt
 *  1 PSI = 0.0266666V
 *  1 PSI = 5.4613333 LSBs 
 *  or 349.52533333 LSBs oversampled 64x LSBs (oLSBs)
 *
 *  The offset at 0.5V is 6553.6 oLSBs
 *
 *  So (oLSBs - 6553.60) /349.53 will give integral PSI.
 *
 *
 * 4 Bar gauge @ std. atmosphere = 58.0150951*0.02666666=0.5+1.5470692V = 26831 adc64
 *
 * 0 PSIS = 14.7 PSIA
 *
 *  
 *  In kPa:
 *
 *  0.5V = 101.325 kPa
 *  4.5V = 1135.539 kPa
 *  4 volt span.  1034.214/4 = 258.554 kPa/volt
 *  1 kPa = 0.003867664V
 *  1 kPa = 0.7921 LSBs
 *  or 50.69 LSBs oversampled 64x (oLSBs)
 *  The offset at 0.5V is 6553.6 oLSBs.
 *  To convert from sealed to absolute pressure, add 101.325
 *  So kPa = (oLSBs - 6553.6) / 50.69 + 101.325
 *
 *
 *
 *
 *
 * 
 */
#define ZERO_OFFSET_10Q6 6554L
#define PSI_TENTHS_DIVISOR_10Q6 35L
#define KPA_TENTHS_DIVISOR_10Q6 5

u16 mlh_150_psi(u16 vofs10q6)
{
    if (vofs10q6 > ZERO_OFFSET_10Q6)
    {
        vofs10q6 -= ZERO_OFFSET_10Q6;
    }
    else
    {
        vofs10q6 = 0;
    }

    /* Convert voltage to mbar */
    /* This is an empirically derived value which estimates
     * the rounding error in the /5 +1013 below. */
    u16 fixup = vofs10q6 / 370;
    
    vofs10q6 /= 5;
    vofs10q6 += 1013;
    vofs10q6 -= fixup;

    /* Display absolute pressure in PSIS, not PSIA... */
    vofs10q6 -= 1013;

    return vofs10q6;
}

/* Honeywell ML100
 * 0.5V = 0 PSIS
 * 4.5V = 100 PSIS  
 *
 *  4 volt span.  100/4 = 25PSI/volt
 *  1 PSI = 0.04V
 *  1 PSI = 8.192 LSBs 
 *  or 524.288 LSBs oversampled 64x LSBs (oLSBs)
 *
 *  The offset at 0.5V is 6553.6 oLSBs
 *
 *  So (oLSBs - 6553.60) /524.288 will give integral PSI.
 * 
 */

u16 mlh_100_psi(u16 vofs10q6)
{
    u32 v = vofs10q6 * 1000L;
    v += 262144L; /* for proper rounding */
    if (v >= 6553600L)
    {
        v -= 6553600L;
    }
    else
    {
        v = 0;
    }

    return (v >> 19);  // divide by 2^19=524288
}

/* Honeywell ML50
 * 0.5V = 0 PSIS
 * 4.5V = 50 PSIS  
 *
 *  4 volt span.  50/4 = 12.5PSI/volt
 *  1 PSI = 0.08V
 *  1 PSI = 16.384 LSBs 
 *  or 1048.576 LSBs oversampled 64x LSBs (oLSBs)
 *
 *  The offset at 0.5V is 6553.6 oLSBs
 *
 *  So (oLSBs - 6553.60) /1048.576 will give integral PSI.
 * 
 */
u16 mlh_50_psi(u16 vofs10q6)
{
    u32 v = vofs10q6 * 1000L;
    v += 524288L; /* for proper rounding */
    if (v >= 6553600L)
    {
        v -= 6553600L;
    }
    else
    {
        v = 0;
    }

    return (v >> 20);  // divide by 2^20=1048576}
}

/* Audi oil temperature sensor
 * Wired with 1k pullup to 5v
 *
 * to get decent resolution and a small table, there are two lookup tables.
 *
 * ADC values above 906 are clamped to 906.
 * ADC values between 906 and 108 inclusive are divided by 4, and used as
 * indices into the 0 - 100C table.
 * ADC values between 2 and 107 inclusive are looked up in the
 * 101-199C table directly.
 * In both cases the tables are addressed by ADC value and return degrees
 * celsius.
 *
 */

const prog_char adc_to_degc1[] = {
100, /* ADC<<2 27 */
99, /* ADC<<2 28 */
98, /* ADC<<2 29 */
97, /* ADC<<2 30 */
96, /* ADC<<2 31 */
95, /* ADC<<2 32 */
94, /* ADC<<2 33 */
93, /* ADC<<2 34 */
92, /* ADC<<2 35 */
91, /* ADC<<2 36 */
90, /* ADC<<2 37 */
89, /* ADC<<2 38 */
88, /* ADC<<2 39 */
87, /* ADC<<2 40 */
87, /* ADC<<2 41 */
86, /* ADC<<2 42 */
85, /* ADC<<2 43 */
84, /* ADC<<2 44 */
84, /* ADC<<2 45 */
83, /* ADC<<2 46 */
82, /* ADC<<2 47 */
82, /* ADC<<2 48 */
81, /* ADC<<2 49 */
80, /* ADC<<2 50 */
80, /* ADC<<2 51 */
79, /* ADC<<2 52 */
78, /* ADC<<2 53 */
78, /* ADC<<2 54 */
77, /* ADC<<2 55 */
76, /* ADC<<2 56 */
76, /* ADC<<2 57 */
75, /* ADC<<2 58 */
75, /* ADC<<2 59 */
74, /* ADC<<2 60 */
74, /* ADC<<2 61 */
73, /* ADC<<2 62 */
72, /* ADC<<2 63 */
72, /* ADC<<2 64 */
71, /* ADC<<2 65 */
71, /* ADC<<2 66 */
70, /* ADC<<2 67 */
70, /* ADC<<2 68 */
69, /* ADC<<2 69 */
69, /* ADC<<2 70 */
68, /* ADC<<2 71 */
68, /* ADC<<2 72 */
67, /* ADC<<2 73 */
67, /* ADC<<2 74 */
66, /* ADC<<2 75 */
66, /* ADC<<2 76 */
65, /* ADC<<2 77 */
65, /* ADC<<2 78 */
64, /* ADC<<2 79 */
64, /* ADC<<2 80 */
64, /* ADC<<2 81 */
63, /* ADC<<2 82 */
63, /* ADC<<2 83 */
62, /* ADC<<2 84 */
62, /* ADC<<2 85 */
61, /* ADC<<2 86 */
61, /* ADC<<2 87 */
60, /* ADC<<2 88 */
60, /* ADC<<2 89 */
60, /* ADC<<2 90 */
59, /* ADC<<2 91 */
59, /* ADC<<2 92 */
58, /* ADC<<2 93 */
58, /* ADC<<2 94 */
58, /* ADC<<2 95 */
57, /* ADC<<2 96 */
57, /* ADC<<2 97 */
56, /* ADC<<2 98 */
56, /* ADC<<2 99 */
56, /* ADC<<2 100 */
55, /* ADC<<2 101 */
55, /* ADC<<2 102 */
54, /* ADC<<2 103 */
54, /* ADC<<2 104 */
54, /* ADC<<2 105 */
53, /* ADC<<2 106 */
53, /* ADC<<2 107 */
52, /* ADC<<2 108 */
52, /* ADC<<2 109 */
52, /* ADC<<2 110 */
51, /* ADC<<2 111 */
51, /* ADC<<2 112 */
50, /* ADC<<2 113 */
50, /* ADC<<2 114 */
50, /* ADC<<2 115 */
49, /* ADC<<2 116 */
49, /* ADC<<2 117 */
49, /* ADC<<2 118 */
48, /* ADC<<2 119 */
48, /* ADC<<2 120 */
47, /* ADC<<2 121 */
47, /* ADC<<2 122 */
47, /* ADC<<2 123 */
46, /* ADC<<2 124 */
46, /* ADC<<2 125 */
46, /* ADC<<2 126 */
45, /* ADC<<2 127 */
45, /* ADC<<2 128 */
44, /* ADC<<2 129 */
44, /* ADC<<2 130 */
44, /* ADC<<2 131 */
43, /* ADC<<2 132 */
43, /* ADC<<2 133 */
43, /* ADC<<2 134 */
42, /* ADC<<2 135 */
42, /* ADC<<2 136 */
42, /* ADC<<2 137 */
41, /* ADC<<2 138 */
41, /* ADC<<2 139 */
40, /* ADC<<2 140 */
40, /* ADC<<2 141 */
40, /* ADC<<2 142 */
39, /* ADC<<2 143 */
39, /* ADC<<2 144 */
39, /* ADC<<2 145 */
38, /* ADC<<2 146 */
38, /* ADC<<2 147 */
38, /* ADC<<2 148 */
37, /* ADC<<2 149 */
37, /* ADC<<2 150 */
36, /* ADC<<2 151 */
36, /* ADC<<2 152 */
36, /* ADC<<2 153 */
35, /* ADC<<2 154 */
35, /* ADC<<2 155 */
35, /* ADC<<2 156 */
34, /* ADC<<2 157 */
34, /* ADC<<2 158 */
33, /* ADC<<2 159 */
33, /* ADC<<2 160 */
33, /* ADC<<2 161 */
32, /* ADC<<2 162 */
32, /* ADC<<2 163 */
32, /* ADC<<2 164 */
31, /* ADC<<2 165 */
31, /* ADC<<2 166 */
30, /* ADC<<2 167 */
30, /* ADC<<2 168 */
30, /* ADC<<2 169 */
29, /* ADC<<2 170 */
29, /* ADC<<2 171 */
28, /* ADC<<2 172 */
28, /* ADC<<2 173 */
28, /* ADC<<2 174 */
27, /* ADC<<2 175 */
27, /* ADC<<2 176 */
26, /* ADC<<2 177 */
26, /* ADC<<2 178 */
26, /* ADC<<2 179 */
25, /* ADC<<2 180 */
25, /* ADC<<2 181 */
24, /* ADC<<2 182 */
24, /* ADC<<2 183 */
24, /* ADC<<2 184 */
23, /* ADC<<2 185 */
23, /* ADC<<2 186 */
22, /* ADC<<2 187 */
22, /* ADC<<2 188 */
21, /* ADC<<2 189 */
21, /* ADC<<2 190 */
20, /* ADC<<2 191 */
20, /* ADC<<2 192 */
20, /* ADC<<2 193 */
19, /* ADC<<2 194 */
19, /* ADC<<2 195 */
18, /* ADC<<2 196 */
18, /* ADC<<2 197 */
17, /* ADC<<2 198 */
17, /* ADC<<2 199 */
16, /* ADC<<2 200 */
16, /* ADC<<2 201 */
15, /* ADC<<2 202 */
15, /* ADC<<2 203 */
14, /* ADC<<2 204 */
14, /* ADC<<2 205 */
13, /* ADC<<2 206 */
13, /* ADC<<2 207 */
12, /* ADC<<2 208 */
12, /* ADC<<2 209 */
11, /* ADC<<2 210 */
10, /* ADC<<2 211 */
10, /* ADC<<2 212 */
9, /* ADC<<2 213 */
9, /* ADC<<2 214 */
8, /* ADC<<2 215 */
8, /* ADC<<2 216 */
7, /* ADC<<2 217 */
6, /* ADC<<2 218 */
6, /* ADC<<2 219 */
5, /* ADC<<2 220 */
4, /* ADC<<2 221 */
4, /* ADC<<2 222 */
3, /* ADC<<2 223 */
2, /* ADC<<2 224 */
1, /* ADC<<2 225 */
1, /* ADC<<2 226 */
};

const prog_char adc_to_degc2[] = {
255, /* ADC 0 */
255, /* ADC 1 */
254, /* ADC 2 */
234, /* ADC 3 */
221, /* ADC 4 */
211, /* ADC 5 */
203, /* ADC 6 */
196, /* ADC 7 */
191, /* ADC 8 */
186, /* ADC 9 */
182, /* ADC 10 */
178, /* ADC 11 */
175, /* ADC 12 */
172, /* ADC 13 */
169, /* ADC 14 */
167, /* ADC 15 */
164, /* ADC 16 */
162, /* ADC 17 */
160, /* ADC 18 */
158, /* ADC 19 */
156, /* ADC 20 */
154, /* ADC 21 */
153, /* ADC 22 */
151, /* ADC 23 */
150, /* ADC 24 */
148, /* ADC 25 */
147, /* ADC 26 */
145, /* ADC 27 */
144, /* ADC 28 */
143, /* ADC 29 */
142, /* ADC 30 */
141, /* ADC 31 */
140, /* ADC 32 */
139, /* ADC 33 */
139, /* ADC 34 */
137, /* ADC 35 */
136, /* ADC 36 */
135, /* ADC 37 */
134, /* ADC 38 */
133, /* ADC 39 */
132, /* ADC 40 */
131, /* ADC 41 */
131, /* ADC 42 */
130, /* ADC 43 */
129, /* ADC 44 */
128, /* ADC 45 */
128, /* ADC 46 */
127, /* ADC 47 */
126, /* ADC 48 */
126, /* ADC 49 */
125, /* ADC 50 */
124, /* ADC 51 */
124, /* ADC 52 */
123, /* ADC 53 */
122, /* ADC 54 */
122, /* ADC 55 */
121, /* ADC 56 */
121, /* ADC 57 */
120, /* ADC 58 */
120, /* ADC 59 */
119, /* ADC 60 */
118, /* ADC 61 */
118, /* ADC 62 */
117, /* ADC 63 */
117, /* ADC 64 */
116, /* ADC 65 */
116, /* ADC 66 */
115, /* ADC 67 */
115, /* ADC 68 */
115, /* ADC 69 */
114, /* ADC 70 */
114, /* ADC 71 */
113, /* ADC 72 */
113, /* ADC 73 */
112, /* ADC 74 */
112, /* ADC 75 */
111, /* ADC 76 */
111, /* ADC 77 */
111, /* ADC 78 */
110, /* ADC 79 */
110, /* ADC 80 */
109, /* ADC 81 */
109, /* ADC 82 */
109, /* ADC 83 */
108, /* ADC 84 */
108, /* ADC 85 */
108, /* ADC 86 */
107, /* ADC 87 */
107, /* ADC 88 */
106, /* ADC 89 */
106, /* ADC 90 */
106, /* ADC 91 */
105, /* ADC 92 */
105, /* ADC 93 */
105, /* ADC 94 */
104, /* ADC 95 */
104, /* ADC 96 */
104, /* ADC 97 */
103, /* ADC 98 */
103, /* ADC 99 */
103, /* ADC 100 */
102, /* ADC 101 */
102, /* ADC 102 */
102, /* ADC 103 */
101, /* ADC 104 */
101, /* ADC 105 */
101, /* ADC 106 */
100, /* ADC 107 */
};


/* NOTE: this is only accurate if the reference voltage is
 * 5.0 volts.  In practice it seems to be 4.7 - 4.8, which
 * skews things.  e.g. 100C is actually 105C, 37C is 44C, etc.
 * Close enough... */
u16 adc_to_oil_temp(u16 adc)
{
    adc /= SAMPLES_PER_AVG_INSTANT;
    if (adc > 904)
    {
        return 0; /* zero or below */
    }
    if (adc >= 108)
    {
        adc >>= 2; /* table is in 8 bit ADC values */
        adc -= 27; /* table starts at adc<<2 27 */
        return pgm_read_byte((int)adc_to_degc1 + adc);
    }
    /* adc is between 0 and 107 inclusive */
    return pgm_read_byte((int)adc_to_degc2 + adc);
}


