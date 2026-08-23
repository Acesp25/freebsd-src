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
#include <sys/sglist.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/kdb.h>
#include <sys/gpio.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/gpio/gpiobusvar.h>
#include "gpio_if.h"

#include <dev/virtio/virtio.h>
#include <dev/virtio/virtqueue.h>
#include <dev/virtio/virtio_endian.h>

#include "virtio_gpio.h"

struct vtgpio_softc {
    	device_t vtgpio_dev;
    	uint64_t vtgpio_features;

        uint16_t ngpio;
        uint32_t gpio_names_size;

        device_t vtgpio_busdev;

        struct virtio_gpio_request *req;
        struct virtio_gpio_response *res;

    	struct mtx req_lock;        /* virtqueue operation protector */
    	struct virtqueue *req_vq;   /* Request virtqueue */

    	//struct mtx irq_lock;        /* irq operation protector */ 
    	//struct virtqueue *evnt_vq;  /* Event virtqueue */
};

static int vtgpio_modevent(module_t, int, void *);

static int vtgpio_probe(device_t);
static int vtgpio_attach(device_t);
static int vtgpio_detach(device_t);

static void vtgpio_req_vq_intr(void *xsc);
static int vtgpio_negotiate_features(struct vtgpio_softc *);
static int vtgpio_setup_features(struct vtgpio_softc *);
static int vtgpio_alloc_virtqueue(struct vtgpio_softc *);
static int vtgpio_read_config(struct vtgpio_softc *);
static void vtgpio_stop(struct vtgpio_softc *);

static device_t vtgpio_get_bus(device_t);
static int vtgpio_pin_max(device_t, int *);
static int vtgpio_pin_getname(device_t, uint32_t, char *);
static int vtgpio_pin_getcaps(device_t, uint32_t, uint32_t *);
static int vtgpio_pin_getflags(device_t, uint32_t, uint32_t *);
static int vtgpio_pin_setflags(device_t, uint32_t, uint32_t);

//static char **vtgpio_get_names(device_t);

static device_method_t vtgpio_methods[] = {
        /* Device interface. */
        DEVMETHOD(device_probe,                 vtgpio_probe),
        DEVMETHOD(device_attach,                vtgpio_attach),
        DEVMETHOD(device_detach,                vtgpio_detach),

        /* Bus interface */
        DEVMETHOD(bus_setup_intr,               bus_generic_setup_intr),
        DEVMETHOD(bus_teardown_intr,    bus_generic_teardown_intr),

        /* GPIO protocol */
        DEVMETHOD(gpio_get_bus,                 vtgpio_get_bus),
        DEVMETHOD(gpio_pin_max,                 vtgpio_pin_max),
        DEVMETHOD(gpio_pin_getname,             vtgpio_pin_getname),
        DEVMETHOD(gpio_pin_getcaps,             vtgpio_pin_getcaps),
        DEVMETHOD(gpio_pin_getflags,    vtgpio_pin_getflags),
        DEVMETHOD(gpio_pin_setflags,    vtgpio_pin_setflags),
        //DEVMETHOD(gpio_pin_get,               vtgpio_pin_get),
        //DEVMETHOD(gpio_pin_set,               vtgpio_pin_set),
        //DEVMETHOD(gpio_pin_toggle,    vtgpio_pin_toggle),

        /* VTGPIO controller methods */
        //DEVMETHOD(virtio_gpio_get_names, vtgpio_get_names);

        DEVMETHOD_END
};

static driver_t vtgpio_driver = {
    	"vtgpio",
    	vtgpio_methods,
    	sizeof(struct vtgpio_softc)
};

