/* linux/include/asm-arm/arch-msm/msm_smd.h
 *
 * Copyright (C) 2007 Google, Inc.
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

#ifndef __ASM_ARCH_MSM_SMD_H
#define __ASM_ARCH_MSM_SMD_H

typedef struct smd_channel smd_channel_t;

#ifdef CONFIG_FIH_FXX

/* FIH, Debbie Sun, 2009/06/18 { */
/* get share memory command address dynamically */
struct smem_oem_info
{
        unsigned int hw_id;
        unsigned int keypad_info;
        unsigned int power_on_cause;
	/* FIH, MichaelKao, 2009/07/16 { */
	/* add for Read modem mode from smem */
        unsigned int network_mode;
	/* FIH, MichaelKao, 2009/07/16 { */
	/* FIH, Debbie modify, 2010/01/06 { */
        /* add for display info about download and sd ram dump */
        unsigned int oemsbl_mode;
        char   flash_name[32];
        char   oem_mod_rev[16];
        unsigned int progress;
        unsigned int msg_counter;
        /* FIH, Debbie modify, 2010/01/06 } */
	/* FIH, Debbie Sun, 2010/05/24 { */
	/* get ram info form share memory */
        unsigned int dram_info;
	/* FIH, Debbie Sun, 2010/05/24 { */
};
/* FIH, Debbie Sun, 2009/06/18 }*/

/* FIH, Debbie, 2009/09/11 { */
/* switch UART for printk log */
struct smem_host_oem_info
{
        unsigned int host_usb_id;
        unsigned int host_log_from_uart;
        unsigned int host_enable_kpd;
        unsigned int host_used4;
};
/* FIH, Debbie, 2009/09/11 } */

/* FIH, MichaelKao, 2009/07/16 { */
/* add for Read modem mode from smem */
typedef enum
{
    FIH_ERROR           = 0,
    FIH_GSM             = 0x0001,
    FIH_WCDMA           = 0x0002, // WCDMA only, e.g. in release 4315, the build id is TSNCJOLY
    FIH_CDMA1X          = 0x0004, // CDMA2000 only
    FIH_WCDMA_CDMA1X    = (FIH_WCDMA | FIH_CDMA1X), // WORLD MODE
    FIH_NETWORK_MODE_MAX
} fih_network_mode_type;
/* FIH, MichaelKao, 2009/07/16 } */

/* FIH, JiaHao, 2010/12/17 { */

/* FIH, JiaHao, 2011/01/26 { */
/* New HWID Mechanism */
/* OrigHWID {
    [31:30] = Reserved
    [29]    = HAC_Support
    [28]    = Headdset_Type
    [27:26] = Reserved
    [25:24] = SIM_ID
    [23:16] = Band_ID
    [15: 8] = Phase_ID
    [ 7: 0] = Product_ID } */
/* FIH, JiaHao, 2011/01/26 } */

typedef enum 
{
    Project_DQE = 0x1,
    Project_DMQ = 0x2,
    Project_DQD = 0x3,
    Project_DPD = 0x4,
    Project_DMP = 0x5,
    Project_DP2 = 0x6,
    Project_FAD = 0x7,
    /* FIH, JiaHao, 2010/12/30 { */
    Project_AI1S = 0x8,
    Project_AI1D = 0x9,
    /* FIH, JiaHao, 2010/12/30 } */
    /* FIH, JiaHao, 2011/01/26 { */
    Project_DMO = 0xA,
    Project_DMT = 0xB,
    /* FIH, JiaHao, 2010/01/26 } */
    FIH_PROJECT_MAX
} fih_product_id_type; // [7:0]

typedef enum 
{
    Phase_PR1 = 0x1,
    Phase_PR2,
    Phase_PR3,
    Phase_PR4,
    Phase_PR5,
    Phase_PR6,
    Phase_PR7,
    Phase_PR8,
    FIH_PHASE_MAX,
} fih_phase_id_type; // [15:8]

