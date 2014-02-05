/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Implementation of the Transmission Control Protocol(TCP).
 *
 *		IPv4 specific functions
 *
 *
 *		code split from:
 *		linux/ipv4/tcp.c
 *		linux/ipv4/tcp_input.c
 *		linux/ipv4/tcp_output.c
 *
 *		See tcp.c for author information
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

/*
 * Changes:
 *		David S. Miller	:	New socket lookup architecture.
 *					This code is dedicated to John Dyson.
 *		David S. Miller :	Change semantics of established hash,
 *					half is devoted to TIME_WAIT sockets
 *					and the rest go in the other half.
 *		Andi Kleen :		Add support for syncookies and fixed
 *					some bugs: ip options weren't passed to
 *					the TCP layer, missed a check for an
 *					ACK bit.
 *		Andi Kleen :		Implemented fast path mtu discovery.
 *	     				Fixed many serious bugs in the
 *					request_sock handling and moved
 *					most of it into the af independent code.
 *					Added tail drop and some other bugfixes.
 *					Added new listen semantics.
 *		Mike McLagan	:	Routing by source
 *	Juan Jose Ciarlante:		ip_dynaddr bits
 *		Andi Kleen:		various fixes.
 *	Vitaly E. Lavrov	:	Transparent proxy revived after year
 *					coma.
 *	Andi Kleen		:	Fix new listen.
 *	Andi Kleen		:	Fix accept error reporting.
 *	YOSHIFUJI Hideaki @USAGI and:	Support IPV6_V6ONLY socket option, which
 *	Alexey Kuznetsov		allow both IPv4 and IPv6 sockets to bind
 *					a single port at the same time.
 */


#include <linux/bottom_half.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/cache.h>
#include <linux/jhash.h>
#include <linux/init.h>
#include <linux/times.h>

#include <net/net_namespace.h>
#include <net/icmp.h>
#include <net/inet_hashtables.h>
#include <net/tcp.h>
#include <net/transp_v6.h>
#include <net/ipv6.h>
#include <net/inet_common.h>
#include <net/timewait_sock.h>
#include <net/xfrm.h>
#include <net/netdma.h>

#include <linux/inet.h>
#include <linux/ipv6.h>
#include <linux/stddef.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <linux/crypto.h>
#include <linux/scatterlist.h>


/* FIH; Tiger; 2009/12/10 { */
#ifdef CONFIG_FIH_FXX

int tcpFilter_seq_show(struct seq_file *seq, void *v)
{
	struct tcp_iter_state *st;

	if (v == SEQ_START_TOKEN) {
		goto out;
	}
	st = seq->private;

	switch (st->state) {
	case TCP_SEQ_STATE_LISTENING:
	case TCP_SEQ_STATE_ESTABLISHED: 
		{
			struct sock *sk = (struct sock *)v;
			//struct tcp_sock *tp = tcp_sk(sk);
			//const struct inet_connection_sock *icsk = inet_csk(sk);
			struct inet_sock *inet = inet_sk(sk);
			//__be32 dest = inet->daddr;
			__be32 src = inet->rcv_saddr;
			//__u16 destp = ntohs(inet->dport);
			__u16 srcp = ntohs(inet->sport);

			/*
			if(src == 0) {
				printk(KERN_INFO "#skip source address 0.0.0.0:%d\n", srcp);
			}
			else*/ if((src & 0xff) == 127) {
				printk(KERN_INFO "#skip source address 127.X.X.X:%d\n", srcp);
			}
			else {
				seq_printf(seq, "%04X", srcp);
			}
		}
		
		break;
	case TCP_SEQ_STATE_OPENREQ:
		break;
	case TCP_SEQ_STATE_TIME_WAIT:
		break;
	}
out:
	return 0;
}
EXPORT_SYMBOL(tcpFilter_seq_show);
#if 0
static struct tcp_seq_afinfo tcpFilter_seq_afinfo = {
	.name		= "tcpFilter",
	.family		= AF_INET,
	.seq_fops	= {
		.owner		= THIS_MODULE,
	},
	.seq_ops	= {
		.show		= tcpFilter_seq_show,
	},
};
#endif
#endif
/* } FIH; Tiger; 2009/12/10 */
