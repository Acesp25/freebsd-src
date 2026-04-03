/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026, Aaron Espinoza <acesp25@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Driver for VirtIO General Purpose Input/Output device. */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/ctype.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/kdb.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <sys/bus.h>

#include <dev/virtio/virtio.h>
#include <dev/virtio/virtqueue.h>
//#include <dev/virtio/virtio_endian.h>

#include "virtio_gpio.h"

struct vtgpio_softc {
    device_t vtgpio_dev;
    uint64_t vtgpio_features;

    struct mtx req_lock;        /* virtqueue operation protector */
    struct virtqueue *req_vq;   /* Request virtqueue */

    //struct mtx irq_lock;        /* irq operation protector */ 
    //struct virtqueue *evnt_vq;  /* Event virtqueue */
};

static int vtgpio_modevent(module_t, int, void *);

static int vtgpio_probe(device_t);
static int vtgpio_attach(device_t);
static int vtgpio_detach(device_t);

//static char **vtgpio_get_names(device_t);

static int vtgpio_negotiate_features(struct vtgpio_softc *);
static int vtgpio_setup_features(struct vtgpio_softc *);
static int vtgpio_alloc_virtqueue(struct vtgpio_softc *);

static void vtgpio_stop(struct vtgpio_softc *);

static device_method_t vtgpio_methods[] = {
    /* Device methods. */
	DEVMETHOD(device_probe, vtgpio_probe),
	DEVMETHOD(device_attach, vtgpio_attach),
	DEVMETHOD(device_detach, vtgpio_detach),

    /* GPIO controller methods */
    //DEVMETHOD(virtio_gpio_get_names, vtgpio_get_names);

	DEVMETHOD_END
};

static driver_t vtgpio_driver = {
    "vtgpio",
    vtgpio_methods,
    sizeof(struct vtgpio_softc)
};

static struct virtio_feature_desc vtgpio_feature_desc[] = {
        { VIRTIO_GPIO_F_IRQ, "GpioInterruptSpport" },
        { 0, NULL }
};

static int
vtgpio_modevent(module_t mod, int type, void *unused)
{
    int error = 0;

    switch (type) {
    case MOD_LOAD:
    case MOD_QUIESCE:
    case MOD_UNLOAD:
    case MOD_SHUTDOWN:
        error = 0;
        break;
    default:
        error = EOPNOTSUPP;
        break;
    }

    return (error);
}

#define VTGPIO_REQ_MTX(_sc)                 &(_sc)->req_lock
#define VTGPIO_REQ_LOCK_INIT(_sc, _name)    mtx_init(VTGPIO_REQ_MTX((_sc)), _name, "VirtIO GPIO Request Lock", MTX_DEF)

#define VTGPIO_REQ_LOCK(_sc)                mtx_lock(VTGPIO_REQ_MTX((_sc)))
#define VTGPIO_REQ_UNLOCK(_sc)              mtx_unlock(VTGPIO_REQ_MTX((_sc)))
#define VTGPIO_REQ_LOCK_DESTROY(_sc)        mtx_destroy(VTGPIO_REQ_MTX((_sc)))

#define VTGPIO_EVNT_MTX(_sc)                 &(_sc)->evnt_lock
#define VTGPIO_EVNT_LOCK_INIT(_sc, _name)    mtx_init(VTGPIO_EVNT_MTX((_sc)), _name, "VirtIO GPIO Event Lock", MTX_DEF)

#define VTGPIO_EVNT_LOCK(_sc)                mtx_lock(VTGPIO_EVNT_MTX((_sc)))
#define VTGPIO_EVNT_UNLOCK(_sc)              mtx_unlock(VTGPIO_EVNT_MTX((_sc)))
#define VTGPIO_EVNT_LOCK_DESTROY(_sc)        mtx_destroy(VTGPIO_EVNT_MTX((_sc)))

VIRTIO_DRIVER_MODULE(virtio_gpio, vtgpio_driver, vtgpio_modevent, NULL);
MODULE_VERSION(virtio_gpio, 1);
MODULE_DEPEND(virtio_gpio, virtio, 1, 1, 1);

VIRTIO_SIMPLE_PNPINFO(virtio_gpio, VIRTIO_ID_GPIO, "VirtIO General Purpose Input/Output Device");

static int
vtgpio_probe(device_t dev) {
    printf("Probe!\n");
    return (VIRTIO_SIMPLE_PROBE(dev, virtio_gpio));
}

static int
vtgpio_attach(device_t dev) {
    printf("Attach!\n");

    struct vtgpio_softc *sc;
    int error;
    
    sc = device_get_softc(dev);
    sc->vtgpio_dev = dev;
    virtio_set_feature_desc(dev, vtgpio_feature_desc);

    VTGPIO_REQ_LOCK_INIT(sc, device_get_nameunit(dev));

    error = vtgpio_setup_features(sc);
    if (error) {
        device_printf(dev, "unable to setup features\n");
        goto fail;
    }

    error = vtgpio_alloc_virtqueue(sc);
    if (error) {
        device_printf(dev, "unable to setup virtqueues\n");
        goto fail;
    }
    
    error = virtio_setup_intr(dev, INTR_TYPE_MISC);
    if (error) {
        device_printf(dev, "cannot setup virtqueue interrupts\n");
        goto fail;
    }
    virtqueue_enable_intr(sc->req_vq);

fail:
    if (error) vtgpio_detach(dev);

    return (error);
}

static int
vtgpio_detach(device_t dev) {
    printf("Detach!\n");
    struct vtgpio_softc *sc;
    sc = device_get_softc(dev);

    if (device_is_attached(dev)) vtgpio_stop(sc);

    VTGPIO_REQ_LOCK_DESTROY(sc);

    return (0);
}

static int
vtgpio_negotiate_features(struct vtgpio_softc *sc) {
    device_t dev;
    uint64_t features;
    
    dev = sc->vtgpio_dev;
    features = VIRTIO_GPIO_F_IRQ;
    
    sc->vtgpio_features = virtio_negotiate_features(dev, features);

    return (virtio_finalize_features(dev));
}

static int
vtgpio_setup_features(struct vtgpio_softc *sc) {
    int error;

    error = vtgpio_negotiate_features(sc);
    if (error) {
        return (error);
    }

    return (0);
}

static int
vtgpio_alloc_virtqueue(struct vtgpio_softc *sc) {
    device_t dev = sc->vtgpio_dev;

    // we need to setup 1 vq by default, event queue requires feature
    int nvqs = 1;
    struct vq_alloc_info vq_info[2]; 

    VQ_ALLOC_INFO_INIT(&vq_info[0], 0, NULL, sc, &sc->req_vq,
        "%s request", device_get_nameunit(dev));

    /* TODO: event Q implementation
        something like this will maybe work
    if (dev->vtgpio_features & features) {
        VQ_ALLOC_INFO_INIT(&vq_info[1], 0, NULL, sc, &sc->evnt_vq, "%s event", device_get_name_unit(dev));
        ++nvqs;
        return (virtio_alloc_virtqueues(dev, nvqs, &vq_info));
    } */

    return (virtio_alloc_virtqueues(dev, nvqs, &vq_info[0]));
}

static void
vtgpio_stop(struct vtgpio_softc *sc) {
    virtqueue_disable_intr(sc->req_vq);
    // if (dev->vtgpio_features & features) virtqueue_disable_intr(sc->evnt_vq);

    virtio_stop(sc->vtgpio_dev);
}
