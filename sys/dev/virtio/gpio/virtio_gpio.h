/*-
 * This header is BSD licensed so anyone can use the definitions to implement
 * compatible drivers/servers.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of IBM nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL IBM OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Copyright (C) Aaron Espinoza <acesp25@FreeBSD.org>, 2026
 */

#ifndef _VIRTIO_GPIO_H
#define _VIRTIO_GPIO_H

/* Virtio GPIO Feature bit */
#define VIRTIO_GPIO_F_IRQ                       0
 
/* GPIO message types */ 
#define VIRTIO_GPIO_MSG_GET_LINE_NAMES          0x0001 
#define VIRTIO_GPIO_MSG_GET_DIRECTION           0x0002 
#define VIRTIO_GPIO_MSG_SET_DIRECTION           0x0003 
#define VIRTIO_GPIO_MSG_GET_VALUE               0x0004 
#define VIRTIO_GPIO_MSG_SET_VALUE               0x0005 
#define VIRTIO_GPIO_MSG_SET_IRQ_TYPE            0x0006 
 
/* GPIO Direction types */ 
#define VIRTIO_GPIO_DIRECTION_NONE              0x00 
#define VIRTIO_GPIO_DIRECTION_OUT               0x01 
#define VIRTIO_GPIO_DIRECTION_IN                0x02 

/* GPIO interrupt types */ 
#define VIRTIO_GPIO_IRQ_TYPE_NONE               0x00 
#define VIRTIO_GPIO_IRQ_TYPE_EDGE_RISING        0x01 
#define VIRTIO_GPIO_IRQ_TYPE_EDGE_FALLING       0x02 
#define VIRTIO_GPIO_IRQ_TYPE_EDGE_BOTH          0x03 
#define VIRTIO_GPIO_IRQ_TYPE_LEVEL_HIGH         0x04 
#define VIRTIO_GPIO_IRQ_TYPE_LEVEL_LOW          0x08 

struct virtio_gpio_config {
        uint16_t ngpio;
        uint8_t padding[2];
        uint32_t gpio_names_size; /* little endian */
} __packed;

struct virtio_gpio_request {
        /* all are little endian */
        uint16_t type;
        uint16_t gpio;
        uint32_t value;
} __packed;
 
struct virtio_gpio_response {
        uint8_t status;
        uint8_t value;
};

/* Possible values of the status field */ 
#define VIRTIO_GPIO_STATUS_OK                   0x0 
#define VIRTIO_GPIO_STATUS_ERR                  0x1
 
struct virtio_gpio_response_names {
        uint8_t status;
        uint8_t value[];
};

/* Virtio GPIO IRQ Request / Response */
struct virtio_gpio_irq_request {
        uint16_t gpio; /* little endian */
} __packed;

struct virtio_gpio_irq_response {
        uint8_t status;
};

/* Possible values of the interrupt status field */
#define VIRTIO_GPIO_IRQ_STATUS_INVALID          0x0
#define VIRTIO_GPIO_IRQ_STATUS_VALID            0x1

#endif /* _VIRTIO_GPIO_H */