typedef enum 
{
    GSM_900_1800_CDMA_BC0 = 0x1,
    CDMA_BC0              = 0x2,
    WCDMA_145_band        = 0x6,
    WCDMA_148_band        = 0x7,
    WCDMA_245_band        = 0x8,
    WCDMA_15_band         = 0x9,
    WCDMA_18_band         = 0xA,
    WCDMA_25_band         = 0xB,
    /* FIH, JiaHao, 2010/12/30 { */
    WCDMA_125_band        = 0xC,
    WCDMA_128_band        = 0xD,
    WCDMA_14_band         = 0xE,
    /* FIH, JiaHao, 2010/12/30 } */
    FIH_BAND_MAX,
} fih_band_id_type; // [23:16]

/* FIH, JiaHao, 2011/01/26 { */
typedef enum
{
    SIM_SINGLE = 0x0,
    SIM_DUAL   = 0x1,
} fih_sim_id_type; // [25:24]
/* FIH, JiaHao, 2011/01/26 } */

typedef enum
{
    NO_OMTP = 0x0,
    BE_OMTP = 0x1,
} fih_headset_type; //[28]

typedef enum
{
    NO_HAC = 0x0,
    BE_HAC = 0x1,
} fih_hac_support; // [29]

/* FIH, JiaHao, 2010/12/17 } */

#endif /* CONFIG_FIH_FXX */

/* warning: notify() may be called before open returns */
int smd_open(const char *name, smd_channel_t **ch, void *priv,
	     void (*notify)(void *priv, unsigned event));

#define SMD_EVENT_DATA 1
#define SMD_EVENT_OPEN 2
#define SMD_EVENT_CLOSE 3

int smd_close(smd_channel_t *ch);

/* passing a null pointer for data reads and discards */
int smd_read(smd_channel_t *ch, void *data, int len);
int smd_read_from_cb(smd_channel_t *ch, void *data, int len);

/* Write to stream channels may do a partial write and return
** the length actually written.
** Write to packet channels will never do a partial write --
** it will return the requested length written or an error.
*/
int smd_write(smd_channel_t *ch, const void *data, int len);

int smd_write_avail(smd_channel_t *ch);
int smd_read_avail(smd_channel_t *ch);

/* Returns the total size of the current packet being read.
** Returns 0 if no packets available or a stream channel.
*/
int smd_cur_packet_size(smd_channel_t *ch);


#if 0
/* these are interruptable waits which will block you until the specified
** number of bytes are readable or writable.
*/
int smd_wait_until_readable(smd_channel_t *ch, int bytes);
int smd_wait_until_writable(smd_channel_t *ch, int bytes);
#endif

/* these are used to get and set the IF sigs of a channel.
 * DTR and RTS can be set; DSR, CTS, CD and RI can be read.
 */
int smd_tiocmget(smd_channel_t *ch);
int smd_tiocmset(smd_channel_t *ch, unsigned int set, unsigned int clear);

#if defined(CONFIG_MSM_N_WAY_SMD)
enum {
	SMD_APPS_MODEM = 0,
	SMD_APPS_QDSP,
	SMD_MODEM_QDSP,
	SMD_LOOPBACK_TYPE = 100,

};
#else
enum {
	SMD_APPS_MODEM = 0,
	SMD_LOOPBACK_TYPE = 100,
};
#endif

int smd_named_open_on_edge(const char *name, uint32_t edge, smd_channel_t **_ch,
			   void *priv, void (*notify)(void *, unsigned));

#ifdef CONFIG_FIH_FXX

/* FIH, Debbie Sun, 2009/06/18 { */
/* get share memory command address dynamically */
void fih_smem_alloc(void);

/* switch UART for printk log */
void fih_smem_alloc_for_host_used(void);

unsigned int fih_read_hwid_from_smem(void);

/* modify new hardware id */
unsigned int fih_read_orig_hwid_from_smem(void);

unsigned int fih_read_mode_from_smem(void);

/* add for Read modem mode from smem */
unsigned int fih_read_network_mode_from_smem(void);
#define POWER_ON_CAUSE_PROC_READ_ENTRY 1
#ifdef POWER_ON_CAUSE_PROC_READ_ENTRY
unsigned int fih_read_power_on_cuase_from_smem(void);
#endif

