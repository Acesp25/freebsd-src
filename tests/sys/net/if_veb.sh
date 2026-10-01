#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 Aaron Espinoza <acesp25@FreeBSD.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

. $(atf_get_srcdir)/../common/vnet.subr

atf_test_case "create_destroy" "cleanup"
create_destroy_head()
{
	atf_set descr 'Create and destroy a veb(4) and check its default state'
	atf_set require.user root
	atf_set require.kmods if_veb
}

create_destroy_body()
{
	vnet_init
	vnet_mkjail vebtest

	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 create
	atf_check -s exit:0 -o match:"timeout: 1200" ifconfig -j vebtest veb0
	atf_check -s exit:0 -o not-match:"port:" ifconfig -j vebtest veb0

	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 destroy
	atf_check -s not-exit:0 -e ignore ifconfig -j vebtest veb0
}

create_destroy_cleanup()
{
	vnet_cleanup
}

atf_test_case "port_add_del" "cleanup"
port_add_del_head()
{
	atf_set descr 'Verify vebportadd and vebportdel alongside their respective errors'
	atf_set require.user root
	atf_set require.kmods if_veb
}

port_add_del_body()
{
	local epair

	vnet_init
	epair=$(vnet_mkepair)
	vnet_mkjail vebtest ${epair}a ${epair}b

	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 create up
	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb1 create

	atf_check -s exit:2 -e ignore \
	    ifconfig -j vebtest veb0 vebportadd nonexistent0
	atf_check -s exit:2 -e ignore \
	    ifconfig -j vebtest veb0 vebportdel ${epair}a

	ifconfig -j vebtest ${epair}a inet 192.0.2.5/24
	atf_check -s exit:22 -e ignore \
	    ifconfig -j vebtest veb0 vebportadd ${epair}a

	ifconfig -j vebtest ${epair}a inet 192.0.2.5 -alias
	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 vebportadd ${epair}a
	# Check that we are still UP after adding a port
	atf_check -s exit:0 -o match:"flags=[0-9a-f]+<UP," ifconfig -j vebtest veb0
	atf_check -s exit:0 -o match:"port: ${epair}a flags=<LEARNING,DISCOVER>" \
	    ifconfig -j vebtest veb0

	atf_check -s exit:17 -e ignore \
	    ifconfig -j vebtest veb0 vebportadd ${epair}a

	atf_check -s exit:16 -e ignore \
	    ifconfig -j vebtest veb1 vebportadd ${epair}a

	atf_check -s exit:0 -o match:"port: ${epair}a " \
	    ifconfig -j vebtest veb0
	atf_check -s exit:0 -o not-match:"port:" ifconfig -j vebtest veb1

	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 vebportdel ${epair}a
	atf_check -s exit:0 -o not-match:"port:" ifconfig -j vebtest veb0
}

port_add_del_cleanup()
{
	vnet_cleanup
}

atf_test_case "delete_with_ports" "cleanup"
delete_with_ports_head()
{
	atf_set descr 'Destroy a veb(4) that still has ports and reuse them'
	atf_set require.user root
	atf_set require.kmods if_veb
}

delete_with_ports_body()
{
	local epair1 epair2

	vnet_init
	epair1=$(vnet_mkepair)
	epair2=$(vnet_mkepair)
	vnet_mkjail vebtest ${epair1}a ${epair2}a

	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 create
	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 vebportadd ${epair1}a
	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 vebportadd ${epair2}a

	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 destroy

	# ports must be fully released; readding with no issues
	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb1 create
	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb1 vebportadd ${epair1}a
	atf_check -s exit:0 -o match:"port: ${epair1}a " \
	    ifconfig -j vebtest veb1
}

delete_with_ports_cleanup()
{
	vnet_cleanup
}

atf_test_case "port_departure" "cleanup"
port_departure_head()
{
	atf_set descr 'Destroy a veb(4) port and check it leaves the veb'
	atf_set require.user root
	atf_set require.kmods if_veb
}

