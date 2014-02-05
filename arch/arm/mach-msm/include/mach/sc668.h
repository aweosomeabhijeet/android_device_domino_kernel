#ifndef _SC668_H_
#define _SC668_H_

#define SC668_OFF  0
#define SC668_ON   1


/*Add misc device ioctl command*/
//IO conrol definition.
#define SC668 's'
#define SC668_IO_ON			_IOW(SC668, 0, int)
#define SC668_IO_OFF			_IOW(SC668, 1, int)


struct sc668_platform_data {
	unsigned int sc668_enable_pin;
	unsigned int sc668_pwm_pin;
};

/*!
  The result codes for all of the API function calls.
 */
typedef enum
{
   SC668_SUCCESS                = 0,   // The desired action has been performed.
   SC668_RESERVED_BITS_EXCLUDED = 1,   // The parameters indicated the setting 
                                         // of invalid bits. These bits have been
                                         // masked off and the remainder of the 
                                         // bits have been used to perform the 
                                         // desired function.
   SC668_INVALID_ADDRESS        = 2,   // An invalid address has been passed 
                                         // into the WriteRegister function.
   SC668_INVALID_PARAMETER      = 3,   // An invalid parameter has been used 
                                         // and no action has taken place.
   SC668_I2C_ERROR           = 4    // read/write I2C fail
                                         
} SC668_ResultCode;

/*!
  The register definitions for the SC668.
 */
typedef enum
{
   BACKLIGHT_1TO6_ENABLE                          = 0x00,
   BACKLIGHT_7TO8_AND_BANK_ENABLE                 = 0x01,
   BANK1_CONTROL                                  = 0x02,
   BANK2_CONTROL                                  = 0x03,
   BANK3_CONTROL                                  = 0x04,
   BANK4_CONTROL                                  = 0x05,
   BANK1_SETTINGS                                 = 0x06,
   BANK2_SETTINGS                                 = 0x07,
   BANK3_SETTINGS                                 = 0x08,
   BANK4_SETTINGS                                 = 0x09,
   LDO1_CONTROL                                   = 0x0A,
   LDO2_CONTROL                                   = 0x0B,
   LDO3_CONTROL                                   = 0x0C,
   LDO4_CONTROL                                   = 0x0D,
   LIGHTING_ASSIGNMENTS                           = 0x0E,
   EFFECT_RATE_SETTINGS                           = 0x0F,
   EFFECT_RATE1_TIME                              = 0x10,
   EFFECT_RATE2_TIME                              = 0x11,
   ADC_ENABLE                                     = 0x12,
   ADC_OUTPUT                                     = 0x13,
   ADC_RISE                                       = 0x14,
   ADC_FALL                                       = 0x15,
   ADP_OLE_CONTROLS                               = 0x16,
} SC668_Register;

/*!
  The register bit position definitions of the SC668.
 */
enum
{
   //======= Register Masks ======= 
   BACKLIGHT_1TO6_ENABLE_MASK                          = 0x3F,
   BACKLIGHT_7TO8_AND_BANK_ENABLE_MASK                 = 0x3F,
   BANK1_CONTROL_MASK                                  = 0x3F,
   BANK2_CONTROL_MASK                                  = 0x3F,
   BANK3_CONTROL_MASK                                  = 0x3F,
   BANK4_CONTROL_MASK                                  = 0x3F,
   BANK1_SETTINGS_MASK                                 = 0x3F,
   BANK2_SETTINGS_MASK                                 = 0x3F,
   BANK3_SETTINGS_MASK                                 = 0x3F,
   BANK4_SETTINGS_MASK                                 = 0x3F,
   LDO1_CONTROL_MASK                                   = 0x0F,
   LDO2_CONTROL_MASK                                   = 0x07,
   LDO3_CONTROL_MASK                                   = 0x0F,
   LDO4_CONTROL_MASK                                   = 0x0F,
   LIGHTING_ASSIGNMENTS_MASK                           = 0x1F,
   EFFECT_RATE_SETTINGS_MASK                           = 0x3F,
   EFFECT_RATE1_TIME_MASK                              = 0x3F,
   EFFECT_RATE2_TIME_MASK                              = 0x3F,
   ADC_ENABLE_MASK                                     = 0x23,
   ADC_OUTPUT_MASK                                     = 0xFF,
   ADC_RISE_MASK                                       = 0xFF,
   ADC_FALL_MASK                                       = 0xFF,
   ADP_OLE_CONTROLS_MASK                               = 0x7F,
 
