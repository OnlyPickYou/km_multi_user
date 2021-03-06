
#pragma once

#include "../common/types.h"
#include "../common/bit.h"
#include "../common/utility.h"
#include "../mcu/compiler.h"
#include "../mcu/register.h"
#include "gpio_default_8510.h"
//#include "register_8366.h"

enum{
		GPIO_GP1  = BIT(0),
		GPIO_GP2  = BIT(1),
		GPIO_GP3  = BIT(2),
		GPIO_GP4  = BIT(3),
		GPIO_GP5  = BIT(4),
		GPIO_GP6  = BIT(5), 	//GPIO_DP = GPIO_GP6,
		GPIO_GP7  = BIT(6),
		GPIO_SWS  = BIT(7),


		GPIO_MSCN = 0x100 | BIT(0),
		GPIO_MCLK = 0x100 | BIT(1),
		GPIO_MSDO = 0x100 | BIT(2),
		GPIO_MSDI = 0x100 | BIT(3),
};

#define reg_gpio_in(i)			REG_ADDR8(0x580+((i>>8)<<3))
#define reg_gpio_ie(i)			REG_ADDR8(0x581+((i>>8)<<3))
#define reg_gpio_oen(i)			REG_ADDR8(0x582+((i>>8)<<3))
#define reg_gpio_out(i)			REG_ADDR8(0x583+((i>>8)<<3))
#define reg_gpio_pol(i)			REG_ADDR8(0x584+((i>>8)<<3))
#define reg_gpio_ds(i)			REG_ADDR8(0x585+((i>>8)<<3))
#define reg_gpio_gpio_func(i)	REG_ADDR8(0x586+((i>>8)<<3))
#define reg_gpio_irq_en(i)		REG_ADDR8(0x587+((i>>8)<<3))

//GPIO_GP5��GPIO_GP6��GPIO_GP7 support 1M_PULLUP and 100K_PULLDOWN
#define	GPIO567_1M_PULLUP				( analog_write(0x02, (analog_read(0x02)&0xc0) | BIT(5)) )
#define GPIO567_100K_PULLDOWN			( analog_write(0x02, (analog_read(0x02)&0xc0) | BIT(4)) )
#define GPIO567_PULL_DISABLE			( analog_write(0x02, analog_read(0x02)&0xc0 )

//MSDI only support 1M pull up
#define MSDI_1M_PULLUP_ENABLE			( analog_write(0x02,( analog_read(0x02)& (~BIT(6)))) )
#define MSDI_1M_PULLUP_DISABLE			( analog_write(0x02,( analog_read(0x02) | BIT(6))) )

#define MCLK_GPIO34_PULLUP_ENABLE			( analog_write(0x02,( analog_read(0x02)& (~BIT(6)))) )	//MCLK, GP3 30k pull up, GP4 1M pull up
#define MCLK_GPIO34_PULLUP_DISABLE			( analog_write(0x02,( analog_read(0x02) | BIT(6))) )
/*******************************************************************************

*******************************************************************************/

static inline int gpio_is_output_en(u32 pin){
	return !BM_IS_SET(reg_gpio_oen(pin), pin & 0xff);
}

static inline int gpio_is_input_en(u32 pin){
	return BM_IS_SET(reg_gpio_ie(pin), pin & 0xff);
}

#if 1

static inline void gpio_set_output_en(u32 pin, u32 value){
	u8	bit = pin & 0xff;
	if(!value){
		BM_SET(reg_gpio_oen(pin), bit);
	}else{
		BM_CLR(reg_gpio_oen(pin), bit);
	}
}

#else

static inline void gpio_set_output_en(u32 pin, u32 value){
	u8	bit = pin & 0xff;
	if(!value){
		BM_SET(reg_gpio_oen(pin), bit);
	}else{
		BM_CLR(reg_gpio_oen(pin), bit);
	}
}

#endif

static inline void gpio_set_input_en(u32 pin, u32 value){
	u8	bit = pin & 0xff;
	if(value){
		BM_SET(reg_gpio_ie(pin), bit);
	}else{
		BM_CLR(reg_gpio_ie(pin), bit);
	}
}

