/*
 * QEMU System Emulator
 *
 * Copyright (c) 2003-2020 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "qemu-main.h"
#include "qemu/main-loop.h"
#include "system/replay.h"
#include "system/system.h"

#ifdef __linux__
#include <sys/prctl.h>
#ifndef PR_SET_THP_DISABLE
#define PR_SET_THP_DISABLE 41
#endif
#endif

#ifdef CONFIG_SDL
/*
 * SDL insists on wrapping the main() function with its own implementation on
 * some platforms; it does so via a macro that renames our main function, so
 * <SDL.h> must be #included here even with no SDL code called from this file.
 */
#include <SDL.h>
#endif

#ifdef CONFIG_DARWIN
#include <CoreFoundation/CoreFoundation.h>
#endif

static void *qemu_default_main(void *opaque)
{
    int status;

    replay_mutex_lock();
    bql_lock();
    status = qemu_main_loop();
    qemu_cleanup(status);
    bql_unlock();
    replay_mutex_unlock();

    exit(status);
}

int (*qemu_main)(void);

#ifdef CONFIG_DARWIN
static int os_darwin_cfrunloop_main(void)
{
    CFRunLoopRun();
    g_assert_not_reached();
}
int (*qemu_main)(void) = os_darwin_cfrunloop_main;
#endif

int main(int argc, char **argv)
{
    /*
     * Disable THP (Transparent Huge Pages) for this process.
     *
     * Under Gunyah, guest RAM is LEND'd to the hypervisor and the host
     * CPU loses access.  If any THPs were allocated, kswapd's
     * deferred_split_scan will try to inspect those pages via the kernel
     * direct map (memchr_inv), triggering a fatal synchronous external
     * abort.
     *
     * PREVIOUSLY we used PR_SET_THP_DISABLE here, but that prevents ALL
     * THPs — including the ones we NEED for demand paging efficiency.
     * Each 2MB THP uses ONE hypervisor page table entry instead of 512
     * 4KB entries.  Without THPs, 8GB of demand-paged memory exhausts
     * the hypervisor's fixed-size page table pool (ENOMEM).
     *
     * The deferred_split_scan crash is now handled by the
     * gh_disable_deferred_split KernelPatch module (KPM), which hooks
     * deferred_split_scan to skip LEND'd pages safely.
     *
     * DO NOT re-enable PR_SET_THP_DISABLE without the KPM installed.
     */
#if 0  /* Disabled — use gh_disable_deferred_split KPM instead */
#ifdef __linux__
    prctl(PR_SET_THP_DISABLE, 1, 0, 0, 0);
#endif
#endif

    qemu_init(argc, argv);
    bql_unlock();
    replay_mutex_unlock();
    if (qemu_main) {
        QemuThread main_loop_thread;
        qemu_thread_create(&main_loop_thread, "qemu_main",
                           qemu_default_main, NULL, QEMU_THREAD_DETACHED);
        return qemu_main();
    } else {
        qemu_default_main(NULL);
        g_assert_not_reached();
    }
}
