/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <media/msm_camera.h>
#include <mach/gpio.h>
#include <mach/camera.h>
#include <mach/msm_smd.h>
#include "s5k5cag.h"
#include <mach/vreg.h>
#include "s5k5cag_reg.h"

#define CAMIF_SW_DMT 17 //GPIO17
#define S5K5CAG_REG_MODEL_ID   0x0040
#define S5K5CAG_MODEL_ID       0x05CA
#define S5K5CAG_MCLK 26000000

/* PLL Registers */
#define REG_PRE_PLL_CLK_DIV           0x0305
#define REG_PLL_MULTIPLIER_MSB        0x0306
#define REG_PLL_MULTIPLIER_LSB        0x0307
#define REG_VT_PIX_CLK_DIV            0x0301
#define REG_VT_SYS_CLK_DIV            0x0303
#define REG_OP_PIX_CLK_DIV            0x0309
#define REG_OP_SYS_CLK_DIV            0x030B

/* Data Format Registers */
#define REG_CCP_DATA_FORMAT_MSB       0x0112
#define REG_CCP_DATA_FORMAT_LSB       0x0113

/* Output Size */
#define REG_X_OUTPUT_SIZE_MSB         0x034C
#define REG_X_OUTPUT_SIZE_LSB         0x034D
#define REG_Y_OUTPUT_SIZE_MSB         0x034E
#define REG_Y_OUTPUT_SIZE_LSB         0x034F

/* Binning */
#define REG_X_EVEN_INC                0x0381
#define REG_X_ODD_INC                 0x0383
#define REG_Y_EVEN_INC                0x0385
#define REG_Y_ODD_INC                 0x0387
/*Reserved register */
#define REG_BINNING_ENABLE            0x3014

/* Frame Fotmat */
#define REG_FRAME_LENGTH_LINES_MSB    0x0340
#define REG_FRAME_LENGTH_LINES_LSB    0x0341
#define REG_LINE_LENGTH_PCK_MSB       0x0342
#define REG_LINE_LENGTH_PCK_LSB       0x0343

/* MSR setting */
/* Reserved registers */
#define REG_SHADE_CLK_ENABLE          0x30AC
#define REG_SEL_CCP                   0x30C4
#define REG_VPIX                      0x3024
#define REG_CLAMP_ON                  0x3015
#define REG_OFFSET                    0x307E

/* CDS timing settings */
/* Reserved registers */
#define REG_LD_START                  0x3000
#define REG_LD_END                    0x3001
#define REG_SL_START                  0x3002
#define REG_SL_END                    0x3003
#define REG_RX_START                  0x3004
#define REG_S1_START                  0x3005
#define REG_S1_END                    0x3006
#define REG_S1S_START                 0x3007
#define REG_S1S_END                   0x3008
#define REG_S3_START                  0x3009
#define REG_S3_END                    0x300A
#define REG_CMP_EN_START              0x300B
#define REG_CLP_SL_START              0x300C
#define REG_CLP_SL_END                0x300D
#define REG_OFF_START                 0x300E
#define REG_RMP_EN_START              0x300F
#define REG_TX_START                  0x3010
#define REG_TX_END                    0x3011
#define REG_STX_WIDTH                 0x3012
#define REG_TYPE1_AF_ENABLE           0x3130
#define DRIVER_ENABLED                0x0001
#define AUTO_START_ENABLED            0x0010
#define REG_NEW_POSITION              0x3131
#define REG_3152_RESERVED             0x3152
#define REG_315A_RESERVED             0x315A
#define REG_ANALOGUE_GAIN_CODE_GLOBAL_MSB 0x0204
#define REG_ANALOGUE_GAIN_CODE_GLOBAL_LSB 0x0205
#define REG_FINE_INTEGRATION_TIME         0x0200
#define REG_COARSE_INTEGRATION_TIME       0x0202
#define REG_COARSE_INTEGRATION_TIME_LSB   0x0203

/* Mode select register */
#define s5k5cag_REG_MODE_SELECT      0x0100
#define s5k5cag_MODE_SELECT_STREAM     0x01   /* start streaming */
#define s5k5cag_MODE_SELECT_SW_STANDBY 0x00   /* software standby */
#define s5k5cag_REG_SOFTWARE_RESET   0x0103
#define s5k5cag_SOFTWARE_RESET         0x01
#define REG_TEST_PATTERN_MODE         0x0601

static struct wake_lock s5k5cag_wake_lock_suspend;
static int32_t s5k5cag_power_down(void);
static int32_t s5k5cag_power_up(void); //[DMQ.B-1598], Vince
struct reg_struct {
  uint8_t pre_pll_clk_div;               /* 0x0305 */
  uint8_t pll_multiplier_msb;            /* 0x0306 */
  uint8_t pll_multiplier_lsb;            /* 0x0307 */
  uint8_t vt_pix_clk_div;                /* 0x0301 */
  uint8_t vt_sys_clk_div;                /* 0x0303 */
  uint8_t op_pix_clk_div;                /* 0x0309 */
  uint8_t op_sys_clk_div;                /* 0x030B */
  uint8_t ccp_data_format_msb;           /* 0x0112 */
  uint8_t ccp_data_format_lsb;           /* 0x0113 */
  uint8_t x_output_size_msb;             /* 0x034C */
  uint8_t x_output_size_lsb;             /* 0x034D */
  uint8_t y_output_size_msb;             /* 0x034E */
  uint8_t y_output_size_lsb;             /* 0x034F */
  uint8_t x_even_inc;                    /* 0x0381 */
  uint8_t x_odd_inc;                     /* 0x0383 */
  uint8_t y_even_inc;                    /* 0x0385 */
  uint8_t y_odd_inc;                     /* 0x0387 */
  uint8_t binning_enable;                /* 0x3014 */
  uint8_t frame_length_lines_msb;        /* 0x0340 */
  uint8_t frame_length_lines_lsb;        /* 0x0341 */
  uint8_t line_length_pck_msb;           /* 0x0342 */
  uint8_t line_length_pck_lsb;           /* 0x0343 */
  uint8_t shade_clk_enable ;             /* 0x30AC */
  uint8_t sel_ccp;                       /* 0x30C4 */
  uint8_t vpix;                          /* 0x3024 */
  uint8_t clamp_on;                      /* 0x3015 */
  uint8_t offset;                        /* 0x307E */
  uint8_t ld_start;                      /* 0x3000 */
  uint8_t ld_end;                        /* 0x3001 */
  uint8_t sl_start;                      /* 0x3002 */
  uint8_t sl_end;                        /* 0x3003 */
  uint8_t rx_start;                      /* 0x3004 */
  uint8_t s1_start;                      /* 0x3005 */
  uint8_t s1_end;                        /* 0x3006 */
  uint8_t s1s_start;                     /* 0x3007 */
  uint8_t s1s_end;                       /* 0x3008 */
  uint8_t s3_start;                      /* 0x3009 */
  uint8_t s3_end;                        /* 0x300A */
  uint8_t cmp_en_start;                  /* 0x300B */
  uint8_t clp_sl_start;                  /* 0x300C */
  uint8_t clp_sl_end;                    /* 0x300D */
  uint8_t off_start;                     /* 0x300E */
  uint8_t rmp_en_start;                  /* 0x300F */
  uint8_t tx_start;                      /* 0x3010 */
  uint8_t tx_end;                        /* 0x3011 */
  uint8_t stx_width;                     /* 0x3012 */
  uint8_t reg_3152_reserved;             /* 0x3152 */
  uint8_t reg_315A_reserved;             /* 0x315A */
  uint8_t analogue_gain_code_global_msb; /* 0x0204 */
  uint8_t analogue_gain_code_global_lsb; /* 0x0205 */
  uint8_t fine_integration_time;         /* 0x0200 */
  uint8_t coarse_integration_time;       /* 0x0202 */
  uint32_t  size_h;
  uint32_t  blk_l;
  uint32_t  size_w;
  uint32_t  blk_p;
};

struct reg_struct s5k5cag_reg_pat[2] =  {
  {	/* Preview */
    0x06,  /* pre_pll_clk_div       REG=0x0305 */
    0x00,  /* pll_multiplier_msb    REG=0x0306 */
    0x88,  /* pll_multiplier_lsb    REG=0x0307 */
    0x0a,  /* vt_pix_clk_div        REG=0x0301 */
    0x01,  /* vt_sys_clk_div        REG=0x0303 */
    0x0a,  /* op_pix_clk_div        REG=0x0309 */
    0x01,  /* op_sys_clk_div        REG=0x030B */
    0x0a,  /* ccp_data_format_msb   REG=0x0112 */
    0x0a,  /* ccp_data_format_lsb   REG=0x0113 */
    0x05,  /* x_output_size_msb     REG=0x034C */
    0x10,  /* x_output_size_lsb     REG=0x034D */
    0x03,  /* y_output_size_msb     REG=0x034E */
    0xcc,  /* y_output_size_lsb     REG=0x034F */

    /* enable binning for preview */
    0x01,  /* x_even_inc             REG=0x0381 */
    0x01,  /* x_odd_inc              REG=0x0383 */
    0x01,  /* y_even_inc             REG=0x0385 */
    0x03,  /* y_odd_inc              REG=0x0387 */
    0x06,  /* binning_enable         REG=0x3014 */