static inline void gpio_set_data_strength(u32 pin, u32 value){
	u8	bit = pin & 0xff;
	if(value){
		BM_SET(reg_gpio_ds(pin), bit);
	}else{
		BM_CLR(reg_gpio_ds(pin), bit);
	}
}



static inline void gpio_write(u32 pin, u32 value){
	u8	bit = pin & 0xff;
	if(value){
		BM_SET(reg_gpio_out(pin), bit);
	}else{
		BM_CLR(reg_gpio_out(pin), bit);
	}
}

static inline u32 gpio_read(u32 pin){
	return BM_IS_SET(reg_gpio_in(pin), pin & 0xff);
}

void gpio_write_in_ram(u32 pin, u32 value);


/*******************************************************************************

*******************************************************************************/
//irq default m2
static inline void gpio_set_interrupt(u32 pins, u32 falling){
#if 0
#if(CHIP_8366_A1) //GP4 GP5 GP6 SWS shift form  MSDO MSDI MCLK MSCN
	if(pins & 0xf000){
		pins = (pins&0x600fff) | (pins&0xf000)>>12;
	}
#endif

	if(falling){
		BM_SET(reg_gpio_pol, pins);
	}else{
		BM_CLR(reg_gpio_pol, pins);
	}
	BM_SET(reg_gpio_2risc2, pins);
#endif
}

static inline void gpio_clr_interrupt(u32 pins){
#if 0

#if(CHIP_8366_A1) //GP4 GP5 GP6 SWS shift form  MSDO MSDI MCLK MSCN
	if(pins & 0xf000){
		pins = (pins&0x600fff) | (pins&0xf000)>>12;
	}
#endif

	BM_CLR(reg_gpio_2risc2, pins);
#endif
}


static inline void gpio_set_interrupt_and_wakeup(u32 pins, u32 level){
#if 0

#if(CHIP_8366_A1) //GP4 GP5 GP6 SWS shift form  MSDO MSDI MCLK MSCN
	if(pins & 0xf000){
		pins = (pins&0x600fff) | (pins&0xf000)>>12;
	}
#endif

	if(level){  //1:rising edge irq��   high level wakeup
		        //0:falling edge irq��low level wakeup
		BM_CLR(reg_gpio_pol, pins);
	}else{
		BM_SET(reg_gpio_pol, pins);
	}

	BM_SET(reg_gpio_2risc2, pins);
	reg_gpio_wakeup_en |= pins;
#endif
}

static inline void gpio_clr_interrupt_and_wakeup(u32 pins){
#if 0

#if(CHIP_8366_A1) //GP4 GP5 GP6 SWS shift form  MSDO MSDI MCLK MSCN
	if(pins & 0xf000){
		pins = (pins&0x600fff) | (pins&0xf000)>>12;
	}
#endif

	BM_CLR(reg_gpio_2risc2, pins);
	reg_gpio_wakeup_en &= ~pins;
#endif
}


static inline void gpio_set_interrupt_pol(u32 pins, u32 falling){
#if 0

#if(CHIP_8366_A1) //GP4 GP5 GP6 SWS shift form  MSDO MSDI MCLK MSCN
	if(pins & 0xf000){
		pins = (pins&0x600fff) | (pins&0xf000)>>12;
	}
#endif

	if(falling){
		BM_SET(reg_gpio_pol, pins);
	}else{
		BM_CLR(reg_gpio_pol, pins);
	}
#endif
}

//enable interrupt wheel interrupt and wakeup
static inline void gpio_enable_irq_wakeup_pin(u32 pins, u32 levels){

}


static inline void gpio_enable_wakeup_pin(u32 pins, u32 level, int en){

	u8 bit = 0xff & pins;

	if (level) {   //1:high level wakeup   0:low level wakeup
 		BM_CLR(reg_gpio_pol(pins),bit);		//
	}
	else {
		BM_SET(reg_gpio_pol(pins),bit);
	}

	if (en) {
		BM_SET(reg_gpio_irq_en(pins),bit);
	}
	else {
		BM_CLR(reg_gpio_irq_en(pins),bit);
	}

}