port_departure_body()
{
	local epair1 epair2

	vnet_init
	epair1=$(vnet_mkepair)
	epair2=$(vnet_mkepair)
	vnet_mkjail vebtest ${epair1}a ${epair2}a

	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 create
	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 vebportadd ${epair1}a
	atf_check -s exit:0 -o ignore ifconfig -j vebtest ${epair1}a destroy
	atf_check -s exit:0 -o not-match:"port:" ifconfig -j vebtest veb0

	# veb must remain usable afterwards.
	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 vebportadd ${epair2}a
}

port_departure_cleanup()
{
	vnet_cleanup
}

atf_test_case "flags" "cleanup"
flags_head()
{
	atf_set descr 'Set and clear veb(4) per-port flags'
	atf_set require.user root
	atf_set require.kmods if_veb
}

flags_body()
{
	local epair

	vnet_init
	epair=$(vnet_mkepair)
	vnet_mkjail vebtest ${epair}a ${epair}b

	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 create
	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 vebportadd ${epair}a
	atf_check -s exit:0 -o match:"port: ${epair}a flags=<LEARNING,DISCOVER>" \
	    ifconfig -j vebtest veb0

	atf_check -s exit:0 -o ignore \
	    ifconfig -j vebtest veb0 vebsticky ${epair}a -vebdiscover ${epair}a
	atf_check -s exit:0 -o match:"port: ${epair}a flags=<LEARNING,STICKY>" \
	    ifconfig -j vebtest veb0
	atf_check -s exit:0 -o ignore \
	    ifconfig -j vebtest veb0 vebprivate ${epair}a -veblearn ${epair}a
	atf_check -s exit:0 -o match:"port: ${epair}a flags=<STICKY,PRIVATE>" \
	    ifconfig -j vebtest veb0
	atf_check -s exit:0 -o ignore \
	    ifconfig -j vebtest veb0 -vebsticky ${epair}a -vebprivate ${epair}a
	atf_check -s exit:0 -o match:"port: ${epair}a flags=<>" \
	    ifconfig -j vebtest veb0
	atf_check -s exit:0 -o ignore \
	    ifconfig -j vebtest veb0 veblearn ${epair}a vebdiscover ${epair}a
	atf_check -s exit:0 -o match:"port: ${epair}a flags=<LEARNING,DISCOVER>" \
	    ifconfig -j vebtest veb0

	atf_check -s exit:2 -e ignore \
	    ifconfig -j vebtest veb0 veblearn ${epair}b
	atf_check -s exit:2 -e ignore \
	    ifconfig -j vebtest veb0 veblearn nonexistent0
}

flags_cleanup()
{
	vnet_cleanup
}

atf_test_case "learning" "cleanup"
learning_head()
{
	atf_set descr 'Verify that veb(4) learns source addresses on the receiving port'
	atf_set require.user root
	atf_set require.kmods if_veb
}