   //======= Backlight Enable ======= 
   BACKLIGHT_8_ENABLE_MASK                         = 0x02,
   BACKLIGHT_7_ENABLE_MASK                         = 0x01,
   BACKLIGHT_6_ENABLE_MASK                         = 0x20,
   BACKLIGHT_5_ENABLE_MASK                         = 0x10,
   BACKLIGHT_4_ENABLE_MASK                         = 0x08,
   BACKLIGHT_3_ENABLE_MASK                         = 0x04,
   BACKLIGHT_2_ENABLE_MASK                         = 0x02,
   BACKLIGHT_1_ENABLE_MASK                         = 0x01,

   //======= Bank Enable Masks ======= 
   BANK_4_ENABLE_MASK                              = 0x08,
   BANK_3_ENABLE_MASK                              = 0x04,
   BANK_2_ENABLE_MASK                              = 0x02,
   BANK_1_ENABLE_MASK                              = 0x01,
   BANK_ENABLE_BIT                                 = 0x10,
   BANK_DISABLE_BIT                                = 0x20,
   BANK_4_ENABLE                                   = 0x08,
   BANK_3_ENABLE                                   = 0x04,
   BANK_2_ENABLE                                   = 0x02,
   BANK_1_ENABLE                                   = 0x01,

   //====== Main Backlight Current Control Register =======
   BACKLIGHT_CURRENT_MASK                          = 0x1F,
   BANK_EFFECT_ENABLE_BIT                          = 0x20,
   BANK_BLINK_ENABLE_BIT                           = 0x20,

   ENABLE_FADE                                     = 0x01,
   ENABLE_BREATHE                                  = 0x03,
   ENABLE_BLINK                                    = 0x02,
   NO_EFFECT                                       = 0x00,

   //======== Backlight and Fade Control Register ========= 
   FADE_MASK                                       = 0x07,
   FADE_SHIFT                                      = 0x00,

   //============== Bank Lighting Assignment ============== 
   BANK1_BL1_BL8                                   = 0x00,
   BANK1_BL2_BL8_BANK2_BL1                         = 0x01,
   BANK1_BL3_BL8_BANK2_BL1_BL2                     = 0x02,
   BANK1_BL3_BL8_BANK2_BL2_BANK3_BL1               = 0x03,
   BANK1_BL4_BL8_BANK2_BL1_BL3                     = 0x04,
   BANK1_BL4_BL8_BANK2_BL3_BANK3_BL2_BANK4_BL1     = 0x05,
   BANK1_BL5_BL8_BANK2_BL3_BL4_BANK3_BL2_BANK4_BL1 = 0x06,
   BANK1_BL6_BL8_BANK2_BL3_BL5_BANK3_BL2_BANK4_BL1 = 0x07,

   //================ Group Bank Assignment ===============
   GROUP1_BK1_GROUP2_BK2_BK3_BK4                   = 0x00,
   GROUP1_BK1_BK2_GROUP2_BK3_BK4                   = 0x01,
   GROUP1_BK1_BK2_BK3_GROUP2_BK4                   = 0x02,
   GROUP1_BK1_BK2_BK3_BK4                          = 0x03,

   //===================== Effect Rates ===================
   BREATH_0_FADE_0                                 = 0x00,
   BREATH_4_FADE_1                                 = 0x01,
   BREATH_8_FADE_2                                 = 0x02,
   BREATH_16_FADE_4                                = 0x03,
   BREATH_24_FADE_6                                = 0x04,
   BREATH_32_FADE_8                                = 0x05,
   BREATH_48_FADE_12                               = 0x06,
   BREATH_64_FADE_16                               = 0x07,

   //===================== Effect Times ===================
   TIME_32                                         = 0x00,
   TIME_64                                         = 0x01,
   TIME_256                                        = 0x02,
   TIME_512                                        = 0x03,
   TIME_1024                                       = 0x04,
   TIME_2048                                       = 0x05,
   TIME_3072                                       = 0x06,
   TIME_4096                                       = 0x07,