/* switch UART for printk log */
unsigned int fih_read_uart_switch_from_smem(void);
/* FIH, Debbie Sun, 2009/06/18 }*/

unsigned int fih_read_usb_id_from_smem(void);
unsigned int fih_read_kpd_from_smem(void);
/* FIH, Debbie Sun, 2010/05/24 { */
/* get ram info and device name form share memory */
unsigned int fih_read_dram_info_from_smem(void);
char* fih_read_flash_name_from_smem(void);
/* FIH, Debbie Sun, 2010/05/24 } */

/* FIH, JiaHao, 2010/08/04 } */
/* return jogball device is exist or not { (0)=not_exist, (1)=is_exist } */
int fih_read_jogball_exist_from_smem(void);
/* FIH, JiaHao, 2010/08/04 } */

/* FIH, JiaHao, 2010/12/17 { */
/* FIH, JiaHao, 2011/01/26 { */
/* New HWID Mechanism */
/* OrigHWID {
    [31:30] = Reserved
    [29]    = HAC_Support
    [28]    = Headdset_Type
    [27:26] = Reserved
    [25:24] = SIM_ID
    [23:16] = Band_ID
    [15: 8] = Phase_ID
    [ 7: 0] = Product_ID } */
/* FIH, JiaHao, 2011/01/26 } */
bool fih_read_new_hwid_mech_from_orighwid(void);
unsigned int fih_read_product_id_from_orighwid(void);
unsigned int fih_read_phase_id_from_orighwid(void);
unsigned int fih_read_band_id_from_orighwid(void);
unsigned int fih_read_sim_id_from_orighwid(void); /* FIH, JiaHao, 2011/01/26 */
unsigned int fih_read_headset_type_from_orighwid(void);
unsigned int fih_read_hac_support_from_orighwid(void);
/* FIH, JiaHao, 2010/12/17 } */

/* FIH, Paul Huang, 2009/08/04 { */
/* add for QXDM LOG to SD card */
#define SAVE_QXDM_LOG_TO_SD_CARD    1
/* FIH, Paul Huang, 2009/08/04 } */