learning_body()
{
	local epair1 epair2 ip1 ip2 mac1 mac2

	ip1=192.0.2.1
	ip2=192.0.2.2

	vnet_init
	epair1=$(vnet_mkepair)
	epair2=$(vnet_mkepair)
	vnet_mkjail vebtest ${epair1}a ${epair2}a
	vnet_mkjail vebtest1 ${epair1}b
	vnet_mkjail vebtest2 ${epair2}b

	ifconfig -j vebtest1 ${epair1}b inet ${ip1}/24 up
	ifconfig -j vebtest2 ${epair2}b inet ${ip2}/24 up
	ifconfig -j vebtest ${epair1}a up
	ifconfig -j vebtest ${epair2}a up

	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 create up
	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 vebportadd ${epair1}a
	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 vebportadd ${epair2}a

	atf_check -s exit:0 -o ignore jexec vebtest1 ping -nc 1 -t 1 ${ip2}

	mac1=$(ifconfig -j vebtest1 ${epair1}b | awk '/ether/ { print $2 }')
	mac2=$(ifconfig -j vebtest2 ${epair2}b | awk '/ether/ { print $2 }')
	atf_check -s exit:0 -o match:"${mac1} Vlan0 ${epair1}a [0-9]+ dynamic" \
	    ifconfig -j vebtest veb0 vebaddr
	atf_check -s exit:0 -o match:"${mac2} Vlan0 ${epair2}a [0-9]+ dynamic" \
	    ifconfig -j vebtest veb0 vebaddr

	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 vebflush
	atf_check -s exit:0 -o not-match:"${mac1}" ifconfig -j vebtest veb0 vebaddr

	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 vebsticky ${epair1}a
	atf_check -s exit:0 -o ignore jexec vebtest1 ping -nc 1 -t 1 ${ip2}
	atf_check -s exit:0 -o match:"${mac1} Vlan0 ${epair1}a 0 sticky" \
	    ifconfig -j vebtest veb0 vebaddr

	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 vebflush
	atf_check -s exit:0 -o match:"${mac1} Vlan0 ${epair1}a 0 sticky" \
	    ifconfig -j vebtest veb0 vebaddr
	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 vebflushall
	atf_check -s exit:0 -o not-match:"${mac1}" ifconfig -j vebtest veb0 vebaddr
}

learning_cleanup()
{
	vnet_cleanup
}

atf_test_case "mtu" "cleanup"
mtu_head()
{
	atf_set descr 'Verify that the first veb(4) port sets the MTU and later ports follow it'
	atf_set require.user root
	atf_set require.kmods if_veb
}

mtu_body()
{
	local epair1 epair2

	vnet_init
	epair1=$(vnet_mkepair)
	epair2=$(vnet_mkepair)
	vnet_mkjail vebtest ${epair1}a ${epair2}a

	ifconfig -j vebtest ${epair1}a mtu 9000

	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 create
	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 vebportadd ${epair1}a
	atf_check -s exit:0 -o match:"mtu 9000" ifconfig -j vebtest veb0

	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 vebportadd ${epair2}a
	atf_check -s exit:0 -o match:"mtu 9000" ifconfig -j vebtest ${epair2}a

	atf_check -s exit:0 -o ignore ifconfig -j vebtest vport0 create
	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 vebportadd vport0
	atf_check -s exit:0 -o match:"mtu 9000" ifconfig -j vebtest vport0

	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 mtu 1400
	atf_check -s exit:0 -o match:"mtu 1400" ifconfig -j vebtest vport0
	atf_check -s exit:0 -o match:"mtu 1400" ifconfig -j vebtest ${epair2}a

	# The veb owns a member vport's MTU
	atf_check -s exit:1 -e ignore ifconfig -j vebtest vport0 mtu 1500
}

mtu_cleanup()
{
	vnet_cleanup
}

atf_test_case "timeout" "cleanup"
timeout_head()
{
	atf_set descr 'Set and dump the veb(4) address cache timeout'
	atf_set require.user root
	atf_set require.kmods if_veb
}

timeout_body()
{
	vnet_init
	vnet_mkjail vebtest

	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 create
	atf_check -s exit:0 -o match:"timeout: 1200" ifconfig -j vebtest veb0
	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 vebtimeout 240
	atf_check -s exit:0 -o match:"timeout: 240" ifconfig -j vebtest veb0

	atf_check -s exit:1 -e ignore ifconfig -j vebtest veb0 vebtimeout -1
	atf_check -s exit:1 -e ignore ifconfig -j vebtest veb0 vebtimeout abc
	atf_check -s exit:0 -o match:"timeout: 240" ifconfig -j vebtest veb0

	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb1 create
	atf_check -s exit:0 -o match:"timeout: 1200" ifconfig -j vebtest veb1
}

timeout_cleanup()
{
	vnet_cleanup
}

atf_test_case "private_ports" "cleanup"
private_ports_head()
{
	atf_set descr 'Verify that private ports do not exchange traffic'
	atf_set require.user root
	atf_set require.kmods if_veb
}

