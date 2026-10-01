/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Aaron Espinoza <acesp25@FreeBSD.org>
 */

#ifndef NET_IF_VEB_NL_H
#define NET_IF_VEB_NL_H

#define VEB_NL_FAMILY_NAME	"veb"

enum veb_nl_cmd {
	VEB_NL_CMD_UNSPEC,
	VEB_NL_CMD_GETRTABLE,	/* dump */
	__VEB_NL_CMD_MAX,
};
#define VEB_NL_CMD_MAX	(__VEB_NL_CMD_MAX - 1)

enum veb_nl_rtable_attr {
	VEB_NL_RTABLE_UNSPEC,
	VEB_NL_RTABLE_MAC,	/* binary, ETHER_ADDR_LEN */
	VEB_NL_RTABLE_IFNAME,	/* string */
	VEB_NL_RTABLE_PORT,	/* string */
	VEB_NL_RTABLE_STATIC,	/* bool */
	VEB_NL_RTABLE_EXPIRE,	/* u32 */
	__VEB_NL_RTABLE_MAX,
};
#define VEB_NL_RTABLE_MAX	(__VEB_NL_RTABLE_MAX - 1)

#endif	/* !_NET_IF_VEB_NL_H */
