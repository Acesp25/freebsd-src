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

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/nv.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_strings.h>
#include <net/if_veb_nl.h>

#include <netlink/netlink.h>
#include <netlink/netlink_generic.h>
#include <netlink/netlink_snl.h>
#include <netlink/netlink_snl_generic.h>
#include <netlink/netlink_snl_route.h>

#include <netdb.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>

#include "ifconfig.h"
#include "ifconfig_netlink.h"

struct nl_parsed_veb {
	struct snl_parray	ifla_ports;
	uint32_t		ifla_timeout;
};

struct nl_parsed_veb_port {
	char	*ifla_ifname;
	bool	ifla_learning;
	bool	ifla_discover;
	bool	ifla_sticky;
	bool	ifla_private;
	bool	ifla_vport;
};

struct nla_veb_info {
	const char		*kind;
	struct nl_parsed_veb	data;
};

struct nla_veb_link {
	uint32_t		ifi_index;
	struct nla_veb_info	linkinfo;
};

struct veb_rt_entry {
	char		*port;
	struct nlattr	*mac;
	bool		 is_static;
	uint32_t	 expire;
};

#define _OUT(_field)	offsetof(struct nl_parsed_veb_port, _field)
static const struct snl_attr_parser nla_veb_port[] = {
	{ .type = IFLA_VEB_PORT_IFNAME, .off = _OUT(ifla_ifname),
	    .cb = snl_attr_get_string },
	{ .type = IFLA_VEB_PORT_LEARNING, .off = _OUT(ifla_learning),
	    .cb = snl_attr_get_bool },
	{ .type = IFLA_VEB_PORT_DISCOVER, .off = _OUT(ifla_discover),
	    .cb = snl_attr_get_bool },
	{ .type = IFLA_VEB_PORT_STICKY, .off = _OUT(ifla_sticky),
	    .cb = snl_attr_get_bool },
	{ .type = IFLA_VEB_PORT_PRIVATE, .off = _OUT(ifla_private),
	    .cb = snl_attr_get_bool },
	{ .type = IFLA_VEB_PORT_VPORT, .off = _OUT(ifla_vport),
	    .cb = snl_attr_get_bool },
};
#undef _OUT
SNL_DECLARE_ATTR_PARSER_EXT(veb_port_parser, sizeof(struct nl_parsed_veb_port), nla_veb_port, NULL);

#define _OUT(_field)	offsetof(struct nl_parsed_veb, _field)
static const struct snl_attr_parser nla_veb[] = {
	{ .type = IFLA_VEB_PORT, .off = _OUT(ifla_ports),
	    .arg = &veb_port_parser, .cb = snl_attr_get_multi },
	{ .type = IFLA_VEB_RTABLE_TIMEOUT, .off = _OUT(ifla_timeout),
	    .cb = snl_attr_get_uint32 },
};
#undef _OUT
SNL_DECLARE_ATTR_PARSER(veb_parser, nla_veb);

#define _OUT(_field)	offsetof(struct nla_veb_info, _field)
static const struct snl_attr_parser ap_veb_linkinfo[] = {
	{ .type = IFLA_INFO_KIND, .off = _OUT(kind), .cb = snl_attr_get_string },
	{ .type = IFLA_INFO_DATA, .off = _OUT(data),
	    .arg = &veb_parser, .cb = snl_attr_get_nested },
};
#undef _OUT
SNL_DECLARE_ATTR_PARSER(veb_linkinfo_parser, ap_veb_linkinfo);

#define _OUT(_field) offsetof(struct veb_rt_entry, _field)
static const struct snl_attr_parser nla_veb_rtable[] = {
	{ .type = VEB_NL_RTABLE_MAC,    .off = _OUT(mac),       .cb = snl_attr_get_nla },
	{ .type = VEB_NL_RTABLE_PORT,   .off = _OUT(port),      .cb = snl_attr_get_string },
	{ .type = VEB_NL_RTABLE_STATIC, .off = _OUT(is_static), .cb = snl_attr_get_bool },
	{ .type = VEB_NL_RTABLE_EXPIRE,    .off = _OUT(expire),       .cb = snl_attr_get_uint32 },
};
#undef _OUT
SNL_DECLARE_GENL_PARSER(veb_rtable_parser, nla_veb_rtable);