private_ports_body()
{
	local epair1 epair2 epair3 ip1 ip2 ip3

	ip1=192.0.2.1
	ip2=192.0.2.2
	ip3=192.0.2.3

	vnet_init
	epair1=$(vnet_mkepair)
	epair2=$(vnet_mkepair)
	epair3=$(vnet_mkepair)
	vnet_mkjail vebtest ${epair1}a ${epair2}a ${epair3}a
	vnet_mkjail vebtest1 ${epair1}b
	vnet_mkjail vebtest2 ${epair2}b
	vnet_mkjail vebtest3 ${epair3}b

	ifconfig -j vebtest1 ${epair1}b inet ${ip1}/24 up
	ifconfig -j vebtest2 ${epair2}b inet ${ip2}/24 up
	ifconfig -j vebtest3 ${epair3}b inet ${ip3}/24 up

	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 create up
	for port in ${epair1}a ${epair2}a ${epair3}a; do
		ifconfig -j vebtest ${port} up
		atf_check -s exit:0 -o ignore \
		    ifconfig -j vebtest veb0 vebportadd ${port}
	done

	atf_check -s exit:0 -o ignore \
	    ifconfig -j vebtest veb0 vebprivate ${epair1}a
	atf_check -s exit:0 -o ignore \
	    ifconfig -j vebtest veb0 vebprivate ${epair2}a

	atf_check -s exit:2 -o ignore jexec vebtest1 ping -nc 1 -t 1 ${ip2}
	atf_check -s exit:2 -o ignore jexec vebtest2 ping -nc 1 -t 1 ${ip1}

	atf_check -s exit:0 -o ignore jexec vebtest1 ping -nc 1 -t 1 ${ip3}
	atf_check -s exit:0 -o ignore jexec vebtest2 ping -nc 1 -t 1 ${ip3}
}

private_ports_cleanup()
{
	vnet_cleanup
}

atf_test_case "unprivileged" "cleanup"
unprivileged_head()
{
	atf_set descr 'Verify that unprivileged users can read but not modify veb(4)'
	atf_set require.user root
	atf_set require.kmods if_veb
}

unprivileged_body()
{
	local epair

	vnet_init
	epair=$(vnet_mkepair)
	vnet_mkjail vebtest ${epair}a

	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 create

	atf_check -s exit:0 -o ignore jexec -U nobody vebtest ifconfig veb0

	atf_check -s exit:0 -o ignore jexec -U nobody vebtest ifconfig veb0 vebaddr

	atf_check -s exit:1 -e ignore \
	    jexec -U nobody vebtest ifconfig veb0 vebportadd ${epair}a
	atf_check -s exit:1 -e ignore \
	    jexec -U nobody vebtest ifconfig veb0 vebtimeout 60
	atf_check -s exit:1 -e ignore \
	    jexec -U nobody vebtest ifconfig veb0 vebstatic ${epair}a 02:aa:bb:cc:dd:01

	atf_check -s exit:0 -o not-match:"port:" ifconfig -j vebtest veb0
	atf_check -s exit:0 -o match:"timeout: 1200" ifconfig -j vebtest veb0
}

unprivileged_cleanup()
{
	vnet_cleanup
}

atf_test_case "bridge_mutual_exclusion" "cleanup"
bridge_mutual_exclusion_head()
{
	atf_set descr 'Verify that an if_bridge(4) member cannot be a veb(4) port and vice versa'
	atf_set require.user root
	atf_set require.kmods if_veb
}

bridge_mutual_exclusion_body()
{
	local epair1 epair2

	vnet_init
	vnet_init_bridge
	epair1=$(vnet_mkepair)
	epair2=$(vnet_mkepair)
	vnet_mkjail vebtest ${epair1}a ${epair2}a

	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 create
	atf_check -s exit:0 -o ignore ifconfig -j vebtest bridge0 create

	atf_check -s exit:0 -o ignore \
	    ifconfig -j vebtest bridge0 addm ${epair1}a
	atf_check -s exit:16 -e ignore \
	    ifconfig -j vebtest veb0 vebportadd ${epair1}a

	atf_check -s exit:0 -o ignore \
	    ifconfig -j vebtest veb0 vebportadd ${epair2}a
	atf_check -s exit:1 -e ignore \
	    ifconfig -j vebtest bridge0 addm ${epair2}a
}

