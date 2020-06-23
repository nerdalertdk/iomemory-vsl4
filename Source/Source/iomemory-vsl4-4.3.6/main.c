//-----------------------------------------------------------------------------
// Copyright (c) 2006-2014, Fusion-io, Inc.(acquired by SanDisk Corp. 2014)
// Copyright (c) 2014-2016 SanDisk Corp. and/or all its affiliates. (acquired by Western Digital Corp. 2016)
// Copyright (c) 2016-2018 Western Digital Technologies, Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of the SanDisk Corp. nor the names of its contributors
//   may be used to endorse or promote products derived from this software
//   without specific prior written permission.
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
// OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//-----------------------------------------------------------------------------
#if defined (__linux__)
#include "port-internal.h"
#include <fio/port/compiler.h>
#include <fio/port/dbgset.h>
#include <fio/port/message_ids.h>
#if KFIOC_USE_LINUX_UACCESS_H && !defined(__VMKLNX__)
# include <linux/uaccess.h>
#else
# include <asm/uaccess.h>
#endif
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/sched.h>
#endif
#include <fio/port/fio-port.h>
#if defined (__linux__)
#include <linux/module.h>
#endif
#include <fio/port/ifio.h>
#include <fio/port/nexus.h>
#if defined(__linux__) || defined(__VMKLNX__)
#  include <fio/port/common-linux/kblock.h>
#endif

/**
 * @ingroup PORT_COMMON_LINUX
 * @{
 */

extern int use_workqueue;

extern int iodrive_init(void);
extern void iodrive_cleanup(void);

extern int kfio_pci_register_driver(void);
extern void kfio_pci_unregister_driver(void);

extern void fio_auto_attach_wait(void);

extern void kfio_iodrive_sysrq_keys(void);
extern void kfio_iodrive_unreg_sysrq_keys(void);
#if defined (__VMKLNX__)
extern int  ifio_init_memory();
extern void ifio_cleanup_memory();
#endif

static void fio_do_exit(void);

static uint32_t fio_init_state;
#define FIO_INIT_STATE_IFIOMEM 0x0001
#define FIO_INIT_STATE_IODRIVE 0x0002
#define FIO_INIT_STATE_PCIDEV  0x0004
#define FIO_INIT_STATE_SYSRQ   0x0008

/// @brief Driver initialization for parts that do not require cleanup if initialization fails.
/// @note  The companion fio_do_exit() function may be called at any time, regardless of the
///        status of fio_do_init(). However, on error, a call to fio_do_exit() is unnecessary
///        because everything is automatically cleaned up.
static int fio_do_init(void)
{
    int rc;

#if defined(__VMKLNX__)
    rc = ifio_init_memory();
    if (rc)
    {
        goto error_exit;
    }
    fio_init_state |= FIO_INIT_STATE_IFIOMEM;
#endif

    rc = iodrive_init();
    if (rc)
    {
        goto error_exit;
    }
    fio_init_state |= FIO_INIT_STATE_IODRIVE;

    rc = kfio_pci_register_driver();
    if (rc < 0)
    {
        /* Not sure why this is needed. */
        kfio_pci_unregister_driver();

        rc = -ENODEV;
        goto error_exit;
    }
    fio_init_state |= FIO_INIT_STATE_PCIDEV;

#if !defined(__VMKLNX__)
    kfio_iodrive_sysrq_keys();
    fio_init_state |= FIO_INIT_STATE_SYSRQ;
#endif

    return 0;

error_exit:

    if (rc > 0)
    {
        rc = -rc; // follow convention of 0/-error
    }

    errprint_all(ERRID_CMN_LINUX_MAIN_LOAD_FAIL, "Failed to load " FIO_DRIVER_NAME " error %d: %s\n",
                 rc, ifio_strerror(rc));

    fio_do_exit();

    return rc;
}

/// @brief Driver cleanup for parts that require cleanup if initialization fails.
static void fio_do_exit(void)
{
#if !defined(__VMKLNX__)
    if (fio_init_state & FIO_INIT_STATE_SYSRQ)
    {
        kfio_iodrive_unreg_sysrq_keys();
    }
#endif

    if (fio_init_state & FIO_INIT_STATE_PCIDEV)
    {
        kfio_pci_unregister_driver();
    }

    if (fio_init_state & FIO_INIT_STATE_IODRIVE)
    {
        iodrive_cleanup();
    }

#if defined(__VMKLNX__)
    if (fio_init_state & FIO_INIT_STATE_IFIOMEM)
    {
        ifio_cleanup_memory();
    }
#endif

    fio_init_state = 0;
}

static int __init init_fio_driver(void)
{
    int rc = 0;

#if defined(__MONO_KERNEL__)
    auto_attach = 0;
#endif

    // USE_QUEUE_MQ (i.e. use_workqueue=4) is disabled in all builds and OS's. FH-24331
    // USE_QUEUE_MQ (i.e. use_workqueue=4) is disabled in all builds and OS's. FH-24331
    if (use_workqueue == USE_QUEUE_MQ)
    {
        infprint("blk-mq not supported: Reverting use_workqueue=4 to use_workqueue=0.\n");
        use_workqueue = USE_QUEUE_NONE;
    }

    if (use_workqueue == USE_QUEUE_RQ)
    {
        infprint("Using Linux I/O Scheduler\n");
    }

    /* If the LEB map isn't loaded then don't bother trying to auto attach */
    if (!iodrive_load_eb_map)
    {
        auto_attach = 0;
    }

    /* Call swiss knife of all init functions. */
    rc = fio_do_init();
    if (rc != 0)
    {
         /*
          * Do not bother waiting for parallel attaches - we should have
          * terminated the driver by now.
          */
         return rc;
    }

#if !defined(__VMKLNX__)
    fio_auto_attach_wait();
#else
    // ESX[i] holds up the entire boot process while the driver is being loaded,
    // which is unacceptable if a long metadata scan is needed.
    // Don't wait; let the attach thread run in the background.
    // fio-status will report the status as "Attaching" until it is done.
    //
    // Unfortunately, it appears that dev node exposure can race with the
    // vmkernel's datastore scan.  This is rare, however, and has only been
    // observed once. Hence, we provide an option to re-enable the attach wait.
    if (!background_attach)
    {
        infprint("Background-attach disabled; waiting for all devices...\n");
        fio_auto_attach_wait();
    }
#endif

    return rc;
}

static void __exit exit_fio_driver(void)
{
#if defined(__VMKLNX__)
    if (fio_init_state & FIO_INIT_STATE_PCIDEV)
    {
        /* Wait for background attaches to finish. */
        if (background_attach)
        {
            fio_auto_attach_wait();
        }
    }
#endif
    fio_do_exit();
}

/**
 * @}
 */

module_init(init_fio_driver);
module_exit(exit_fio_driver);

// MODULE_LICENSE is found in license.c - which is auto-generated
// Module metadata parameters will be found in license.c - which is auto-generated