    0x03,  /* frame_length_lines_msb        REG=0x0340 */
    0xde,  /* frame_length_lines_lsb        REG=0x0341 */
    0x0a,  /* line_length_pck_msb           REG=0x0342 */
    0xac,  /* line_length_pck_lsb           REG=0x0343 */
    0x81,  /* shade_clk_enable              REG=0x30AC */
    0x01,  /* sel_ccp                       REG=0x30C4 */
    0x04,  /* vpix                          REG=0x3024 */
    0x00,  /* clamp_on                      REG=0x3015 */
    0x02,  /* offset                        REG=0x307E */
    0x03,  /* ld_start                      REG=0x3000 */
    0x9c,  /* ld_end                        REG=0x3001 */
    0x02,  /* sl_start                      REG=0x3002 */
    0x9e,  /* sl_end                        REG=0x3003 */
    0x05,  /* rx_start                      REG=0x3004 */
    0x0f,  /* s1_start                      REG=0x3005 */
    0x24,  /* s1_end                        REG=0x3006 */
    0x7c,  /* s1s_start                     REG=0x3007 */
    0x9a,  /* s1s_end                       REG=0x3008 */
    0x10,  /* s3_start                      REG=0x3009 */
    0x14,  /* s3_end                        REG=0x300A */
    0x10,  /* cmp_en_start                  REG=0x300B */
    0x04,  /* clp_sl_start                  REG=0x300C */
    0x26,  /* clp_sl_end                    REG=0x300D */
    0x02,  /* off_start                     REG=0x300E */
    0x0e,  /* rmp_en_start                  REG=0x300F */
    0x30,  /* tx_start                      REG=0x3010 */
    0x4e,  /* tx_end                        REG=0x3011 */
    0x1E,  /* stx_width                     REG=0x3012 */
    0x08,  /* reg_3152_reserved             REG=0x3152 */
    0x10,  /* reg_315A_reserved             REG=0x315A */
    0x00,  /* analogue_gain_code_global_msb REG=0x0204 */
    0x80,  /* analogue_gain_code_global_lsb REG=0x0205 */
    0x02,  /* fine_integration_time         REG=0x0200 */
    0x03,  /* coarse_integration_time       REG=0x0202 */
		972,
		18,
		1296,
		1436
  },
  { /* Snapshot */
    0x06,  /* pre_pll_clk_div               REG=0x0305 */
    0x00,  /* pll_multiplier_msb            REG=0x0306 */
    0x88,  /* pll_multiplier_lsb            REG=0x0307 */
    0x0a,  /* vt_pix_clk_div                REG=0x0301 */
    0x01,  /* vt_sys_clk_div                REG=0x0303 */
    0x0a,  /* op_pix_clk_div                REG=0x0309 */
    0x01,  /* op_sys_clk_div                REG=0x030B */
    0x0a,  /* ccp_data_format_msb           REG=0x0112 */
    0x0a,  /* ccp_data_format_lsb           REG=0x0113 */
    0x0a,  /* x_output_size_msb             REG=0x034C */
    0x30,  /* x_output_size_lsb             REG=0x034D */
    0x07,  /* y_output_size_msb             REG=0x034E */
    0xa8,  /* y_output_size_lsb             REG=0x034F */

    /* disable binning for snapshot */
    0x01,  /* x_even_inc                    REG=0x0381 */
    0x01,  /* x_odd_inc                     REG=0x0383 */
    0x01,  /* y_even_inc                    REG=0x0385 */
    0x01,  /* y_odd_inc                     REG=0x0387 */
    0x00,  /* binning_enable                REG=0x3014 */

    0x07,  /* frame_length_lines_msb        REG=0x0340 */
    0xb6,  /* frame_length_lines_lsb        REG=0x0341 */
    0x0a,  /* line_length_pck_msb           REG=0x0342 */
    0xac,  /* line_length_pck_lsb           REG=0x0343 */
    0x81,  /* shade_clk_enable              REG=0x30AC */
    0x01,  /* sel_ccp                       REG=0x30C4 */
    0x04,  /* vpix                          REG=0x3024 */
    0x00,  /* clamp_on                      REG=0x3015 */
    0x02,  /* offset                        REG=0x307E */
    0x03,  /* ld_start                      REG=0x3000 */
    0x9c,  /* ld_end                        REG=0x3001 */
    0x02,  /* sl_start                      REG=0x3002 */
    0x9e,  /* sl_end                        REG=0x3003 */
    0x05,  /* rx_start                      REG=0x3004 */
    0x0f,  /* s1_start                      REG=0x3005 */
    0x24,  /* s1_end                        REG=0x3006 */
    0x7c,  /* s1s_start                     REG=0x3007 */
    0x9a,  /* s1s_end                       REG=0x3008 */
    0x10,  /* s3_start                      REG=0x3009 */
    0x14,  /* s3_end                        REG=0x300A */
    0x10,  /* cmp_en_start                  REG=0x300B */
    0x04,  /* clp_sl_start                  REG=0x300C */
    0x26,  /* clp_sl_end                    REG=0x300D */
    0x02,  /* off_start                     REG=0x300E */
    0x0e,  /* rmp_en_start                  REG=0x300F */
    0x30,  /* tx_start                      REG=0x3010 */
    0x4e,  /* tx_end                        REG=0x3011 */
    0x1E,  /* stx_width                     REG=0x3012 */
    0x08,  /* reg_3152_reserved             REG=0x3152 */
    0x10,  /* reg_315A_reserved             REG=0x315A */
    0x00,  /* analogue_gain_code_global_msb REG=0x0204 */
    0x80,  /* analogue_gain_code_global_lsb REG=0x0205 */
    0x02,  /* fine_integration_time         REG=0x0200 */
    0x03,  /* coarse_integration_time       REG=0x0202 */
		1960,
		14,
		2608,
		124
	}
};

struct s5k5cag_work {
	struct work_struct work;
};
static struct s5k5cag_work *s5k5cag_sensorw;
static struct i2c_client *s5k5cag_client;

struct s5k5cag_ctrl {
	const struct msm_camera_sensor_info *sensordata;

	int sensormode;
	uint32_t fps_divider; /* init to 1 * 0x00000400 */
	uint32_t pict_fps_divider; /* init to 1 * 0x00000400 */

	uint16_t curr_lens_pos;
	uint16_t init_curr_lens_pos;
	uint16_t my_reg_gain;
	uint32_t my_reg_line_count;

	enum msm_s_resolution prev_res;
	enum msm_s_resolution pict_res;
	enum msm_s_resolution curr_res;
	enum msm_s_test_mode  set_test;
};

struct s5k5cag_i2c_reg_conf {
	unsigned short waddr;
	unsigned char  bdata;
};

static struct s5k5cag_ctrl *s5k5cag_ctrl;
static DECLARE_WAIT_QUEUE_HEAD(s5k5cag_wait_queue);
DEFINE_MUTEX(s5k5cag_mutex);

static int s5k5cag_i2c_rxdata(unsigned short saddr, unsigned char *rxdata,
	int length)
{
	struct i2c_msg msgs[] = {
		{
			.addr   = saddr,
			.flags = 0,
			.len   = 2,
			.buf   = rxdata,
		},
		{
			.addr   = saddr,
			.flags = I2C_M_RD,
			.len   = length,
			.buf   = rxdata,
		},
	};

	if (i2c_transfer(s5k5cag_client->adapter, msgs, 2) < 0) {
		CDBG("s5k5cag_i2c_rxdata failed!\n");
		return -EIO;
	}

	return 0;
}

static int32_t s5k5cag_i2c_txdata(unsigned short saddr,
	unsigned char *txdata, int length)
{
	struct i2c_msg msg[] = {
		{
		.addr  = saddr,
		.flags = 0,
		.len = length,
		.buf = txdata,
		},
	};

	if (i2c_transfer(s5k5cag_client->adapter, msg, 1) < 0) {
		CDBG("s5k5cag_i2c_txdata failed\n");
		return -EIO;
	}

	return 0;
}

static int32_t s5k5cag_i2c_write_w(unsigned short saddr,
	unsigned short waddr, unsigned short wdata )
{
	int32_t rc = -EFAULT;
	unsigned char buf[4];

	memset(buf, 0, sizeof(buf));

	buf[0] = (waddr & 0xFF00)>>8;
	buf[1] = (waddr & 0x00FF);
	buf[2] = (wdata & 0xFF00)>>8;
	buf[3] = (wdata & 0x00FF);

	rc = s5k5cag_i2c_txdata(saddr, buf, 4);

	if (rc < 0)
		CDBG(
		"i2c_write failed, addr = 0x%x, val = 0x%x!\n",
		waddr, wdata);

	return rc;
}

static int32_t s5k5cag_i2c_write_b(unsigned short saddr, unsigned short waddr,
	unsigned char bdata)
{
	int32_t rc = -EIO;
	unsigned char buf[4];

	memset(buf, 0, sizeof(buf));
	buf[0] = (waddr & 0xFF00)>>8;
	buf[1] = (waddr & 0x00FF);
	buf[2] = bdata;

	rc = s5k5cag_i2c_txdata(saddr, buf, 3);

	if (rc < 0)
		CDBG("i2c_write_w failed, addr = 0x%x, val = 0x%x!\n",
			waddr, bdata);

	return rc;
}

static int32_t s5k5cag_i2c_write_w_table(
	struct s5k5cag_i2c_reg_conf_w const *reg_cfg_tbl, int num)
{
	int i;
	int32_t rc = -EIO;
	for (i = 0; i < num; i++) {
		rc = s5k5cag_i2c_write_w(s5k5cag_client->addr,
			reg_cfg_tbl->waddr, reg_cfg_tbl->wdata);
		if (rc < 0)
			break;
		reg_cfg_tbl++;
	}

	return rc;
}

static int32_t s5k5cag_i2c_write_table(
	struct s5k5cag_i2c_reg_conf *reg_cfg_tbl, int num)
{
	int i;
	int32_t rc = -EIO;
	for (i = 0; i < num; i++) {
		rc = s5k5cag_i2c_write_b(s5k5cag_client->addr,
			reg_cfg_tbl->waddr, reg_cfg_tbl->bdata);
		if (rc < 0)
			break;
		reg_cfg_tbl++;
	}

	return rc;
}

static int32_t s5k5cag_i2c_read_w(unsigned short saddr, unsigned short raddr,
	unsigned short *rdata)
{
	int32_t rc = 0;
	unsigned char buf[4];

	if (!rdata)
		return -EIO;

    memset(buf, 0, sizeof(buf));

	buf[0] = (raddr & 0xFF00)>>8;
	buf[1] = (raddr & 0x00FF);

	rc = s5k5cag_i2c_rxdata(saddr, buf, 2);
	if (rc < 0)
		return rc;

	*rdata = buf[0] << 8 | buf[1];

	if (rc < 0)
		CDBG("s5k5cag_i2c_read failed!\n");

	return rc;
}

static int s5k5cag_probe_init_done(const struct msm_camera_sensor_info *data)
{
    printk("s5k5cag_probe_init_done call sensor power down\n");
    s5k5cag_power_down();
	// gpio_direction_output(data->sensor_reset, 0);
	// gpio_direction_output(data->sensor_reset, 1);
	// gpio_free(data->sensor_reset);
	return 0;
}

static int s5k5cag_probe_init_sensor(const struct msm_camera_sensor_info *data)
{
	int32_t  rc = 0;
	//int i = 0;
	uint16_t chipid = 0;
	/* ACORE 2.8V MQ4:GP3*/
	// struct vreg *vreg_acore;	
	struct vreg *vreg_dcore;
	
	printk( "s5k5cag_probe_init_sensor\r\n" );	
	
	// Enable the power for MQ4	
	{	
// MQ4
// IO   :     VREG_MSMP
// A    :     7540.N12(GP3)
// D    :     LDO (Input : VGEG_MSMP¡AEnable : AVDD )	
//		gpio_tlmm_config(GPIO_CFG(0, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
//		gpio_tlmm_config(GPIO_CFG(31, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
//		
//		vreg_acore = vreg_get(NULL, "gp3");
//		vreg_set_level(vreg_acore, 2800);
//		vreg_enable(vreg_acore);
	}
	
// DominoQ
// IO   :     LDO (Input : VBAT¡AEnable : GPIO93 )
// A    :     LDO (Input : VBAT¡AEnable : GPIO93 )
// D    :     7540.J13(GP2)	
	// Add HWID
	// Enable the power for DMQ
	{	

        gpio_tlmm_config(GPIO_CFG(0, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
        gpio_tlmm_config(GPIO_CFG(31, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
        gpio_tlmm_config(GPIO_CFG(93, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);		
			
        vreg_dcore = vreg_get(NULL, "gp2");
        // vreg_set_level(vreg_dcore, 1800);
        vreg_set_level(vreg_dcore, 1500);
        //vreg_enable(vreg_dcore);
        vreg_disable(vreg_dcore);
	}
	
	mdelay(5);	
	// Enable the CAM LDO
	gpio_direction_output(93, 1);
	
	mdelay( 10 );
	vreg_dcore = vreg_get(NULL, "gp2");
	vreg_set_level(vreg_dcore, 1500);
	vreg_enable(vreg_dcore);	
#if 0	
	rc = gpio_request(data->sensor_reset, "s5k5cag");
	if (!rc) {
		printk( " HW reset pull down \r\n" );
		gpio_direction_output(data->sensor_reset, 0);
	}
	else
		goto init_probe_done;
		
			
	rc = gpio_request(data->sensor_pwd, "s5k5cag");
	if (!rc) {
        printk( " HW standby pull down \r\n" );
		gpio_direction_output(data->sensor_pwd, 0);	
		
	}
	else
		goto init_probe_done;		
			
	mdelay( 10 );
	printk( " HW standby pull up \r\n" );
	gpio_direction_output(data->sensor_pwd, 1);		
	
	mdelay( 10 );
	printk( " HW reset pull up \r\n" );
	gpio_direction_output(data->sensor_reset, 1);			
	
	mdelay(10);
#endif 
	gpio_direction_output(31, 0);
	gpio_direction_output(0, 0);
	mdelay(5);
	gpio_direction_output(31, 1);
	mdelay(5);
	gpio_direction_output(0, 1);
	mdelay(10);
	printk( "s5k5cag_sensor_init(): reseting sensor.\r\n" );


	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002C, 0x0000 );
	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002E, 0x0040 );
	s5k5cag_i2c_read_w( s5k5cag_client->addr, 0X0F12, &chipid );
	printk("s5k5cag model_id = 0x%x\n", chipid);
	if (chipid != S5K5CAG_MODEL_ID) {
		printk("s5k5cag wrong model_id = 0x%x\n", chipid);
		rc = -ENODEV;
		goto init_probe_fail;
	}

	goto init_probe_done;

init_probe_fail:
	s5k5cag_probe_init_done(data);
init_probe_done:
	return rc;
}

static int s5k5cag_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&s5k5cag_wait_queue);
	return 0;
}

static const struct i2c_device_id s5k5cag_i2c_id[] = {
	{ "s5k5cag", 0},
	{ }
};

static int s5k5cag_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	CDBG("s5k5cag_probe called!\n");
	printk("s5k5cag_probe called!\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CDBG("i2c_check_functionality failed\n");
		goto probe_failure;
	}

	s5k5cag_sensorw = kzalloc(sizeof(struct s5k5cag_work), GFP_KERNEL);
	if (!s5k5cag_sensorw) {
		CDBG("kzalloc failed.\n");
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, s5k5cag_sensorw);
	s5k5cag_init_client(client);
	s5k5cag_client = client;

	mdelay(50);
	printk("s5k5cag_probe successed! rc = %d\n", rc);
	CDBG("s5k5cag_probe successed! rc = %d\n", rc);
	return 0;

probe_failure:
	CDBG("s5k5cag_probe failed! rc = %d\n", rc);
	return rc;
}


