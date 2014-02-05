/* arch/arm/mach-msm/proc_comm.c
 *
 * Copyright (C) 2007-2008 Google, Inc.
 * Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <mach/msm_iomap.h>
#include <mach/system.h>

#include "proc_comm.h"
#include "smd_private.h"

#if defined(CONFIG_ARCH_MSM7X30)
#define MSM_TRIG_A2M_PC_INT (writel(1 << 6, MSM_GCC_BASE + 0x8))
#elif defined(CONFIG_ARCH_MSM8X60)
#define MSM_TRIG_A2M_PC_INT (writel(1 << 5, MSM_GCC_BASE + 0x8))
#else
#define MSM_TRIG_A2M_PC_INT (writel(1, MSM_CSR_BASE + 0x400 + (6) * 4))
#endif

static inline void notify_other_proc_comm(void)
{
	MSM_TRIG_A2M_PC_INT;
}

#define APP_COMMAND 0x00
#define APP_STATUS  0x04
#define APP_DATA1   0x08
#define APP_DATA2   0x0C

#define MDM_COMMAND 0x10
#define MDM_STATUS  0x14
#define MDM_DATA1   0x18
#define MDM_DATA2   0x1C

static DEFINE_SPINLOCK(proc_comm_lock);

/* Poll for a state change, checking for possible
 * modem crashes along the way (so we don't wait
 * forever while the ARM9 is blowing up.
 *
 * Return an error in the event of a modem crash and
 * restart so the msm_proc_comm() routine can restart
 * the operation from the beginning.
 */
static int proc_comm_wait_for(unsigned addr, unsigned value)
{
	while (1) {
		if (readl(addr) == value)
			return 0;

		if (smsm_check_for_modem_crash())
			return -EAGAIN;

		udelay(5);
	}
}

void msm_proc_comm_reset_modem_now(void)
{
	unsigned base = (unsigned)MSM_SHARED_RAM_BASE;
	unsigned long flags;

	spin_lock_irqsave(&proc_comm_lock, flags);

again:
	if (proc_comm_wait_for(base + MDM_STATUS, PCOM_READY))
		goto again;

	writel(PCOM_RESET_MODEM, base + APP_COMMAND);
	writel(0, base + APP_DATA1);
	writel(0, base + APP_DATA2);

	spin_unlock_irqrestore(&proc_comm_lock, flags);

	notify_other_proc_comm();

	return;
}
EXPORT_SYMBOL(msm_proc_comm_reset_modem_now);

int msm_proc_comm(unsigned cmd, unsigned *data1, unsigned *data2)
{
	unsigned base = (unsigned)MSM_SHARED_RAM_BASE;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&proc_comm_lock, flags);

again:
	if (proc_comm_wait_for(base + MDM_STATUS, PCOM_READY))
		goto again;

	writel(cmd, base + APP_COMMAND);
	writel(data1 ? *data1 : 0, base + APP_DATA1);
	writel(data2 ? *data2 : 0, base + APP_DATA2);

	notify_other_proc_comm();

	if (proc_comm_wait_for(base + APP_COMMAND, PCOM_CMD_DONE))
		goto again;

	if (readl(base + APP_STATUS) == PCOM_CMD_SUCCESS) {
		if (data1)
			*data1 = readl(base + APP_DATA1);
		if (data2)
			*data2 = readl(base + APP_DATA2);
		ret = 0;
	} else {
		ret = -EIO;
	}

	writel(PCOM_CMD_IDLE, base + APP_COMMAND);

	spin_unlock_irqrestore(&proc_comm_lock, flags);
	return ret;
}
EXPORT_SYMBOL(msm_proc_comm);

