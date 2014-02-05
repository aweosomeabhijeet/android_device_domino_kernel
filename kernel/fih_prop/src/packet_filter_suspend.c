/*
 * kernel/power/suspend.c - Suspend to RAM and standby functionality.
 *
 * Copyright (c) 2003 Patrick Mochel
 * Copyright (c) 2003 Open Source Development Lab
 * Copyright (c) 2009 Rafael J. Wysocki <rjw@sisk.pl>, Novell Inc.
 *
 * This file is released under the GPLv2.
 */

#include <linux/string.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/cpu.h>
#include <linux/syscalls.h>

//#include "power.h"

/* FIH;Tiger;2009/12/10 { */
#ifdef CONFIG_FIH_FXX
#include <linux/fs.h>
#include "../../../arch/arm/mach-msm/proc_comm.h"
#endif
/* } FIH;Tiger;2009/12/10 */

/* FIH; Tiger; 2009/12/10 { */
#ifdef CONFIG_FIH_FXX
void set_packet_filter(void)
{
	struct file *tcpFs = NULL;
	struct file *tcpFs6 = NULL;
	char tcpState[5];
	unsigned short tcpDestPort = 0; 

	if(tcpFs==NULL)
	{
		tcpFs = filp_open("/proc/net/tcpFilter", O_RDONLY, 0);
		if(IS_ERR(tcpFs))
		{
			tcpFs = NULL;
			printk(KERN_ERR "Tiger> can't open /proc/net/tcpFilter\n");
		}
		else
		{
			tcpFs6 = filp_open("/proc/net/tcpFilter6", O_RDONLY, 0);
			if(IS_ERR(tcpFs6))
			{
				filp_close(tcpFs, NULL);   
				tcpFs = NULL;
				tcpFs6 = NULL;
				printk(KERN_ERR "Tiger> can't open /proc/net/tcpFilter6\n");
			}
			else
			{
				unsigned char oem_cmd_buf[128];
				unsigned short *oem_ptr = (unsigned short *)oem_cmd_buf;
				unsigned short *port_start = NULL;
				int portCount = 0, cmdSize = 0;

				*oem_ptr = 0;
				oem_ptr ++;
				cmdSize ++;

				*oem_ptr = CLEAR_TABLE;
				oem_ptr ++;
				cmdSize ++;

				/* IPv4 */
				//tcpFs->f_op->llseek(tcpFs, 0, 0);
				while(tcpFs->f_op->read(tcpFs, (char *)&tcpState, 4, &tcpFs->f_pos) == 4)
				{
					tcpState[4] = 0;
					tcpDestPort = (tcpState[0]<='9') ? (tcpState[0]-'0')<<12 : (tcpState[0]-'A'+10)<<12;
					tcpDestPort += (tcpState[1]<='9') ? (tcpState[1]-'0')<<8 : (tcpState[1]-'A'+10)<<8;
					tcpDestPort += (tcpState[2]<='9') ? (tcpState[2]-'0')<<4 : (tcpState[2]-'A'+10)<<4;
					tcpDestPort += (tcpState[3]<='9') ? (tcpState[3]-'0') : (tcpState[3]-'A'+10);
					
					//printk(KERN_ERR "filter tcp destination port (%d)\n", tcpDestPort);
					//printk(KERN_ERR "filter tcp destination port (%s)\n", tcpState);

					if(port_start == NULL) 
					{
						port_start = oem_ptr;
						*oem_ptr = ADD_DEST_PORT;
						oem_ptr += 2;
						cmdSize += 2;
						*oem_ptr = tcpDestPort;
						oem_ptr ++;
						cmdSize ++;
						portCount = 1;
					}
					else 
					{
						int i;
						// search dummy port
						for(i=0; i<portCount; i++) 
						{
							if(*(port_start+i+2) == tcpDestPort) 
							{
								break;
							}
						}
						
						if(i == portCount) 
						{
							*oem_ptr = tcpDestPort;
							oem_ptr ++;
							cmdSize ++;
							portCount ++;
							if(portCount == 59) {
								oem_ptr = (unsigned short *)oem_cmd_buf;
								*oem_ptr = cmdSize<<1;
								*(port_start+1) = portCount;
								msm_proc_comm_oem_tcp_filter(oem_ptr, cmdSize<<1); 

								*oem_ptr = 0;
								oem_ptr ++;
								cmdSize = 1;
								port_start = NULL;
								portCount = 0;
							}
						}
					}
				}

				/* IPv6 */
				//tcpFs6->f_op->llseek(tcpFs6, 0, 0);
				while(tcpFs6->f_op->read(tcpFs6, (char *)&tcpState, 4, &tcpFs6->f_pos) == 4)
				{
					tcpState[4] = 0;
					tcpDestPort = (tcpState[0]<='9') ? (tcpState[0]-'0')<<12 : (tcpState[0]-'A'+10)<<12;
					tcpDestPort += (tcpState[1]<='9') ? (tcpState[1]-'0')<<8 : (tcpState[1]-'A'+10)<<8;
					tcpDestPort += (tcpState[2]<='9') ? (tcpState[2]-'0')<<4 : (tcpState[2]-'A'+10)<<4;
					tcpDestPort += (tcpState[3]<='9') ? (tcpState[3]-'0') : (tcpState[3]-'A'+10);
					
					//printk(KERN_ERR "filter tcp destination port (%d)\n", tcpDestPort);
					//printk(KERN_ERR "filter tcp destination port (%s)\n", tcpState);

					if(port_start == NULL) 
					{
						port_start = oem_ptr;
						*oem_ptr = ADD_DEST_PORT;
						oem_ptr += 2;
						cmdSize += 2;
						*oem_ptr = tcpDestPort;
						oem_ptr ++;
						cmdSize ++;
						portCount = 1;
					}
					else 
					{
						int i;
						// search dummy port
						for(i=0; i<portCount; i++) 
						{
							if(*(port_start+i+2) == tcpDestPort) 
							{
								break;
							}
						}
						
						if(i == portCount) 
						{
							*oem_ptr = tcpDestPort;
							oem_ptr ++;
							cmdSize ++;
							portCount ++;
							if(portCount == 59) {
								oem_ptr = (unsigned short *)oem_cmd_buf;
								*oem_ptr = cmdSize<<1;
								*(port_start+1) = portCount;
								msm_proc_comm_oem_tcp_filter(oem_ptr, cmdSize<<1); 

								*oem_ptr = 0;
								oem_ptr ++;
								cmdSize = 1;
								port_start = NULL;
								portCount = 0;
							}
						}
					}
				}

				*oem_ptr = UPDATE_COMPLETE;
				cmdSize ++;
				oem_ptr = (unsigned short *)oem_cmd_buf;
				*oem_ptr = cmdSize<<1;
				if(port_start != NULL) 
				{
					*(port_start+1) = portCount;
				}

				msm_proc_comm_oem_tcp_filter(oem_ptr, cmdSize<<1);  

				filp_close(tcpFs, NULL);   
				tcpFs = NULL;
				filp_close(tcpFs6, NULL);   
				tcpFs6 = NULL;				
			}
		}
	}
}
EXPORT_SYMBOL(set_packet_filter);
#endif
/* } FIH; Tiger; 2009/12/10 */