#ifdef CONFIG_PM
static int s5k5cag_suspend(struct i2c_client *client, pm_message_t mesg)
{
	printk(KERN_INFO "s5k5cag_suspend!\n");
#if 0
	int32_t  rc = 0;	
	struct vreg *vreg_dcore;
	
	/* sensor_pwd pin gpio31 */
	printk(KERN_INFO "s5k5cag_suspend!\n");
	if (s5k5cag_ctrl)
	{
		// GPIO 0 RESET, GPIO31 PWRON(STBY), GPIO93 LDO ON/OFF
		printk(KERN_INFO "s5k5cag STANDBY LOW!\n");
		gpio_direction_output(31, 0);
		mdelay(5);
		
		printk(KERN_INFO "s5k5cag RESET LOW!\n");
		gpio_direction_output(0, 0);
		mdelay(5);
		
		// Enable the CAM LDO
		gpio_direction_output(93, 1);
		mdelay(5);
	}

// DominoQ
// IO   :     LDO (Input : VBAT¡AEnable : GPIO93 )
// A    :     LDO (Input : VBAT¡AEnable : GPIO93 )
// D    :     7540.J13(GP2)	
	// Add HWID
	// Enable the power for DMQ
	{	

			
		vreg_dcore = vreg_get(NULL, "gp2");
		// vreg_set_level(vreg_dcore, 1800);
		vreg_set_level(vreg_dcore, 1500);
		vreg_disable(vreg_dcore);
	}
#endif	
	return 0;
}

static int s5k5cag_resume(struct i2c_client *client)
{
	printk(KERN_INFO "s5k5cag_resume+++\n");
	printk( "s5k5cag_resume---\r\n" );	

	return 0;
}
#else
# define s5k5cag_suspend NULL
# define s5k5cag_resume  NULL
#endif


static struct i2c_driver s5k5cag_i2c_driver = {
	.id_table = s5k5cag_i2c_id,
	.probe  = s5k5cag_i2c_probe,
	.remove = __exit_p(s5k5cag_i2c_remove),
	.suspend  	= s5k5cag_suspend,
	.resume   	= s5k5cag_resume,
	.driver = {
		.name = "s5k5cag",
	},
};

static int32_t s5k5cag_setting(enum msm_s_reg_update rupdate,
	enum msm_s_setting rt)
{
	int32_t rc = 0;
#if 0 // jones marked	
  uint16_t num_lperf;

	switch (rupdate) {
	case S_UPDATE_PERIODIC:
	if (rt == S_RES_PREVIEW || rt == S_RES_CAPTURE) {

		struct s5k5cag_i2c_reg_conf tbl_1[] = {
			{REG_CCP_DATA_FORMAT_MSB,
				s5k5cag_reg_pat[rt].ccp_data_format_msb},
			{REG_CCP_DATA_FORMAT_LSB,
				s5k5cag_reg_pat[rt].ccp_data_format_lsb},
			{REG_X_OUTPUT_SIZE_MSB,
				s5k5cag_reg_pat[rt].x_output_size_msb},
			{REG_X_OUTPUT_SIZE_LSB,
				s5k5cag_reg_pat[rt].x_output_size_lsb},
			{REG_Y_OUTPUT_SIZE_MSB,
				s5k5cag_reg_pat[rt].y_output_size_msb},
			{REG_Y_OUTPUT_SIZE_LSB,
				s5k5cag_reg_pat[rt].y_output_size_lsb},
			{REG_X_EVEN_INC,
				s5k5cag_reg_pat[rt].x_even_inc},
			{REG_X_ODD_INC,
				s5k5cag_reg_pat[rt].x_odd_inc},
			{REG_Y_EVEN_INC,
				s5k5cag_reg_pat[rt].y_even_inc},
			{REG_Y_ODD_INC,
				s5k5cag_reg_pat[rt].y_odd_inc},
			{REG_BINNING_ENABLE,
				s5k5cag_reg_pat[rt].binning_enable},
		};

		struct s5k5cag_i2c_reg_conf tbl_2[] = {
			{REG_FRAME_LENGTH_LINES_MSB, 0},
			{REG_FRAME_LENGTH_LINES_LSB, 0},
			{REG_LINE_LENGTH_PCK_MSB,
				s5k5cag_reg_pat[rt].line_length_pck_msb},
			{REG_LINE_LENGTH_PCK_LSB,
				s5k5cag_reg_pat[rt].line_length_pck_lsb},
			{REG_SHADE_CLK_ENABLE,
				s5k5cag_reg_pat[rt].shade_clk_enable},
			{REG_SEL_CCP, s5k5cag_reg_pat[rt].sel_ccp},
			{REG_VPIX, s5k5cag_reg_pat[rt].vpix},
			{REG_CLAMP_ON, s5k5cag_reg_pat[rt].clamp_on},
			{REG_OFFSET, s5k5cag_reg_pat[rt].offset},
			{REG_LD_START, s5k5cag_reg_pat[rt].ld_start},
			{REG_LD_END, s5k5cag_reg_pat[rt].ld_end},
			{REG_SL_START, s5k5cag_reg_pat[rt].sl_start},
			{REG_SL_END, s5k5cag_reg_pat[rt].sl_end},
			{REG_RX_START, s5k5cag_reg_pat[rt].rx_start},
			{REG_S1_START, s5k5cag_reg_pat[rt].s1_start},
			{REG_S1_END, s5k5cag_reg_pat[rt].s1_end},
			{REG_S1S_START, s5k5cag_reg_pat[rt].s1s_start},
			{REG_S1S_END, s5k5cag_reg_pat[rt].s1s_end},
			{REG_S3_START, s5k5cag_reg_pat[rt].s3_start},
			{REG_S3_END, s5k5cag_reg_pat[rt].s3_end},
			{REG_CMP_EN_START, s5k5cag_reg_pat[rt].cmp_en_start},
			{REG_CLP_SL_START, s5k5cag_reg_pat[rt].clp_sl_start},
			{REG_CLP_SL_END, s5k5cag_reg_pat[rt].clp_sl_end},
			{REG_OFF_START, s5k5cag_reg_pat[rt].off_start},
			{REG_RMP_EN_START, s5k5cag_reg_pat[rt].rmp_en_start},
			{REG_TX_START, s5k5cag_reg_pat[rt].tx_start},
			{REG_TX_END, s5k5cag_reg_pat[rt].tx_end},
			{REG_STX_WIDTH, s5k5cag_reg_pat[rt].stx_width},
			{REG_3152_RESERVED,
				s5k5cag_reg_pat[rt].reg_3152_reserved},
			{REG_315A_RESERVED,
				s5k5cag_reg_pat[rt].reg_315A_reserved},
			{REG_ANALOGUE_GAIN_CODE_GLOBAL_MSB,
				s5k5cag_reg_pat[rt].
				analogue_gain_code_global_msb},
			{REG_ANALOGUE_GAIN_CODE_GLOBAL_LSB,
				s5k5cag_reg_pat[rt].
				analogue_gain_code_global_lsb},
			{REG_FINE_INTEGRATION_TIME,
				s5k5cag_reg_pat[rt].fine_integration_time},
			{REG_COARSE_INTEGRATION_TIME,
				s5k5cag_reg_pat[rt].coarse_integration_time},
			{s5k5cag_REG_MODE_SELECT, s5k5cag_MODE_SELECT_STREAM},
		};

		rc = s5k5cag_i2c_write_table(&tbl_1[0],
			ARRAY_SIZE(tbl_1));
		if (rc < 0)
			return rc;

		num_lperf = (uint16_t)
			((s5k5cag_reg_pat[rt].frame_length_lines_msb << 8)
			& 0xFF00)
			+ s5k5cag_reg_pat[rt].frame_length_lines_lsb;

		num_lperf = num_lperf * s5k5cag_ctrl->fps_divider / 0x0400;

		tbl_2[0] = (struct s5k5cag_i2c_reg_conf)
			{REG_FRAME_LENGTH_LINES_MSB, (num_lperf & 0xFF00) >> 8};
		tbl_2[1] = (struct s5k5cag_i2c_reg_conf)
			{REG_FRAME_LENGTH_LINES_LSB, (num_lperf & 0x00FF)};

		rc = s5k5cag_i2c_write_table(&tbl_2[0],
			ARRAY_SIZE(tbl_2));
		if (rc < 0)
			return rc;

		mdelay(5);

		rc = s5k5cag_test(s5k5cag_ctrl->set_test);
		if (rc < 0)
			return rc;
	  }
    break; /* UPDATE_PERIODIC */

	case S_REG_INIT:
	if (rt == S_RES_PREVIEW || rt == S_RES_CAPTURE) {

		struct s5k5cag_i2c_reg_conf tbl_3[] = {
			{s5k5cag_REG_SOFTWARE_RESET, s5k5cag_SOFTWARE_RESET},
			{s5k5cag_REG_MODE_SELECT,
				s5k5cag_MODE_SELECT_SW_STANDBY},
			/* PLL setting */
			{REG_PRE_PLL_CLK_DIV,
				s5k5cag_reg_pat[rt].pre_pll_clk_div},
			{REG_PLL_MULTIPLIER_MSB,
				s5k5cag_reg_pat[rt].pll_multiplier_msb},
			{REG_PLL_MULTIPLIER_LSB,
				s5k5cag_reg_pat[rt].pll_multiplier_lsb},
			{REG_VT_PIX_CLK_DIV,
				s5k5cag_reg_pat[rt].vt_pix_clk_div},
			{REG_VT_SYS_CLK_DIV,
				s5k5cag_reg_pat[rt].vt_sys_clk_div},
			{REG_OP_PIX_CLK_DIV,
				s5k5cag_reg_pat[rt].op_pix_clk_div},
			{REG_OP_SYS_CLK_DIV,
				s5k5cag_reg_pat[rt].op_sys_clk_div},
			/*Data Format */
			{REG_CCP_DATA_FORMAT_MSB,
				s5k5cag_reg_pat[rt].ccp_data_format_msb},
			{REG_CCP_DATA_FORMAT_LSB,
				s5k5cag_reg_pat[rt].ccp_data_format_lsb},
			/*Output Size */
			{REG_X_OUTPUT_SIZE_MSB,
				s5k5cag_reg_pat[rt].x_output_size_msb},
			{REG_X_OUTPUT_SIZE_LSB,
				s5k5cag_reg_pat[rt].x_output_size_lsb},
			{REG_Y_OUTPUT_SIZE_MSB,
				s5k5cag_reg_pat[rt].y_output_size_msb},
			{REG_Y_OUTPUT_SIZE_LSB,
				s5k5cag_reg_pat[rt].y_output_size_lsb},
			/* Binning */
			{REG_X_EVEN_INC, s5k5cag_reg_pat[rt].x_even_inc},
			{REG_X_ODD_INC, s5k5cag_reg_pat[rt].x_odd_inc },
			{REG_Y_EVEN_INC, s5k5cag_reg_pat[rt].y_even_inc},
			{REG_Y_ODD_INC, s5k5cag_reg_pat[rt].y_odd_inc},
			{REG_BINNING_ENABLE,
				s5k5cag_reg_pat[rt].binning_enable},
			/* Frame format */
			{REG_FRAME_LENGTH_LINES_MSB,
				s5k5cag_reg_pat[rt].frame_length_lines_msb},
			{REG_FRAME_LENGTH_LINES_LSB,
				s5k5cag_reg_pat[rt].frame_length_lines_lsb},
			{REG_LINE_LENGTH_PCK_MSB,
				s5k5cag_reg_pat[rt].line_length_pck_msb},
			{REG_LINE_LENGTH_PCK_LSB,
				s5k5cag_reg_pat[rt].line_length_pck_lsb},
			/* MSR setting */
			{REG_SHADE_CLK_ENABLE,
				s5k5cag_reg_pat[rt].shade_clk_enable},
			{REG_SEL_CCP, s5k5cag_reg_pat[rt].sel_ccp},
			{REG_VPIX, s5k5cag_reg_pat[rt].vpix},
			{REG_CLAMP_ON, s5k5cag_reg_pat[rt].clamp_on},
			{REG_OFFSET, s5k5cag_reg_pat[rt].offset},
			/* CDS timing setting */
			{REG_LD_START, s5k5cag_reg_pat[rt].ld_start},
			{REG_LD_END, s5k5cag_reg_pat[rt].ld_end},
			{REG_SL_START, s5k5cag_reg_pat[rt].sl_start},
			{REG_SL_END, s5k5cag_reg_pat[rt].sl_end},
			{REG_RX_START, s5k5cag_reg_pat[rt].rx_start},
			{REG_S1_START, s5k5cag_reg_pat[rt].s1_start},
			{REG_S1_END, s5k5cag_reg_pat[rt].s1_end},
			{REG_S1S_START, s5k5cag_reg_pat[rt].s1s_start},
			{REG_S1S_END, s5k5cag_reg_pat[rt].s1s_end},
			{REG_S3_START, s5k5cag_reg_pat[rt].s3_start},
			{REG_S3_END, s5k5cag_reg_pat[rt].s3_end},
			{REG_CMP_EN_START, s5k5cag_reg_pat[rt].cmp_en_start},
			{REG_CLP_SL_START, s5k5cag_reg_pat[rt].clp_sl_start},
			{REG_CLP_SL_END, s5k5cag_reg_pat[rt].clp_sl_end},
			{REG_OFF_START, s5k5cag_reg_pat[rt].off_start},
			{REG_RMP_EN_START, s5k5cag_reg_pat[rt].rmp_en_start},
			{REG_TX_START, s5k5cag_reg_pat[rt].tx_start},
			{REG_TX_END, s5k5cag_reg_pat[rt].tx_end},
			{REG_STX_WIDTH, s5k5cag_reg_pat[rt].stx_width},
			{REG_3152_RESERVED,
				s5k5cag_reg_pat[rt].reg_3152_reserved},
			{REG_315A_RESERVED,
				s5k5cag_reg_pat[rt].reg_315A_reserved},
			{REG_ANALOGUE_GAIN_CODE_GLOBAL_MSB,
				s5k5cag_reg_pat[rt].
				analogue_gain_code_global_msb},
			{REG_ANALOGUE_GAIN_CODE_GLOBAL_LSB,
				s5k5cag_reg_pat[rt].
				analogue_gain_code_global_lsb},
			{REG_FINE_INTEGRATION_TIME,
				s5k5cag_reg_pat[rt].fine_integration_time},
			{REG_COARSE_INTEGRATION_TIME,
				s5k5cag_reg_pat[rt].coarse_integration_time},
			{s5k5cag_REG_MODE_SELECT, s5k5cag_MODE_SELECT_STREAM},
		};

		/* reset fps_divider */
		s5k5cag_ctrl->fps_divider = 1 * 0x0400;
		rc = s5k5cag_i2c_write_table(&tbl_3[0],
			ARRAY_SIZE(tbl_3));
		if (rc < 0)
			return rc;
		}
		break; /* case REG_INIT: */

	default:
		rc = -EINVAL;
		break;
	} /* switch (rupdate) */
#endif // jones
	return rc;
}