#ifdef CONFIG_FIH_FXX
int msm_proc_comm_oem(unsigned cmd, unsigned *data1, unsigned *data2, unsigned *cmd_parameter)
{
	unsigned base = (unsigned)MSM_SHARED_RAM_BASE;
	unsigned long flags;
	int ret;
	size_t sizeA, sizeB;
	smem_oem_cmd_data *cmd_buf;

         void* test;
	/* FIH, Debbie Sun, 2009/06/18 { */
	/* get share memory command address dynamically */
	int size;
	sizeA=40;
	sizeB=64;

	
	cmd_buf = smem_get_entry(SMEM_ID_VENDOR1, &size);
     //    test = cmd_buf+sizeof(unsigned int);
     //FIH, WilsonWHLee, 2009/11/26++
     /* [FXX_CR], read product id as serial number*/
     test= (unsigned*)&cmd_buf->cmd_data.cmd_parameter[0];
	//FIH, WilsonWHLee, 2009/11/26--
	/* FIH, Debbie Sun, 2009/06/18 } */
	spin_lock_irqsave(&proc_comm_lock, flags);

again:
	if (proc_comm_wait_for(base + MDM_STATUS, PCOM_READY))
		goto again;

	writel(cmd, base + APP_COMMAND);
	writel(data1 ? *data1 : 0, base + APP_DATA1);
	writel(data2 ? *data2 : 0, base + APP_DATA2);
	cmd_buf->cmd_data.check_flag = smem_oem_locked_flag;
	if(*data1==SMEM_PROC_COMM_OEM_PRODUCT_ID_WRITE)
	{
		//memcpy(cmd_buf->cmd_data.cmd_parameter[0],cmd_parameter,40);
	memcpy(test,(const void *)cmd_parameter,sizeA);

	}else if(*data1==SMEM_PROC_COMM_OEM_TEST_FLAG_WRITE)
	{
		//memcpy(cmd_buf->cmd_data.cmd_parameter[0],cmd_parameter,64);
	memcpy(test,(const void *)cmd_parameter,sizeB);
	}else if(*data1==SMEM_PROC_COMM_OEM_POWER_OFF)
	{
	    memcpy(test,(const void *)cmd_parameter, SMEM_OEM_CMD_BUF_SIZE*sizeof(int));
	}else
	{
		// Set the parameter of OEM_CMD1
		cmd_buf->cmd_data.cmd_parameter[0] = cmd_parameter[0];
	}
	notify_other_proc_comm();

	if (proc_comm_wait_for(base + APP_COMMAND, PCOM_CMD_DONE))
		goto again;
	#if 0
	if (readl(base + APP_STATUS) == PCOM_CMD_SUCCESS) {
		if (data1)
			*data1 = readl(base + APP_DATA1);
		if (data2)
			*data2 = readl(base + APP_DATA2);
		ret = 0;
	} else {
		ret = -EIO;
	}
	#endif
	writel(PCOM_CMD_IDLE, base + APP_COMMAND);

	//spin_unlock_irqrestore(&proc_comm_lock, flags);


	/* read response value, Hanson Lin */
	while(!(cmd_buf->return_data.check_flag & smem_oem_unlocked_flag))
	{
		//waiting
	}
	ret = (cmd_buf->return_data.check_flag & 0x1111);
	//if(ret)
		//return ret;	
	if(!ret)
	{
		if(*data1==SMEM_PROC_COMM_OEM_PRODUCT_ID_READ)
		{
			//memcpy(data2,cmd_buf->return_data.return_value[0],40);
		memcpy((void *)data2, test,sizeA);
		
		}else if(*data1==SMEM_PROC_COMM_OEM_TEST_FLAG_READ)
		{
			//memcpy(data2,cmd_buf->return_data.return_value[0],64);
		memcpy((void *)data2,test,sizeB);
		/* FIH, WilsonWHLee, 2009/11/19 { */
        /* [FXX_CR], add for download tool */ 
		}else if(*data1==SMEM_PROC_COMM_OEM_NV_READ)
		{
			memcpy(data2,&cmd_buf->return_data.return_value[0],128); //WilsonWHLee, 2010/08/12 extend length for pc tool
		  //*test = cmd_buf->return_data.return_value;
		 // memcpy((void *)data2,test,32);
	    /* }FIH:WilsonWHLee 2009/11/19 */
		}else{
			*data2 = cmd_buf->return_data.return_value[0];
		}
	}
	//*data2 = cmd_buf->return_data.return_value[0];
	spin_unlock_irqrestore(&proc_comm_lock, flags);
	return ret;
	/* read response value, Hanson Lin */	
}
EXPORT_SYMBOL(msm_proc_comm_oem);