#define _IN(_field)	offsetof(struct ifinfomsg, _field)
#define _OUT(_field)	offsetof(struct nla_veb_link, _field)
static const struct snl_attr_parser ap_veb_link[] = {
	{ .type = IFLA_LINKINFO, .off = _OUT(linkinfo),
	    .arg = &veb_linkinfo_parser, .cb = snl_attr_get_nested },
};
static const struct snl_field_parser fp_veb_link[] = {
	{ .off_in = _IN(ifi_index), .off_out = _OUT(ifi_index),
	    .cb = snl_field_get_uint32 },
};
#undef _IN
#undef _OUT
SNL_DECLARE_PARSER(veb_link_parser, struct ifinfomsg, fp_veb_link, ap_veb_link);

static const struct snl_hdr_parser *all_parsers[] = {
	&veb_parser,
	&veb_port_parser,
	&veb_link_parser,
	&veb_linkinfo_parser,
};

static void
veb_print_port(const struct nl_parsed_veb_port *port)
{
	const struct {
		bool		on;
		const char	*name;
	} f[] = {
		{ port->ifla_learning,	"LEARNING" },
		{ port->ifla_discover,	"DISCOVER" },
		{ port->ifla_sticky,	"STICKY" },
		{ port->ifla_private,	"PRIVATE" },
		{ port->ifla_vport,	"VPORT" },
	};
	const char *sep = "";

	printf("\tport: %s flags=<", port->ifla_ifname);
	for (size_t i = 0; i < nitems(f); i++) {
		if (f[i].on) {
			printf("%s%s", sep, f[i].name);
			sep = ",";
		}
	}
	printf(">\n");
}

static inline void
veb_nl_init(if_ctx *ctx, struct snl_writer *nw, uint32_t flags)
{
	struct nlmsghdr *hdr;

	snl_init_writer(ctx->io_ss, nw);
	hdr = snl_create_msg_request(nw, NL_RTM_NEWLINK);
	hdr->nlmsg_flags |= flags;
	snl_reserve_msg_object(nw, struct ifinfomsg);
	snl_add_msg_attr_string(nw, IFLA_IFNAME, ctx->ifname);
}

static inline void
veb_nl_fini(if_ctx *ctx, struct snl_writer *nw)
{
	struct nlmsghdr *hdr;
	struct snl_errmsg_data errmsg = {};

	hdr = snl_finalize_msg(nw);
	if (hdr == NULL || !snl_send_message(ctx->io_ss, hdr))
		err(1, "unable to send netlink message");

	if (!snl_read_reply_code(ctx->io_ss, hdr->nlmsg_seq, &errmsg)) {
		errx(errmsg.error, "%s", errmsg.error_str);
	}
}

static void
veb_status_nl(if_ctx *ctx)
{
	struct snl_writer nw;
	struct nlmsghdr *hdr;
	struct nla_veb_link veb_link = {};
	uint32_t i;

	snl_init_writer(ctx->io_ss, &nw);
	hdr = snl_create_msg_request(&nw, NL_RTM_GETLINK);
	snl_reserve_msg_object(&nw, struct ifinfomsg);
	snl_add_msg_attr_string(&nw, IFLA_IFNAME, ctx->ifname);

	if ((hdr = snl_finalize_msg(&nw)) == NULL ||
	    !snl_send_message(ctx->io_ss, hdr))
		return;

	hdr = snl_read_reply(ctx->io_ss, hdr->nlmsg_seq);
	if (hdr == NULL || hdr->nlmsg_type != NL_RTM_NEWLINK)
		return;
	if (!snl_parse_nlmsg(ctx->io_ss, hdr, &veb_link_parser, &veb_link))
		return;
	if (veb_link.linkinfo.kind == NULL ||
	    strcmp(veb_link.linkinfo.kind, "veb") != 0)
		return;

	printf("\ttimeout: %u\n", veb_link.linkinfo.data.ifla_timeout);

	for (i = 0; i < veb_link.linkinfo.data.ifla_ports.count; i++)
		veb_print_port(veb_link.linkinfo.data.ifla_ports.items[i]);
}