static struct virtio_feature_desc vtgpio_feature_desc[] = {
        { VIRTIO_GPIO_F_IRQ, "GpioInterSupp" },
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

VIRTIO_DRIVER_MODULE(virtio_gpio, vtgpio_driver, vtgpio_modevent, NULL);
MODULE_VERSION(virtio_gpio, 1);
MODULE_DEPEND(virtio_gpio, virtio, 1, 1, 1);

VIRTIO_SIMPLE_PNPINFO(virtio_gpio, VIRTIO_ID_GPIO, "VirtIO General Purpose Input/Output Device");

#define VTGPIO_REQ_MTX(_sc)			&(_sc)->req_lock
#define VTGPIO_REQ_LOCK_INIT(_sc, _name)	mtx_init(VTGPIO_REQ_MTX((_sc)), _name, "VirtIO GPIO Request Lock", MTX_DEF)

#define VTGPIO_REQ_LOCK(_sc)			mtx_lock(VTGPIO_REQ_MTX((_sc)))
#define VTGPIO_REQ_UNLOCK(_sc)			mtx_unlock(VTGPIO_REQ_MTX((_sc)))
#define VTGPIO_REQ_LOCK_DESTROY(_sc)		mtx_destroy(VTGPIO_REQ_MTX((_sc)))

#define VTGPIO_EVNT_MTX(_sc)			&(_sc)->evnt_lock
#define VTGPIO_EVNT_LOCK_INIT(_sc, _name)       mtx_init(VTGPIO_EVNT_MTX((_sc)), _name, "VirtIO GPIO Event Lock", MTX_DEF)

#define VTGPIO_EVNT_LOCK(_sc)                   mtx_lock(VTGPIO_EVNT_MTX((_sc)))
#define VTGPIO_EVNT_UNLOCK(_sc)                 mtx_unlock(VTGPIO_EVNT_MTX((_sc)))
#define VTGPIO_EVNT_LOCK_DESTROY(_sc)           mtx_destroy(VTGPIO_EVNT_MTX((_sc)))

#define vtgpio_modern(_sc)                      (((_sc)->vtgpio_features & VIRTIO_F_VERSION_1) != 0)
#define vtgpio_htog16(_sc, _uint16_t)           virtio_htog16(vtgpio_modern(_sc), _uint16_t)
#define vtgpio_htog32(_sc, _uint32_t)           virtio_htog32(vtgpio_modern(_sc), _uint32_t)

#define VTGPIO_ALLOWED_CAPS                     (GPIO_PIN_INPUT | GPIO_PIN_OUTPUT | GPIO_INTR_EDGE_BOTH | \
                                                GPIO_INTR_EDGE_RISING | GPIO_INTR_EDGE_FALLING | \
                                                GPIO_INTR_LEVEL_HIGH | GPIO_INTR_LEVEL_LOW )

static int
vtgpio_probe(device_t dev)
{
        device_printf(dev, "Probe!\n");
	return (VIRTIO_SIMPLE_PROBE(dev, virtio_gpio));
}

static int
vtgpio_attach(device_t dev)
{
	device_printf(dev, "Attach!\n");

    	struct vtgpio_softc *sc;
    	int error;
    	
    	sc = device_get_softc(dev);
    	sc->vtgpio_dev = dev;
        	sc->req = malloc(sizeof(*sc->req), M_DEVBUF, M_WAITOK | M_ZERO);
        	sc->res = malloc(sizeof(*sc->res), M_DEVBUF, M_WAITOK | M_ZERO);

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
        	device_printf(dev, "unable to setup virtqueue interrupts\n");
        	goto fail;
    	}
    	virtqueue_enable_intr(sc->req_vq);

        error = vtgpio_read_config(sc);
        if (error) {
                device_printf(dev, "unable to read from config\n");
                goto fail;
        }

        sc->vtgpio_busdev = gpiobus_add_bus(dev);
        if (sc->vtgpio_busdev == NULL) {
                device_printf(dev, "unable to add gpiobus child\n");
                error = ENXIO;
                goto fail;
        }
        bus_attach_children(dev);

fail:
    	if (error) vtgpio_detach(dev);

    	return (error);
}

static int
vtgpio_detach(device_t dev)
{
        device_printf(dev, "Detach!\n");
    	struct vtgpio_softc *sc;
    	sc = device_get_softc(dev);

        if (sc->vtgpio_busdev)
                gpiobus_detach_bus(dev);

    	if (device_is_attached(dev))
                	vtgpio_stop(sc);

    	VTGPIO_REQ_LOCK_DESTROY(sc);

        free(sc->req, M_DEVBUF);
        free(sc->res, M_DEVBUF);

	return (0);
}