/* FIH, Tiger, 2009/12/10 { */
#ifdef CONFIG_FIH_FXX
int msm_proc_comm_oem_tcp_filter(void *cmd_data, unsigned cmd_size)
{
	unsigned cmd = PCOM_CUSTOMER_CMD1;
	unsigned oem_cmd = SMEM_PROC_COMM_OEM_UPDATE_TCP_FILTER;
	//unsigned oem_resp;
	unsigned *data1 = &oem_cmd;
	unsigned *data2 = NULL;
	unsigned base = (unsigned)MSM_SHARED_RAM_BASE;
	unsigned long flags;
	int ret;
	smem_oem_cmd_data *cmd_buf;

	void* ptr;
	int size;

#if 1
	unsigned short *content = (unsigned short *)cmd_data;
	for(ret=0; ret<cmd_size/2; ret++) {
		printk(KERN_INFO "tcp filter [%d, %4x]\n", ret, *(content+ret));
	}
#endif

	cmd_buf = smem_get_entry(SMEM_ID_VENDOR1, &size);

	ptr = (unsigned*)&cmd_buf->cmd_data.cmd_parameter[0];

	spin_lock_irqsave(&proc_comm_lock, flags);

again:
	if (proc_comm_wait_for(base + MDM_STATUS, PCOM_READY))
		goto again;

	writel(cmd, base + APP_COMMAND);
	writel(data1 ? *data1 : 0, base + APP_DATA1);
	writel(data2 ? *data2 : 0, base + APP_DATA2);
	cmd_buf->cmd_data.check_flag = smem_oem_locked_flag;
	{
		memcpy(ptr,(const void *)cmd_data,cmd_size);
	}
	notify_other_proc_comm();

	if (proc_comm_wait_for(base + APP_COMMAND, PCOM_CMD_DONE))
		goto again;

	writel(PCOM_CMD_IDLE, base + APP_COMMAND);

	/* read response value, Hanson Lin */
	while(!(cmd_buf->return_data.check_flag & smem_oem_unlocked_flag))
	{
		//waiting
	}
	ret = (cmd_buf->return_data.check_flag & 0x1111);

	spin_unlock_irqrestore(&proc_comm_lock, flags);

	if(ret) {
		printk(KERN_ERR "msm_proc_comm_oem_tcp_filter() returns %d\n", ret);
	}
	
	return ret;
}
EXPORT_SYMBOL(msm_proc_comm_oem_tcp_filter);
#endif
/* } FIH, Tiger, 2009/12/10 */




//Added for new touch calibration by Stanley++
int msm_proc_comm_oem_for_nv(unsigned cmd, unsigned *data1, unsigned *data2, unsigned *cmd_parameter)
{
	unsigned base = (unsigned)MSM_SHARED_RAM_BASE;
	unsigned long flags;
	int ret;
	size_t sizeA, sizeB;
	smem_oem_cmd_data *cmd_buf;

         void* test;
	/* FIH, Debbie Sun, 2009/06/18 { */
	/* get share memory command address dynamically */
	int size;
	sizeA=40;
	sizeB=64;

	
	cmd_buf = smem_get_entry(SMEM_ID_VENDOR1, &size);
         test = cmd_buf+sizeof(unsigned int);
	/* FIH, Debbie Sun, 2009/06/18 } */
	spin_lock_irqsave(&proc_comm_lock, flags);

again:
	if (proc_comm_wait_for(base + MDM_STATUS, PCOM_READY))
		goto again;

	writel(cmd, base + APP_COMMAND);
	writel(data1 ? *data1 : 0, base + APP_DATA1);
	writel(data2 ? *data2 : 0, base + APP_DATA2);
	cmd_buf->cmd_data.check_flag = smem_oem_locked_flag;
	if(*data1==SMEM_PROC_COMM_OEM_PRODUCT_ID_WRITE)
	{
		//memcpy(cmd_buf->cmd_data.cmd_parameter[0],cmd_parameter,40);
	memcpy(test,(const void *)cmd_parameter,sizeA);

	}else if(*data1==SMEM_PROC_COMM_OEM_TEST_FLAG_WRITE)
	{
		//memcpy(cmd_buf->cmd_data.cmd_parameter[0],cmd_parameter,64);
	memcpy(test,(const void *)cmd_parameter,sizeB);
	}else
	{
		// Set the parameter of OEM_CMD1
		cmd_buf->cmd_data.cmd_parameter[0] = cmd_parameter[0];
		cmd_buf->cmd_data.cmd_parameter[1] = cmd_parameter[1];  //Added for new touch calibration by Stanley		
		cmd_buf->cmd_data.cmd_parameter[2] = cmd_parameter[2];  //Added for new touch calibration by Stanley
	}
	notify_other_proc_comm();

	if (proc_comm_wait_for(base + APP_COMMAND, PCOM_CMD_DONE))
		goto again;
	#if 0
	if (readl(base + APP_STATUS) == PCOM_CMD_SUCCESS) {
		if (data1)
			*data1 = readl(base + APP_DATA1);
		if (data2)
			*data2 = readl(base + APP_DATA2);
		ret = 0;
	} else {
		ret = -EIO;
	}
	#endif
	writel(PCOM_CMD_IDLE, base + APP_COMMAND);

	//spin_unlock_irqrestore(&proc_comm_lock, flags);


	/* read response value, Hanson Lin */
	while(!(cmd_buf->return_data.check_flag & smem_oem_unlocked_flag))
	{
		//waiting
	}
	ret = (cmd_buf->return_data.check_flag & 0x1111);
	//if(ret)
		//return ret;	
	if(!ret)
	{
		if(*data1==SMEM_PROC_COMM_OEM_PRODUCT_ID_READ)
		{
			//memcpy(data2,cmd_buf->return_data.return_value[0],40);
		memcpy((void *)data2, test,sizeA);
		
		}else if(*data1==SMEM_PROC_COMM_OEM_TEST_FLAG_READ)
		{
			//memcpy(data2,cmd_buf->return_data.return_value[0],64);
		memcpy((void *)data2,test,sizeB);
		}else{
			*data2 = cmd_buf->return_data.return_value[0];
		}

		memcpy(cmd_parameter, cmd_buf->return_data.return_value, 8);  //Added for new touch calibration by Stanley		
	}
	//*data2 = cmd_buf->return_data.return_value[0];
	spin_unlock_irqrestore(&proc_comm_lock, flags);
	return ret;
	/* read response value, Hanson Lin */	
}
EXPORT_SYMBOL(msm_proc_comm_oem_for_nv);
//Added for new touch calibration by Stanley--
#endif