   //==================== ADC Rise/Fall ==================
   AD_ENABLE                                       = 0x01,
   AD_DISABLE                                      = 0x00,
   AD_AUTO_ENABLE                                  = 0x01,
   AD_AUTO_DISABLE                                 = 0x00,
   AD_SAT_ENABLE                                   = 0x01,
   AD_SAT_DISABLE                                  = 0x00,
   ADC_VALUE_MASK                                  = 0xFF,

   //======================= ADP/ OLE ====================
   ADP_ACTIVATE                                    = 0x01,
   ADP_DEACTIVATE                                  = 0x00,
   MSEC_4_DELAY                                    = 0x01,
   MICROSEC_256_DELAY                              = 0x00,
   ADP_ENABLE                                      = 0x01,
   ADP_DISABLE                                     = 0x00,
   OLE_NONE_GRP1                                   = 0x00,
   OLE_FULL_AUTO_DIM_GRP1                          = 0x01,
   OLE_PART_AUTO_DIM_GRP1                          = 0x02,
   OLE_SYNCH_GRP1                                  = 0x03,
   OLE_NONE_GRP2                                   = 0x04,
   OLE_FULL_AUTO_DIM_GRP2                          = 0x05,
   OLE_PART_AUTO_DIM_GRP2                          = 0x06,
   OLE_SYNCH_GRP2                                  = 0x07,
   PWM_ENABLE                                      = 0x01,
   PWM_DISABLE                                     = 0x00,
   LIGHT_EFFECT_MASK                               = 0x0E,

   TEST_OUT_OF_BOUND_ONE                           = 0xF,
   TEST_OUT_OF_BOUND_TWO                           = 0xFF,
   TEST_OUT_OF_BOUND_THREE                         = 0xFFF,
   TEST_OUT_OF_BOUND_FOUR                          = 0xFFFF,
};

/*!
  Bank definitions used in the EnableFade,
  EnableBlink and EnableBreathe and disableLightEffect functions. 
 */
typedef enum
{
   BANK_1 = 1,
   BANK_2 = 2,
   BANK_3 = 3,
   BANK_4 = 4
}BankType;

/*!
  Current definitions used in the EnableFade,
  EnableBlink and EnableBreathe functions. 
  Current levels may be set between 0 mA and 25 mA.
 */
typedef enum
{
   BACKLIGHT_CURRENT_0_0   = 0x00,                              
   BACKLIGHT_CURRENT_0_0_5 = 0x01, 
   BACKLIGHT_CURRENT_0_1   = 0x02,
   BACKLIGHT_CURRENT_0_2   = 0x03, 
   BACKLIGHT_CURRENT_0_5   = 0x04, 
   BACKLIGHT_CURRENT_1_0   = 0x05, 
   BACKLIGHT_CURRENT_1_5   = 0x06, 
   BACKLIGHT_CURRENT_2_0   = 0x07, 
   BACKLIGHT_CURRENT_2_5   = 0x08, 
   BACKLIGHT_CURRENT_3_0   = 0x09,  
   BACKLIGHT_CURRENT_3_5   = 0x0A, 
   BACKLIGHT_CURRENT_4_0   = 0x0B, 
   BACKLIGHT_CURRENT_4_5   = 0x0C, 
   BACKLIGHT_CURRENT_5_0   = 0x0D, 
   BACKLIGHT_CURRENT_6_0   = 0x0E, 
   BACKLIGHT_CURRENT_7_0   = 0x0F,  
   BACKLIGHT_CURRENT_8_0   = 0x10, 
   BACKLIGHT_CURRENT_9_0   = 0x11, 
   BACKLIGHT_CURRENT_10_0  = 0x12, 
   BACKLIGHT_CURRENT_11_0  = 0x13, 
   BACKLIGHT_CURRENT_12_0  = 0x14,
   BACKLIGHT_CURRENT_13_0  = 0x15,
   BACKLIGHT_CURRENT_14_0  = 0x16,
   BACKLIGHT_CURRENT_15_0  = 0x17, 
   BACKLIGHT_CURRENT_16_0  = 0x18, 
   BACKLIGHT_CURRENT_17_0  = 0x19, 
   BACKLIGHT_CURRENT_18_0  = 0x1A, 
   BACKLIGHT_CURRENT_19_0  = 0x1B, 
   BACKLIGHT_CURRENT_20_0  = 0x1C, 
   BACKLIGHT_CURRENT_21_0  = 0x1D, 
   BACKLIGHT_CURRENT_23_0  = 0x1E, 
   BACKLIGHT_CURRENT_25_0  = 0x1F      
} BacklightCurrent;

