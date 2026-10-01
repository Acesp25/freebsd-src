/*
 * Copyright (c) 2026 Aaron Espinoza <acesp25@freebsd.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>

#include <net/ethernet.h>
#include <net/if.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>

#include "ifconfig.h"
#include "libifconfig.h"

static const char *const veb_rtable_types[] = {
	[0] = "dynamic",
	[1] = "static",
	[2] = "sticky"
};

static void
veb_status(if_ctx *ctx)
{
	struct ifconfig_veb_status *veb = NULL;

	if (ifconfig_veb_get_veb_status(lifh, ctx->ifname, &veb) != 0)
		return;

	printf("\tmaxaddr: %u", veb->maxaddr);
	printf("\ttimeout: %u", veb->timeout);
	printf("\texceeded: %u\n", veb->exceeded);

	for (uint32_t i = 0; i < veb->numports; i++) {
		struct ifconfig_veb_port *port = &veb->ports[i]; 

		const struct {
			bool		on;
			const char	*name;
		} f[] = {
			{ port->learning,	"LEARNING" },
			{ port->discover,	"DISCOVER" },
			{ port->sticky,		"STICKY" },
			{ port->private,	"PRIVATE" },
			{ port->vport,		"VPORT" },
		};
		const char *sep = "";

		printf("\tport: %s flags=<", port->ifname);
		for (size_t ii = 0; ii < nitems(f); ii++) {
			if (f[ii].on) {
				printf("%s%s", sep, f[ii].name);
				sep = ",";
			}
		}
		printf("> maxaddr: %u\n", port->maxaddr);
	}

	ifconfig_veb_free_veb_status(veb);
}

static void
setveb_addport(if_ctx *ctx, const char *portname, int dummy __unused)
{
	if (ifconfig_veb_add_port(lifh, ctx->ifname, portname) != 0)
		errc(1, ifconfig_err_errno(lifh), "vebportadd %s", portname);
}

static void
setveb_delport(if_ctx *ctx, const char *portname, int dummy __unused)
{
	if (ifconfig_veb_del_port(lifh, ctx->ifname, portname) != 0)
		errc(1, ifconfig_err_errno(lifh), "vebportdel %s", portname);
}

static void
setveb_setportmaxaddr(if_ctx *ctx, const char *portname, const char *arg)
{
	const char *errstr;
	uint32_t maxaddr;

	maxaddr = strtonum(arg, 0, UINT32_MAX, &errstr);
	if (errstr != NULL)
		errx(1, "portmaxaddr %s: %s", arg, errstr);

	if (ifconfig_veb_set_port_maxaddr(lifh, ctx->ifname,
	    portname, maxaddr) != 0)
		errc(1, ifconfig_err_errno(lifh),
		    "vebsetportmaxaddr %s", portname);
}

static void
setveb_learn(if_ctx *ctx, const char *portname, int dummy __unused)
{
	if (ifconfig_veb_set_port_flag(lifh, ctx->ifname, portname,
	    IFCONFIG_VEB_PORT_LEARNING, true) != 0)
		errc(1, ifconfig_err_errno(lifh), "veblearn %s", portname);
}

static void
setveb_unlearn(if_ctx *ctx, const char *portname, int dummy __unused)
{
	if (ifconfig_veb_set_port_flag(lifh, ctx->ifname, portname,
	    IFCONFIG_VEB_PORT_LEARNING, false) != 0)
		errc(1, ifconfig_err_errno(lifh), "-veblearn %s", portname);
}

static void
setveb_discover(if_ctx *ctx, const char *portname, int dummy __unused)
{
	if (ifconfig_veb_set_port_flag(lifh, ctx->ifname, portname,
	    IFCONFIG_VEB_PORT_DISCOVER, true) != 0)
		errc(1, ifconfig_err_errno(lifh), "vebdiscover %s", portname);
}

static void
setveb_undiscover(if_ctx *ctx, const char *portname, int dummy __unused)
{
	if (ifconfig_veb_set_port_flag(lifh, ctx->ifname, portname,
	    IFCONFIG_VEB_PORT_DISCOVER, false) != 0)
		errc(1, ifconfig_err_errno(lifh), "-vebdiscover %s", portname);
}

static void
setveb_sticky(if_ctx *ctx, const char *portname, int dummy __unused)
{
	if (ifconfig_veb_set_port_flag(lifh, ctx->ifname, portname,
	    IFCONFIG_VEB_PORT_STICKY, true) != 0)
		errc(1, ifconfig_err_errno(lifh), "vebsticky %s", portname);
}

static void
setveb_unsticky(if_ctx *ctx, const char *portname, int dummy __unused)
{
	if (ifconfig_veb_set_port_flag(lifh, ctx->ifname, portname,
	    IFCONFIG_VEB_PORT_STICKY, false) != 0)
		errc(1, ifconfig_err_errno(lifh), "-vebsticky %s", portname);
}

static void
setveb_private(if_ctx *ctx, const char *portname, int dummy __unused)
{
	if (ifconfig_veb_set_port_flag(lifh, ctx->ifname, portname,
	    IFCONFIG_VEB_PORT_PRIVATE, true) != 0)
		errc(1, ifconfig_err_errno(lifh), "vebprivate %s", portname);
}

static void
setveb_unprivate(if_ctx *ctx, const char *portname, int dummy __unused)
{
	if (ifconfig_veb_set_port_flag(lifh, ctx->ifname, portname,
	    IFCONFIG_VEB_PORT_PRIVATE, false) != 0)
		errc(1, ifconfig_err_errno(lifh), "-vebprivate %s", portname);
}

static void
setveb_settimeout(if_ctx *ctx, const char *arg, int dummy __unused)
{
	const char *errstr;
	uint32_t timeout;

	timeout = strtonum(arg, 0, UINT32_MAX, &errstr);
	if (errstr != NULL)
		errx(1, "timeout %s: %s", arg, errstr);

	if (ifconfig_veb_set_timeout(lifh, ctx->ifname, timeout) != 0)
		errc(1, ifconfig_err_errno(lifh), "vebtimeout %u", timeout);
}

static void
setveb_setmaxaddr(if_ctx *ctx, const char *arg, int dummy __unused)
{
	const char *errstr;
	uint32_t maxaddr;

	maxaddr = strtonum(arg, 0, UINT32_MAX, &errstr);
	if (errstr != NULL)
		errx(1, "maxaddr %s: %s", arg, errstr);

	if (ifconfig_veb_set_maxaddr(lifh, ctx->ifname, maxaddr) != 0)
		errc(1, ifconfig_err_errno(lifh), "vebmaxaddr %u", maxaddr);
}

static int
setveb_static(if_ctx *ctx, int argc, const char *const *argv)
{
	struct ether_addr *ea;
	const char *port, *errstr;
	ether_vlanid_t vlan = DOT1Q_VID_NULL;
	int arg = 0;

	if (argc < 2)
		errx(1, "usage: vebstatic <interface> <address> [vlan <id>]");

	port = argv[arg++];

	ea = ether_aton(argv[arg]);
	if (ea == NULL)
		errx(1, "invalid address: %s", argv[arg]);
	arg++;

	if (argc > 2 && strcmp(argv[arg], "vlan") == 0) {
		if (argc < 4)
			errx(1, "usage: vebstatic <interface> <address> "
			    "[vlan <id>]");

		vlan = strtonum(argv[arg + 1],
		    DOT1Q_VID_MIN, DOT1Q_VID_MAX, &errstr);

		if (errstr != NULL)
			errx(1, "vlan %s: %s", argv[arg + 1], errstr);

		arg += 2;
	}
	
	if (ifconfig_veb_add_static(lifh, ctx->ifname, port, ea, vlan) != 0)
		errc(1, ifconfig_err_errno(lifh), "vebstatic %s %s %u",
		    port, ether_ntoa(ea), vlan);

	return (arg);
}

static int
setveb_deladdr(if_ctx *ctx, int argc, const char *const *argv)
{
	struct ether_addr *ea;
	const char *errstr;
	ether_vlanid_t vlan = DOT1Q_VID_NULL;
	int arg = 0;

	if (argc < 1)
		errx(1, "usage: vebdeladdr <address> [vlan <id>]");

	ea = ether_aton(argv[arg]);
	if (ea == NULL)
		errx(1, "invalid address: %s", argv[arg]);
	arg++;

	if (argc > 1 && strcmp(argv[arg], "vlan") == 0) {
		if (argc < 3)
			errx(1, "usage: vebdeladdr <address> [vlan <id>]");

		vlan = strtonum(argv[arg + 1],
		    DOT1Q_VID_MIN, DOT1Q_VID_MAX, &errstr);

		if (errstr != NULL)
			errx(1, "vlan %s: %s", argv[arg + 1], errstr);

		arg += 2;
	}

	if (ifconfig_veb_del_addr(lifh, ctx->ifname, ea, vlan) != 0)
		errc(1, ifconfig_err_errno(lifh), "vebdeladdr %s %u",
		    ether_ntoa(ea), vlan);

	return (arg);
}

static void
setveb_flush(if_ctx *ctx, const char *val __unused, int d)
{
	if (ifconfig_veb_flush(lifh, ctx->ifname, d) != 0)
		errc(1, ifconfig_err_errno(lifh), "vebflush %d", d);
}

static void
setveb_showaddr(if_ctx *ctx, const char *val __unused, int dummy __unused)
{
	struct ifconfig_veb_rtnode *rtnodes;
	uint32_t count;

	if (ifconfig_veb_showaddr(lifh, ctx->ifname, &rtnodes, &count) != 0)
		errc(1, ifconfig_err_errno(lifh), "vebaddr");

	for (uint32_t i = 0; i < count; i++) {
		printf("%s Vlan%u %s %u %s\n", ether_ntoa(&rtnodes[i].mac),
		    rtnodes[i].vlan, rtnodes[i].port, rtnodes[i].expire,
		    rtnodes[i].type < nitems(veb_rtable_types)
		    ? veb_rtable_types[rtnodes[i].type] : "?");
	}

	ifconfig_veb_free_veb_showaddr(rtnodes);
}

static struct cmd veb_cmds[] = {
	DEF_CMD_ARG("vebportadd",	setveb_addport),
	DEF_CMD_ARG("vebportdel",	setveb_delport),
	DEF_CMD_ARG2("vebportmaxaddr",	setveb_setportmaxaddr),
	DEF_CMD_ARG("veblearn",		setveb_learn),
	DEF_CMD_ARG("-veblearn",	setveb_unlearn),
	DEF_CMD_ARG("vebdiscover",	setveb_discover),
	DEF_CMD_ARG("-vebdiscover",	setveb_undiscover),
	DEF_CMD_ARG("vebsticky",	setveb_sticky),
	DEF_CMD_ARG("-vebsticky",	setveb_unsticky),
	DEF_CMD_ARG("vebprivate",	setveb_private),
	DEF_CMD_ARG("-vebprivate",	setveb_unprivate),
	DEF_CMD_ARG("vebtimeout",	setveb_settimeout),
	DEF_CMD_ARG("vebmaxaddr",	setveb_setmaxaddr),
	DEF_CMD_VARG("vebstatic",	setveb_static),
	DEF_CMD_VARG("vebdeladdr",	setveb_deladdr),
	DEF_CMD("vebflush", 0,		setveb_flush),
	DEF_CMD("vebflushall", 1,	setveb_flush),
	DEF_CMD("vebaddr", 1,		setveb_showaddr),
};

static struct afswtch af_veb = {
	.af_name		= "af_veb",
	.af_af			= AF_UNSPEC,
	.af_other_status	= veb_status,
};

static __constructor void
veb_constructor(void)
{
	for (size_t i = 0; i < nitems(veb_cmds); i++)
		cmd_register(&veb_cmds[i]);

	af_register(&af_veb);
}
