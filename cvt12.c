/* cvt12.c  Generate mode timings using the CVT v1.2 or CVT v1.1 Timing Standards.
 *
 * gcc cvt12.c -O2 -o cvt12 -lm -Wall
 *
 * This is a modification of cvt.c on May 25, 2015 at https://github.com/kevinlekiller/cvt_timing_calculator_12
 *
 * Changes:
 * Add support for CVT v1.2 reduced blanking timings.
 * Add support for 1000/1001 reducing of refresh rate for better movie support (ie 24hz * 1000 / 1001 = 23.976hz).
 * Add check for reduce blanking on CVT v1.1 for refresh rates not a multiple of 60hz.
 * Add checks for bad x y and refresh values.
 * Display 3 digits after decimal point on CLI output for the commented out part of the xf86 modeline. (ie 23.98hz -> 23.976hz)
 * Fix some default timing values on v1.1 to be in line with the Vesa Public Standards CVT Generator.
 * Change xf86 modeline printout to be similar to [x.org](http://www.x.org/wiki/)'s cvt.
 * Various minor changes.
 *
 * For more information on CVT v1.2, please see:
 * http://www.vesa.org/vesa-standards/free-standards/
 *
 * I've left some of the original cvt.c information:
 *
 *----------------------------------------------------------------
 * cvt.c  Generate mode timings using the CVT 1.1 Timing Standard
 *
 * http://www.uruk.org/~erich/projects/cvt/
 *
 * Originally created from "gtf" written by Andy Ritger at NVidia.
 * Original block comment at the beginning of the program follows:
 *
 *---------------------------------------------------------------------
 * gtf.c  Generate mode timings using the GTF Timing Standard
 *
 * http://gtf.sourceforge.net/
 *
 * Copyright (c) 2001, Andy Ritger  aritger@nvidia.com
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * o Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * o Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer
 *   in the documentation and/or other materials provided with the
 *   distribution.
 * o Neither the name of NVIDIA nor the names of its contributors
 *   may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
 * NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * This program is based on the Generalized Timing Formula(GTF TM)
 * Standard Version: 1.0, Revision: 1.0
 *
 * The GTF Document contains the following Copyright information:
 *
 * Copyright (c) 1994, 1995, 1996 - Video Electronics Standards
 * Association. Duplication of this document within VESA member
 * companies for review purposes is permitted. All other rights
 * reserved.
 *
 * While every precaution has been taken in the preparation
 * of this standard, the Video Electronics Standards Association and
 * its contributors assume no responsibility for errors or omissions,
 * and make no warranties, expressed or implied, of functionality
 * of suitability for any purpose. The sample code contained within
 * this standard may be used without restriction.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// These are CVT 1.1 default timings, the global variables are
// changed below if we need to use CVT 1.2 timings.
float   CLOCK_STEP      =   0.250; // Clock steps in MHz
#define MARGIN_PERCENT      1.800  // % of active vertical image
#define H_SYNC_PER          8.000  // sync % of horizontal image
float   CELL_GRAN       =   8.000; // assumed character cell granularity
float   CELL_GRAN_RND   =   8.000; // assumed character cell granularity (round)
float   MIN_V_BPORCH    =   6.000; // width of vsync in lines
#define MIN_V_PORCH_RND     3.000  // width of vsync in lines
#define M                 600.000  // blanking formula gradient
#define C                  40.000  // blanking formula offset
#define K                 128.000  // blanking formula scaling factor
#define J                  20.000  // blanking formula scaling factor
// Standard Timing Parameters
#define MIN_VSYNC_BP      550.000  // min time of vsync + back porch (us)
#define H_SYNC_PERCENT      8.000  // width of hsync as % of total line
// Reduced Blanking defines
#define RB_MIN_V_BPORCH     6.000  // lines
float   RB_V_FPORCH     =   3.000; // lines
#define RB_MIN_V_BLANK    460.000  // us
#define RB_H_SYNC          32.000  // pixels
#define RB_H_BLANK        160.000  // pixels
// C' and M' are part of the Blanking Duty Cycle computation.
float   C_PRIME         =   0.000;
float   M_PRIME         =   0.000;

// Struct definitions.
typedef struct __mode {
    int hr, hss, hse, hfl;
    int vr, vss, vse, vfl;
    float pclk, h_freq, v_freq;
    float real_v_rate;
    int rb, in;
} mode;

typedef struct __options {
    int x, y;
    int reduced_blank, reduced_blank_v1_2, film_optimized, interlaced;
    int xf86mode, fbmode;
    float v_freq;
} options;

// Prototypes.
void print_value(int n, char *name, float val);
void print_xf86_mode (mode *m);
void print_fb_mode (mode *m);
mode *vert_refresh (
    int h_pixels, int v_lines, float freq,
    int interlaced, int reduced_blank, int margins,
    int film_optimized
);
void set_global_timings_v1_2();
options *parse_command_line (int argc, char *argv[]);

int global_verbose = 0;

/**
 * Print the result of the named computation.
 * This is useful when comparing against the CVT EXCEL spreadsheet.
 */