static int
vtgpio_negotiate_features(struct vtgpio_softc *sc)
{
    	device_t dev;
    	uint64_t features;
    	
    	dev = sc->vtgpio_dev;
    	features = VIRTIO_GPIO_F_IRQ;
    	
    	sc->vtgpio_features = virtio_negotiate_features(dev, features);

    	return (virtio_finalize_features(dev));
}

static int
vtgpio_setup_features(struct vtgpio_softc *sc)
{
    	int error;

    	error = vtgpio_negotiate_features(sc);
    	if (error) {
        	return (error);
    	}

    	return (0);
}

static int
vtgpio_alloc_virtqueue(struct vtgpio_softc *sc)
{
    	device_t dev = sc->vtgpio_dev;

    	// we need to set up 1 vq by default, event queue requires feature
    	int nvqs = 1;
    	struct vq_alloc_info vq_info[2]; // max 2 vq

    	VQ_ALLOC_INFO_INIT(&vq_info[0], 0, vtgpio_req_vq_intr, sc, &sc->req_vq,
        	"%s request", device_get_nameunit(dev));

    	/* TODO: event Q implementation
        	something like this will maybe work
    	if (dev->vtgpio_features & features) {
        	VQ_ALLOC_INFO_INIT(&vq_info[1], 0, vtgpio_evnt_vq_intr, sc, &sc->evnt_vq, "%s event", device_get_name_unit(dev));
        	++nvqs;
    	} */

    	return (virtio_alloc_virtqueues(dev, nvqs, vq_info));
}

static void
vtgpio_req_vq_intr(void *xsc)
{
        struct vtgpio_softc *sc;

        sc = xsc;

        VTGPIO_REQ_LOCK(sc);
        wakeup_one(sc);
        VTGPIO_REQ_UNLOCK(sc);
}

static int
vtgpio_read_config(struct vtgpio_softc *sc)
{
        // read_dev_config_x where x is the number of bytes
        uint16_t desired_ngpio = virtio_read_dev_config_2(sc->vtgpio_dev,
                offsetof(struct virtio_gpio_config, ngpio));
        uint32_t desired_nsgpio = virtio_read_dev_config_4(sc->vtgpio_dev,
                offsetof(struct virtio_gpio_config, gpio_names_size));

        desired_ngpio = vtgpio_htog16(sc, desired_ngpio);
        if (desired_ngpio == 0)
                return (1); // throw an error if there's 0 pins (what is driver supposed to do then lol)

        sc->ngpio = desired_ngpio;
        sc->gpio_names_size = vtgpio_htog32(sc, desired_nsgpio); // this can be 0

        return (0);
}

static void
vtgpio_stop(struct vtgpio_softc *sc)
{
    	virtqueue_disable_intr(sc->req_vq);
    	// if (dev->vtgpio_features & features) virtqueue_disable_intr(sc->evnt_vq);

    	virtio_stop(sc->vtgpio_dev);
}

static device_t
vtgpio_get_bus(device_t dev)
{
        struct vtgpio_softc *sc;
        sc = device_get_softc(dev);

        return (sc->vtgpio_busdev);
}

static int
vtgpio_pin_max(device_t dev, int *maxpin)
{
        struct vtgpio_softc *sc;
        sc = device_get_softc(dev);

        *maxpin = sc->ngpio - 1;

        return (0);
}

static int
vtgpio_pin_getname(device_t dev, uint32_t pin, char *name)
{
        struct vtgpio_softc *sc;
        sc = device_get_softc(dev);

        if (pin >= sc->ngpio)
                return (EINVAL);

        snprintf(name, GPIOMAXNAME, "p%u", pin);
        name[GPIOMAXNAME - 1] = '\0';

        return (0);
}

static int
vtgpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
        struct vtgpio_softc *sc;
        sc = device_get_softc(dev);

        if (pin >= sc->ngpio)
                return (EINVAL);

        *caps = VTGPIO_ALLOWED_CAPS;

        return (0);
}