/*FIH, Debbie, 2011/01/17 { */
int msm_proc_comm_oem_otp_process(unsigned *return_size, unsigned *return_data, unsigned *cmd_parameter)
{
	unsigned cmd = PCOM_CUSTOMER_CMD1;
	unsigned oem_cmd = SMEM_PROC_COMM_OEM_OTP_PROCESS;
	unsigned base = (unsigned)MSM_SHARED_RAM_BASE;
	unsigned long flags;
	int ret;
	size_t data_size;
	smem_oem_cmd_data *cmd_buf = NULL;

	void* test;
	int size;
	unsigned *data1 = &oem_cmd;
	unsigned *data2 = NULL;
				
	cmd_buf = smem_get_entry(SMEM_ID_VENDOR1, &size);
	
	data_size = cmd_parameter[2];
	
	//test = cmd_buf+sizeof(unsigned int);
	//FIH, WilsonWHLee, 2009/11/26++
	/* [FXX_CR], read product id as serial number*/
	test= (unsigned*)&cmd_buf->cmd_data.cmd_parameter[0];
	//FIH, WilsonWHLee, 2009/11/26--
	
	/* FIH, Debbie Sun, 2009/06/18 } */
	spin_lock_irqsave(&proc_comm_lock, flags);

again:
	if (proc_comm_wait_for(base + MDM_STATUS, PCOM_READY))
		goto again;

	writel(cmd, base + APP_COMMAND);
	writel(data1 ? *data1 : 0, base + APP_DATA1);
	writel(data2 ? *data2 : 0, base + APP_DATA2);
	cmd_buf->cmd_data.check_flag = smem_oem_locked_flag;

	// Set the parameter of OEM_CMD1
	cmd_buf->cmd_data.cmd_parameter[0] = cmd_parameter[0];
	cmd_buf->cmd_data.cmd_parameter[1] = cmd_parameter[1];
	cmd_buf->cmd_data.cmd_parameter[2] = cmd_parameter[2];

	if(cmd_parameter[0] == SMSM_WRITE_OTP)
	{
		memcpy(cmd_buf->cmd_data.cmd_parameter+3, cmd_parameter+3, data_size);
		//memcpy(test, cmd_parameter, data_size);
	}
	else
	{
		cmd_buf->cmd_data.cmd_parameter[3] = cmd_parameter[3];
	}
	

	notify_other_proc_comm();

	if (proc_comm_wait_for(base + APP_COMMAND, PCOM_CMD_DONE))
		goto again;
	#if 0
	if (readl(base + APP_STATUS) == PCOM_CMD_SUCCESS) {
		if (data1)
			*data1 = readl(base + APP_DATA1);
		if (data2)
			*data2 = readl(base + APP_DATA2);
		ret = 0;
	} else {
		ret = -EIO;
	}
	#endif
	writel(PCOM_CMD_IDLE, base + APP_COMMAND);

	//spin_unlock_irqrestore(&proc_comm_lock, flags);

	/* read response value, Hanson Lin */
	while(!(cmd_buf->return_data.check_flag & smem_oem_unlocked_flag))
	{
		//waiting
	}
	ret = (cmd_buf->return_data.check_flag & 0x1111);
	//if(ret)
		//return ret;	

	printk(KERN_INFO "OTP cmd_parameter[0]  = %d\n", cmd_parameter[0] );
	//printk(KERN_INFO "cmd_buf->cmd_data.cmd_parameter[0]  = %d\n", cmd_buf->cmd_data.cmd_parameter[0] );

	
	if(!ret)
	{
		if(cmd_parameter[0] == SMSM_READ_UUID)
		{
			//return_size = cmd_buf->return_data.return_value[0];
			memcpy((void *)return_size,(const void *)&cmd_buf->return_data.return_value[0], 1);

			printk(KERN_INFO "uuid return_size = %d\n", *return_size);
			memcpy((void *)return_data,(const void *)&cmd_buf->return_data.return_value[1], *return_size);

			printk(KERN_INFO "uuid, cmd_buf->return_data.return_value[1] = 0x%X\n", cmd_buf->return_data.return_value[1]);
			printk(KERN_INFO "uuid, cmd_buf->return_data.return_value[2] = 0x%X\n", cmd_buf->return_data.return_value[2]);
			printk(KERN_INFO "uuid, cmd_buf->return_data.return_value[3] = 0x%X\n", cmd_buf->return_data.return_value[3]);
			printk(KERN_INFO "uuid, cmd_buf->return_data.return_value[4] = 0x%X\n", cmd_buf->return_data.return_value[4]);
		
		}
		else if(cmd_parameter[0] == SMSM_READ_OTP)	
		{
			return_size = (unsigned *)&data_size;
			printk(KERN_INFO "read otp return_size= %d\n", *return_size);
			memcpy((void *)return_data,(const void *)&cmd_buf->return_data.return_value[1], data_size);
			printk(KERN_INFO "read otp, cmd_buf->return_data.return_value[0] = 0x%X\n", cmd_buf->return_data.return_value[0]);
			printk(KERN_INFO "read otp, cmd_buf->return_data.return_value[1] = 0x%X\n", cmd_buf->return_data.return_value[1]);
			printk(KERN_INFO "read otp, cmd_buf->return_data.return_value[2] = 0x%X\n", cmd_buf->return_data.return_value[2]);
			printk(KERN_INFO "read otp, cmd_buf->return_data.return_value[3] = 0x%X\n", cmd_buf->return_data.return_value[3]);		
		}
		else  // SMSM_WRITE_OTP
		{
			memcpy((void *)return_size,(const void *)&cmd_buf->return_data.return_value[0], 1);
			
			printk(KERN_INFO "write_otp, return_size = %d\n", *return_size);
		}
	}
	spin_unlock_irqrestore(&proc_comm_lock, flags);

	return ret;
	/* read response value, Hanson Lin */	
}
/*FIH, Debbie, 2011/01/17 } */