/*!
  Fade time definitions used in the SetMainBacklightFade 
  and SetSubBacklightFade functions. Fade rates may be set 
  between 16 ms/step and 64 ms/step.
 */
typedef enum
{
   NO_FADE        = 0x00,
   MS_PER_STEP_32 = 0x01,   
   MS_PER_STEP_24 = 0x03,   
   MS_PER_STEP_16 = 0x05,   
   MS_PER_STEP_8  = 0x07
} FadeTime;

/*!
  Definitions of the backlights used in the SetBacklights,
  EnableBacklights, and DisableBacklights functions. These
  definitions may be bit wise ORed together.
 */
typedef enum
{
   BACKLIGHT_1 = 0x01,
   BACKLIGHT_2 = 0x02,
   BACKLIGHT_3 = 0x04,
   BACKLIGHT_4 = 0x08,
   BACKLIGHT_5 = 0x10,
   BACKLIGHT_6 = 0x20,
   BACKLIGHT_7 = 0x40,
   BACKLIGHT_8 = 0x80,
} Backlight;

/*!
  Voltage definitions for LDO1, LD03 and LD04. These are used in the 
  SetLDO1Voltage function. Voltage levels may be set to 
  OFF and between 1.5v and 3.3v.
 */
typedef enum
{
   LDO_OFF           = 0x00,
   LDO_VOLTAGE_3_3   = 0x01,
   LDO_VOLTAGE_3_2   = 0x02,
   LDO_VOLTAGE_3_1   = 0x03,
   LDO_VOLTAGE_3_0   = 0x04,
   LDO_VOLTAGE_2_9   = 0x05,
   LDO_VOLTAGE_2_8   = 0x06,
   LDO_VOLTAGE_2_7   = 0x07,
   LDO_VOLTAGE_2_6   = 0x08,
   LDO_VOLTAGE_2_5   = 0x09,
   LDO_VOLTAGE_2_4   = 0x0A,
   LDO_VOLTAGE_2_2   = 0x0B,
   LDO_VOLTAGE_1_8   = 0x0C,
   LDO_VOLTAGE_1_7   = 0x0D,
   LDO_VOLTAGE_1_6   = 0x0E,
   LDO_VOLTAGE_1_5   = 0x0F
} LDO_Voltage;

/*!
  Voltage definitions for LDO2. These are used in the 
  SetLDO2Voltage function. Voltage levels may be set to 
  OFF and between 1.2v and 1.8v.
 */
typedef enum
{
   LDO2_OFF           = 0x00,
   LDO2_VOLTAGE_1_8   = 0x01,
   LDO2_VOLTAGE_1_7   = 0x02,
   LDO2_VOLTAGE_1_6   = 0x03,
   LDO2_VOLTAGE_1_5   = 0x04,
   LDO2_VOLTAGE_1_4   = 0x05,
   LDO2_VOLTAGE_1_3   = 0x06,
   LDO2_VOLTAGE_1_2   = 0x07
} LDO2_Voltage;

//=========================================================
//=========================================================
// General Interface
//=========================================================
//=========================================================
/************************************************************
 sc668_enable_chip                                                                         
param
	on:0-disable chip  
		 1-enable chip    	
 return 
		0:chip disable 
		1:chip enable                                                                        
************************************************************/
int sc668_enable_chip(int on, const char * device_name);
/************************************************************
 sc688_get_chip_state                                                                         
 return 
		0:chip disable 
		1:chip enable                                                                        
************************************************************/
int sc668_get_chip_state(void);