void print_value(int n, char *name, float val)
{
    if (global_verbose) {
        printf("%2d: %-27s: %15f\n", n, name, val);
    }
}

/**
 * Print the XFree86 modeline, given mode timings.
 */
void print_xf86_mode (mode *m)
{
    printf (
        "# %dx%d @ %.3f Hz %s%s(CVT)"
        " field rate %.3f Hz; hsync: %.3f kHz; pclk: %.2f MHz\n"
        "Modeline \"%dx%d_%.2f%s%s\"  %.2f"
        "  %d %d %d %d"
        "  %d %d %d %d"
        " %s%chsync %cvsync\n",
        m->hr, m->vr, m->v_freq, (m->in ? "Interlaced " : ""), (m->rb ? "Reduced Blank " : ""),
        m->real_v_rate, m->h_freq, m->pclk,
        m->hr, m->vr, m->v_freq,  (m->in ? "i" : ""), (m->rb ? "_rb" : ""), m->pclk,
        m->hr, m->hss, m->hse, m->hfl,
        m->vr, m->vss, m->vse, m->vfl,
        (m->in?"Interlace  ":""), (m->rb?'+':'-'), (m->rb?'-':'+')
    );
}

/**
 * Print a mode description in fbset(8) format.
 * See the fb.modes(8) manpage.
 * The timing description used in this is rather odd,
 * they use "left and right margin" to refer
 * to the portion of the hblank before and after the sync pulse
 * by conceptually wrapping the portion of the blank after the pulse
 * to infront of the visible region; ie:
 *
 * Timing description I'm accustomed to:
 *
 *     <--------1--------> <--2--> <--3--> <--4-->
 *                                _________
 *    |-------------------|_______|       |_______
 *
 *                        R       SS      SE     FL
 *
 * 1: visible image
 * 2: blank before sync (aka front porch)
 * 3: sync pulse
 * 4: blank after sync (aka back porch)
 * R: Resolution
 * SS: Sync Start
 * SE: Sync End
 * FL: Frame Length
 *
 * But the fb.modes format is:
 *
 *    <--4--> <--------1--------> <--2--> <--3--> 
 *                                       _________
 *    _______|-------------------|_______|       |
 *
 * The fb.modes(8) manpage refers to <4> and <2> as the left and
 * right "margin" (as well as upper and lower margin in the vertical
 * direction) -- note that this has nothing to do with the term
 * "margin" used in the CVT Timing Standard.
 *
 * XXX always prints the 32 bit mode -- should I provide a command
 * line option to specify the bpp?  It's simple enough for a user
 * to edit the mode description after it's generated.
 */
void print_fb_mode (mode *m)
{
    printf (
        "\nmode \"%dx%d %.2fHz %s%s32bit (CVT)\"\n"
        "    # PCLK: %.2f MHz, H: %.2f kHz, V: %.2f Hz\n"
        "    geometry %d %d %d %d 32\n",
        m->hr, m->vr, m->v_freq, (m->in ? "INT " : ""), (m->rb ? "RBlank " : ""),
        m->pclk, m->h_freq, m->real_v_rate,
        m->hr, m->vr, m->hr, m->vr
    );
    printf (
        "    timings %d %d %d %d %d %d %d\n",
        (int) rint(1000000.0/m->pclk), // pixclock in picoseconds
        m->hfl - m->hse,               // left margin (in pixels)
        m->hss - m->hr,                // right margin (in pixels)
        m->vfl - m->vse,               // upper margin (in pixel lines)
        m->vss - m->vr,                // lower margin (in pixel lines)
        m->hse - m->hss,               // horizontal sync length (in pixels)
        m->vse - m->vss                // vert sync length (in pixel lines)
    );
    printf (
        "    hsync %s\n"
        "    vsync %s\n",
        (m->rb ? "high" : "low"),
        (m->rb ? "low" : "high")
    );
    if (m->in) {
        printf("    laced true\n");
    }
    printf ("endmode\n\n");
}