/* FIH, WillChen 2009/08/24++ { */
#ifdef CONFIG_FIH_FXX
int proc_comm_read_adie_adc(unsigned *cmd_parameter)
{
	unsigned smem_response;
	uint32_t oem_cmd = SMEM_PROC_COMM_OEM_ADIE_ADC_READ;
	/* cmd_parameter setting */

	msm_proc_comm_oem(PCOM_CUSTOMER_CMD1, &oem_cmd, &smem_response, cmd_parameter );

	return smem_response;
}
EXPORT_SYMBOL(proc_comm_read_adie_adc);
#endif
//FIH, WillChen, 2009/8/24--
/* FIH, Michael Kao, 2009/07/02{ */
/* [FXX_CR], Add For TC6507 LED Expander */
#ifdef CONFIG_FIH_FXX
int proc_comm_read_adc(unsigned *cmd_parameter)
{
	unsigned smem_response;
	uint32_t oem_cmd = SMEM_PROC_COMM_OEM_ADC_READ;
	/* cmd_parameter setting */

	if(msm_proc_comm_oem(PCOM_CUSTOMER_CMD1, &oem_cmd, &smem_response, cmd_parameter ))
		return -1;

	return smem_response;
}
EXPORT_SYMBOL(proc_comm_read_adc);
#endif
/* } FIH, Michael Kao, 2009/07/02 */