static int
vtgpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
        struct vtgpio_softc *sc;
        sc = device_get_softc(dev);

        if (pin >= sc->ngpio)
                return (EINVAL);

        struct sglist sg;
        struct sglist_seg segs[2];
        void *c;
        int error __diagused;
        *flags = 0;

        sglist_init(&sg, 2, segs);

        VTGPIO_REQ_LOCK(sc);
        sc->req->type = vtgpio_htog16(sc, VIRTIO_GPIO_MSG_GET_DIRECTION);
        sc->req->gpio = vtgpio_htog16(sc, (uint16_t)pin);

        error = sglist_append(&sg, sc->req, sizeof(*sc->req));
        KASSERT(error == 0, ("error adding virtio_gpio_request to sglist"));

        error = sglist_append(&sg, sc->res, sizeof(*sc->res));
        KASSERT(error == 0, ("error adding virtio_gpio_response to sglist"));

        error = virtqueue_enqueue(sc->req_vq, sc->req_vq, &sg, 1, 1);
        KASSERT(error == 0, ("error enqueuing virtio_gpio_request to virtqueue"));
        virtqueue_notify(sc->req_vq);

        while ((c = virtqueue_dequeue(sc->req_vq, NULL)) == NULL)
                msleep(sc, VTGPIO_REQ_MTX(sc), 0, "vtgpgf", 0);

        if (sc->res->status != VIRTIO_GPIO_STATUS_OK) {
                VTGPIO_REQ_UNLOCK(sc);
                return (EIO);
        }

        if (sc->res->value == VIRTIO_GPIO_DIRECTION_IN)
                *flags |= GPIO_PIN_INPUT;
        else if (sc->res->value == VIRTIO_GPIO_DIRECTION_OUT)
                *flags |= GPIO_PIN_OUTPUT;
        // *flags is already 0 if sc->res->value = VIRTIO_GPIO_DIRECTION_NONE
        VTGPIO_REQ_UNLOCK(sc);

        return (0);
}

static int
vtgpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
        struct vtgpio_softc *sc;
        sc = device_get_softc(dev);

        if (pin >= sc->ngpio)
                return (EINVAL);

        uint32_t vtgpioflag;

        if (flags & GPIO_PIN_OUTPUT)
                vtgpioflag = VIRTIO_GPIO_DIRECTION_OUT;
        else if (flags & GPIO_PIN_INPUT)
                vtgpioflag = VIRTIO_GPIO_DIRECTION_IN;
        else if (flags == 0)
                vtgpioflag = VIRTIO_GPIO_DIRECTION_NONE;
        else
                return (EINVAL);

        struct sglist sg;
        struct sglist_seg segs[2];
        void *c;
        int error __diagused;

        sglist_init(&sg, 2, segs);

        VTGPIO_REQ_LOCK(sc);
        sc->req->type = vtgpio_htog16(sc, VIRTIO_GPIO_MSG_SET_DIRECTION);
        sc->req->gpio = vtgpio_htog16(sc, (uint16_t)pin);
        sc->req->value = vtgpio_htog32(sc, vtgpioflag);

        error = sglist_append(&sg, sc->req, sizeof(*sc->req));
        KASSERT(error == 0, ("error adding virtio_gpio_request to sglist"));

        error = sglist_append(&sg, sc->res, sizeof(*sc->res));
        KASSERT(error == 0, ("error adding virtio_gpio_response to sglist"));

        error = virtqueue_enqueue(sc->req_vq, sc->req_vq, &sg, 1, 1);
        KASSERT(error == 0, ("error enqueuing virtio_gpio_request to virtqueue"));
        virtqueue_notify(sc->req_vq);

        while ((c = virtqueue_dequeue(sc->req_vq, NULL)) == NULL)
                msleep(sc, VTGPIO_REQ_MTX(sc), 0, "vtgpgf", 0);

        if (sc->res->status != VIRTIO_GPIO_STATUS_OK)
                error = EIO;

        VTGPIO_REQ_UNLOCK(sc);

        return (error);
} 