static void
veb_create_nl(if_ctx *ctx, struct ifreq *ifr)
{
	struct snl_writer nw = {};
	struct nlmsghdr *hdr;
	int off;

	snl_init_writer(ctx->io_ss, &nw);
	hdr = snl_create_msg_request(&nw, RTM_NEWLINK);
	hdr->nlmsg_flags |= (NLM_F_CREATE | NLM_F_EXCL);

	snl_reserve_msg_object(&nw, struct ifinfomsg);
	snl_add_msg_attr_string(&nw, IFLA_IFNAME, ifr->ifr_name);

	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
	snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "veb");
	snl_end_attr_nested(&nw, off);

	veb_nl_fini(ctx, &nw);
}

static void
veb_port_cmd(if_ctx *ctx, int attr, const char *ifname)
{
	struct snl_writer nw = {};
	int off, off2;

	veb_nl_init(ctx, &nw, 0);
	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
	snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "veb");

	off2 = snl_add_msg_attr_nested(&nw, IFLA_INFO_DATA);
	snl_add_msg_attr_string(&nw, attr, ifname);

	snl_end_attr_nested(&nw, off2);
	snl_end_attr_nested(&nw, off);

	veb_nl_fini(ctx, &nw);
}

static void
setveb_addport(if_ctx *ctx, const char *ifname, int dummy __unused)
{
	veb_port_cmd(ctx, IFLA_VEB_ADDPORT, ifname);
}

static void
setveb_delport(if_ctx *ctx, const char *ifname, int dummy __unused)
{
	veb_port_cmd(ctx, IFLA_VEB_DELPORT, ifname);
}

static void
veb_port_flag(if_ctx *ctx, const char *ifname, int attr, bool on)
{
	struct snl_writer nw = {};
	int off, off2, off3;

	veb_nl_init(ctx, &nw, 0);
	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
	snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "veb");

	off2 = snl_add_msg_attr_nested(&nw, IFLA_INFO_DATA);
	off3 = snl_add_msg_attr_nested(&nw, IFLA_VEB_SETPORT);
	snl_add_msg_attr_string(&nw, IFLA_VEB_PORT_IFNAME, ifname);
	snl_add_msg_attr_bool(&nw, attr, on);
	snl_end_attr_nested(&nw, off3);

	snl_end_attr_nested(&nw, off2);
	snl_end_attr_nested(&nw, off);

	veb_nl_fini(ctx, &nw);
}

static void
setveb_learn(if_ctx *ctx, const char *ifname, int dummy __unused)
{
	veb_port_flag(ctx, ifname, IFLA_VEB_PORT_LEARNING, true);
}

static void
setveb_unlearn(if_ctx *ctx, const char *ifname, int dummy __unused)
{
	veb_port_flag(ctx, ifname, IFLA_VEB_PORT_LEARNING, false);
}

static void
setveb_discover(if_ctx *ctx, const char *ifname, int dummy __unused)
{
	veb_port_flag(ctx, ifname, IFLA_VEB_PORT_DISCOVER, true);
}

static void
setveb_undiscover(if_ctx *ctx, const char *ifname, int dummy __unused)
{
	veb_port_flag(ctx, ifname, IFLA_VEB_PORT_DISCOVER, false);
}

static void
setveb_sticky(if_ctx *ctx, const char *ifname, int dummy __unused)
{
	veb_port_flag(ctx, ifname, IFLA_VEB_PORT_STICKY, true);
}

static void
setveb_unsticky(if_ctx *ctx, const char *ifname, int dummy __unused)
{
	veb_port_flag(ctx, ifname, IFLA_VEB_PORT_STICKY, false);
}

static void
setveb_private(if_ctx *ctx, const char *ifname, int dummy __unused)
{
	veb_port_flag(ctx, ifname, IFLA_VEB_PORT_PRIVATE, true);
}

static void
setveb_unprivate(if_ctx *ctx, const char *ifname, int dummy __unused)
{
	veb_port_flag(ctx, ifname, IFLA_VEB_PORT_PRIVATE, false);
}