//+++ FIH, KarenLiao @2009/09/21: Enable headset mic bias by smem command for F0X.B-4104.
#ifdef CONFIG_FIH_FXX
int proc_comm_enable_pm_mic(unsigned *cmd_parameter)
{
	unsigned smem_response;
	uint32_t oem_cmd = SMEM_PROC_COMM_OEM_PM_MIC_EN;
	/* cmd_parameter setting */
	printk(KERN_INFO "Send SMEM_PROC_COMM_OEM_PM_MIC_EN to SMEM\n");

	msm_proc_comm_oem(PCOM_CUSTOMER_CMD1, &oem_cmd, &smem_response, cmd_parameter);
	
	return smem_response;
}

EXPORT_SYMBOL(proc_comm_enable_pm_mic);
#endif
//--- FIH, KarenLiao @2009/09/21: Enable headset mic bias by smem command for F0X.B-4104.

#if 1
//Added for new touch calibration by Stanley++
int proc_comm_read_nv(unsigned *cmd_parameter)
{
	unsigned smem_response;
	uint32_t oem_cmd = SMEM_PROC_COMM_OEM_NV_READ;

	/* cmd_parameter setting */

        if(msm_proc_comm_oem_for_nv(PCOM_CUSTOMER_CMD1, &oem_cmd, &smem_response, cmd_parameter))
        {
        	return -1;
        }

    //memcpy(cmd_parameter, &oem_cmd, 8);
	return 0;
}
EXPORT_SYMBOL(proc_comm_read_nv);
int proc_comm_write_nv(unsigned *cmd_parameter)
{
	unsigned smem_response;
	uint32_t oem_cmd = SMEM_PROC_COMM_OEM_NV_WRITE;
	/* cmd_parameter setting */

	msm_proc_comm_oem_for_nv(PCOM_CUSTOMER_CMD1, &oem_cmd, &smem_response, cmd_parameter);

	return oem_cmd;
}

EXPORT_SYMBOL(proc_comm_write_nv);
//Added for new touch calibration by Stanley--
#endif

#ifdef CONFIG_FIH_FXX
// +++ For SD card download, paul huang
int proc_comm_alloc_sd_dl_smem(int action_id)
{
	unsigned smem_response = 0;
	uint32_t oem_cmd = SMEM_PROC_COMM_OEM_ALLOC_SD_DL_INFO;
    unsigned int cmd_parameter = action_id;
	/* cmd_parameter setting */

    return msm_proc_comm_oem(PCOM_CUSTOMER_CMD1, &oem_cmd, &smem_response, &cmd_parameter );
}
EXPORT_SYMBOL(proc_comm_alloc_sd_dl_smem);
#endif
// --- For SD card download, paul huang

/* FIH, IvanCHTang, 2010/11/11{ */
/* [FXX_CR], Migrate from 4215 FTM++ */

/* FIH, Michael Kao, 2009/08/28{ */
/* [FXX_CR], Add to turn on Mic Bias */
int proc_comm_micbias_set(unsigned *cmd_parameter)
{
	unsigned smem_response;
	uint32_t oem_cmd = SMEM_PROC_COMM_OEM_PM_MIC_EN;
	/* cmd_parameter setting */

	msm_proc_comm_oem(PCOM_CUSTOMER_CMD1, &oem_cmd, &smem_response, cmd_parameter );

	return smem_response;
}
EXPORT_SYMBOL(proc_comm_micbias_set);
/* FIH, Michael Kao, 2009/08/28{ */

/* FIH, Henry Juang, 2009/08/10{ */
/* [FXX_CR], Add For FTM PID and testFlag ++*/
void proc_comm_read_PID_testFlag( uint32_t oem_cmd,unsigned* data )
{
	unsigned cmd_parameter=0;
	/* cmd_parameter setting */
	msm_proc_comm_oem(PCOM_CUSTOMER_CMD1, &oem_cmd, data, &cmd_parameter );
//	msm_proc_comm_oem(PCOM_CUSTOMER_CMD1, &oem_cmd, &cmd_parameter, data );
	

}
EXPORT_SYMBOL( proc_comm_read_PID_testFlag);