/*!
The sc668_write_register function is the lowest level API provided. All functionality 
of the chip may be accessed through this single function which writes data to 
the specified register. The function verifies that the register is within the 
valid range before attempting the write. In addition, all reserved bits are 
masked off before the data is transferred.

@param
	address	The register to write to. This must be one of the following values:
       BACKLIGHT_1TO6_ENABLE         
       BACKLIGHT_7TO8_AND_BANK_ENABLE
       BANK1_CONTROL                 
       BANK2_CONTROL                 
       BANK3_CONTROL                 
       BANK4_CONTROL                 
       BANK1_SETTINGS                
       BANK2_SETTINGS                
       BANK3_SETTINGS                
       BANK4_SETTINGS                
       LDO1_CONTROL                  
       LDO2_CONTROL                  
       LDO3_CONTROL                  
       LDO4_CONTROL                  
       LIGHTING_ASSIGNMENTS          
       EFFECT_RATE_SETTINGS          
       EFFECT_RATE1_TIME             
       EFFECT_RATE2_TIME             
       ADC_ENABLE                    
       ADC_OUTPUT                    
       ADC_RISE                      
       ADC_FALL                      
       ADP_OLE_CONTROLS              
	data	The data to be written to the register.

@return
	SC668_SUCCESS			The data has been written to the register of the SC668.
	SC668_INVALID_ADDRESS	The register is outside of the valid range and the data will not be sent to the SC668.

 */
SC668_ResultCode sc668_write_register(SC668_Register address, unsigned char data, const char * device_name);


/*!
The sc668_read_register function is the corresponding read function.  Any of the available 
registers can be read using this function.  The STATUS register may be the most 
useful and values for bit masking are provided (see the example).  
To read single status bits, the GetStatus function may be more convenient.

@param
	address	The register to write to. This must be one of the following values:
       BACKLIGHT_1TO6_ENABLE            
       BACKLIGHT_7TO8_AND_BANK_ENABLE	
       BANK1_CONTROL                 	
       BANK2_CONTROL                 	
       BANK3_CONTROL                 	
       BANK4_CONTROL                 	
       BANK1_SETTINGS                	
       BANK2_SETTINGS                	
       BANK3_SETTINGS                	
       BANK4_SETTINGS                	
       LDO1_CONTROL                  
       LDO2_CONTROL                  	
       LDO3_CONTROL                  	
       LDO4_CONTROL                  	
       LIGHTING_ASSIGNMENTS          	
       EFFECT_RATE_SETTINGS          	
       EFFECT_RATE1_TIME             	
       EFFECT_RATE2_TIME             	
       ADC_ENABLE                    	
       ADC_OUTPUT                    	
       ADC_RISE                      	
       ADC_FALL                      	
       ADP_OLE_CONTROLS           
          
@return
	An unsigned char containing the current value of the given register.  
	See the SC668 Datasheet for more details on these registers.

 */
unsigned char sc668_read_register(SC668_Register address, const char * device_name);

/* 
  The sc668_enable_backlights function is used to enable a subset of backlights 
  without affecting the state of the others. 

  @param backlights    The bits corresponding to the backlights which are to be 
                       enabled are ORed together to create the backlights parameter. 
                       The state of all backlights not included in this parameter are 
                       unaffected. This parameter must be set to the bitwise ORed results 
                       of none, any, or all of the Backlight values. Use the following defines.
         - BACKLIGHT_8                   
         - BACKLIGHT_7         
         - BACKLIGHT_6
         - BACKLIGHT_5
         - BACKLIGHT_4
         - BACKLIGHT_3
         - BACKLIGHT_2
         - BACKLIGHT_1

  @return SC668_ResultCode 
          - #SC668_SUCCESS The backlight settings have been successfully changed.
          - #SC668_I2C_ERROR   read/write I2C fail
 */
SC668_ResultCode sc668_enable_backlights(unsigned char backlights, const char * device_name);

/*! 
  The sc668_disable_backlights function is used to disable a subset of backlights 
  without affecting the state of the others. 
  @param backlight1to6 The bits corresponding to the backlights which are to be 
                       disabled are ORed together to create the backlights parameter. 
                       The state of all backlights not included in this parameter are 
                       unaffected. This parameter must be set to the bitwise ORed results 
                       of none, any, or all of the Backlight values. Use the following defines.
         - BACKLIGHT_8
         - BACKLIGHT_7                    
         - BACKLIGHT_6
         - BACKLIGHT_5
         - BACKLIGHT_4
         - BACKLIGHT_3
         - BACKLIGHT_2
         - BACKLIGHT_1

  @return SC668_ResultCode 
          - #SC668_SUCCESS The backlight settings have been successfully changed.
          - #SC668_I2C_ERROR   read/write I2C fail
 */