//[DMQ.B-1598]++, Vince
static int s5k5cag_set_registers(int retry_count)
{
	int i, freq ;
	int32_t  rc;
	uint16_t reg;
	struct vreg *vreg_dcore;
	for(i=0; i< retry_count; ++i)
	{
		if(i>0)
		{
			s5k5cag_power_down() ;
			mdelay(200);
			s5k5cag_power_up() ;
		        vreg_dcore = vreg_get(NULL, "gp2");
		        // vreg_set_level(vreg_dcore, 1800);
		        vreg_set_level(vreg_dcore, 1500);
		        //vreg_enable(vreg_dcore);
		        vreg_disable(vreg_dcore);
			
			mdelay(5);	
			// Enable the CAM LDO
			gpio_direction_output(93, 1);			
			mdelay( 10 );
			vreg_dcore = vreg_get(NULL, "gp2");
			vreg_set_level(vreg_dcore, 1500);
			vreg_enable(vreg_dcore);
		}
		//	mdelay(10); 
		printk( "s5k5cag FIH 17\r\n" );
		
		printk( "s5k5cag init table-->\r\n" );
		printk("s5k5cag_set_register repeat times: %d\n",i);
		rc = s5k5cag_i2c_write_w_table( &s5k5cag_init_tbl[0],
			ARRAY_SIZE(s5k5cag_init_tbl) );
		msleep(50);
		rc = s5k5cag_i2c_write_w_table( &s5k5cag_init_tbl1[0],
			ARRAY_SIZE(s5k5cag_init_tbl1) );
		msleep(50);
		
	// [DMQ.B-1545]	     
	//	printk( "s5k5cag CURAN PATCH contrast 0x5\r\n" );
	//	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
	//	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x020E );
	//	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0005 );		
		
		rc = s5k5cag_i2c_write_w_table( &s5k5cag_init_tbl2[0],
			ARRAY_SIZE(s5k5cag_init_tbl2) );	
		msleep(10);
		printk( "s5k5cag init table<--\r\n" );
	
	// READ clock settings
	//    s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002C, 0x7000 );
	//	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002E, 0x3274 );
	//	s5k5cag_i2c_read_w( s5k5cag_client->addr, 0X0F12, &reg );	
	//	printk( "s5k5cag  IO driving current A:0x%x\r\n", reg );
	//
	//	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002C, 0x7000 );
	//	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002E, 0x3276 );
	//	s5k5cag_i2c_read_w( s5k5cag_client->addr, 0X0F12, &reg );	
	//	printk( "s5k5cag  IO driving current B:0x%x\r\n", reg );
	//	
	//	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002C, 0x7000 );
	//	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002E, 0x3278 );
	//	s5k5cag_i2c_read_w( s5k5cag_client->addr, 0X0F12, &reg );	
	//	printk( "s5k5cag  IO driving current C:0x%x\r\n", reg );
	//	
		s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002C, 0x7000 );
		s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002E, 0x327A );
		s5k5cag_i2c_read_w( s5k5cag_client->addr, 0X0F12, &reg );	
		printk( "s5k5cag  IO driving current D:0x%x\r\n", reg );

	    	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002C, 0x7000 );
		s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002E, 0x01F8 );
		s5k5cag_i2c_read_w( s5k5cag_client->addr, 0X0F12, &reg );	
		printk( "s5k5cag Preview PCLK_Min:0x%x, %dMHz\r\n", reg, reg * 4000 / 1000000 );
		freq = reg * 4000/1000000 ;
		if(freq !=78) 
			continue ;
		s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002C, 0x7000 );
		s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002E, 0x01FA );
		s5k5cag_i2c_read_w( s5k5cag_client->addr, 0X0F12, &reg );	
		printk( "s5k5cag Preview PCLK_Max:0x%x, %dMHz\r\n", reg, reg * 4000 / 1000000 );
		freq = reg * 4000/1000000 ;
		if(freq !=78) 
			continue ;
		s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002C, 0x7000 );
		s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002E, 0x01FE );
		s5k5cag_i2c_read_w( s5k5cag_client->addr, 0X0F12, &reg );	
		printk( "s5k5cag Capture PCLK_Min:0x%x, %dMHz\r\n", reg, reg * 4000 / 1000000 );
		freq = reg * 4000/1000000 ;
		if(freq !=78) 
			continue ;
		s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002C, 0x7000 );
		s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002E, 0x0200 );
		s5k5cag_i2c_read_w( s5k5cag_client->addr, 0X0F12, &reg );	
		printk( "s5k5cag Capture PCLK_Max:0x%x, %dMHz\r\n", reg, reg * 4000 / 1000000 );
		freq = reg * 4000/1000000 ;
		if(freq !=78) 
			continue ;
	
		break;//PCLK=78MHZ 
	}
	return freq==78?i:-1 ;
}
//[DMQ.B-1598]--, Vince