void proc_comm_write_PID_testFlag( uint32_t oem_cmd,unsigned* data )
{
	unsigned cmd_parameter=0;
	/* cmd_parameter setting */
	msm_proc_comm_oem(PCOM_CUSTOMER_CMD1, &oem_cmd, &cmd_parameter, data );// data input to last parameter.
}
EXPORT_SYMBOL( proc_comm_write_PID_testFlag);
/* }  FIH, Henry Juang, 2009/08/10 --*/
/* [FXX_CR], Add For FTM PID and testFlag */

/* FIH, Henry Juang, 2010/04/26{ */
/* [FXX_CR], Add For R/W NV8029 from at command. ++*/
#define NV_MAX_I 8030
int write_NV_single(int bBoot,int val)
{
	unsigned nv_item_ftm_bootcount[3];
	unsigned smem_response,ret;
	uint32_t oem_cmd = SMEM_PROC_COMM_OEM_NV_WRITE;
	nv_item_ftm_bootcount[0] = bBoot;
	nv_item_ftm_bootcount[1] = val;
	printk("*********write_NV NV=%d,val=%d\n",bBoot,val);
	if(nv_item_ftm_bootcount[0]<0 || nv_item_ftm_bootcount[0]>=NV_MAX_I || nv_item_ftm_bootcount[1]<0)
	{
		printk("*********write_NV8029 fail. NV=%d,val=%d\n",nv_item_ftm_bootcount[0],
nv_item_ftm_bootcount[1]);
		return -1;
	}

	ret =msm_proc_comm_oem(PCOM_CUSTOMER_CMD1, &oem_cmd, &smem_response, nv_item_ftm_bootcount);
	if( ret == 0)
	{
		return 0;
	}
	else
	{
		return -1;  
	}
}

EXPORT_SYMBOL(write_NV_single);
/* FIH, Henry Juang, 2010/04/26{ */
/* [FXX_CR], Add For R/W NV8029 from at command. --*/

//FIH,NeoChen, for read&write FUSE,20100727 ++
void proc_comm_write_FUSE( uint32_t oem_cmd)
{
	unsigned cmd_parameter=0;
	unsigned smem_response=0;
	/* cmd_parameter setting */
	msm_proc_comm_oem(PCOM_CUSTOMER_CMD1, &oem_cmd, &smem_response, &cmd_parameter );
}
EXPORT_SYMBOL( proc_comm_write_FUSE);
void proc_comm_read_FUSE( uint32_t oem_cmd,unsigned* data )
{
	unsigned cmd_parameter=0;
	/* cmd_parameter setting */
	msm_proc_comm_oem(PCOM_CUSTOMER_CMD1, &oem_cmd, data, &cmd_parameter );
}
EXPORT_SYMBOL( proc_comm_read_FUSE);
//FIH,NeoChen, for read&write FUSE,20100727 --

//FIH,Henry Juang, for GRE power key detection,20100816 ++
int proc_comm_read_poc(void)
{
	unsigned smem_response;
	unsigned cmd_parameter[5];
	uint32_t oem_cmd = SMEM_PROC_COMM_OEM_PWR_KEY_DECT;
	int ret;
	/* cmd_parameter setting */
	printk("proc_comm_read_poc oem_cmd=%d\n",smem_response);
	ret=msm_proc_comm_oem_for_nv(PCOM_CUSTOMER_CMD1, &oem_cmd, &smem_response, cmd_parameter );
	printk("msm_proc_comm_oem_PK ret=%d, ome_cmd=%d,smem_response=%d\n",ret,oem_cmd,smem_response);

	return smem_response;
}
EXPORT_SYMBOL(proc_comm_read_poc);
//FIH,Henry Juang, for GRE power key detection,20100816 --

/* [FXX_CR], Migrate from 4215 FTM-- */
/*} FIH, IvanCHTang, 2010/11/11*/