static void
setveb_settimeout(if_ctx *ctx, const char *arg, int dummy __unused)
{
	struct snl_writer nw = {};
	const char *errstr;
	uint32_t timeout;
	int off, off2;

	timeout = strtonum(arg, 0, UINT32_MAX, &errstr);
	if (errstr != NULL)
		errx(1, "timeout %s: %s", arg, errstr);

	veb_nl_init(ctx, &nw, 0);
	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
	snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "veb");

	off2 = snl_add_msg_attr_nested(&nw, IFLA_INFO_DATA);
	snl_add_msg_attr_u32(&nw, IFLA_VEB_RTABLE_TIMEOUT, timeout);
	snl_end_attr_nested(&nw, off2);

	snl_end_attr_nested(&nw, off);

	veb_nl_fini(ctx, &nw);
}

static void
setveb_showaddr(if_ctx *ctx, const char *val __unused, int dummy __unused)
{
	struct snl_state ss = {};
	struct snl_errmsg_data e = {};
	struct snl_writer nw;
	struct nlmsghdr *hdr;
	struct ether_addr ea;
	uint32_t seq_id;
	int family_id;

	if (!snl_init(&ss, NETLINK_GENERIC))
		err(1, "unable to open netlink socket");

	family_id = snl_get_genl_family(&ss, VEB_NL_FAMILY_NAME);
	if (family_id == 0) {
		errx(1, "veb netlink family not found (is if_veb loaded?)");
	}

	snl_init_writer(&ss, &nw);
	hdr = snl_create_genl_msg_request(&nw, family_id, VEB_NL_CMD_GETRTABLE);
	hdr->nlmsg_flags |= NLM_F_DUMP;
	snl_add_msg_attr_string(&nw, VEB_NL_RTABLE_IFNAME, ctx->ifname);

	hdr = snl_finalize_msg(&nw);
	if (hdr == NULL) {
		errx(1, "unable to build rtable request");
	}
	seq_id = hdr->nlmsg_seq;
	if (!snl_send_message(&ss, hdr)) {
		err(1, "unable to send rtable request");
	}

	while ((hdr = snl_read_reply_multi(&ss, seq_id, &e)) != NULL) {
		struct veb_rt_entry rt = {};

		if (!snl_parse_nlmsg(&ss, hdr, &veb_rtable_parser, &rt))
			continue;
		if (rt.port == NULL || rt.mac == NULL ||
		    NLA_DATA_LEN(rt.mac) != ETHER_ADDR_LEN)
			continue;

		memcpy(ea.octet, NLA_DATA(rt.mac), ETHER_ADDR_LEN);
		printf("%s %s %u %s\n", ether_ntoa(&ea), rt.port, rt.expire,
		    rt.is_static ? "static" : "dynamic");
	}

	if (e.error != 0)
		errc(1, e.error, "%s",
		    e.error_str != NULL ? e.error_str : "rtable dump failed");
	snl_free(&ss);
}

static struct cmd veb_cmds[] = {
	DEF_CMD_ARG("addport",		setveb_addport),
	DEF_CMD_ARG("delport",		setveb_delport),
	DEF_CMD_ARG("veblearn",		setveb_learn),
	DEF_CMD_ARG("-veblearn",	setveb_unlearn),
	DEF_CMD_ARG("vebdiscover",	setveb_discover),
	DEF_CMD_ARG("-vebdiscover",	setveb_undiscover),
	DEF_CMD_ARG("vebsticky",	setveb_sticky),
	DEF_CMD_ARG("-vebsticky",	setveb_unsticky),
	DEF_CMD_ARG("vebprivate",	setveb_private),
	DEF_CMD_ARG("-vebprivate",	setveb_unprivate),
	DEF_CMD_ARG("vebtimeout",	setveb_settimeout),
	DEF_CMD("vebaddr", 1,		setveb_showaddr),
};

static struct afswtch af_veb = {
	.af_name		= "af_veb",
	.af_af			= AF_UNSPEC,
	.af_other_status	= veb_status_nl,
};

static __constructor void
veb_constructor(void)
{
	for (size_t i = 0; i < nitems(veb_cmds); i++)
		cmd_register(&veb_cmds[i]);

	af_register(&af_veb);
	clone_setdefcallback_prefix("veb", veb_create_nl);
	SNL_VERIFY_PARSERS(all_parsers);
}