SC668_ResultCode sc668_disable_backlights(unsigned char backlights, const char * device_name);

/* 
  The sc668_enable_Banks function is used to enable a subset of banks.

  @param bank    The bits corresponding to the banks which are to be 
                 enabled are ORed together to create the banks parameter. 
                 The state of all banks not included in this parameter are 
                 unaffected. This parameter must be set to the bitwise ORed results 
                 of none, any, or all of the Bank values. Use the following defines.
          - BANK_4_ENABLE
          - BANK_3_ENABLE
          - BANK_2_ENABLE
          - BANK_1_ENABLE
  @return SC668_ResultCode 
          - #SC668_SUCCESS The banks settings have been successfully changed.
          - #SC668_RESERVED_BITS_EXCLUDED The bank parameter contains bits which 
                                            should not be sent. These bits have been masked 
                                            off and the remaining bits have been used.
          - #SC668_I2C_ERROR   read/write I2C fail
 */
SC668_ResultCode sc668_enable_Banks(unsigned char bank, const char * device_name);

/*! 
  The sc668_disable_banks function is used to disable a subset of banks. 

  @param bank    The bits corresponding to the backlights which are to be 
                 disabled are ORed together to create the backlights parameter. 
                 The state of all backlights not included in this parameter are 
                 unaffected. This parameter must be set to the bitwise ORed results 
                 of none, any, or all of the Backlight values. Use the following defines.
          - BANK_4_ENABLE
          - BANK_3_ENABLE
          - BANK_2_ENABLE
          - BANK_1_ENABLE
  @return SC668_ResultCode 
          - #SC668_SUCCESS The bank settings have been successfully changed.
          - #SC668_RESERVED_BITS_EXCLUDED The banks parameter contains bits which 
                                            should not be sent. These bits have been masked 
                                            off and the remaining bits have been used.
          - #SC668_I2C_ERROR   read/write I2C fail
 */
SC668_ResultCode sc668_disable_banks(unsigned char bank, const char * device_name);

/* 
  The sc668_set_backLight_currentAndEffect function is used to set the backlight current and target 
  current as well as enable fade, blink, breathe or no effect on a specified bank.
  @param bank Specified bank used for enabling current, target current and light effect. Use the following defines. 
     - BANK_1
     - BANK_2
     - BANK_3
     - BANK_4
  @param BL_current Sets the backlight current. Use the following defines. 
     - BACKLIGHT_CURRENT_0_0                              
     - BACKLIGHT_CURRENT_0_0_5
     - BACKLIGHT_CURRENT_0_1
     - BACKLIGHT_CURRENT_0_2
     - BACKLIGHT_CURRENT_0_5
     - BACKLIGHT_CURRENT_1_0
     - BACKLIGHT_CURRENT_1_5 
     - BACKLIGHT_CURRENT_2_0
     - BACKLIGHT_CURRENT_2_5
     - BACKLIGHT_CURRENT_3_0 
     - BACKLIGHT_CURRENT_3_5
     - BACKLIGHT_CURRENT_4_0
     - BACKLIGHT_CURRENT_4_5
     - BACKLIGHT_CURRENT_5_0
     - BACKLIGHT_CURRENT_6_0
     - BACKLIGHT_CURRENT_7_0 
     - BACKLIGHT_CURRENT_8_0
     - BACKLIGHT_CURRENT_9_0
     - BACKLIGHT_CURRENT_10_0
     - BACKLIGHT_CURRENT_11_0
     - BACKLIGHT_CURRENT_12_0
     - BACKLIGHT_CURRENT_13_0
     - BACKLIGHT_CURRENT_14_0
     - BACKLIGHT_CURRENT_15_0
     - BACKLIGHT_CURRENT_16_0
     - BACKLIGHT_CURRENT_17_0
     - BACKLIGHT_CURRENT_18_0
     - BACKLIGHT_CURRENT_19_0
     - BACKLIGHT_CURRENT_20_0
     - BACKLIGHT_CURRENT_21_0
     - BACKLIGHT_CURRENT_23_0
     - BACKLIGHT_CURRENT_25_0 
  @param targetCurrent Set targeted backlight current. Use same values as for the current variable.
  @param effect Enable Fade, Breathe, Blink or no effect
     - ENABLE_FADE
     - ENABLE_BREATHE
     - ENABLE_BLINK
     - NO_EFFECT
  @return SC668_ResultCode 
          - #SC668_SUCCESS The settings have been successfully changed.
          - #SC668_INVALID_PARAMETER The bank, current, or targetCurrent parameter contains invalid bits. The register has not been changed.
          - #SC668_BUFFER_ERROR Returned only in an SCHEDULED_MODE system when a 
                                  buffer overflow has occurred.
 */