static int s5k5cag_sensor_open_init(const struct msm_camera_sensor_info *data)
{
	int32_t  rc;
	uint16_t reg;
	int productHWID=0 ;

	s5k5cag_ctrl = kzalloc(sizeof(struct s5k5cag_ctrl), GFP_KERNEL);
	if (!s5k5cag_ctrl) {
		CDBG("s5k5cag_init failed!\n");
		rc = -ENOMEM;
		goto init_done;
	}
	printk( " s5k5cag_sensor_open_init\r\n" );
	
	s5k5cag_ctrl->fps_divider = 1 * 0x00000400;
	s5k5cag_ctrl->pict_fps_divider = 1 * 0x00000400;
	s5k5cag_ctrl->set_test = S_TEST_OFF;
	s5k5cag_ctrl->prev_res = S_QTR_SIZE;
	s5k5cag_ctrl->pict_res = S_FULL_SIZE;

	if (data)
		s5k5cag_ctrl->sensordata = data;

	printk(KERN_DEBUG "[Camera] s5k5cag: init wakelock\r\n");
	wake_lock_init(&s5k5cag_wake_lock_suspend, WAKE_LOCK_SUSPEND, "s5k5cag");
	printk(KERN_DEBUG "[Camera] s5k5cag: lock SUSPEND...\r\n");
	wake_lock(&s5k5cag_wake_lock_suspend);
	
	printk(KERN_DEBUG "[Camera] s5k5cag: S5K5CAG_MCLK:%d\r\n", S5K5CAG_MCLK);
//vince++
	if(fih_read_new_hwid_mech_from_orighwid())
   	{
   		productHWID = fih_read_product_id_from_orighwid() ;     
   	}
	if(productHWID==Project_DMO ||productHWID==Project_DMT)
	{	
		gpio_tlmm_config(GPIO_CFG(CAMIF_SW_DMT, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);		
		rc = gpio_request(CAMIF_SW_DMT, "s5k5cag");
		if (!rc) {
			rc = gpio_direction_output(CAMIF_SW_DMT,0);
			printk( KERN_INFO"gpio_direction_output GPIO_17 value->%d.\n",gpio_get_value(CAMIF_SW_DMT));
		}
		gpio_free(CAMIF_SW_DMT);
		msleep( 5 );
	}
//vince--
	/* enable mclk first */
	msm_camio_clk_rate_set(S5K5CAG_MCLK);
	mdelay(20);

	msm_camio_camif_pad_reg_reset();
	mdelay(20);

	rc = s5k5cag_probe_init_sensor(data);
	if (rc < 0)
		goto init_fail1;
	
	s5k5cag_set_registers(10) ; //[DMQ.B-1598], Vince
	
#if 0	
	if (s5k5cag_ctrl->prev_res == S_QTR_SIZE)
		rc = s5k5cag_setting(S_REG_INIT, S_RES_PREVIEW);
	else
		rc = s5k5cag_setting(S_REG_INIT, S_RES_CAPTURE);

	if (rc < 0) {
		CDBG("s5k5cag_setting failed. rc = %d\n", rc);
		goto init_fail1;
	}
#endif 
	/* initialize AF But this module did not have af */
	
	goto init_done;

init_fail1:
	s5k5cag_probe_init_done(data);
	kfree(s5k5cag_ctrl);
init_done:
	return rc;
}

//[DMQ.B-1598]++, Vince
static int32_t s5k5cag_power_up(void)
{
	
	int32_t rc = 0;
	
	printk( "s5k5cag_power_up\r\n" );
	//if (s5k5cag_ctrl)
	{
		// GPIO 0 RESET, GPIO31 PWRON(STBY), GPIO93 LDO ON/OFF
		printk(KERN_INFO "s5k5cag STANDBY HIGH!\n");
		gpio_direction_output(31, 1);
		mdelay(5);
		
		printk(KERN_INFO "s5k5cag RESET LOW!\n");
		gpio_direction_output(0, 1);
		mdelay(5);
		
		// disable the CAM LDO
		gpio_direction_output(93, 1);
		mdelay(5);
	}
	
	return rc;
}
//[DMQ.B-1598]--, Vince

static int32_t s5k5cag_power_down(void)
{
	
	int32_t rc = 0;
	
	printk( "s5k5cag_power_down\r\n" );
	//if (s5k5cag_ctrl)
	{
		// GPIO 0 RESET, GPIO31 PWRON(STBY), GPIO93 LDO ON/OFF
		printk(KERN_INFO "s5k5cag STANDBY LOW!\n");
		gpio_direction_output(31, 0);
		mdelay(5);
		
		printk(KERN_INFO "s5k5cag RESET LOW!\n");
		gpio_direction_output(0, 0);
		mdelay(5);
		
		// disable the CAM LDO
		gpio_direction_output(93, 0);
		mdelay(5);
	}
	
	return rc;
}

static int s5k5cag_sensor_release(void)
{
	int rc = -EBADF;
	printk( "s5k5cag_sensor_release\r\n" );
	mutex_lock(&s5k5cag_mutex);

	s5k5cag_power_down();

//	gpio_direction_output(s5k5cag_ctrl->sensordata->sensor_reset, 0);
//	gpio_free(s5k5cag_ctrl->sensordata->sensor_reset);

	kfree(s5k5cag_ctrl);
	s5k5cag_ctrl = NULL;
	
	printk(KERN_DEBUG "[Camera] s5k5cag: unlock SUSPEND\r\n");
	wake_unlock(&s5k5cag_wake_lock_suspend);
	printk(KERN_DEBUG "[Camera] s5k5cag: destroy wakelock\r\n");
	wake_lock_destroy(&s5k5cag_wake_lock_suspend);
	
	printk("s5k5cag_release completed\n");
	mutex_unlock(&s5k5cag_mutex);
	return rc;
}

static void s5k5cag_get_pict_fps(uint16_t fps, uint16_t *pfps)
{
	/* input fps is preview fps in Q8 format */
	uint32_t divider;   /* Q10 */

	divider = (uint32_t)
		((s5k5cag_reg_pat[S_RES_PREVIEW].size_h +
			s5k5cag_reg_pat[S_RES_PREVIEW].blk_l) *
		 (s5k5cag_reg_pat[S_RES_PREVIEW].size_w +
			s5k5cag_reg_pat[S_RES_PREVIEW].blk_p)) * 0x00000400 /
		((s5k5cag_reg_pat[S_RES_CAPTURE].size_h +
			s5k5cag_reg_pat[S_RES_CAPTURE].blk_l) *
		 (s5k5cag_reg_pat[S_RES_CAPTURE].size_w +
			s5k5cag_reg_pat[S_RES_CAPTURE].blk_p));

	/* Verify PCLK settings and frame sizes. */
	*pfps = (uint16_t)(fps * divider / 0x00000400);
}

static uint16_t s5k5cag_get_prev_lines_pf(void)
{
	return s5k5cag_reg_pat[S_RES_PREVIEW].size_h +
		s5k5cag_reg_pat[S_RES_PREVIEW].blk_l;
}

static uint16_t s5k5cag_get_prev_pixels_pl(void)
{
	return s5k5cag_reg_pat[S_RES_PREVIEW].size_w +
		s5k5cag_reg_pat[S_RES_PREVIEW].blk_p;
}

static uint16_t s5k5cag_get_pict_lines_pf(void)
{
	return s5k5cag_reg_pat[S_RES_CAPTURE].size_h +
		s5k5cag_reg_pat[S_RES_CAPTURE].blk_l;
}

static uint16_t s5k5cag_get_pict_pixels_pl(void)
{
	return s5k5cag_reg_pat[S_RES_CAPTURE].size_w +
		s5k5cag_reg_pat[S_RES_CAPTURE].blk_p;
}

static uint32_t s5k5cag_get_pict_max_exp_lc(void)
{
	uint32_t snapshot_lines_per_frame;

	if (s5k5cag_ctrl->pict_res == S_QTR_SIZE)
		snapshot_lines_per_frame =
		s5k5cag_reg_pat[S_RES_PREVIEW].size_h +
		s5k5cag_reg_pat[S_RES_PREVIEW].blk_l;
	else
		snapshot_lines_per_frame = 3961 * 3;

	return snapshot_lines_per_frame;
}

static int32_t s5k5cag_set_fps(struct fps_cfg *fps)
{
	/* input is new fps in Q10 format */
	int32_t rc = 0;
#if 0// jones
	enum msm_s_setting setting;

	s5k5cag_ctrl->fps_divider = fps->fps_div;

	if (s5k5cag_ctrl->sensormode == SENSOR_PREVIEW_MODE)
		setting = S_RES_PREVIEW;
	else
		setting = S_RES_CAPTURE;

  rc = s5k5cag_i2c_write_b(s5k5cag_client->addr,
		REG_FRAME_LENGTH_LINES_MSB,
		(((s5k5cag_reg_pat[setting].size_h +
			s5k5cag_reg_pat[setting].blk_l) *
			s5k5cag_ctrl->fps_divider / 0x400) & 0xFF00) >> 8);
	if (rc < 0)
		goto set_fps_done;

  rc = s5k5cag_i2c_write_b(s5k5cag_client->addr,
		REG_FRAME_LENGTH_LINES_LSB,
		(((s5k5cag_reg_pat[setting].size_h +
			s5k5cag_reg_pat[setting].blk_l) *
			s5k5cag_ctrl->fps_divider / 0x400) & 0x00FF));

set_fps_done:
#endif //jones	
	return rc;
}

static int32_t s5k5cag_write_exp_gain(uint16_t gain, uint32_t line)
{
	int32_t rc = 0;
#if 0 // jones
	uint16_t max_legal_gain = 0x0200;
	uint32_t ll_ratio; /* Q10 */
	uint32_t ll_pck, fl_lines;
	uint16_t offset = 4;
	uint32_t  gain_msb, gain_lsb;
	uint32_t  intg_t_msb, intg_t_lsb;
	uint32_t  ll_pck_msb, ll_pck_lsb;

	struct s5k5cag_i2c_reg_conf tbl[2];

	CDBG("Line:%d s5k5cag_write_exp_gain \n", __LINE__);

	if (s5k5cag_ctrl->sensormode == SENSOR_PREVIEW_MODE) {

		s5k5cag_ctrl->my_reg_gain = gain;
		s5k5cag_ctrl->my_reg_line_count = (uint16_t)line;

		fl_lines = s5k5cag_reg_pat[S_RES_PREVIEW].size_h +
			s5k5cag_reg_pat[S_RES_PREVIEW].blk_l;

		ll_pck = s5k5cag_reg_pat[S_RES_PREVIEW].size_w +
			s5k5cag_reg_pat[S_RES_PREVIEW].blk_p;

	} else {

		fl_lines = s5k5cag_reg_pat[S_RES_CAPTURE].size_h +
			s5k5cag_reg_pat[S_RES_CAPTURE].blk_l;

		ll_pck = s5k5cag_reg_pat[S_RES_CAPTURE].size_w +
			s5k5cag_reg_pat[S_RES_CAPTURE].blk_p;
	}

	if (gain > max_legal_gain)
		gain = max_legal_gain;

	/* in Q10 */
	line = (line * s5k5cag_ctrl->fps_divider);

	if (fl_lines < (line / 0x400))
		ll_ratio = (line / (fl_lines - offset));
	else
		ll_ratio = 0x400;

	/* update gain registers */
	gain_msb = (gain & 0xFF00) >> 8;
	gain_lsb = gain & 0x00FF;
	tbl[0].waddr = REG_ANALOGUE_GAIN_CODE_GLOBAL_MSB;
	tbl[0].bdata = gain_msb;
	tbl[1].waddr = REG_ANALOGUE_GAIN_CODE_GLOBAL_LSB;
	tbl[1].bdata = gain_lsb;
	rc = s5k5cag_i2c_write_table(&tbl[0], ARRAY_SIZE(tbl));
	if (rc < 0)
		goto write_gain_done;

	ll_pck = ll_pck * ll_ratio;
	ll_pck_msb = ((ll_pck / 0x400) & 0xFF00) >> 8;
	ll_pck_lsb = (ll_pck / 0x400) & 0x00FF;
	tbl[0].waddr = REG_LINE_LENGTH_PCK_MSB;
	tbl[0].bdata = ll_pck_msb;
	tbl[1].waddr = REG_LINE_LENGTH_PCK_LSB;
	tbl[1].bdata = ll_pck_lsb;
	rc = s5k5cag_i2c_write_table(&tbl[0], ARRAY_SIZE(tbl));
	if (rc < 0)
		goto write_gain_done;

	line = line / ll_ratio;
	intg_t_msb = (line & 0xFF00) >> 8;
	intg_t_lsb = (line & 0x00FF);
	tbl[0].waddr = REG_COARSE_INTEGRATION_TIME;
	tbl[0].bdata = intg_t_msb;
	tbl[1].waddr = REG_COARSE_INTEGRATION_TIME_LSB;
	tbl[1].bdata = intg_t_lsb;
	rc = s5k5cag_i2c_write_table(&tbl[0], ARRAY_SIZE(tbl));

write_gain_done:
#endif // jones	
	return rc;
}

static int32_t s5k5cag_set_pict_exp_gain(uint16_t gain, uint32_t line)
{
	int32_t rc = 0;

	CDBG("Line:%d s5k5cag_set_pict_exp_gain \n", __LINE__);

	rc =
		s5k5cag_write_exp_gain(gain, line);

	return rc;
}
#define CONFIG_DELAY 10
static int32_t s5k5cag_video_config(int mode, int res)
{
	int32_t rc;
	
	printk( "s5k5cag_video_config, CONFIG_DELAY:%d\r\n", CONFIG_DELAY );
	
	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x023C );
	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0000 );
	
	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x0240 );
	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0001 );
	
	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x0230 );
	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0001 );

	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x023E );	
	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0001 );
	
	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x0220 );
	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0001 ); 
	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0001 ); 
	