/*
 * As defined by the CVT Timing Standard, compute the Stage 1 Parameters
 * using the vertical refresh frequency.
 * In other words: input a desired resolution and desired refresh rate, and
 * output the CVT mode timings.
 *
 * XXX margin computations are implemented but not tested (nor used by
 * XFree86 of fbset mode descriptions, from what I can tell).
 */
mode *vert_refresh (int h_pixels, int v_lines, float freq, int interlaced, int reduced_blank, int margins, int film_optimized)
{
    float h_pixels_rnd, v_lines_rnd;
    float v_field_rate_rqd;
    float top_margin, bot_margin;
    float interlace;
    float h_period_est;
    float v_sync_bp;
    float left_margin, right_margin;
    float total_active_pixels;
    float ideal_duty_cycle, cur_duty_cycle;
    float v_sync, h_blank;
    float total_pixels, total_v_lines;
    float v_sync_rnd, h_sync_rnd;
    float h_back_porch, v_front_porch, h_front_porch;
    float vbi_lines, act_vbi_lines, rb_min_vbi;
    float act_pixel_freq, act_h_freq;
    float act_field_rate, act_frame_rate;
    char *aspect_ratio;
    int stage;

    mode *m = (mode*) malloc (sizeof (mode));

    /*  1. Required Field Rate
     *
     *  This is slightly different from the spreadsheet because we use
     *  a different result for interlaced video modes.  Simplifies this
     *  to the input field rate.
     *
     *  [V FIELD RATE RQD] = [I/P FREQ RQD]
     */
    v_field_rate_rqd = freq;
    print_value(1, "[V FIELD RATE RQD]", v_field_rate_rqd);

    /*  2. Horizontal Pixels
     *
     *  In order to give correct results, the number of horizontal
     *  pixels requested is first processed to ensure that it is divisible
     *  by the character size, by rounding it to the nearest character
     *  cell boundary.
     *
     *  [H PIXELS RND] = ((ROUNDDOWN([H PIXELS]/[CELL GRAN RND],0))
     *       *[CELLGRAN RND])
     */
    h_pixels_rnd = floor((float) h_pixels / CELL_GRAN_RND) * CELL_GRAN_RND;
    print_value(2, "[H PIXELS RND]", h_pixels_rnd);

    /*  2.5th Calculation, aspect_ratio & v_sync_rnd
     *
     *  [ASPECT_RATIO] = IF(H_PIXELS_RND = CELL_GRAN_RND*ROUND((V_LINES*
     *     4.0/3.0)/CELL_GRAN_RND),"4:3")
     *     etc...
     *  [V_SYNC] = [value from table based on aspect ratio]
     *  [V_SYNC_RND] = ROUND(V_SYNC,0)  // Not needed in principle
     */
    if (h_pixels_rnd == CELL_GRAN_RND * floor(((float)v_lines * 4.0 / 3.0) / CELL_GRAN_RND)) {
        aspect_ratio = "4:3";
        v_sync = 4;
    } else if (h_pixels_rnd == CELL_GRAN_RND * floor(((float)v_lines * 16.0 / 9.0) / CELL_GRAN_RND)) {
        aspect_ratio = "16:9";
        v_sync = 5;
    } else if (h_pixels_rnd == CELL_GRAN_RND * floor(((float)v_lines * 16.0 / 10.0) / CELL_GRAN_RND)) {
        aspect_ratio = "16:10";
        v_sync = 6;
    } else if (h_pixels_rnd == CELL_GRAN_RND * floor(((float)v_lines * 5.0 / 4.0) / CELL_GRAN_RND)) {
        aspect_ratio = "5:4";
        v_sync = 7;
    } else if (h_pixels_rnd == CELL_GRAN_RND * floor(((float)v_lines * 15.0 / 9.0) / CELL_GRAN_RND)) {
        aspect_ratio = "15:9";
        v_sync = 7;
    } else {
        // Default case of unknown aspect ratio.
        aspect_ratio = "Custom";
        v_sync = 10;
    }
    v_sync_rnd = v_sync;

    if (global_verbose) {
        printf(
            " 2.5: [ASPECT_RATIO]           : %10s\n"
            " 2.5: [V SYNC]                 : %15f\n",
            aspect_ratio,
            v_sync_rnd
        );
    }

    /*
     *  3. Determine Left & Right Borders
     *
     *  Calculate the margins on the left and right side.
     *
     *  [LEFT MARGIN (PIXELS)] = (IF( [MARGINS RQD?]="Y",
     *          (ROUNDDOWN( ([H PIXELS RND] * [MARGIN%] / 100 /
     *                   [CELL GRAN RND]),0)) * [CELL GRAN RND],
     *          0))
     *  [RIGHT MARGIN (PIXELS)] = (IF( [MARGINS RQD?]="Y",
     *          (ROUNDDOWN( ([H PIXELS RND] * [MARGIN%] / 100 /
     *                   [CELL GRAN RND]),0)) * [CELL GRAN RND],
     *          0))
     */
    left_margin = margins ? floor(h_pixels_rnd * MARGIN_PERCENT / 100.0 / CELL_GRAN_RND) * CELL_GRAN_RND : 0.0;
    right_margin = left_margin;
    print_value(3, "[LEFT MARGIN (PIXELS)]", left_margin);
    print_value(3, "[RIGHT MARGIN (PIXELS)]", right_margin);

    /*  4. Find total active pixels.
     *
     *  Find total number of active pixels in image and left and right
     *  margins.
     *
     *  [TOTAL ACTIVE PIXELS] = [H PIXELS RND] + [LEFT MARGIN (PIXELS)] +
     *                          [RIGHT MARGIN (PIXELS)]
     */
    total_active_pixels = h_pixels_rnd + left_margin + right_margin;
    print_value(4, "[TOTAL ACTIVE PIXELS]", total_active_pixels);

    /*  5. Find number of lines per field.
     *
     *  If interlace is requested, the number of vertical lines assumed
     *  by the calculation must be halved, as the computation calculates
     *  the number of vertical lines per field. In either case, the
     *  number of lines is rounded to the nearest integer.
     *
     *  [V LINES RND] = IF([INT RQD?]="y", ROUNDDOWN([V LINES]/2,0),
     *                                     ROUNDDOWN([V LINES],0))
     */
    v_lines_rnd = interlaced ? floor((float) v_lines / 2.0) : floor((float) v_lines);
    print_value(5, "[V LINES RND]", v_lines_rnd);

    /*  6. Find Top and Bottom margins.
     *
     *  [TOP MARGIN (LINES)] = IF([MARGINS RQD?]="Y",
     *          ROUNDDOWN(([MARGIN%]/100*[V LINES RND]),0),
     *          0)
     *  [BOT MARGIN (LINES)] = IF([MARGINS RQD?]="Y",
     *          ROUNDDOWN(([MARGIN%]/100*[V LINES RND]),0),
     *          0)
     */
    top_margin = margins ? floor(MARGIN_PERCENT / 100.0 * v_lines_rnd) : (0.0);
    bot_margin = top_margin;
    print_value(6, "[TOP MARGIN (LINES)]", top_margin);
    print_value(6, "[BOT MARGIN (LINES)]", bot_margin);

    /*  7. If interlace is required, then set variable [INTERLACE]=0.5:
     *
     *  [INTERLACE]=(IF([INT RQD?]="y",0.5,0))
     */
    interlace = interlaced ? 0.5 : 0.0;
    print_value(7, "[INTERLACE]", interlace);

    /*
     *  Here it diverges for "reduced blanking" or normal blanking modes.
     */
    if (reduced_blank) {
        h_blank = RB_H_BLANK;

        /*  8. Estimate Horiz. Period (us).
        *
        *  [H PERIOD EST] = ((1000000/V_FIELD_RATE_RQD)-RB_MIN_V_BLANK)/(V_LINES_RND+TOP_MARGIN+BOT_MARGIN)
        */
        h_period_est = (1000000.0/v_field_rate_rqd - RB_MIN_V_BLANK) / (v_lines_rnd + top_margin + bot_margin);
        print_value(8, "[H PERIOD EST]", h_period_est);

        /*  9. Find number of lines in vertical blanking.
        *
        *  [Actual VBI_LINES] = RB_MIN_V_BLANK/H_PERIOD_EST
        *  [VBI_LINES] = ROUNDDOWN(RB_MIN_V_BLANK/H_PERIOD_EST,0) + 1
        */
        vbi_lines = RB_MIN_V_BLANK/h_period_est;
        print_value(9, "[Actual VBI LINES]", vbi_lines);

        vbi_lines = floor(vbi_lines) + 1.0;
        print_value(9, "[VBI LINES]", vbi_lines);

        /*  10. Check Vertical Blanking is sufficient.
        *
        *  [RB MIN VBI] = RB_V_FPORCH+V_SYNC_RND+RB_MIN_V_BPORCH
        *  [ACT VBI LINES] = IF(VBI_LINES<RB_MIN_VBI,RB_MIN_VBI,VBI_LINES)
        */
        rb_min_vbi = RB_V_FPORCH + v_sync_rnd + RB_MIN_V_BPORCH;
        act_vbi_lines = (vbi_lines < rb_min_vbi) ? rb_min_vbi : vbi_lines;
        print_value(10, "[Minimum VBI Lines]", rb_min_vbi);
        print_value(10, "[ACT VBI LINES]", act_vbi_lines);


        /*  11. Find total number of lines in vertical field.
        *
        *  [TOTAL V LINES] = ACT_VBI_LINES+V_LINES_RND+TOP_MARGIN+BOT_MARGIN+INTERLACE
        */
        total_v_lines = act_vbi_lines + v_lines_rnd + top_margin + bot_margin + interlace;
        print_value(11, "[TOTAL V LINES]", total_v_lines);


        /*  12. Find total number of pixels in a line (pixels).
        *
        *  [TOTAL PIXELS] = RB_H_BLANK+TOTAL_ACTIVE_PIXELS
        */
        total_pixels = total_active_pixels + RB_H_BLANK;
        print_value(12, "[TOTAL PIXELS]", total_pixels);


        /*  13. Find Pixel Clock Frequency (MHz).
        *
        *  [Non-rounded PIXEL_FREQ] = V_FIELD_RATE_RQD*TOTAL_V_LINES*TOTAL_PIXELS/1000000
        *  [ACT PIXEL FREQ] = CLOCK_STEP * ROUND((V_FIELD_RATE_RQD*TOTAL_V_LINES*TOTAL_PIXELS/1000000)/CLOCK_STEP,0)
        */
        act_pixel_freq = v_field_rate_rqd * total_v_lines * total_pixels / 1000000.0;
        print_value(13, "[Non-rounded PIXEL FREQ]", act_pixel_freq);

        act_pixel_freq = CLOCK_STEP * floor(act_pixel_freq / CLOCK_STEP);
        print_value(13, "[ACT PIXEL FREQ]", act_pixel_freq);

        // Lower pixel clock to get the vertical refresh rate to match video better.
        // ie (24 * 1000 / 1001) = 23.976
        if (film_optimized == 1) {
            act_pixel_freq = (act_pixel_freq * 1000 / 1001);
        }

        stage = 14;

    } else { // Normal Blanking

        /*  8. Estimate Horiz. Period (us).
        *
        *  [H PERIOD EST] = ((1/V_FIELD_RATE_RQD)-MIN_VSYNC_BP/1000000)/(V_LINES_RND+(2*TOP_MARGIN)+MIN_V_PORCH_RND+INTERLACE)*1000000
        */
        h_period_est = ((1/v_field_rate_rqd) - MIN_VSYNC_BP/1000000.0) / (v_lines_rnd + (2*top_margin) + MIN_V_PORCH_RND + interlace) * 1000000.0;
        print_value(8, "[H PERIOD EST]", h_period_est);

        /*  9. Find number of lines in (SYNC + BACK PORCH).
        *
        *  [Estimated V_SYNC_BP] = ROUNDDOWN((MIN_VSYNC_BP/H_PERIOD_EST),0)+1
        *  [Actual V_SYNC_BP] = MIN_VSYNC_BP/H_PERIOD_EST
        *  [V_SYNC_BP] = IF(Estimated V_SYNC_BP<(V_SYNC+MIN_V_BPORCH),
        *       V_SYNC+MIN_V_BPORCH,Estimated V_SYNC_BP)
        */
        v_sync_bp = MIN_VSYNC_BP/h_period_est;
        print_value(9, "[Actual V_SYNC_BP]", v_sync_bp);

        v_sync_bp = floor(v_sync_bp) + 1;
        print_value(9, "[Estimated V_SYNC_BP]", v_sync_bp);

        v_sync_bp = (v_sync_bp < v_sync + MIN_V_BPORCH) ? v_sync + MIN_V_BPORCH : v_sync_bp;
        print_value(9, "[V_SYNC_BP]", v_sync_bp);

        /*  10. Find number of lines in back porch (Lines).
        *
        *  [Back porch] = V_SYNC_BP - V_SYNC_RND;
        */
        print_value(10, "[Back porch]", v_sync_bp - v_sync_rnd);

        /*  11. Find total number of lines in vertical field.
        *
        *  [TOTAL V LINES] = V_LINES_RND+TOP_MARGIN+BOT_MARGIN
        *      +V_SYNC_BP+INTERLACE+MIN_V_PORCH_RND
        */
        total_v_lines = v_lines_rnd + top_margin + bot_margin + v_sync_bp + interlace + MIN_V_PORCH_RND;
        print_value(11, "[TOTAL V LINES]", total_v_lines);

        /*  12. Find ideal blanking duty cycle from formula (%):
        *
        *  [IDEAL DUTY CYCLE] = C_PRIME-(M_PRIME*H_PERIOD_EST/1000)
        */
        ideal_duty_cycle = C_PRIME - (M_PRIME * h_period_est / 1000.0);
        print_value(12, "[IDEAL DUTY CYCLE]", ideal_duty_cycle);


        /*  13. Find blanking time to nearest cell (Pixels).
        *
        *  [H BLANK] = IF(IDEAL_DUTY_CYCLE<20,(ROUNDDOWN((TOTAL_ACTIVE_PIXELS*20/(100-20)/(2*CELL_GRAN_RND)),0))*(2*CELL_GRAN_RND),(ROUNDDOWN((TOTAL_ACTIVE_PIXELS*IDEAL_DUTY_CYCLE/(100-IDEAL_DUTY_CYCLE)/(2*CELL_GRAN_RND)),0))*(2*CELL_GRAN_RND))
        */
        cur_duty_cycle = (ideal_duty_cycle < 20.0) ? 20.0 : ideal_duty_cycle;
        h_blank = floor((total_active_pixels * cur_duty_cycle/(100.0 - cur_duty_cycle)/(2.0*CELL_GRAN_RND))) * (2.0*CELL_GRAN_RND);
        print_value(13, "[H BLANK]", h_blank);

        /*  14. Find total number of pixels in a line (Pixels).
        *
        *  [TOTAL PIXELS] = TOTAL_ACTIVE_PIXELS + H_BLANK
        */
        total_pixels = total_active_pixels + h_blank;
        print_value(14, "[TOTAL PIXELS]", total_pixels);

        /*  15. Find pixel clock frequency (MHz).
        *
        *  [Non-rounded PIXEL FREQ] = TOTAL_PIXELS / H_PERIOD_EST
        *  [ACT PIXEL FREQ] = CLOCK_STEP * ROUNDDOWN(
        */
        act_pixel_freq = total_pixels / h_period_est;
        print_value(15, "[Non-rounded PIXEL FREQ]", act_pixel_freq);
        act_pixel_freq = CLOCK_STEP * floor(act_pixel_freq / CLOCK_STEP);
        print_value(15, "[ACT PIXEL FREQ]", act_pixel_freq);

        stage = 16;
    }

    /*  14/16. Find actual horizontal frequency (kHz)
     *
     *  [ACT H FREQ] = 1000*ACT_PIXEL_FREQ/TOTAL_PIXELS
     */
    act_h_freq = 1000 * act_pixel_freq / total_pixels;
    print_value(stage, "[ACT H FREQ]", act_h_freq);
    stage += 1;

    /*  15/17. Find actual field rate (Hz)
     *
     *  [ACT FIELD RATE] = 1000*ACT_H_FREQ/TOTAL_V_LINES
     */
    act_field_rate = 1000 * act_h_freq / total_v_lines;
    print_value(stage, "[ACT FIELD RATE]", act_field_rate);
    stage += 1;

    /*  16/18. Find actual vertical frame frequency (Hz)
     *
     *  [ACT FRAME RATE] = IF(INT_RQD?=Y,ACT_FIELD_RATE/2,ACT_FIELD_RATE)
     */
    act_frame_rate = interlace ? (act_field_rate / 2) : act_field_rate;
    print_value(stage, "[ACT FRAME RATE]", act_frame_rate);

    // Extra computations not numbered in the CVT spreadsheet.

    /*  20. Find Horizontal Back Porch.
     *
     *  [H BACK PORCH] = H_BLANK/2
     */
    h_back_porch = h_blank/2;
    print_value(20, "[H BACK PORCH]", h_back_porch);


    /*  21. Find Horizontal Front Porch.
     *
     *  [H SYNC RND] = IF(RED_BLANK_RQD?="Y",RB_H_SYNC,(ROUNDDOWN((H_SYNC_PER/100*TOTAL_PIXELS/CELL_GRAN_RND),0))*CELL_GRAN_RND)
     */
    if (reduced_blank) {
        h_sync_rnd = RB_H_SYNC;
    } else {
        h_sync_rnd = floor(H_SYNC_PER/100.0*total_pixels/CELL_GRAN_RND) * CELL_GRAN_RND;
    }

    print_value(21, "[H SYNC RND]", h_sync_rnd);

    /*  22. Find Horizontal Front Porch.
     *
     *  [H FRONT PORCH] = H_BLANK - H_BACK_PORCH - H_SYNC_RND
     */
    h_front_porch = h_blank - h_back_porch - h_sync_rnd;
    print_value(22, "[H FRONT PORCH]", h_front_porch);

    /*  23. Find Vertical Front Porch.
     *
     *  [V FRONT PORCH] = IF(RED_BLANK_RQD?="y",RB_V_FPORCH,MIN_V_PORCH_RND)
     */
    v_front_porch = reduced_blank ? RB_V_FPORCH : MIN_V_PORCH_RND;
    print_value(23, "[V FRONT PORCH]", v_front_porch);

    // Finally, pack the results in the mode struct.
    m->hr  = (int) (h_pixels_rnd);
    m->hss = (int) (h_pixels_rnd + h_front_porch);
    m->hse = (int) (h_pixels_rnd + h_front_porch + h_sync_rnd);
    m->hfl = (int) (total_pixels);

    int real_v_lines = v_lines;
    m->vr  = (int) (real_v_lines);
    m->vss = (int) (real_v_lines + v_front_porch);
    m->vse = (int) (real_v_lines + v_front_porch + v_sync_rnd);
    m->vfl = (int) (total_v_lines - v_lines_rnd + real_v_lines);

    m->pclk   = act_pixel_freq;
    m->h_freq = act_h_freq;
    m->v_freq = freq;
    m->real_v_rate = act_field_rate;

    m->in = interlaced;
    m->rb = reduced_blank;

    return (m);
}