bridge_mutual_exclusion_cleanup()
{
	vnet_cleanup
}

atf_test_case "no_host_presence" "cleanup"
no_host_presence_head()
{
	atf_set descr 'Verify that veb(4) refuses addresses and still forwards'
	atf_set require.user root
	atf_set require.kmods if_veb
}

no_host_presence_body()
{
	local epair1 epair2 ip0 ip1 ip2

	ip0=192.0.2.1
	ip1=192.0.2.2
	ip2=192.0.2.3

	vnet_init
	epair1=$(vnet_mkepair)
	epair2=$(vnet_mkepair)
	vnet_mkjail vebtest ${epair1}a ${epair2}a
	vnet_mkjail vebtest1 ${epair1}b
	vnet_mkjail vebtest2 ${epair2}b

	ifconfig -j vebtest1 ${epair1}b inet ${ip1}/24 up
	ifconfig -j vebtest2 ${epair2}b inet ${ip2}/24 up
	ifconfig -j vebtest ${epair1}a up
	ifconfig -j vebtest ${epair2}a up

	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 create up
	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 vebportadd ${epair1}a
	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 vebportadd ${epair2}a

	atf_check -s exit:1 -e match:"Operation not supported" \
	    ifconfig -j vebtest veb0 inet ${ip0}/24
	atf_check -s exit:0 -o not-match:"inet " ifconfig -j vebtest veb0

	atf_check -s exit:0 -o ignore jexec vebtest1 ping -nc 1 -t 1 ${ip2}
	atf_check -s not-exit:0 -o ignore -e ignore \
	    jexec vebtest1 ping -nc 1 -t 1 ${ip0}
}

no_host_presence_cleanup()
{
	vnet_cleanup
}

atf_test_case "transmit_ipv4_unicast" "cleanup"
transmit_ipv4_unicast_head()
{
	atf_set descr 'Create a veb(4) with two epair ports and pass traffic between jails'
	atf_set require.user root
	atf_set require.kmods if_veb
}

transmit_ipv4_unicast_body()
{
	local epair1 epair2 ip1 ip2

	ip1=192.0.2.1
	ip2=192.0.2.2

	vnet_init
	epair1=$(vnet_mkepair)
	epair2=$(vnet_mkepair)
	vnet_mkjail vebtest ${epair1}a ${epair2}a
	vnet_mkjail vebtest1 ${epair1}b
	vnet_mkjail vebtest2 ${epair2}b

	ifconfig -j vebtest1 ${epair1}b inet ${ip1}/24 up
	ifconfig -j vebtest2 ${epair2}b inet ${ip2}/24 up
	ifconfig -j vebtest ${epair1}a up
	ifconfig -j vebtest ${epair2}a up

	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 create up
	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 vebportadd ${epair1}a
	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 vebportadd ${epair2}a

	atf_check -s exit:0 -o ignore jexec vebtest1 ping -nc 1 -t 1 ${ip2}
	atf_check -s exit:0 -o ignore jexec vebtest2 ping -nc 1 -t 1 ${ip1}
}

transmit_ipv4_unicast_cleanup()
{
	vnet_cleanup
}

atf_test_case "transmit_ipv4_multicast" "cleanup"
transmit_ipv4_multicast_head()
{
	atf_set descr 'Create a veb(4) with three epair ports and check that multicast reaches every other port'
	atf_set require.user root
	atf_set require.kmods if_veb
}