//	s5k5cag_i2c_write_w_table( &s5k5cag_init_tbl2[0],
//		ARRAY_SIZE(s5k5cag_init_tbl2) );	
	switch (res) {
	case S_QTR_SIZE:
#if 0		
		rc = s5k5cag_setting(S_UPDATE_PERIODIC, S_RES_PREVIEW);
		if (rc < 0)
			return rc;
#endif 
		CDBG("s5k5cag sensor configuration done!\n");
		printk( "s5k5cag S_QTR_SIZE configuration done\r\n" );
		break;

	case S_FULL_SIZE:
#if 0		
		rc = s5k5cag_setting(S_UPDATE_PERIODIC, S_RES_CAPTURE);
		if (rc < 0)
			return rc;
#endif 
		break;

	default:
		return 0;
	} /* switch */

	s5k5cag_ctrl->prev_res = res;
	s5k5cag_ctrl->curr_res = res;
	s5k5cag_ctrl->sensormode = mode;
#if 0
	rc =
		s5k5cag_write_exp_gain(s5k5cag_ctrl->my_reg_gain,
			s5k5cag_ctrl->my_reg_line_count);
#endif			
	mdelay(CONFIG_DELAY);
	
	return rc;
}

static int32_t s5k5cag_snapshot_config(int mode)
{
	int32_t rc = 0;
#if 0
	rc = s5k5cag_setting(S_UPDATE_PERIODIC, S_RES_CAPTURE);
	if (rc < 0)
		return rc;
#endif 

	printk( "s5k5cag_snapshot_config, CONFIG_DELAY:%d \r\n", CONFIG_DELAY );

	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x0244 );
	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0000 );
	
	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x0230 );
	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0001 );
	
	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x0246 );
	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0001 );

	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x0224 );	
	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0001 );
	s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0001 ); //#REG_TC_GP_EnableCaptureChanged

	s5k5cag_ctrl->curr_res = s5k5cag_ctrl->pict_res;
	s5k5cag_ctrl->sensormode = mode;

	mdelay(CONFIG_DELAY);
	return rc;
}

static int32_t s5k5cag_raw_snapshot_config(int mode)
{
	int32_t rc = 0;
#if 0
	rc = s5k5cag_setting(S_UPDATE_PERIODIC, S_RES_CAPTURE);
	if (rc < 0)
		return rc;
#endif 
	s5k5cag_ctrl->curr_res = s5k5cag_ctrl->pict_res;
	s5k5cag_ctrl->sensormode = mode;

	return rc;
}

static int32_t s5k5cag_set_sensor_mode(int mode, int res)
{
	int32_t rc = 0;

	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		rc = s5k5cag_video_config(mode, res);
		break;

	case SENSOR_SNAPSHOT_MODE:
		rc = s5k5cag_snapshot_config(mode);
		break;

	case SENSOR_RAW_SNAPSHOT_MODE:
		rc = s5k5cag_raw_snapshot_config(mode);
		break;

	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int32_t s5k5cag_set_default_focus(void)
{
	int32_t rc = 0;
#if 0 // jones
  rc = s5k5cag_i2c_write_b(s5k5cag_client->addr,
		0x3131, 0);
	if (rc < 0)
		return rc;

  rc = s5k5cag_i2c_write_b(s5k5cag_client->addr,
		0x3132, 0);
	if (rc < 0)
		return rc;

	s5k5cag_ctrl->curr_lens_pos = 0;
#endif // jones
	return rc;
}

static int32_t s5k5cag_move_focus(int direction, int32_t num_steps)
{
	int32_t rc = 0;
#if 0 // jones	
	int32_t i;
	int16_t step_direction;
	int16_t actual_step;
	int16_t next_pos, pos_offset;
	int16_t init_code = 50;
	uint8_t next_pos_msb, next_pos_lsb;
	int16_t s_move[5];
	uint32_t gain; /* Q10 format */
	
	if (direction == MOVE_NEAR)
		step_direction = 20;
	else if (direction == MOVE_FAR)
		step_direction = -20;
	else {
		CDBG("s5k5cag_move_focus failed at line %d ...\n", __LINE__);
		return -EINVAL;
	}

	actual_step = step_direction * (int16_t)num_steps;
	pos_offset = init_code + s5k5cag_ctrl->curr_lens_pos;
	gain = actual_step * 0x400 / 5;

	for (i = 0; i <= 4; i++) {
		if (actual_step >= 0)
			s_move[i] = (((i+1)*gain+0x200)-(i*gain+0x200))/0x400;
		else
			s_move[i] = (((i+1)*gain-0x200)-(i*gain-0x200))/0x400;
	}

	/* Ring Damping Code */
	for (i = 0; i <= 4; i++) {
		next_pos = (int16_t)(pos_offset + s_move[i]);

		if (next_pos > (738 + init_code))
			next_pos = 738 + init_code;
		else if (next_pos < 0)
			next_pos = 0;

		CDBG("next_position in damping mode = %d\n", next_pos);
		/* Writing the Values to the actuator */
		if (next_pos == init_code)
			next_pos = 0x00;

		next_pos_msb = next_pos >> 8;
		next_pos_lsb = next_pos & 0x00FF;

		rc = s5k5cag_i2c_write_b(s5k5cag_client->addr,
			0x3131, next_pos_msb);
		if (rc < 0)
			break;

		rc = s5k5cag_i2c_write_b(s5k5cag_client->addr,
			0x3132, next_pos_lsb);
		if (rc < 0)
			break;

		pos_offset = next_pos;
		s5k5cag_ctrl->curr_lens_pos = pos_offset - init_code;
		if (i < 4)
			mdelay(3);
	}
#endif // jones
	return rc;
}


/////////////////////////////////////////////////////////////

//1. Color Effect
//-Mono
//-Sepia
//-Negative
static long s5k5cag_set_effect(int mode, int effect)
{
	int rc = 0;
	printk( "s5k5cag_set_effect, mode:%d, effect:%d\r\n", mode, effect );
	
	//Special Effect s5k5cag reg
	//0: No special effect 
	//1: Monochrome 
	//2: Negative monochrome 
	//3: Sepia 
	//4: Aqua 
	//5: Sketch 
	switch (effect) {
		case CAMERA_EFFECT_OFF: {//Normal
			printk( "s5k5cag_set_effect, CAMERA_EFFECT_OFF\r\n" );
			rc = s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			rc = s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x021E );
			rc = s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0000 );    	
    	
			if (rc < 0)
				return rc;
		}
				break;
    	
		case CAMERA_EFFECT_MONO: {//B&W
			printk( "s5k5cag_set_effect, CAMERA_EFFECT_MONO\r\n" );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x021E );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0001 );
    	
			if (rc < 0)
				return rc;
		}
			break;
    	
		case CAMERA_EFFECT_NEGATIVE: {//Negative
			printk( "s5k5cag_set_effect, CAMERA_EFFECT_NEGATIVE\r\n" );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x021E );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0003 );
			if (rc < 0)
				return rc;
		}
			break;
		
		case CAMERA_EFFECT_SEPIA: {
			printk( "s5k5cag_set_effect, CAMERA_EFFECT_SEPIA\r\n" );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x021E );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0004 );
			if (rc < 0)
				return rc;
		}	
			break;
		
		default: {
			if (rc < 0)
				return rc;

			/*return -EFAULT;*/
			/* -EFAULT makes app fail */
			return 0;
		}
	} // #endif case
			
	return 0;
}

//2. Metering Mode
//-Frame Average
//-Center Weighted
//-Spot Metering
static long s5k5cag_set_meteringmod(int mode, int meteringmod)
{
	int rc = 0;
	printk( "s5k5cag_set_meteringmod, mode:%d, meteringmod:%d\r\n", mode, meteringmod );
	switch (meteringmod) {
		case CAMERA_AVERAGE_METERING: {
		}
		break;
		 
		case CAMERA_CENTER_METERING: {
		}
		break;
		
		case CAMERA_SPOT_METERING: {
		}
		break;
		
		default: {
			if (rc < 0)
				return rc;

			/*return -EFAULT;*/
			/* -EFAULT makes app fail */
			return 0;
		}
	} // #endif case
			
	return 0;
}

//3. Saturation
static long s5k5cag_set_saturation(int mode, int saturation)
{
	int rc = 0;
	printk( "s5k5cag_set_saturation, mode:%d, saturation:%d\r\n", mode, saturation );
	
	switch (saturation) {
		case CAMERA_SATURATION_MINUS_2: {    
			printk( "s5k5cag_set_saturation, CAMERA_SATURATION_MINUS_2\r\n" );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x0210 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0xFF81 );		
			if (rc < 0)
				return rc;
		}
		break;
		
		case CAMERA_SATURATION_MINUS_1: {
    		printk( "s5k5cag_set_saturation, CAMERA_SATURATION_MINUS_1\r\n" );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x0210 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0xFFC0 );		
			if (rc < 0)
				return rc;
		}
		break;
		
		case CAMERA_SATURATION_ZERO: {
    		printk( "s5k5cag_set_saturation, CAMERA_SATURATION_ZERO\r\n" );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x0210 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0000 );
			if (rc < 0)
				return rc;
		}
		break;
		
		case CAMERA_SATURATION_POSITIVE_1: {
    		printk( "s5k5cag_set_saturation, CAMERA_SATURATION_POSITIVE_1\r\n" );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x0210 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0040 );
			
			if (rc < 0)
				return rc;
		}
		break;
		
		case CAMERA_SATURATION_POSITIVE_2: {
    		printk( "s5k5cag_set_saturation, CAMERA_SATURATION_POSITIVE_2\r\n" );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x0210 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x007F );
			
			if (rc < 0)
				return rc;
		}
		break;
		
		default: {
			if (rc < 0)
				return rc;

			/*return -EFAULT;*/
			/* -EFAULT makes app fail */
			return 0;
		}	
		
		
	} // Endof case
	
	return 0;
}