/* 20101208, SquareCHFang, NVBackup { */
int proc_comm_ftm_backup_unique_nv(void)
{    
    uint32_t oem_cmd = SMEM_PROC_COMM_OEM_BACKUP_UNIQUE_NV;
    unsigned smem_response = 0;
    unsigned cmd_parameter = 0;       
    int32_t ret = 0;
    
    printk(KERN_ERR "%s: Call SMEM_PROC_COMM_OEM_BACKUP_UNIQUE_NV\n", __func__);    
    
    ret = msm_proc_comm_oem(PCOM_CUSTOMER_CMD1, &oem_cmd, &smem_response, &cmd_parameter );
        
    if (ret != 0){
        printk(KERN_ERR
                "%s: SMEM_PROC_COMM_OEM_BACKUP_UNIQUE_NV FAILED!!\n",
                __func__);
                
    	return ret;
    }
    
    printk(KERN_INFO "proc_comm_ftm_backup_unique_nv()\n");
    
    return ret;
}
EXPORT_SYMBOL(proc_comm_ftm_backup_unique_nv);

int proc_comm_ftm_nv_flag(void)
{
    uint32_t oem_cmd = SMEM_PROC_COMM_OEM_BACKUP_NV_FLAG;
    unsigned smem_response = 0;
    unsigned cmd_parameter = 0;       
    int32_t ret = 0;   
    
    printk(KERN_ERR "%s: Call SMEM_PROC_COMM_OEM_BACKUP_NV_FLAG\n", __func__);  
    
    ret = msm_proc_comm_oem(PCOM_CUSTOMER_CMD1, &oem_cmd, &smem_response, &cmd_parameter );
        
    if (ret != 0){
        printk(KERN_ERR
                "%s: SMEM_PROC_COMM_OEM_BACKUP_NV_FLAG FAILED!!\n",
                __func__);
                
    	return ret;
    }
    
    printk(KERN_INFO "proc_comm_ftm_backup_unique_nv()\n");
    
    return ret;
}
EXPORT_SYMBOL(proc_comm_ftm_nv_flag);
/* } 20101208, SquareCHFang, NVBackup */

/* FIH, Debbie, 2011/01/12 {  */
int proc_comm_read_uuid(unsigned * uuid_ptr)
{
	unsigned return_len = 0;
	unsigned cmd_parameter[4];
	int32_t ret = 0;   
	cmd_parameter[0] = SMSM_READ_UUID;
	cmd_parameter[1] = 0;  //temp
	cmd_parameter[2] = 16;
	cmd_parameter[3] = 0;  //temp

	/* cmd_parameter setting */
	ret = msm_proc_comm_oem_otp_process(&return_len, uuid_ptr, cmd_parameter);
        if(ret != 0)
        {
        	printk(KERN_ERR "%s: read UUID FAILED!!\n",__func__);
        	return ret;
        }
	printk(KERN_INFO "uuid return_len = %d\n", return_len);
	printk(KERN_INFO "finish read uuid, ret = %d\n", ret);
	return 0;
}
EXPORT_SYMBOL(proc_comm_read_uuid);

int proc_comm_read_otp(unsigned int page, unsigned int size_otp, unsigned * otp_data)
{
	unsigned return_len = 0;
	unsigned cmd_parameter[4];
	int32_t ret = 0;   
	cmd_parameter[0] = SMSM_READ_OTP;
	cmd_parameter[1] = page;
	cmd_parameter[2] = size_otp;
	cmd_parameter[3] = 0; 

	/* cmd_parameter setting */
	ret = msm_proc_comm_oem_otp_process(&return_len, otp_data, cmd_parameter);
        if(ret != 0)
        {
        	printk(KERN_ERR "%s: read OTP FAILED!!\n",__func__);
        	return ret;
        }
	return 0;
}
EXPORT_SYMBOL(proc_comm_read_otp);

int proc_comm_write_otp(int page, int size_otp, unsigned * otp_data)
{
	unsigned return_len = 0;
	unsigned cmd_parameter[4];
	int32_t ret = 0;   
	int k = 0;
	cmd_parameter[0] = SMSM_WRITE_OTP;
	cmd_parameter[1] = page;
	cmd_parameter[2] = size_otp;
	//cmd_parameter[3] = *otp_data;

	memcpy(cmd_parameter+3, otp_data, size_otp);
 
       for(k = 0; k<4; k++)
       {
		printk(KERN_INFO "cmd_parameter[%d]  = 0x%X\n", k, cmd_parameter[k] );
       }

	/* cmd_parameter setting */
	ret = msm_proc_comm_oem_otp_process(&return_len, NULL, cmd_parameter);
        if(ret != 0)
        {
        	printk(KERN_ERR "%s: write OTP FAILED!!\n",__func__);
        	return ret;
        }
	return 0;
}
EXPORT_SYMBOL(proc_comm_write_otp);
/* FIH, Debbie, 2011/01/12 }  */


