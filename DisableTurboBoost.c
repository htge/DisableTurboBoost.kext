/*
 * Copyright (c) 2012 Adam Strzelecki
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Description:
 *   Disables upon load and re-enables upon unload TurboBoost Intel Core
 *   CPU feature using MSR register MSR_IA32_MISC_ENABLE (0x1a0)
 */

#define DEBUGLOG 0

#include <libkern/libkern.h>
#include <mach/mach_types.h>
#include <i386/proc_reg.h>
#include <sys/time.h>
#include <kern/thread_call.h>
#include <kern/clock.h>

//0.5s,0.1s
static const uint64_t timeElapsed       = 500000000LL;
static const uint64_t exitTimeElapsed   = 100000000LL;

static volatile int isStopFUnctionCalled = 0;
static volatile int isStoped = 0;

extern void mp_rendezvous_no_intrs(
                                   void (*action_func)(void *),
                                   void *arg);


const uint64_t expectedFeatures  = 0x850089LL;
const uint64_t disableTurboBoost = 0x4000000000LL;

static thread_call_t autoThread = NULL;

static void disable_tb(__unused void * param_not_used) {
	wrmsr64(MSR_IA32_MISC_ENABLE, rdmsr64(MSR_IA32_MISC_ENABLE) | disableTurboBoost);
}

static void enable_tb(__unused void * param_not_used) {
	wrmsr64(MSR_IA32_MISC_ENABLE, rdmsr64(MSR_IA32_MISC_ENABLE) & ~disableTurboBoost);
}

static void PrintLog(char *logInfo)
{
    printf("Disabled Turbo Boost: %s\n", logInfo);
}

static void auto_disable_tb(thread_call_param_t	param0,
                            thread_call_param_t	param1)
{
    uint64_t time = 0;
    
    while(!isStopFUnctionCalled)
    {
        uint64_t prev = rdmsr64(MSR_IA32_MISC_ENABLE);
        if(!(prev & disableTurboBoost))
        {
            mp_rendezvous_no_intrs(disable_tb, NULL);
            uint64_t current = rdmsr64(MSR_IA32_MISC_ENABLE);
            if(prev != current)
                PrintLog("State Changed Successful.");
            else
                PrintLog("State Changed Failed.");
#if DEBUGLOG
            printf("Disabled Turbo Boost: %llx -> %llx\n", prev, rdmsr64(MSR_IA32_MISC_ENABLE));
#endif
        }
#if DEBUGLOG
        else
            printf("Disabled Turbo Boost: Current State Matched.\n");
#endif
        
        time = mach_absolute_time();
#if DEBUGLOG
        printf("Disabled Turbo Boost: uptime:%llx\n", time);
#endif
        clock_delay_until(time+timeElapsed);
    }
    
    isStoped = 1;
}

static kern_return_t start(kmod_info_t *ki, void *d) {
    autoThread = thread_call_allocate(auto_disable_tb, NULL);
    thread_call_enter(autoThread);
	return KERN_SUCCESS;
}

static kern_return_t stop(kmod_info_t *ki, void *d) {
    uint64_t time = 0;
    
    if(autoThread)
    {
        while (!isStoped) {
#if DEBUGLOG
            printf("Disabled Turbo Boost: Exiting while thread exit\n");
#endif
            isStopFUnctionCalled = 1;
            
            time = mach_absolute_time();
#if DEBUGLOG
            printf("Disabled Turbo Boost: wait uptime:%llx\n", time);
#endif
            clock_delay_until(time+exitTimeElapsed);
        }
        
        thread_call_free(autoThread);
#if DEBUGLOG
        printf("Disabled Turbo Boost: Exit Thread Successful.\n");
#endif
    }
    
	uint64_t prev = rdmsr64(MSR_IA32_MISC_ENABLE);
	mp_rendezvous_no_intrs(enable_tb, NULL);
    uint64_t current = rdmsr64(MSR_IA32_MISC_ENABLE);
    if(prev != current)
        PrintLog("Re-enabled Turbo Boost Successful.");
    else
        PrintLog("Re-enabled Turbo Boost Failed.");
    
#if DEBUGLOG
	printf("Re-enabled Turbo Boost: %llx -> %llx\n", prev, rdmsr64(MSR_IA32_MISC_ENABLE));
#endif
	return KERN_SUCCESS;
}

extern kern_return_t _start(kmod_info_t *ki, void *data);
extern kern_return_t _stop(kmod_info_t *ki, void *data);

KMOD_EXPLICIT_DECL(com.nanoant.DisableTurboBoost, "0.0.1", _start, _stop)
__private_extern__ kmod_start_func_t *_realmain = start;
__private_extern__ kmod_stop_func_t  *_antimain = stop;
__private_extern__ int _kext_apple_cc = __APPLE_CC__;