//4. Contrast
static long s5k5cag_set_contrast(int mode, int contrast)
{
	int rc = 0;
	printk( "s5k5cag_set_contrast, mode:%d, contrast:%d\r\n", mode, contrast );
	switch (contrast) {
		case CAMERA_CONTRAST_MINUS_2: {			
    		printk( "s5k5cag_set_contrast, CAMERA_CONTRAST_MINUS_2\r\n" );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x020E );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0xFF81 );
			
			if (rc < 0)
				return rc;
		}
		break;
		
		case CAMERA_CONTRAST_MINUS_1: {			
    		printk( "s5k5cag_set_contrast, CAMERA_CONTRAST_MINUS_1\r\n" );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x020E );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0xFFC0 );
			
			if (rc < 0)
				return rc;
		}
		break;
		
		case CAMERA_CONTRAST_ZERO: {
    		printk( "s5k5cag_set_contrast, CAMERA_CONTRAST_ZERO\r\n" );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x020E );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0000 );
			
			if (rc < 0)
				return rc;
		}
		break;
		
		case CAMERA_CONTRAST_POSITIVE_1: {
    		printk( "s5k5cag_set_contrast, CAMERA_CONTRAST_POSITIVE_1\r\n" );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x020E );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0040 );
			
			if (rc < 0)
				return rc;
		}
		break;
		
		case CAMERA_CONTRAST_POSITIVE_2: {
    		printk( "s5k5cag_set_contrast, CAMERA_CONTRAST_POSITIVE_2\r\n" );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x020E );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x007F );
			
			if (rc < 0)
				return rc;
		}
		break;
		
		default: {
			if (rc < 0)
				return rc;

			/*return -EFAULT;*/
			/* -EFAULT makes app fail */
			return 0;
		}	
		
		
	} // Endof case
	return 0;
}

//5. Sharpness
static long s5k5cag_set_sharpness(int mode, int sharpness)
{
	int rc = 0;
	printk( "s5k5cag_set_sharpness, mode:%d, sharpness:%d\r\n", mode, sharpness );
	
	switch (sharpness) {
		case CAMERA_SHARPNESS_ZERO: {			
    		printk( "s5k5cag_set_sharpness, CAMERA_SHARPNESS_ZERO\r\n" );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x0212 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0xFF81 );
			
			if (rc < 0)
				return rc;
		}
		break;
		
		case CAMERA_SHARPNESS_POSITIVE_1: {			
			printk( "s5k5cag_set_sharpness, CAMERA_SHARPNESS_POSITIVE_1\r\n" );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x0212 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0000 );    		
			if (rc < 0)
				return rc;
		}
		break;
		
		case CAMERA_SHARPNESS_POSITIVE_2: {			
    		printk( "s5k5cag_set_sharpness, CAMERA_SHARPNESS_POSITIVE_2\r\n" );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x0212 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x007F );
			if (rc < 0)
				return rc;
		}
		break;
		
		default: {
			if (rc < 0)
				return rc;

			/*return -EFAULT;*/
			/* -EFAULT makes app fail */
			return 0;
		}	
		
		
	} // Endof case
	
	return 0;
}

//6. Brightness
static long s5k5cag_set_brightness(int mode, int brightness)
{
	int rc = 0;
	printk( "s5k5cag_set_brightness, mode:%d, brightness:%d\r\n", mode, brightness );
	
	switch (brightness) {
		case CAMERA_BRIGHTNESS_0: {	
			printk( "s5k5cag_set_brightness, CAMERA_BRIGHTNESS_0\r\n" );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x020C );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0xFF81 );		
    		
			if (rc < 0)
				return rc;
		}
		break;
		
		case CAMERA_BRIGHTNESS_1: {			
    		printk( "s5k5cag_set_brightness, CAMERA_BRIGHTNESS_1\r\n" );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x020C );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0xFFAC );
			if (rc < 0)
				return rc;
		}
		break;
		
		case CAMERA_BRIGHTNESS_2: {		
			printk( "s5k5cag_set_brightness, CAMERA_BRIGHTNESS_2\r\n" );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x020C );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0xFFD6 );			
    		
			if (rc < 0)
				return rc;
		}
		break;
		
		case CAMERA_BRIGHTNESS_3: {		
			printk( "s5k5cag_set_brightness, CAMERA_BRIGHTNESS_3\r\n" );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x020C );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0000 );			
    		
			if (rc < 0)
				return rc;
		}
		break;
		
		case CAMERA_BRIGHTNESS_4: {			
    		printk( "s5k5cag_set_brightness, CAMERA_BRIGHTNESS_4\r\n" );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x020C );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x002A );
			if (rc < 0)
				return rc;
		}
		break;
		
		case CAMERA_BRIGHTNESS_5: {			
    		printk( "s5k5cag_set_brightness, CAMERA_BRIGHTNESS_5\r\n" );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x020C );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0054 );			
			if (rc < 0)
				return rc;
		}
		break;
		
		case CAMERA_BRIGHTNESS_6: {			
    		printk( "s5k5cag_set_brightness, CAMERA_BRIGHTNESS_6\r\n" );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x020C );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x007F );			
			if (rc < 0)
				return rc;
		}
		break;
		
		default: {
			if (rc < 0)
				return rc;

			/*return -EFAULT;*/
			/* -EFAULT makes app fail */
			return 0;
		}	
		
		
	} // Endof case
	return 0;
}


//Anti Banding
//-50 Hz
//-60 Hz
//-auto
static long s5k5cag_set_antibanding(int mode, int antibanding)
{
	int rc = 0;
	uint16_t reg;

	printk( "s5k5cag_set_antibanding, mode:%d, antibanding:%d\r\n", mode, antibanding );
//2. to disable
// 2.1 REG_TC_DBG_AutoAlgEnBits 0x7000 04D2 bit 5 clear to 0
// 2.2 REG_SF_USER_FlickerQuant 0x7000 04BA // select 1:50Hz, 2:60Hz
// 2.3 REG_SF_USER_FlickerQuantChanged 0x7000 04BC // write a '1' to active
//
//3.return to AFC on
// 3.1 REG_TC_DBG_AutoAlgEnBits 0x7000 04D2 bit 5 set to 1
	
// read : 0x2C, 0x2E
// write: 0x28, 0x2A
	
	switch (antibanding) {
		case CAMERA_ANTIBANDING_OFF: {			
    		printk( "s5k5cag_set_antibanding, CAMERA_ANTIBANDING_OFF\r\n" );    		
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002C, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002E, 0x04D2 );
			s5k5cag_i2c_read_w( s5k5cag_client->addr, 0X0F12, &reg );
			
			printk( "s5k5cag_set_antibanding->, reg:0x%x\r\n", reg );
			reg &= 0xFFDF;
			printk( "s5k5cag_set_antibanding<-, reg:0x%x\r\n", reg );			
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x04D2 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, reg );
			// Disable AFC done
			
			if (rc < 0)
				return rc;
		}
		break;
		
		case CAMERA_ANTIBANDING_60HZ: {	
			// REG_TC_DBG_AutoAlgEnBits 0x7000 04D2 bit 5 clear to 0
			printk( "s5k5cag_set_antibanding, CAMERA_ANTIBANDING_60HZ\r\n" );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002C, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002E, 0x04D2 );
			s5k5cag_i2c_read_w( s5k5cag_client->addr, 0X0F12, &reg );
			
			printk( "s5k5cag_set_antibanding->, reg:0x%x\r\n", reg );
			reg &= 0xFFDF;
			printk( "s5k5cag_set_antibanding<-, reg:0x%x\r\n", reg );			
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x04D2 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, reg );
			// Disable AFC done	
			
			// REG_SF_USER_FlickerQuant 0x7000 04BA // select 1:50Hz, 2:60Hz	
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x04BA );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0X0002 );
			
			//REG_SF_USER_FlickerQuantChanged 0x7000 04BC // write a '1' to active
    		s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x04BC );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0X0001 );
			if (rc < 0)
				return rc;
		}
		break;
		
		case CAMERA_ANTIBANDING_50HZ: {	
			
			printk( "s5k5cag_set_antibanding, CAMERA_ANTIBANDING_50HZ\r\n" );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002C, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002E, 0x04D2 );
			s5k5cag_i2c_read_w( s5k5cag_client->addr, 0X0F12, &reg );
			
			printk( "s5k5cag_set_antibanding->, reg:0x%x\r\n", reg );
			reg &= 0xFFDF;
			printk( "s5k5cag_set_antibanding<-, reg:0x%x\r\n", reg );			
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x04D2 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, reg );
			// Disable AFC done	
			
			// REG_SF_USER_FlickerQuant 0x7000 04BA // select 1:50Hz, 2:60Hz	
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x04BA );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0X0001 );
			
			//REG_SF_USER_FlickerQuantChanged 0x7000 04BC // write a '1' to active
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x04BC );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0X0001 );					
    		
			if (rc < 0)
				return rc;
		}
		break;
		
		case CAMERA_ANTIBANDING_AUTO: {
			// REG_TC_DBG_AutoAlgEnBits 0x7000 04D2 bit 5 set to 1
			printk( "s5k5cag_set_antibanding, CAMERA_ANTIBANDING_AUTO\r\n" );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x04D2 );
			s5k5cag_i2c_read_w( s5k5cag_client->addr, 0X0F12, &reg );
			
			printk( "s5k5cag_set_antibanding->, reg:0x%x\r\n", reg );
			reg |= 0x0020;
			printk( "s5k5cag_set_antibanding<-, reg:0x%x\r\n", reg );			
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x04D2 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, reg );
			// Enable AFC done
			if (rc < 0)
				return rc;
		}
		break;
		
		default: {
			if (rc < 0)
				return rc;

			/*return -EFAULT;*/
			/* -EFAULT makes app fail */
			return 0;
		}		
		
	} // Endof case
	
	return 0;

}