SC668_ResultCode sc668_set_backLight_currentAndEffect(BankType bank, unsigned char BL_current, unsigned char targetCurrent, unsigned char effect, const char * device_name);


//==============================================================================
//==============================================================================
// LDO Interface                                                                
//==============================================================================
//==============================================================================


/************************************************************************/
/*The sc668_LDO_setting function sets the voltage output by LDO1~LDO4. The LDO may be 				*/
/*set between 1.5 volts and 3.3 volts or turned off.                            		                                       */
/*param                                                                                                    							       */
/*		num : 1 : LDO1, 2 : LDO2, 3 : LDO3, 4 : LDO4                        								                     */
/*		command : voltage settings --- LDO_Voltage   
 - #LDO_OFF
         - #LDO_VOLTAGE_3_3
         - #LDO_VOLTAGE_3_2
         - #LDO_VOLTAGE_3_1
         - #LDO_VOLTAGE_3_0
         - #LDO_VOLTAGE_2_9
         - #LDO_VOLTAGE_2_8
         - #LDO_VOLTAGE_2_7
         - #LDO_VOLTAGE_2_6
         - #LDO_VOLTAGE_2_5
         - #LDO_VOLTAGE_2_4
         - #LDO_VOLTAGE_2_2
         - #LDO_VOLTAGE_1_8
         - #LDO_VOLTAGE_1_7
         - #LDO_VOLTAGE_1_6
         - #LDO_VOLTAGE_1_5                                                */
/*		device_name : dev->name                                                                            */ 
/*	return                                                                                                             */
/*		SC668_SUCCESS:setting ok                                                                          */
/*		SC668_INVALID_PARAMETER:voltage setting wrong                                        */
/*		SC668_I2C_ERROR:I2C command fail                                                                            */
/*************************************************************/
SC668_ResultCode sc668_LDO_setting(int num, u8 command, const char * device_name);

/*! 
   The SetLDO2Voltage function sets the voltage output by LDO2. The LDO may be 
   set in .1 volt increments between 1.2 volts and 1.8 volts or turned off. 

  @param voltage Sets the voltage of LDO2.
         - #LDO2_OFF
         - #LDO2_VOLTAGE_1_8
         - #LDO2_VOLTAGE_1_7
         - #LDO2_VOLTAGE_1_6
         - #LDO2_VOLTAGE_1_5
         - #LDO2_VOLTAGE_1_4 
         - #LDO2_VOLTAGE_1_3
         - #LDO2_VOLTAGE_1_2
  @return SC668_ResultCode 
         - #SC668_SUCCESS The LDO settings have been successfully changed.
         - #SC668_INVALID_PARAMETER The voltage parameter is outside of the valid 
                                      range. The LDO voltage has not been changed.
         - #SC668_BUFFER_ERROR Returned only in a SCHEDULED_MODE 
                                 system when a buffer overflow has occurred.
 */


/*! 
  The sc668_set_light_assignments function assigns individual lights to four different banks 
  and then assigns each bank to a group.

  @param bankAssignment Assigns lights to banks. 
         - BANK1_BL1_BL8 (Assignes backlights 1 to 8 to bank 1)                                  
         - BANK1_BL2_BL8_BANK2_BL1 (Assigns backlights 2 to 8 to bank 1 and backlight 1 to bank 2)                        
         - BANK1_BL3_BL8_BANK2_BL1_BL2                     
         - BANK1_BL3_BL8_BANK2_BL2_BANK3_BL1               
         - BANK1_BL4_BL8_BANK2_BL1_BL3                     
         - BANK1_BL4_BL8_BANK2_BL3_BANK3_BL2_BANK4_BL1     
         - BANK1_BL5_BL8_BANK2_BL3_BL4_BANK3_BL2_BANK4_BL1 
         - BANK1_BL6_BL8_BANK2_BL3_BL5_BANK3_BL2_BANK4_BL1 

  @param groupAssignment Assigns banks to two different groups. 
         - GROUP1_BK1_GROUP2_BK2_BK3_BK4   (Assigns Bank 1 to Group 1 and Bank 2 to Bank 4 to Group 2)
         - GROUP1_BK1_BK2_GROUP2_BK3_BK4
         - GROUP1_BK1_BK2_BK3_GROUP2_BK4
         - GROUP1_BK1_BK2_BK3_BK4

 @return SC668_ResultCode 
         - #SC668_SUCCESS The light assignement settings have been successfully changed.
         - #SC668_INVALID_PARAMETER The bankAssignment and/or the groupAssignment parameter is outside of the valid 
                                      range. The light assignment has not been changed.
         - #SC668_I2C_ERROR   read/write I2C fail
 */