/* FIH, Charles Huang, 2009/05/18 { */
/* [FXX_CR], HW ID */
typedef enum 
{
    CMCS_HW_VER_EVB1=0,  //40k resister  
    CMCS_HW_VER_EVB2,    
    CMCS_HW_VER_EVB3,
    CMCS_HW_VER_EVB4,
    CMCS_HW_VER_EVB5,

    CMCS_RTP_PR1,        //10k resister
    CMCS_RTP_PR2,        //30k resister
    CMCS_RTP_PR3,
    CMCS_RTP_PR4,
    CMCS_RTP_PR5,
    CMCS_RTP_MP1,
    CMCS_RTP_MP2,
    CMCS_RTP_MP3,

    CMCS_CTP_PR1,        //20k resister
    CMCS_CTP_PR2,        //68.2k resister
    CMCS_CTP_PR3,
    CMCS_CTP_PR4,
    CMCS_CTP_PR5,
    CMCS_CTP_MP1,
    CMCS_CTP_MP2,
    CMCS_CTP_MP3,

    /* FIH, Terry { */
    /* FM6 family */
    CMCS_FM6_PR1 = 0x100,
    CMCS_FM6_PR2,
    CMCS_FM6_PR3,
    CMCS_FM6_8L,
    CMCS_FM6_MP,
    /* FIH, Terry } */

    /* add for 7627 F905 { */
    /* FIH, Debbie, 2010/05/04 { */
    /* modify 7627 start index because 7227 hwid is not enough */
    CMCS_7627_EVB1 = 0x500,
    /* FIH, Debbie, 2010/05/04 } */
    CMCS_7627_PR1,       //100k resister
    CMCS_7627_PR2,
    CMCS_7627_PR3,
    CMCS_7627_PR4,
    CMCS_7627_PR5,

    /* F913 family */
    CMCS_F913_PR1,
    CMCS_F913_PR2,    
    CMCS_F913_PR3,
    CMCS_F913_PR4,
    CMCS_F913_PR5,
    CMCS_F913_MP1,

    /* FIH, Paul Huang, 2010/06/18 { */
    /* FN6 family */
    CMCS_FN6_PR1 = 0x600, //Product ID 100k,HW version ID 100k
    CMCS_FN6_PR2,         //Product ID 100k,HW version ID 137k
    CMCS_FN6_PR3,         //Product ID 100k,HW version ID 180k
    CMCS_FN6_MP1,         //Product ID 100k,HW version ID 249k
    /* FIH, Paul Huang, 2010/06/18 } */

    /* FIH, JiaHao, 2010/12/17 { */
    /* New HWID Mechanism */
    /* HWID = OrigHWID [16:FFFF][8:Phase_ID][8:Product_ID] */

    /* DQE family */
    CMCS_DQE_PR1 = 0xFFFF0101,
    CMCS_DQE_PR2 = 0xFFFF0201,
    CMCS_DQE_PR3 = 0xFFFF0301,
    CMCS_DQE_PR4 = 0xFFFF0401,
    CMCS_DQE_PR5 = 0xFFFF0501,
    CMCS_DQE_PR6 = 0xFFFF0601,
    CMCS_DQE_PR7 = 0xFFFF0701,
    CMCS_DQE_PR8 = 0xFFFF0801,

    /* DMQ family */
    CMCS_DMQ_PR1 = 0xFFFF0102,
    CMCS_DMQ_PR2 = 0xFFFF0202,
    CMCS_DMQ_PR3 = 0xFFFF0302,
    CMCS_DMQ_PR4 = 0xFFFF0402,
    CMCS_DMQ_PR5 = 0xFFFF0502,
    CMCS_DMQ_PR6 = 0xFFFF0602,
    CMCS_DMQ_PR7 = 0xFFFF0702,
    CMCS_DMQ_PR8 = 0xFFFF0802,

    /* DQD family */
    CMCS_DQD_PR1 = 0xFFFF0103,
    CMCS_DQD_PR2 = 0xFFFF0203,
    CMCS_DQD_PR3 = 0xFFFF0303,
    CMCS_DQD_PR4 = 0xFFFF0403,
    CMCS_DQD_PR5 = 0xFFFF0503,
    CMCS_DQD_PR6 = 0xFFFF0603,
    CMCS_DQD_PR7 = 0xFFFF0703,
    CMCS_DQD_PR8 = 0xFFFF0803,

    /* DPD family */
    CMCS_DPD_PR1 = 0xFFFF0104,
    CMCS_DPD_PR2 = 0xFFFF0204,
    CMCS_DPD_PR3 = 0xFFFF0304,
    CMCS_DPD_PR4 = 0xFFFF0404,
    CMCS_DPD_PR5 = 0xFFFF0504,
    CMCS_DPD_PR6 = 0xFFFF0604,
    CMCS_DPD_PR7 = 0xFFFF0704,
    CMCS_DPD_PR8 = 0xFFFF0804,

    /* DMP family */
    CMCS_DMP_PR1 = 0xFFFF0105,
    CMCS_DMP_PR2 = 0xFFFF0205,
    CMCS_DMP_PR3 = 0xFFFF0305,
    CMCS_DMP_PR4 = 0xFFFF0405,
    CMCS_DMP_PR5 = 0xFFFF0505,
    CMCS_DMP_PR6 = 0xFFFF0605,
    CMCS_DMP_PR7 = 0xFFFF0705,
    CMCS_DMP_PR8 = 0xFFFF0805,

    /* DP2 family */
    CMCS_DP2_PR1 = 0xFFFF0106,
    CMCS_DP2_PR2 = 0xFFFF0206,
    CMCS_DP2_PR3 = 0xFFFF0306,
    CMCS_DP2_PR4 = 0xFFFF0406,
    CMCS_DP2_PR5 = 0xFFFF0506,
    CMCS_DP2_PR6 = 0xFFFF0606,
    CMCS_DP2_PR7 = 0xFFFF0706,
    CMCS_DP2_PR8 = 0xFFFF0806,

    /* FAD family */
    CMCS_FAD_PR1 = 0xFFFF0107,
    CMCS_FAD_PR2 = 0xFFFF0207,
    CMCS_FAD_PR3 = 0xFFFF0307,
    CMCS_FAD_PR4 = 0xFFFF0407,
    CMCS_FAD_PR5 = 0xFFFF0507,
    CMCS_FAD_PR6 = 0xFFFF0607,
    CMCS_FAD_PR7 = 0xFFFF0707,
    CMCS_FAD_PR8 = 0xFFFF0807,

    /* FIH, JiaHao, 2010/12/30 { */
    /* AI1S family */
    CMCS_AI1S_PR1 = 0xFFFF0108,
    CMCS_AI1S_PR2 = 0xFFFF0208,
    CMCS_AI1S_PR3 = 0xFFFF0308,
    CMCS_AI1S_PR4 = 0xFFFF0408,
    CMCS_AI1S_PR5 = 0xFFFF0508,
    CMCS_AI1S_PR6 = 0xFFFF0608,
    CMCS_AI1S_PR7 = 0xFFFF0708,
    CMCS_AI1S_PR8 = 0xFFFF0808,
    /* FIH, JiaHao, 2010/12/30 } */

    /* FIH, JiaHao, 2010/12/30 { */
    /* AI1D family */
    CMCS_AI1D_PR1 = 0xFFFF0109,
    CMCS_AI1D_PR2 = 0xFFFF0209,
    CMCS_AI1D_PR3 = 0xFFFF0309,
    CMCS_AI1D_PR4 = 0xFFFF0409,
    CMCS_AI1D_PR5 = 0xFFFF0509,
    CMCS_AI1D_PR6 = 0xFFFF0609,
    CMCS_AI1D_PR7 = 0xFFFF0709,
    CMCS_AI1D_PR8 = 0xFFFF0809,
    /* FIH, JiaHao, 2010/12/30 } */

    /* FIH, JiaHao, 2011/01/26 { */
    /* DMO family */
    CMCS_DMO_PR1 = 0xFFFF010A,
    CMCS_DMO_PR2 = 0xFFFF020A,
    CMCS_DMO_PR3 = 0xFFFF030A,
    CMCS_DMO_PR4 = 0xFFFF040A,
    CMCS_DMO_PR5 = 0xFFFF050A,
    CMCS_DMO_PR6 = 0xFFFF060A,
    CMCS_DMO_PR7 = 0xFFFF070A,
    CMCS_DMO_PR8 = 0xFFFF080A,
    /* FIH, JiaHao, 2011/01/26 } */

    /* FIH, JiaHao, 2011/01/26 { */
    /* DMT family */
    CMCS_DMT_PR1 = 0xFFFF010B,
    CMCS_DMT_PR2 = 0xFFFF020B,
    CMCS_DMT_PR3 = 0xFFFF030B,
    CMCS_DMT_PR4 = 0xFFFF040B,
    CMCS_DMT_PR5 = 0xFFFF050B,
    CMCS_DMT_PR6 = 0xFFFF060B,
    CMCS_DMT_PR7 = 0xFFFF070B,
    CMCS_DMT_PR8 = 0xFFFF080B,
    /* FIH, JiaHao, 2011/01/26 } */

    /* FIH, JiaHao, 2010/12/17 } */

    CMCS_HW_VER_MAX
}cmcs_hw_version_type;