static long s5k5cag_set_wb(int mode, int wb)
{
	int rc = 0;
	printk( "s5k5cag_set_wb, mode:%d, wb:%d\r\n", mode, wb );
	
		switch (wb) {
		case CAMERA_WB_AUTO: {			
    		printk( "s5k5cag_set_wb, CAMERA_WB_AUTO\r\n" );
    		//===============
			//	AWB_auto 
			//===============
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x04D2 );
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x067F ); // #REG_TC_DBG_AutoAlgEnBits, [b:3] 1: Auto WB active. 0: not active.    		
			
			if (rc < 0)
				return rc;
		}
		break;
		
		case CAMERA_WB_INCANDESCENT: {			
    		printk( "s5k5cag_set_wb, CAMERA_WB_INCANDESCENT\r\n" );
    		//================================================================================================
			//	MWB : INCANDESCENT  
			//================================================================================================
			s5k5cag_i2c_write_w( s5k5cag_client->addr,	0x0028, 0x7000);
			s5k5cag_i2c_write_w( s5k5cag_client->addr,	0x002A, 0x04D2);
			s5k5cag_i2c_write_w( s5k5cag_client->addr,	0x0F12, 0x0777);	// #REG_TC_DBG_AutoAlgEnBits, AWB Off
			s5k5cag_i2c_write_w( s5k5cag_client->addr,	0x002A, 0x04A0);
			s5k5cag_i2c_write_w( s5k5cag_client->addr,	0x0F12, 0x0433);	// #REG_SF_USER_Rgain
			s5k5cag_i2c_write_w( s5k5cag_client->addr,	0x0F12, 0x0001);
			s5k5cag_i2c_write_w( s5k5cag_client->addr,	0x0F12, 0x0400);	// #REG_SF_USER_Ggain
			s5k5cag_i2c_write_w( s5k5cag_client->addr,	0x0F12, 0x0001);
			s5k5cag_i2c_write_w( s5k5cag_client->addr,	0x0F12, 0x0828);	// #REG_SF_USER_Bgain
			s5k5cag_i2c_write_w( s5k5cag_client->addr,	0x0F12, 0x0001);
			if (rc < 0)
				return rc;
		}
		break;
		
		case CAMERA_WB_FLUORESCENT: {		
			printk( "s5k5cag_set_wb, CAMERA_WB_FLUORESCENT\r\n" );	
    		//================================================================================================
			//	MWB : CAMERA_WB_FLUORESCENT  
			//================================================================================================
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000);
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x04D2);
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0777);	// #REG_TC_DBG_AutoAlgEnBits, AWB Off
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x04A0);
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x05D1);	// #REG_SF_USER_Rgain
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0001);
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0400);	// #REG_SF_USER_Ggain
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0001);
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x074C);	// #REG_SF_USER_Bgain
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0001);
			if (rc < 0)
				return rc;
		}
		break;
		
		case CAMERA_WB_DAYLIGHT: {	
			printk( "s5k5cag_set_wb, CAMERA_WB_DAYLIGHT\r\n" );			
    		//================================================================================================
			//	MWB : CAMERA_WB_DAYLIGHT  
			//================================================================================================
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000);
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x04D2);
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0777);	// #REG_TC_DBG_AutoAlgEnBits, AWB Off
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x04A0);
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x06BB);	// #REG_SF_USER_Rgain
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0001);
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0400);	// #REG_SF_USER_Ggain
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0001);
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x04D0);	// #REG_SF_USER_Bgain
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0001);
     
			if (rc < 0)
				return rc;
		}
		break;
		
		case CAMERA_WB_CLOUDY_DAYLIGHT: {	
			printk( "s5k5cag_set_wb, CAMERA_WB_CLOUDY_DAYLIGHT\r\n" );			
    		//================================================================================================
			//	MWB : CAMERA_WB_CLOUDY_DAYLIGHT  
			//================================================================================================
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0028, 0x7000);
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x04D2);
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0777);	// #REG_TC_DBG_AutoAlgEnBits, AWB Off
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x002A, 0x04A0);
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x07A0);	// #REG_SF_USER_Rgain
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0001);
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0400);	// #REG_SF_USER_Ggain
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0001);
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0480);	// #REG_SF_USER_Bgain
			s5k5cag_i2c_write_w( s5k5cag_client->addr, 0x0F12, 0x0001);
			if (rc < 0)
				return rc;
		}
		break;		
		
		default: {
			if (rc < 0)
				return rc;

			/*return -EFAULT;*/
			/* -EFAULT makes app fail */
			return 0;
		}	
		
		
	} // Endof case
	
	return 0;

}
/////////////////////////////////////////////////////////////


static int s5k5cag_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cfg_data;
	long   rc = 0;

	if (copy_from_user(&cfg_data,
			(void *)argp,
			sizeof(struct sensor_cfg_data)))
		return -EFAULT;

	mutex_lock(&s5k5cag_mutex);

	CDBG("%s: cfgtype = %d\n", __func__, cfg_data.cfgtype);
	switch (cfg_data.cfgtype) {
	case CFG_GET_PICT_FPS:
		s5k5cag_get_pict_fps(cfg_data.cfg.gfps.prevfps,
			&(cfg_data.cfg.gfps.pictfps));

		if (copy_to_user((void *)argp, &cfg_data,
				sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;

	case CFG_GET_PREV_L_PF:
		cfg_data.cfg.prevl_pf = s5k5cag_get_prev_lines_pf();

		if (copy_to_user((void *)argp,
				&cfg_data,
				sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;

	case CFG_GET_PREV_P_PL:
		cfg_data.cfg.prevp_pl = s5k5cag_get_prev_pixels_pl();

		if (copy_to_user((void *)argp,
				&cfg_data,
				sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;

	case CFG_GET_PICT_L_PF:
		cfg_data.cfg.pictl_pf = s5k5cag_get_pict_lines_pf();

		if (copy_to_user((void *)argp,
				&cfg_data,
				sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;

	case CFG_GET_PICT_P_PL:
		cfg_data.cfg.pictp_pl = s5k5cag_get_pict_pixels_pl();

		if (copy_to_user((void *)argp,
				&cfg_data,
				sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;

	case CFG_GET_PICT_MAX_EXP_LC:
		cfg_data.cfg.pict_max_exp_lc =
			s5k5cag_get_pict_max_exp_lc();

		if (copy_to_user((void *)argp,
				&cfg_data,
				sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;

	case CFG_SET_FPS:
	case CFG_SET_PICT_FPS:
		rc = s5k5cag_set_fps(&(cfg_data.cfg.fps));
		break;

	case CFG_SET_EXP_GAIN:
		rc =
			s5k5cag_write_exp_gain(cfg_data.cfg.exp_gain.gain,
				cfg_data.cfg.exp_gain.line);
		break;

	case CFG_SET_PICT_EXP_GAIN:
		CDBG("Line:%d CFG_SET_PICT_EXP_GAIN \n", __LINE__);
		rc =
			s5k5cag_set_pict_exp_gain(
				cfg_data.cfg.exp_gain.gain,
				cfg_data.cfg.exp_gain.line);
		break;

	case CFG_SET_MODE:
		rc =
			s5k5cag_set_sensor_mode(
			cfg_data.mode, cfg_data.rs);
		break;

	case CFG_PWR_DOWN:
		rc = s5k5cag_power_down();
		break;

	case CFG_MOVE_FOCUS:
		rc =
			s5k5cag_move_focus(
			cfg_data.cfg.focus.dir,
			cfg_data.cfg.focus.steps);
		break;

	case CFG_SET_DEFAULT_FOCUS:
		rc =
			s5k5cag_set_default_focus();
		break;

/////////////////////////////////////////////////////////////

//1. Color Effect
//-Mono
//-Sepia
//-Negative
//static long s5k5cag_set_effect(int mode, int effect)
	case CFG_SET_EFFECT:
		rc = s5k5cag_set_effect(
					cfg_data.mode,
					cfg_data.cfg.effect);
		break;

//2. Metering Mode
//-Frame Average
//-Center Weighted
//-Spot Metering
// static long s5k5cag_set_meteringmod(int mode, int meteringmod)
	case CFG_SET_METERINGMOD:
		rc = s5k5cag_set_meteringmod(
					cfg_data.mode,
					cfg_data.cfg.meteringmod);
		break;

//3. Saturation
//static long s5k5cag_set_saturation(int mode, int saturation)
	case CFG_SET_SATURATION:
		rc = s5k5cag_set_saturation(
					cfg_data.mode,
					cfg_data.cfg.saturation);
		break;

//4. Contrast
//static long s5k5cag_set_contrast(int mode, int contrast)
	case CFG_SET_CONTRAST:
		rc = s5k5cag_set_contrast(
					cfg_data.mode,
					cfg_data.cfg.contrast);
		break;

//5. Sharpness
// static long s5k5cag_set_sharpness(int mode, int sharpness)
	case CFG_SET_SHARPNESS:
		rc = s5k5cag_set_sharpness(
					cfg_data.mode,
					cfg_data.cfg.sharpness);
		break;

//6. Brightness
// static long s5k5cag_set_brightness(int mode, int brightness)
	case CFG_SET_BRIGHTNESS:
		rc = s5k5cag_set_brightness(
					cfg_data.mode,
					cfg_data.cfg.brightness);
		break;

//Anti Banding
//-50 Hz
//-60 Hz
//-auto
// static long s5k5cag_set_antibanding(int mode, int antibanding)
	case CFG_SET_ANTIBANDING:
		rc = s5k5cag_set_antibanding(
					cfg_data.mode,
					cfg_data.cfg.antibanding);
		break;
		
//awb
	case CFG_SET_WB:
		rc = s5k5cag_set_wb(
					cfg_data.mode,
					cfg_data.cfg.wb);
		break;		


/////////////////////////////////////////////////////////////

	case CFG_GET_AF_MAX_STEPS:	
	case CFG_SET_LENS_SHADING:
	default:
		rc = -EINVAL;
		break;
	}

	mutex_unlock(&s5k5cag_mutex);
	return rc;
}


static int s5k5cag_sensor_probe(const struct msm_camera_sensor_info *info,
		struct msm_sensor_ctrl *s)
{
	int rc = 0;
	int productHWID = 0 ;
	rc = i2c_add_driver(&s5k5cag_i2c_driver);
	if (rc < 0 || s5k5cag_client == NULL) {
		if( s5k5cag_client == NULL )
			printk( " s5k5cag_client == NULL\r\n" );
		rc = -ENOTSUPP;
		goto probe_fail;
	}
	
//vince++
	if(fih_read_new_hwid_mech_from_orighwid())
   	{
   		productHWID = fih_read_product_id_from_orighwid() ;     
   	}
	if(productHWID==Project_DMO ||productHWID==Project_DMT)
	{	
		gpio_tlmm_config(GPIO_CFG(CAMIF_SW_DMT, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);		
		rc = gpio_request(CAMIF_SW_DMT, "s5k5cag");
		if (!rc) {
			rc = gpio_direction_output(CAMIF_SW_DMT,0);
			printk( KERN_INFO"gpio_direction_output GPIO_17 value->%d.\n",gpio_get_value(CAMIF_SW_DMT));
		}
		gpio_free(CAMIF_SW_DMT);
		msleep( 5 );
	}
//vince--
	
	printk( " s5k5cag_sensor_probe\r\n" );
	msm_camio_clk_rate_set(S5K5CAG_MCLK);
	mdelay(20);

	rc = s5k5cag_probe_init_sensor(info);
	if (rc < 0)
		goto probe_fail;

	s->s_init = s5k5cag_sensor_open_init;
	s->s_release = s5k5cag_sensor_release;
	s->s_config  = s5k5cag_sensor_config;
	s5k5cag_probe_init_done(info);
	
	// set flash type
	info->flash_data->flash_type = MSM_CAMERA_FLASH_NONE;
	printk( "sk5kcag SENSOR PROBE DONE!\n" );
	return rc;

probe_fail:
	printk( "sk5kcag SENSOR PROBE FAILS!\n" );
	CDBG("SENSOR PROBE FAILS!\n");
	return rc;
}

static int __s5k5cag_probe(struct platform_device *pdev)
{
	printk( "s5k5cag_probe\r\n" );
	return msm_camera_drv_start(pdev, s5k5cag_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __s5k5cag_probe,
	.driver = {
		.name = "msm_camera_s5k5cag",
		.owner = THIS_MODULE,
	},
};

static int __init s5k5cag_init(void)
{
	int rc = 0xAC;
	printk( "s5k5cag_init\r\n" );
	rc = platform_driver_register(&msm_camera_driver);
	printk( "s5k5cag_init rc:%d\r\n", rc );
	return rc;
}

module_init(s5k5cag_init);