static inline void gpio_pullup_dpdm_internal( u32 dp_dm_pullup ){
    reg_gpio_wakeup_en |= dp_dm_pullup;
}


/*******************************************************************************

*******************************************************************************/


#define GPIO_VALUE(type,pol,n)					(GPIO##n##_##type==(pol)?(1<<((n-1)&0x07)):0)
#define GPIO_FUNC_VALUE(type,pol,func,pos)		(func##_##type==(pol)?(1<<pos):0)


#define GPIO_REG_VALUE_GROUP0(type,pol)		\
 (  GPIO_VALUE(type, pol , 1) | GPIO_VALUE(type, pol , 2) | \
	GPIO_VALUE(type, pol , 3) | GPIO_VALUE(type, pol , 4) | \
	GPIO_VALUE(type, pol , 5) | GPIO_VALUE(type, pol , 6) | \
	GPIO_VALUE(type, pol , 7) | GPIO_FUNC_VALUE(type, pol , SWS, 7) )


#define GPIO_REG_VALUE_GROUP1(type, pol)		\
	( GPIO_FUNC_VALUE(type, pol, MSCN, 0) | GPIO_FUNC_VALUE(type, pol, MCLK, 1) | \
	  GPIO_FUNC_VALUE(type, pol, MSDO, 2) | GPIO_FUNC_VALUE(type, pol, MSDI, 3) )

static inline void gpio_init(void){

	reg_gpio_group0_ie 	 = (u8)GPIO_REG_VALUE_GROUP0(INPUT_ENABLE,1);
	reg_gpio_group0_oen  = (u8)GPIO_REG_VALUE_GROUP0(OUTPUT_ENABLE,0);
	reg_gpio_group0_out  = (u8)GPIO_REG_VALUE_GROUP0(DATA_OUT,1);
	reg_gpio_group0_ds 	 = (u8)GPIO_REG_VALUE_GROUP0(DATA_STRENGTH,1);
	reg_gpio_group0_func = (u8)GPIO_REG_VALUE_GROUP0(FUNC,0);

#if(CAVYM_ROM_CODE_ENABLE)
	reg_gpio_group1_ie 	 = (u8)GPIO_REG_VALUE_GROUP1(INPUT_ENABLE,1);
	reg_gpio_group1_oen  = (u8)GPIO_REG_VALUE_GROUP1(OUTPUT_ENABLE,0);
	reg_gpio_group1_out  = (u8)GPIO_REG_VALUE_GROUP1(DATA_OUT,1);
	reg_gpio_group1_ds 	 = (u8)GPIO_REG_VALUE_GROUP1(DATA_STRENGTH,1);
	reg_gpio_group1_func = (u8)GPIO_REG_VALUE_GROUP1(FUNC,1);		//run in rom, func set as gpio
#else
	reg_gpio_group1_ie 	 = (u8)GPIO_REG_VALUE_GROUP1(INPUT_ENABLE,1);
	reg_gpio_group1_oen  = (u8)GPIO_REG_VALUE_GROUP1(OUTPUT_ENABLE,0);
	reg_gpio_group1_out  = (u8)GPIO_REG_VALUE_GROUP1(DATA_OUT,1);
	reg_gpio_group1_ds 	 = (u8)GPIO_REG_VALUE_GROUP1(DATA_STRENGTH,1);
	reg_gpio_group1_func = (u8)GPIO_REG_VALUE_GROUP1(FUNC,0);
#endif

// did not know addr of GPIO1~GPIO7 pull up or down register
	u8 areg = analog_read (0x02);

	analog_write(0x02, areg | PULLUP_WAKEUP_SRC_GPIO567 << 4 | PULLDOWN_WAKEUP_SRC_GPIO567 << 5 | (!PULLUP_WAKEUP_SRC_MSDI) << 6 );

}


static inline void gpio_set_func(u32 pin, u32 func){
	u8 bit = (u8)(pin & 0xff);

	if(AS_GPIO == func){
		BM_SET(reg_gpio_gpio_func(pin), bit);
	}else{
		BM_CLR(reg_gpio_gpio_func(pin), bit);
	}
}