/* FIH, Debbie Sun, 2009/07/15 { */
/* 7227_CR_XX start, modify for new HWID list*/
typedef enum 
{
    CMCS_HW_EVB1=0,            //40k resister  
    CMCS_HW_EVB2,    
    CMCS_HW_EVB3,
    CMCS_HW_EVB4,
    CMCS_HW_EVB5,
    CMCS_ORIG_RTP_PR1,         //10k resister
    CMCS_ORIG_CTP_PR1 = 0xd,   //20k resister

    /* 850 family */
    CMCS_850_RTP_PR2 = 0x10,   //30k resister
    CMCS_850_RTP_PR3,
    CMCS_850_RTP_PR4,
    CMCS_850_RTP_PR5,
    CMCS_850_RTP_MP1,          //4.7k resister
    CMCS_850_RTP_MP2,
    CMCS_850_RTP_MP3,

    CMCS_850_CTP_PR2 = 0x17,   //68.1k resister
    CMCS_850_CTP_PR3,
    CMCS_850_CTP_PR4,
    CMCS_850_CTP_PR5,
    CMCS_850_CTP_MP1,          //82K resister
    CMCS_850_CTP_MP2,
    CMCS_850_CTP_MP3,

    /* 900 family */
    CMCS_900_RTP_PR2 = 0x20,   //51k resister
    CMCS_900_RTP_PR3,
    CMCS_900_RTP_PR4,
    CMCS_900_RTP_PR5,
    CMCS_900_RTP_MP1,          //60.4k resister
    CMCS_900_RTP_MP2,
    CMCS_900_RTP_MP3,

    CMCS_900_CTP_PR2 = 0x27,   //90.9k resister
    CMCS_900_CTP_PR3,
    CMCS_900_CTP_PR4,
    CMCS_900_CTP_PR5,
    CMCS_900_CTP_MP1,          //100k resister
    CMCS_900_CTP_MP2,
    CMCS_900_CTP_MP3,

    /* AWS family */
    CMCS_145_CTP_PR1 = 0x2E,   //750k resister

    /* FST family */
    CMCS_125_FST_PR1 = 0x30,   //220k resister
    CMCS_125_FST_PR2,          //270k resister
    CMCS_125_FST_MP1,          //390k resister

    CMCS_128_FST_PR1 = 0x40,   //240k resister
    CMCS_128_FST_PR2,          //300k resister
    CMCS_128_FST_MP1,          //430k resister

    /* F917 family */
    CMCS_CTP_F917_PR1 = 0x50,
    CMCS_CTP_F917_PR2,
    CMCS_CTP_F917_PR3,
    CMCS_CTP_F917_PR4,
    CMCS_CTP_F917_PR5,
    CMCS_CTP_F917_MP1,
    CMCS_CTP_F917_MP2,
    CMCS_CTP_F917_MP3,

    /* FIH, Paul Huang, 2010/03/03 { */
    /* Greco family */
    CMCS_125_CTP_GRE_PR1 = 0x60,  //160k resister 
    CMCS_125_CTP_GRE_PR2,         //220K resister
    CMCS_125_CTP_GRE_MP1,  
    CMCS_125_CTP_GRE_MP2, 
    /* FIH, Paul Huang, 2010/03/03 } */

    /* FIH, Debbie, 2010/05/04 { */
    /* FA9 family */
    /* without real key */
    CMCS_125_FA9_PR1 = 0x70,   //MPP3 4.7K, MPP4 High
    CMCS_125_FA9_PR2,
    CMCS_125_FA9_PR3,
    CMCS_125_FA9_MP1,
    /* FIH, Debbie, 2010/05/04 } */

    /* FIH, Debbie, 2010/05/24 { */
    /* FAA family */
    CMCS_125_4G4G_FAA_PR1 = 0x80,
    CMCS_125_4G4G_FAA_PR2,
    CMCS_125_4G4G_FAA_PR3,
    CMCS_125_4G4G_FAA_MP1,

    /* FIH, JiaHao, 2010/07/30 { */
    CMCS_15_4G4G_FAA_PR2 = 0x84,
    CMCS_15_4G4G_FAA_PR3,
    CMCS_15_4G4G_FAA_MP1,
    /* FIH, JiaHao, 2010/07/30 } */

    CMCS_128_4G4G_FAA_PR1 = 0x87,
    CMCS_128_4G4G_FAA_PR2,
    CMCS_128_4G4G_FAA_PR3,
    CMCS_128_4G4G_FAA_MP1,
    /* FIH, Debbie, 2010/05/24 } */

    /* FIH, JiaHao, 2010/07/28 { */
    CMCS_MQ4_PR0 = 0x90,
    CMCS_MQ4_18_PR1,
    CMCS_MQ4_18_PR2,
    CMCS_MQ4_18_PR3,
    CMCS_MQ4_25_PR1,
    CMCS_MQ4_25_PR2,
    CMCS_MQ4_25_PR3,
    /* FIH, JiaHao, 2010/07/28 } */

    /* FIH, Terry { */
    /* FM6 family */
    CMCS_128_FM6_PR1 = 0x100,
    CMCS_128_FM6_PR2,
    CMCS_125_FM6_PR2,
    CMCS_128_FM6_PR3,
    CMCS_125_FM6_PR3,
    CMCS_145_FM6_PR3,
    CMCS_128_FM6_8L,
    CMCS_125_FM6_8L,
    CMCS_145_FM6_8L,
    CMCS_128_FM6_MP,
    CMCS_125_FM6_MP,
    /* FIH, Terry, } */

    /* add for 7627 { */
    /* FIH, Debbie, 2010/05/04 { */
    /* modify 7627 start index because 7227 hwid is not enough */
    /*** 7627 definition start from 0x500 ***/
    CMCS_7627_ORIG_EVB1 = 0x500,
    /* FIH, Debbie, 2010/05/04 } */

    /* F905 family */
    CMCS_7627_F905_PR1,        //100 k
    CMCS_7627_F905_PR2,        //130 k
    CMCS_7627_F905_PR3,
    CMCS_7627_F905_PR4,
    CMCS_7627_F905_PR5,

    /* F913 family */
    CMCS_7627_F913_PR1,        //160 k
    CMCS_7627_F913_PR2,        //180 k
    CMCS_7627_F913_PR3,
    CMCS_7627_F913_PR4,
    CMCS_7627_F913_PR5,
    CMCS_7627_F913_MP1_W,
    CMCS_7627_F913_MP1_C_G,
    /* FIH, Debbie, 2010/05/24 { */
    CMCS_7627_F913_MP1_W_4G4G,
    CMCS_7627_F913_MP1_C_G_4G4G,
    /* FIH, Debbie, 2010/05/24 } */
    /* FIH, Henry Juang, 2010/08/26 Added this for FA3 8 layers PCB++{ */
    CMCS_7627_F913_PCB_W = 0x510,
    CMCS_7627_F913_PCB_C_G,
    /* FIH, Henry Juang, 2010/08/26 } --*/
    /* FIH, JiaHao, 2010/10/08 { */
    CMCS_7627_F913_HAIER_C,
    /* FIH, JiaHao, 2010/10/08 } */

    /* FIH, Debbie, 2010/05/04 { */
    /* F20 family */
    CMCS_7627_F20_PR1 = 0x520,
    CMCS_7627_F20_PR2,
    CMCS_7627_F20_PR3,
    CMCS_7627_F20_MP1,
    /* FIH, Debbie, 2010/05/04 } */

    /* FIH, Paul Huang, 2010/06/18 { */
    /* FN6 family */
    CMCS_FN6_ORIG_PR1 = 0x600, //Product ID 100k,HW version ID 100k
    CMCS_FN6_ORIG_PR2,         //Product ID 100k,HW version ID 137k
    CMCS_FN6_ORIG_PR3,         //Product ID 100k,HW version ID 180k
    CMCS_FN6_ORIG_MP1,         //Product ID 100k,HW version ID 249k
    /* FIH, Paul Huang, 2010/06/18 } */

    CMCS_HW_VERSION_MAX
}cmcs_hw_orig_version_type;
/* 7227_CR_XX end*/
/* FIH, Debbie Sun, 2009/07/15 } */

#ifdef MSM_SHARED_RAM_BASE
/* get share memory command address dynamically */
#define FIH_READ_HWID_FROM_SMEM()  fih_read_hwid_from_smem()

/* modify new hardware id */
#define FIH_READ_ORIG_HWID_FROM_SMEM()  fih_read_orig_hwid_from_smem()

/* add for Read modem mode from smem */
#define FIH_READ_NETWORK_MODE_FROM_SMEM()  fih_read_network_mode_from_smem()
    #ifdef POWER_ON_CAUSE_PROC_READ_ENTRY
    #define FIH_READ_POWER_ON_CAUSE()  fih_read_power_on_cuase_from_smem()
    #endif
#endif
/* FIH, Charles Huang, 2009/05/18 } */

#endif /* CONFIG_FIH_FXX */

#endif