transmit_ipv4_multicast_body()
{
	local epair1 epair2 epair3 ip1 ip2 ip3 mc_group

	ip1=192.0.2.1
	ip2=192.0.2.2
	ip3=192.0.2.3
	mc_group=224.0.0.1

	vnet_init
	epair1=$(vnet_mkepair)
	epair2=$(vnet_mkepair)
	epair3=$(vnet_mkepair)
	vnet_mkjail vebtest ${epair1}a ${epair2}a ${epair3}a
	vnet_mkjail vebtest1 ${epair1}b
	vnet_mkjail vebtest2 ${epair2}b
	vnet_mkjail vebtest3 ${epair3}b

	ifconfig -j vebtest1 ${epair1}b inet ${ip1}/24 up
	ifconfig -j vebtest2 ${epair2}b inet ${ip2}/24 up
	ifconfig -j vebtest3 ${epair3}b inet ${ip3}/24 up

	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 create up
	for port in ${epair1}a ${epair2}a ${epair3}a; do
		ifconfig -j vebtest ${port} up
		atf_check -s exit:0 -o ignore \
		    ifconfig -j vebtest veb0 vebportadd ${port}
	done

	route -j vebtest1 add -net 224.0.0.0/4 -interface ${epair1}b

	atf_check -s exit:0 -o ignore sysctl -j vebtest2 net.inet.icmp.bmcastecho=1
	atf_check -s exit:0 -o ignore sysctl -j vebtest3 net.inet.icmp.bmcastecho=1

	atf_check -s exit:0 -o save:ping.out \
	    jexec vebtest1 ping -nc 2 -t 3 ${mc_group}
	atf_check -s exit:0 -o match:"from ${ip2}:" cat ping.out
	atf_check -s exit:0 -o match:"from ${ip3}:" cat ping.out
}

transmit_ipv4_multicast_cleanup()
{
	vnet_cleanup
}

atf_test_case "static_addresses" "cleanup"
static_addresses_head()
{
	atf_set descr 'Add, show, flush and delete static veb(4) addresses'
	atf_set require.user root
	atf_set require.kmods if_veb
}

static_addresses_body()
{
	local epair1 epair2 mac

	mac=02:aa:bb:cc:dd:01

	vnet_init
	epair1=$(vnet_mkepair)
	epair2=$(vnet_mkepair)
	vnet_mkjail vebtest ${epair1}a ${epair2}a

	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 create
	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 vebportadd ${epair1}a

	atf_check -s exit:0 -o ignore \
	    ifconfig -j vebtest veb0 vebstatic ${epair1}a ${mac}
	atf_check -s exit:0 -o ignore \
	    ifconfig -j vebtest veb0 vebstatic ${epair1}a ${mac} vlan 10
	atf_check -s exit:0 -o match:"${mac} Vlan0 ${epair1}a 0 static" \
	    ifconfig -j vebtest veb0 vebaddr
	atf_check -s exit:0 -o match:"${mac} Vlan10 ${epair1}a 0 static" \
	    ifconfig -j vebtest veb0 vebaddr

	atf_check -s exit:2 -e ignore \
	    ifconfig -j vebtest veb0 vebstatic ${epair2}a ${mac}
	atf_check -s exit:22 -e ignore \
	    ifconfig -j vebtest veb0 vebstatic ${epair1}a 01:00:5e:00:00:01
	atf_check -s exit:1 -e ignore \
	    ifconfig -j vebtest veb0 vebstatic ${epair1}a ${mac} vlan 4095

	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 vebflush
	atf_check -s exit:0 -o match:"${mac} Vlan0 " \
	    ifconfig -j vebtest veb0 vebaddr

	atf_check -s exit:0 -o ignore \
	    ifconfig -j vebtest veb0 vebdeladdr ${mac}
	atf_check -s exit:0 -o not-match:"${mac} Vlan0 " \
	    ifconfig -j vebtest veb0 vebaddr
	atf_check -s exit:0 -o match:"${mac} Vlan10 " \
	    ifconfig -j vebtest veb0 vebaddr
	atf_check -s exit:2 -e ignore \
	    ifconfig -j vebtest veb0 vebdeladdr ${mac}

	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 vebportdel ${epair1}a
	atf_check -s exit:0 -o not-match:"${mac}" \
	    ifconfig -j vebtest veb0 vebaddr

	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 vebportadd ${epair1}a
	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 vebmaxaddr 0
	atf_check -s exit:28 -e ignore \
	    ifconfig -j vebtest veb0 vebstatic ${epair1}a ${mac}
}

