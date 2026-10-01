/*
 * Copyright (c) 2026 Aaron Espinoza <acesp25@freebsd.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
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