/**
 * Parse the command line and return an
 * alloced structure containing the results.  On error print usage
 * and return NULL.
 */
options *parse_command_line (int argc, char *argv[])
{
    options *o = (options *) calloc (1, sizeof (options));

    int n = 3;
    if (argc < n+1) {
        goto bad_option;
    }

    o->x = atoi (argv[1]);
    o->y = atoi (argv[2]);
    o->v_freq = atof (argv[3]);
    if (!o->x || o->x <= 0 || !o->y || o->y <= 0 || !o->v_freq || o->v_freq <= 0.0) {
        goto bad_option;
    }

    while (++n < argc) {
        if ((strcmp (argv[n], "-v") == 0) || (strcmp (argv[n], "--verbose") == 0)) {
            global_verbose = 1;
        } else if ((strcmp (argv[n], "-r") == 0) || (strcmp (argv[n], "--reduced-blank") == 0)) {
            o->reduced_blank = 1;
        } else if ((strcmp (argv[n], "-i") == 0) || (strcmp (argv[n], "--interlaced") == 0)) {
            o->interlaced = 1;
        } else if ((strcmp (argv[n], "-f") == 0) || (strcmp (argv[n], "--fbmode") == 0)) {
            o->fbmode = 1;
        } else if ((strcmp (argv[n], "-x") == 0) ||  (strcmp (argv[n], "--xf86mode") == 0)) {
            o->xf86mode = 1;
        } else if ((strcmp (argv[n], "-b") == 0) ||  (strcmp (argv[n], "--rb-v2") == 0)) {
            o->reduced_blank_v1_2 = 1;
        } else if ((strcmp (argv[n], "-o") == 0) ||  (strcmp (argv[n], "--film-optimized") == 0)) {
            o->film_optimized = 1;
        } else {
            goto bad_option;
        }
    }

    // If CVT 1.1 and using reduced blanking, check if vertical frequency is multiple of 60hz.
    if (o->reduced_blank == 1) {
        if (o->v_freq > 60.0 && fmodf(o->v_freq, (float)60) != 0) {
            goto bad_vrefresh;
        } else if (o->v_freq < 60.0 && fmodf((float)60, o->v_freq) != 0) {
            goto bad_vrefresh;
        }
    }

    // If -b is passed, set -r to true.
    if (o->reduced_blank_v1_2 == 1) {
        o->reduced_blank = 1;
    } else { // Ignore -o if -b is not passed.
        o->film_optimized = 0;
    }

    // If neither xf86mode nor fbmode were requested, default to xf86mode.
    if (!o->fbmode && !o->xf86mode) {
        o->xf86mode = 1;
    }
    return (o);

 bad_vrefresh:

    fprintf(
        stderr,
        "ERROR: Refresh rate must be multiple of 60hz according to CVT 1.1 specifications for reduced blanking.\n"
    );
    free(o);
    return(NULL);

 bad_option:

    fprintf (
        stderr,
        "Description:  This program generates video timing descriptions using formulas\n"
        "   from the VESA \"CVT\" (Coordinated Video Timing) v1.1 specification, based\n"
        "   itself on the earlier VESA \"GTF\" (Generalized Timing Formula) v1.0\n   specification.\n\n"
        "usage: %s x y refresh [-v|--verbose]\n"
        "      [-r|--reduced-blank] [-i|--interlaced]\n"
        "      [-f|--fbmode] [-x|-xf86mode]\n"
        "      [-b|--rb-v2] [-o|--film-optimized]\n\n"
        "            x : The desired horizontal resolution (required).\n"
        "            y : The desired vertical resolution (required).\n"
        "      refresh : The desired refresh rate (required).\n\n"
        " -v|--verbose        : Enable verbose printouts (traces each step of the computation).\n"
        " -r|--reduced-blank  : Use CVT 1.1 \"Reduced Blanking\" timings\n"
        "                       Only allows multiple of 60hz.\n"
        " -b|--rb-v2          : Use CVT 1.2 \"Reduced Blanking\" timings, this is more precise.\n"
        "                       Allows any refresh rate.\n"
        " -o|--film-optimized : Change refresh rate for better video support. Requires -b\n"
        "                       (ie. converts 24hz to 23.976hz)\n"
        "                       This is the same as passing 23.976 without -o,\n"
        "                       -o does the calculation for you.\n"
        " -i|--interlaced     : Generate an interlaced video mode.\n"
        " -f|--fbmode         : Output an fbset(8)-style mode description.\n"
        " -x|-xf86mode        : output an XFree86-style mode description (this is the default\n"
        "                       if no mode description is requested).\n",
        argv[0]
    );

    free (o);
    return (NULL);
}

/**
 * Set global timings to CVT v1.2
 */
void set_global_timings_v1_2()
{
    CLOCK_STEP      =   0.001; // Clock steps in MHz
    CELL_GRAN       =   1.000; // assumed character cell granularity
    CELL_GRAN_RND   =   1.000; // assumed character cell granularity (round)
    MIN_V_BPORCH    =   3.000; // width of vsync in lines
    RB_V_FPORCH     =  23.000; // lines
}

int main (int argc, char *argv[])
{
    mode *m;
    options *o;

    o = parse_command_line (argc, argv);
    if (!o) {
        exit (1);
    }

    if (o->reduced_blank_v1_2 == 1) {
        set_global_timings_v1_2();
    }
    C_PRIME = (((C - J) * K/256.0) + J);
    M_PRIME = (M * K / 256.0);

    m = vert_refresh (o->x, o->y, o->v_freq, o->interlaced, o->reduced_blank, 0, o->film_optimized);
    if (!m) {
        exit (2);
    }

    if (o->xf86mode) {
        print_xf86_mode(m);
    }

    if (o->fbmode) {
        print_fb_mode(m);
    }

    return 0;
}
