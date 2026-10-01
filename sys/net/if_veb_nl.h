/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Aaron Espinoza <acesp25@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _NET_IF_VEB_NL_H_
#define _NET_IF_VEB_NL_H_

#define VEB_NL_FAMILY_NAME	"veb"
#define VEB_NL_FAMILY_VERSION	1

enum veb_nl_rtable_type {
	VEB_NL_RTABLE_TYPE_DYNAMIC = 0,
	VEB_NL_RTABLE_TYPE_STATIC = 1,
	VEB_NL_RTABLE_TYPE_STICKY = 2,
};

enum veb_nl_cmd {
	VEB_NL_CMD_UNSPEC,
	VEB_NL_CMD_GETRTABLE,
	VEB_NL_CMD_ADDSTATIC,
	VEB_NL_CMD_DELADDR,
	__VEB_NL_CMD_MAX,
};
#define VEB_NL_CMD_MAX	(__VEB_NL_CMD_MAX - 1)

enum veb_nl_rtable_attr {
	VEB_NL_RTABLE_UNSPEC,
	VEB_NL_RTABLE_MAC,	/* binary, ETHER_ADDR_LEN */
	VEB_NL_RTABLE_VLAN,	/* u16 */
	VEB_NL_RTABLE_IFNAME,	/* string, request-only */
	VEB_NL_RTABLE_PORT,	/* string */
	VEB_NL_RTABLE_TYPE,	/* u8 */
	VEB_NL_RTABLE_EXPIRE,	/* u32, reply-only */
	__VEB_NL_RTABLE_MAX,
};
#define VEB_NL_RTABLE_MAX	(__VEB_NL_RTABLE_MAX - 1)

#endif	/* !_NET_IF_VEB_NL_H_ */
