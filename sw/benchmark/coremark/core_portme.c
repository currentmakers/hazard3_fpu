/*
Copyright 2018 Embedded Microprocessor Benchmark Consortium (EEMBC)

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Original Author: Shay Gal-on

Adapted for Hazard3 on the Colorlight i5 (hazard3_fpu project) from
hazard3/test/sim/coremark/dist/barebones/core_portme.c: UART output, mcycle
timebase at CLK_HZ, CoreMark/MHz summary.
*/
#include "coremark.h"
#include "core_portme.h"
#include <stdarg.h>
#include "uart.h"
#include "tiny_printf.h"

#if VALIDATION_RUN
volatile ee_s32 seed1_volatile = 0x3415;
volatile ee_s32 seed2_volatile = 0x3415;
volatile ee_s32 seed3_volatile = 0x66;
#endif
#if PERFORMANCE_RUN
volatile ee_s32 seed1_volatile = 0x0;
volatile ee_s32 seed2_volatile = 0x0;
volatile ee_s32 seed3_volatile = 0x66;
#endif
#if PROFILE_RUN
volatile ee_s32 seed1_volatile = 0x8;
volatile ee_s32 seed2_volatile = 0x8;
volatile ee_s32 seed3_volatile = 0x8;
#endif
volatile ee_s32 seed4_volatile = ITERATIONS;
volatile ee_s32 seed5_volatile = 0;
/* Porting : Timing functions
        How to capture time and convert to seconds must be ported to whatever is
   supported by the platform. e.g. Read value from on board RTC, read value from
   cpu clock cycles performance counter etc. Sample implementation for standard
   time.h and windows.h definitions included.
*/

#define read_csr(csrname) ({ \
  uint32_t __csr_tmp_u32; \
  __asm__ volatile ("csrr %0, " #csrname : "=r" (__csr_tmp_u32)); \
  __csr_tmp_u32; \
})

CORETIMETYPE
barebones_clock()
{
    return read_csr(mcycle);
}
/* Define : TIMER_RES_DIVIDER
        Divider to trade off timer resolution and total time that can be
   measured.

        Use lower values to increase resolution, but make sure that overflow
   does not occur. If there are issues with the return value overflowing,
   increase this value.
        */
#define GETMYTIME(_t)              (*_t = barebones_clock())
#define MYTIMEDIFF(fin, ini)       ((fin) - (ini))
#define TIMER_RES_DIVIDER          1
#define SAMPLE_TIME_IMPLEMENTATION 1
// #define EE_TICKS_PER_SEC           (CLOCKS_PER_SEC / TIMER_RES_DIVIDER)
#define EE_TICKS_PER_SEC           CLK_HZ   /* mcycle counts core clocks */

/** Define Host specific (POSIX), or target specific global time variables. */
static CORETIMETYPE start_time_val, stop_time_val;

/* Function : start_time
        This function will be called right before starting the timed portion of
   the benchmark.

        Implementation may be capturing a system timer (as implemented in the
   example code) or zeroing some system parameters - e.g. setting the cpu clocks
   cycles to 0.
*/
void
start_time(void)
{
    GETMYTIME(&start_time_val);
}
/* Function : stop_time
        This function will be called right after ending the timed portion of the
   benchmark.

        Implementation may be capturing a system timer (as implemented in the
   example code) or other system parameters - e.g. reading the current value of
   cpu cycles counter.
*/
void
stop_time(void)
{
    GETMYTIME(&stop_time_val);
}
/* Function : get_time
        Return an abstract "ticks" number that signifies time on the system.

        Actual value returned may be cpu cycles, milliseconds or any other
   value, as long as it can be converted to seconds by <time_in_secs>. This
   methodology is taken to accomodate any hardware or simulated platform. The
   sample implementation returns millisecs by default, and the resolution is
   controlled by <TIMER_RES_DIVIDER>
*/
CORE_TICKS
get_time(void)
{
    CORE_TICKS elapsed
        = (CORE_TICKS)(MYTIMEDIFF(stop_time_val, start_time_val));
    return elapsed;
}
/* Function : time_in_secs
        Convert the value returned by get_time to seconds.

        The <secs_ret> type is used to accomodate systems with no support for
   floating point. Default implementation implemented by the EE_TICKS_PER_SEC
   macro above.
*/
secs_ret
time_in_secs(CORE_TICKS ticks)
{
    secs_ret retval = ((secs_ret)ticks) / (secs_ret)EE_TICKS_PER_SEC;
    return retval;
}

ee_u32 default_num_contexts = 1;

/* Function : portable_init
        Target specific initialization code
        Test for some common mistakes.
*/
void
portable_init(core_portable *p, int *argc, char *argv[])
{
    uart_init(115200);
    __asm__ volatile("csrw mcountinhibit, zero"); /* counters are off at reset */
    /* The DAPLink can drop UART data while OpenOCD is still busy after
       "resume"; wait 0.3 s before printing */
    for (CORETIMETYPE t0 = barebones_clock(); barebones_clock() - t0 < CLK_HZ / 10 * 3;)
        ;
    ee_printf("\nCoreMark on Hazard3 @ %u MHz, %u iterations\n",
              (unsigned)(CLK_HZ / 1000000), (unsigned)ITERATIONS);
    if (sizeof(ee_ptr_int) != sizeof(ee_u8 *))
    {
        ee_printf(
            "ERROR! Please define ee_ptr_int to a type that holds a "
            "pointer!\n");
    }
    if (sizeof(ee_u32) != 4)
    {
        ee_printf("ERROR! Please define ee_u32 to a 32b unsigned type!\n");
    }
    p->portable_id = 1;
}
/* Function : portable_fini
        Target specific final code
*/
void
portable_fini(core_portable *p)
{
    p->portable_id = 0;
    /* CoreMark/MHz = iterations per million cycles */
    ee_u32 ticks = get_time();
    unsigned long long x = (unsigned long long)ITERATIONS * 1000000000ull / ticks;
    ee_printf("CoreMark/MHz     : %u.%03u\n", (unsigned)(x / 1000), (unsigned)(x % 1000));
    ee_printf("Done.\n");
}

int
ee_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = tiny_vprintf(fmt, ap);
    va_end(ap);
    return n;
}

void
trap_handler(ee_u32 mcause, ee_u32 mepc)
{
    ee_printf("\nTRAP mcause=%x mepc=%x\n", (unsigned)mcause, (unsigned)mepc);
    for (;;)
        ;
}