static_addresses_cleanup()
{
	vnet_cleanup
}

atf_test_case "vport_constraints" "cleanup"
vport_constraints_head()
{
	atf_set descr 'Check that veb(4) has at most one vport and a vport joins at most one veb'
	atf_set require.user root
	atf_set require.kmods if_veb
}

vport_constraints_body()
{
	vnet_init
	vnet_mkjail vebtest

	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 create
	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb1 create
	atf_check -s exit:0 -o ignore ifconfig -j vebtest vport0 create
	atf_check -s exit:0 -o ignore ifconfig -j vebtest vport1 create

	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 vebportadd vport0

	atf_check -s exit:22 -e ignore ifconfig -j vebtest veb0 vebportadd vport1

	atf_check -s exit:16 -e ignore ifconfig -j vebtest veb1 vebportadd vport0

	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 vebportdel vport0
	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb1 vebportadd vport0
	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 vebportadd vport1

	atf_check -s exit:0 -o ignore ifconfig -j vebtest vport0 destroy
	atf_check -s exit:0 -o not-match:"port:" ifconfig -j vebtest veb1
	atf_check -s exit:0 -o ignore ifconfig -j vebtest vport2 create
	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb1 vebportadd vport2
}

vport_constraints_cleanup()
{
	vnet_cleanup
}

atf_test_case "vport_host_path" "cleanup"
vport_host_path_head()
{
	atf_set descr 'Check we can pass traffic between a vport and a jail'
	atf_set require.user root
	atf_set require.kmods if_veb
}

vport_host_path_body()
{
	local epair ip0 ip1

	ip0=192.0.2.1
	ip1=192.0.2.2

	vnet_init
	epair=$(vnet_mkepair)
	vnet_mkjail vebtest ${epair}a
	vnet_mkjail vebtest1 ${epair}b

	ifconfig -j vebtest1 ${epair}b inet ${ip1}/24 up
	ifconfig -j vebtest ${epair}a up

	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 create up
	atf_check -s exit:0 -o ignore ifconfig -j vebtest vport0 create
	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 vebportadd ${epair}a
	atf_check -s exit:0 -o ignore ifconfig -j vebtest veb0 vebportadd vport0

	atf_check -s exit:0 -o ignore ifconfig -j vebtest vport0 inet ${ip0}/24 up

	atf_check -s exit:0 -o match:"port: vport0 flags=<[A-Z,]*VPORT>" \
	    ifconfig -j vebtest veb0
	atf_check -s exit:0 -o not-match:"port: ${epair}a flags=<[A-Z,]*VPORT" \
	    ifconfig -j vebtest veb0

	atf_check -s exit:0 -o ignore jexec vebtest ping -nc 1 -t 1 ${ip1}
	atf_check -s exit:0 -o ignore jexec vebtest1 ping -nc 1 -t 1 ${ip0}
}

vport_host_path_cleanup()
{
	vnet_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "create_destroy"
	atf_add_test_case "port_add_del"
	atf_add_test_case "delete_with_ports"
	atf_add_test_case "port_departure"
	atf_add_test_case "flags"
	atf_add_test_case "learning"
	atf_add_test_case "mtu"
	atf_add_test_case "timeout"
	atf_add_test_case "private_ports"
	atf_add_test_case "unprivileged"
	atf_add_test_case "bridge_mutual_exclusion"
	atf_add_test_case "no_host_presence"
	atf_add_test_case "transmit_ipv4_unicast"
	atf_add_test_case "transmit_ipv4_multicast"
	atf_add_test_case "static_addresses"
	atf_add_test_case "vport_constraints"
	atf_add_test_case "vport_host_path"
}