SC668_ResultCode sc668_set_light_assignments(unsigned char bankAssignment, unsigned char groupAssignment, const char * device_name);

/*! 
  The sc668_set_effect_rate function sets light effect breathe and fade rates for group 1 and group 2 lights

  @param effectGrp1 Sets breathe and fade rate on group 1 assigned lights. Use the following defines.  
         - BREATH_0_FADE_0
         - BREATH_4_FADE_1
         - BREATH_8_FADE_2
         - BREATH_16_FADE_4
         - BREATH_24_FADE_6
         - BREATH_32_FADE_8
         - BREATH_48_FADE_12
         - BREATH_64_FADE_16
  @param effectGrp2 Sets breathe and fade rate on group 2 assigned lights. Use same defines as in effectGrp1. 
  @return SC668_ResultCode 
         - #SC668_SUCCESS The light assignement settings have been successfully changed.
         - #SC668_INVALID_PARAMETER The bankAssignment and/or the groupAssignment parameter is outside of the valid 
                                      range. The light assignment has not been changed.
         - #SC668_I2C_ERROR   read/write I2C fail
 */
SC668_ResultCode sc668_set_effect_rate(unsigned char effectGrp1, unsigned char effectGrp2, const char * device_name);

/*! 
  The SetEffectTimeGrp1 function sets the duration of time that 
  the backlights will stay at the start value and at the target value. 
  This feature is an additional way to customize the fade, breathe, and blink lighting effects, 
  by pausing the brightness at the beginning and at the end of each lighting effect sequence.
  This function only effects group 1 assigned lights

  @param startTime Sets the starting time. Use the following defines.
         - TIME_32
         - TIME_64
         - TIME_256
         - TIME_512
         - TIME_1024
         - TIME_2048
         - TIME_3072
         - TIME_4096
  @param targetTime Sets the target time. Use same values as for starting time. 
  @return SC668_ResultCode 
         - #SC668_SUCCESS The light assignement settings have been successfully changed.
         - #SC668_INVALID_PARAMETER The bankAssignment and/or the groupAssignment parameter is outside of the valid 
                                      range. The light assignment has not been changed.
         - #SC668_I2C_ERROR   read/write I2C fail
 */
SC668_ResultCode sc668_set_effectTime_grp1(unsigned char startTime, unsigned char targetTime, const char * device_name);

/*! 
  The sc668_set_effectTime_grp2 function sets the duration of time that 
  the backlights will stay at the start value and at the target value. 
  This feature is an additional way to customize the fade, breathe, and blink lighting effects, 
  by pausing the brightness at the beginning and at the end of each lighting effect sequence.
  This function only effects group 2 assigned lights

  @param startTime Sets the starting time
         - TIME_32
         - TIME_64
         - TIME_256
         - TIME_512
         - TIME_1024
         - TIME_2048
         - TIME_3072
         - TIME_4096
  @param targetTime Sets the target time. Use same values as for starting time. 
 @return SC668_ResultCode 
         - #SC668_SUCCESS The light assignement settings have been successfully changed.
         - #SC668_INVALID_PARAMETER The bankAssignment and/or the groupAssignment parameter is outside of the valid 
                                      range. The light assignment has not been changed.
         - #SC668_I2C_ERROR   read/write I2C fail
 */
SC668_ResultCode sc668_set_effectTime_grp2(unsigned char startTime, unsigned char targetTime, const char * device_name);

#endif
