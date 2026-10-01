/*
 * Copyright (c) 2026 Aaron Espinoza <acesp25@freebsd.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/ioctl.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_veb_nl.h>

#include <netlink/netlink.h>
#include <netlink/netlink_generic.h>
#include <netlink/netlink_snl.h>
#include <netlink/netlink_snl_generic.h>
#include <netlink/netlink_snl_route.h>
#include <netlink/route/interface.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>

#include "libifconfig.h"
#include "libifconfig_internal.h"

struct nl_parsed_veb {
	struct snl_parray	ifla_ports;
	uint32_t		ifla_timeout;
	uint32_t		ifla_maxaddr;
	uint32_t		ifla_exceeded;
};

struct nl_parsed_veb_port {
	char		*ifla_ifname;
	uint32_t	ifla_maxaddr;
	bool		ifla_learning;
	bool		ifla_discover;
	bool		ifla_sticky;
	bool		ifla_private;
	bool		ifla_vport;
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
	ether_vlanid_t	vlan;
	struct nlattr	*mac;
	uint8_t		type;
	uint32_t	expire;
};

#define _OUT(_field)	offsetof(struct nl_parsed_veb_port, _field)
static const struct snl_attr_parser nla_veb_port[] = {
	{ .type = IFLA_VEB_PORT_IFNAME, .off = _OUT(ifla_ifname),
	    .cb = snl_attr_get_string },
	{ .type = IFLA_VEB_PORT_MAXADDR, .off = _OUT(ifla_maxaddr),
	    .cb = snl_attr_get_uint32 },
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
SNL_DECLARE_ATTR_PARSER_EXT(veb_port_parser,
    sizeof(struct nl_parsed_veb_port), nla_veb_port, NULL);

#define _OUT(_field)	offsetof(struct nl_parsed_veb, _field)
static const struct snl_attr_parser nla_veb[] = {
	{ .type = IFLA_VEB_PORT,	.off = _OUT(ifla_ports),
	    .arg = &veb_port_parser,	.cb = snl_attr_get_multi },
	{ .type = IFLA_VEB_RTABLE_TIMEOUT,	.off = _OUT(ifla_timeout),
	    .cb = snl_attr_get_uint32 },
	{ .type = IFLA_VEB_RTABLE_MAXADDR,	.off = _OUT(ifla_maxaddr),
	    .cb = snl_attr_get_uint32 },
	{ .type = IFLA_VEB_RTABLE_EXCEEDED,	.off = _OUT(ifla_exceeded),
	    .cb = snl_attr_get_uint32 },
};
#undef _OUT
SNL_DECLARE_ATTR_PARSER(veb_parser, nla_veb);

#define _OUT(_field)	offsetof(struct nla_veb_info, _field)
static const struct snl_attr_parser ap_veb_linkinfo[] = {
	{ .type = IFLA_INFO_KIND, .off = _OUT(kind),
	    .cb = snl_attr_get_string },
	{ .type = IFLA_INFO_DATA, .off = _OUT(data),
	    .arg = &veb_parser, .cb = snl_attr_get_nested },
};
#undef _OUT
SNL_DECLARE_ATTR_PARSER(veb_linkinfo_parser, ap_veb_linkinfo);

#define _OUT(_field) offsetof(struct veb_rt_entry, _field)
static const struct snl_attr_parser nla_veb_rtable[] = {
	{ .type = VEB_NL_RTABLE_MAC, .off = _OUT(mac),
	    .cb = snl_attr_get_nla },
	{ .type = VEB_NL_RTABLE_VLAN, .off = _OUT(vlan),
	    .cb = snl_attr_get_uint16 },
	{ .type = VEB_NL_RTABLE_PORT, .off = _OUT(port),
	    .cb = snl_attr_get_string },
	{ .type = VEB_NL_RTABLE_TYPE, .off = _OUT(type),
	    .cb = snl_attr_get_uint8 },
	{ .type = VEB_NL_RTABLE_EXPIRE, .off = _OUT(expire),
	    .cb = snl_attr_get_uint32 },
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

static inline void
veb_nl_init(struct snl_state *ss, struct snl_writer *nw,
    const char *name, uint32_t flags)
{
	struct nlmsghdr *hdr;

	snl_init_writer(ss, nw);
	hdr = snl_create_msg_request(nw, NL_RTM_NEWLINK);
	hdr->nlmsg_flags |= flags;
	snl_reserve_msg_object(nw, struct ifinfomsg);
	snl_add_msg_attr_string(nw, IFLA_IFNAME, name);
}

static inline int
veb_nl_fini(ifconfig_handle_t *h, struct snl_state *ss, struct snl_writer *nw)
{
	struct snl_errmsg_data e = {};
	struct nlmsghdr *hdr;

	if ((hdr = snl_finalize_msg(nw)) == NULL) {
		ifconfig_error(h, NETLINK, ENOMEM);
		return (-1);
	}
	if (!snl_send_message(ss, hdr)) {
		ifconfig_error(h, NETLINK, EIO);
		return (-1);
	}
	if (!snl_read_reply_code(ss, hdr->nlmsg_seq, &e)) {
		ifconfig_error(h, NETLINK, e.error);
		return (-1);
	}

	return (0);
}

static inline int
veb_genl_init(ifconfig_handle_t *h, struct snl_state *ss)
{
	int family_id;

	if (!snl_init(ss, NETLINK_GENERIC)) {
		ifconfig_error(h, NETLINK, EIO);
		return (-1);
	}

	family_id = snl_get_genl_family(ss, VEB_NL_FAMILY_NAME);
	if (family_id == 0) {
		snl_free(ss);
		ifconfig_error(h, NETLINK, ENOENT);
		return (-1);
	}

	return (family_id);
}

int
ifconfig_veb_get_veb_status(ifconfig_handle_t *h,
    const char *name, struct ifconfig_veb_status **veb_status)
{
	struct snl_state ss = {};
	struct snl_writer nw;
	struct nlmsghdr *hdr;
	struct nla_veb_link veb_link = {};
	struct ifconfig_veb_status *veb = NULL;
	int port_count;

	*veb_status = NULL;

	if (!snl_init(&ss, NETLINK_ROUTE)) {
		ifconfig_error(h, NETLINK, ENOTSUP);
		return (-1);
	}

	snl_init_writer(&ss, &nw);
	hdr = snl_create_msg_request(&nw, NL_RTM_GETLINK);
	snl_reserve_msg_object(&nw, struct ifinfomsg);
	snl_add_msg_attr_string(&nw, IFLA_IFNAME, name);

	if ((hdr = snl_finalize_msg(&nw)) == NULL) {
	    	ifconfig_error(h, NETLINK, ENOMEM);
		goto err;
	}

	if (!snl_send_message(&ss, hdr)) {
	    	ifconfig_error(h, NETLINK, EIO);
		goto err;
	}

	hdr = snl_read_reply(&ss, hdr->nlmsg_seq);
	if (hdr == NULL || hdr->nlmsg_type != NL_RTM_NEWLINK) {
	    	ifconfig_error(h, NETLINK, EIO);
		goto err;
	}
	if (!snl_parse_nlmsg(&ss, hdr, &veb_link_parser, &veb_link)) {
	    	ifconfig_error(h, NETLINK, EINVAL);
		goto err;
	}

	if (veb_link.linkinfo.kind == NULL ||
	    strcmp(veb_link.linkinfo.kind, "veb") != 0) {
	    	ifconfig_error(h, NETLINK, EINVAL);
		goto err;
	}

	port_count = veb_link.linkinfo.data.ifla_ports.count;
	if ((veb = calloc(1, sizeof(*veb))) == NULL ||
	    (port_count > 0 &&
	    (veb->ports = calloc(port_count, sizeof(*veb->ports))) == NULL)) {
		ifconfig_error(h, OTHER, ENOMEM);
		goto err;
	}

	veb->maxaddr = veb_link.linkinfo.data.ifla_maxaddr;
	veb->timeout = veb_link.linkinfo.data.ifla_timeout;
	veb->exceeded = veb_link.linkinfo.data.ifla_exceeded;
	veb->numports = port_count;
	
	for (int i = 0; i < port_count; i++) {
		const struct nl_parsed_veb_port *veb_port = 
		    veb_link.linkinfo.data.ifla_ports.items[i];

		if (veb_port->ifla_ifname == NULL)
			continue;

		strlcpy(veb->ports[i].ifname, veb_port->ifla_ifname,
		    sizeof(veb->ports[i].ifname));
		veb->ports[i].maxaddr = veb_port->ifla_maxaddr;
		veb->ports[i].discover = veb_port->ifla_discover;
		veb->ports[i].learning = veb_port->ifla_learning;
		veb->ports[i].sticky = veb_port->ifla_sticky;
		veb->ports[i].private = veb_port->ifla_private;
		veb->ports[i].vport = veb_port->ifla_vport;
	}

	*veb_status = veb;
	snl_free(&ss);

	return (0);
err:
	snl_free(&ss);
	if (veb != NULL) {
		free(veb->ports);
		free(veb);
	}

	return (-1);
}

void
ifconfig_veb_free_veb_status(struct ifconfig_veb_status *veb_status)
{
	if (veb_status != NULL) {
		free(veb_status->ports);
		free(veb_status);
	}
}

static int
veb_set_cmd(ifconfig_handle_t *h,
    const char *name, int attr, uint32_t value)
{
	struct snl_state ss = {};
	struct snl_writer nw = {};
	int error, off, off2;

	if (!snl_init(&ss, NETLINK_ROUTE)) {
		ifconfig_error(h, NETLINK, ENOTSUP);
		return (-1);
	}

	veb_nl_init(&ss, &nw, name, 0);
	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
	snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "veb");

	off2 = snl_add_msg_attr_nested(&nw, IFLA_INFO_DATA);
	snl_add_msg_attr_u32(&nw, attr, value);
	snl_end_attr_nested(&nw, off2);

	snl_end_attr_nested(&nw, off);

	error = veb_nl_fini(h,&ss, &nw);

	snl_free(&ss);

	return (error);
}


int
ifconfig_veb_set_timeout(ifconfig_handle_t *h,
    const char *ifname, int timeout)
{
	return (veb_set_cmd(h, ifname, IFLA_VEB_RTABLE_TIMEOUT, timeout));
}

int
ifconfig_veb_set_maxaddr(ifconfig_handle_t *h,
    const char *ifname, int maxaddr)
{
	return (veb_set_cmd(h, ifname, IFLA_VEB_RTABLE_MAXADDR, maxaddr));
}

static int
veb_rtable_cmd(ifconfig_handle_t *h,
    const char *name, const char *port, struct ether_addr *ea,
    ether_vlanid_t vlan, int cmd)
{
	struct snl_state ss = {};
	struct snl_writer nw;
	int error, family_id;

	if ((family_id = veb_genl_init(h, &ss)) < 0)
		return (-1);

	snl_init_writer(&ss, &nw);
	snl_create_genl_msg_request(&nw, family_id, cmd);
	snl_add_msg_attr_string(&nw, VEB_NL_RTABLE_IFNAME, name);
	if (cmd == VEB_NL_CMD_ADDSTATIC && port != NULL)
		snl_add_msg_attr_string(&nw, VEB_NL_RTABLE_PORT, port);
	snl_add_msg_attr(&nw, VEB_NL_RTABLE_MAC, ETHER_ADDR_LEN, ea->octet);
	snl_add_msg_attr_u16(&nw, VEB_NL_RTABLE_VLAN, vlan);

	error = veb_nl_fini(h, &ss, &nw);
	snl_free(&ss);

	return (error);
}

int
ifconfig_veb_add_static(ifconfig_handle_t *h, const char *ifname,
    const char *portname, struct ether_addr *ea, ether_vlanid_t vlan)
{
	return (veb_rtable_cmd(h, ifname, portname, ea, vlan, VEB_NL_CMD_ADDSTATIC));
}
int
ifconfig_veb_del_addr(ifconfig_handle_t *h, const char *ifname,
    struct ether_addr *ea, ether_vlanid_t vlan)
{
	return (veb_rtable_cmd(h, ifname, NULL, ea, vlan, VEB_NL_CMD_DELADDR));
}

static int
veb_port_cmd(ifconfig_handle_t *h, const char *ifname, int attr,
    const char *portname)
{
	struct snl_state ss = {};
	struct snl_writer nw = {};
	int error, off, off2;

	if (!snl_init(&ss, NETLINK_ROUTE)) {
		ifconfig_error(h, NETLINK, ENOTSUP);
		return (-1);
	}

	veb_nl_init(&ss, &nw, ifname, 0);
	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
	snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "veb");

	off2 = snl_add_msg_attr_nested(&nw, IFLA_INFO_DATA);
	snl_add_msg_attr_string(&nw, attr, portname);

	snl_end_attr_nested(&nw, off2);
	snl_end_attr_nested(&nw, off);

	error = veb_nl_fini(h, &ss, &nw);

	snl_free(&ss);
	
	return (error);
}

int
ifconfig_veb_add_port(ifconfig_handle_t *h,
    const char *ifname, const char *portname)
{
	return (veb_port_cmd(h, ifname, IFLA_VEB_ADDPORT, portname));
}

int
ifconfig_veb_del_port(ifconfig_handle_t *h,
    const char *ifname, const char *portname)
{
	return (veb_port_cmd(h, ifname, IFLA_VEB_DELPORT, portname));
}

int
ifconfig_veb_set_port_maxaddr(ifconfig_handle_t *h, const char *ifname,
    const char *portname, uint32_t maxaddr)
{
	struct snl_state ss = {};
	struct snl_writer nw = {};
	int error, off, off2, off3;

	if (!snl_init(&ss, NETLINK_ROUTE)) {
		ifconfig_error(h, NETLINK, ENOTSUP);
		return (-1);
	}

	veb_nl_init(&ss, &nw, ifname, 0);
	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
	snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "veb");

	off2 = snl_add_msg_attr_nested(&nw, IFLA_INFO_DATA);
	off3 = snl_add_msg_attr_nested(&nw, IFLA_VEB_SETPORT);
	snl_add_msg_attr_string(&nw, IFLA_VEB_PORT_IFNAME, portname);
	snl_add_msg_attr_u32(&nw, IFLA_VEB_PORT_MAXADDR, maxaddr);

	snl_end_attr_nested(&nw, off3);
	snl_end_attr_nested(&nw, off2);
	snl_end_attr_nested(&nw, off);

	error = veb_nl_fini(h, &ss, &nw);

	snl_free(&ss);

	return (error);
}

static int
veb_set_port_flag(ifconfig_handle_t *h, const char *ifname,
    const char *portname, int attr, bool on)
{
	struct snl_state ss = {};
	struct snl_writer nw = {};
	int error, off, off2, off3;

	if (!snl_init(&ss, NETLINK_ROUTE)) {
		ifconfig_error(h, NETLINK, ENOTSUP);
		return (-1);
	}

	veb_nl_init(&ss, &nw, ifname, 0);
	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
	snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "veb");

	off2 = snl_add_msg_attr_nested(&nw, IFLA_INFO_DATA);
	off3 = snl_add_msg_attr_nested(&nw, IFLA_VEB_SETPORT);
	snl_add_msg_attr_string(&nw, IFLA_VEB_PORT_IFNAME, portname);
	snl_add_msg_attr_bool(&nw, attr, on);

	snl_end_attr_nested(&nw, off3);
	snl_end_attr_nested(&nw, off2);
	snl_end_attr_nested(&nw, off);

	error = veb_nl_fini(h, &ss, &nw);

	snl_free(&ss);

	return (error);
}

int
ifconfig_veb_set_port_flag(ifconfig_handle_t *h, const char *ifname,
    const char *portname, int flag, bool on)
{
	switch(flag) {
	case IFCONFIG_VEB_PORT_DISCOVER: 
		flag = IFLA_VEB_PORT_DISCOVER;
		break;
	case IFCONFIG_VEB_PORT_LEARNING:
		flag = IFLA_VEB_PORT_LEARNING;
		break;
	case IFCONFIG_VEB_PORT_STICKY:
		flag = IFLA_VEB_PORT_STICKY;
		break;
	case IFCONFIG_VEB_PORT_PRIVATE:
		flag = IFLA_VEB_PORT_PRIVATE;
		break;
	default: 
		ifconfig_error(h, OTHER, EINVAL);
		return (-1);
	}

	return (veb_set_port_flag(h, ifname, portname, flag, on));
}

int
ifconfig_veb_flush(ifconfig_handle_t *h,
    const char *name, bool all)
{
	struct snl_state ss = {};
	struct snl_writer nw = {};
	int error, off, off2;

	if (!snl_init(&ss, NETLINK_ROUTE)) {
		ifconfig_error(h, NETLINK, ENOTSUP);
		return (-1);
	}

	veb_nl_init(&ss, &nw, name, 0);
	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
	snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "veb");

	off2 = snl_add_msg_attr_nested(&nw, IFLA_INFO_DATA);

	snl_add_msg_attr_bool(&nw, IFLA_VEB_RTABLE_FLUSH, all);

	snl_end_attr_nested(&nw, off2);
	snl_end_attr_nested(&nw, off);

	error = veb_nl_fini(h, &ss, &nw);

	snl_free(&ss);

	return (error);
}

int
ifconfig_veb_showaddr(ifconfig_handle_t *h, const char *name,
    struct ifconfig_veb_rtnode **vebrtp, uint32_t *rtn_count)
{
	struct snl_state ss = {};
	struct snl_errmsg_data e = {};
	struct snl_writer nw;
	struct nlmsghdr *hdr;
	struct ifconfig_veb_rtnode *vebrt = NULL, *tmp;
	uint32_t n = 0, cap = 0;
	uint32_t seq_id;
	int family_id;

	family_id = veb_genl_init(h, &ss);
	if (family_id == -1) {
		goto err;
	}

	snl_init_writer(&ss, &nw);
	hdr = snl_create_genl_msg_request(&nw, family_id, VEB_NL_CMD_GETRTABLE);
	hdr->nlmsg_flags |= NLM_F_DUMP;
	snl_add_msg_attr_string(&nw, VEB_NL_RTABLE_IFNAME, name);

	hdr = snl_finalize_msg(&nw);
	if (hdr == NULL) {
		ifconfig_error(h, NETLINK, ENOMEM);
		goto err;
	}

	seq_id = hdr->nlmsg_seq;
	if (!snl_send_message(&ss, hdr)) {
		ifconfig_error(h, NETLINK, EIO);
		goto err;
	}

	while ((hdr = snl_read_reply_multi(&ss, seq_id, &e)) != NULL) {
		struct veb_rt_entry rt = {};
		struct ifconfig_veb_rtnode *rn;

		if (!snl_parse_nlmsg(&ss, hdr, &veb_rtable_parser, &rt))
			continue;
		if (rt.port == NULL || rt.mac == NULL ||
		    NLA_DATA_LEN(rt.mac) != ETHER_ADDR_LEN)
			continue;

		if (n == cap) {
			size_t ncap = cap ? cap * 2 : 64;

			tmp = reallocarray(vebrt, ncap, sizeof(*vebrt));
			if (tmp == NULL) {
				ifconfig_error(h, OTHER, ENOMEM);
				goto err;
			}
			vebrt = tmp;
			cap = ncap;
		}

		rn = &vebrt[n++];
		memset(rn, 0, sizeof(*rn));
		strlcpy(rn->port, rt.port, sizeof(rn->port));
		memcpy(&rn->mac, NLA_DATA(rt.mac), ETHER_ADDR_LEN);
		rn->vlan = rt.vlan;
		rn->type = rt.type;
		rn->expire = rt.expire;
	}

	if (e.error != 0) {
		ifconfig_error(h, NETLINK, e.error);
		goto err;
	}

	*vebrtp = vebrt;
	*rtn_count = n;
	snl_free(&ss);

	return (0);
err:
	free(vebrt);
	snl_free(&ss);

	return (-1);
}

void
ifconfig_veb_free_veb_showaddr(struct ifconfig_veb_rtnode *vebrt)
{
	if (vebrt != NULL) {
		free(vebrt);
	}
}
