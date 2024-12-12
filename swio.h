#pragma once
#include <ch32v003fun.h>
#define MAX_IN_TIMEOUT 1000
#define T1COEFF 2

#define DMCONTROL      0x10
#define DMSTATUS       0x11
#define DMHARTINFO     0x12
#define DMABSTRACTCS   0x16
#define DMCOMMAND      0x17
#define DMABSTRACTAUTO 0x18
#define DMPROGBUF0     0x20
#define DMPROGBUF1     0x21
#define DMPROGBUF2     0x22
#define DMPROGBUF3     0x23
#define DMPROGBUF4     0x24
#define DMPROGBUF5     0x25
#define DMPROGBUF6     0x26
#define DMPROGBUF7     0x27

#define DMCPBR       0x7C
#define DMCFGR       0x7D
#define DMSHDWCFGR   0x7E

static inline void PrecDelay( int delay )
{
	asm volatile(
"1:	addi %[delay], %[delay], -1\n"
"bne %[delay], x0, 1b\n" :[delay]"+r"(delay)  );
}

static inline void Send1Bit(uint8_t t1coeff)
{
	// Low for a nominal period of time.
	// High for a nominal period of time.

	AFIO->PCFR1 |= AFIO_PCFR1_SWJ_CFG_DISABLE;
	funDigitalWrite(PD1, 0);
  AFIO->PCFR1 &= ~(AFIO_PCFR1_SWJ_CFG_DISABLE);
  PrecDelay(t1coeff);
  
	AFIO->PCFR1 |= AFIO_PCFR1_SWJ_CFG_DISABLE;
	funDigitalWrite(PD1, 1);
  AFIO->PCFR1 &= ~(AFIO_PCFR1_SWJ_CFG_DISABLE);
	PrecDelay(t1coeff);
}

static inline void Send0Bit(uint8_t t1coeff)
{
	// Low for a LONG period of time.
	// High for a nominal period of time.
	AFIO->PCFR1 |= AFIO_PCFR1_SWJ_CFG_DISABLE;
	funDigitalWrite(PD1, 0);
  AFIO->PCFR1 &= ~(AFIO_PCFR1_SWJ_CFG_DISABLE);
	PrecDelay(t1coeff*4);
	AFIO->PCFR1 |= AFIO_PCFR1_SWJ_CFG_DISABLE;
	funDigitalWrite(PD1, 1);
  AFIO->PCFR1 &= ~(AFIO_PCFR1_SWJ_CFG_DISABLE);
	PrecDelay(t1coeff);
}

// returns 0 if 0
// returns 1 if 1
// returns 2 if timeout.
static inline int ReadBit()
{
	// Drive low, very briefly.  Let drift high.
	// See if CH32V003 is holding low.

	int timeout = 0;
	int ret = 0;
	// int medwait = t1coeff * 2;
	funDigitalWrite(PD1, 0);
	
	asm volatile( "nop" );
	// GPIO_ENABLE_CLEAR = pinmask;
  funPinMode(PD1, GPIO_CNF_IN_FLOATING);
	
	funDigitalWrite(PD1, 1);

#ifdef R_GLITCH_HIGH
	int halfwait = t1coeff / 2;
	PrecDelay( halfwait );
	GPIO_ENABLE_SET = pinmask;
	GPIO_ENABLE_CLEAR = pinmask;
	PrecDelay( halfwait );
#else
	asm volatile( "nop" );asm volatile( "nop" );
#endif
	// ret = GPIO_IN;
	ret = funDigitalRead(PD1);

#ifdef R_GLITCH_HIGH
	if( !(ret & pinmask) )
	{ret
		// Wait if still low.
		PrecDelay( medwait );
		GPIO_ENABLE_SET = pinmask;
		GPIO_ENABLE_CLEAR = pinmask;
	}
#endif
	for( timeout = 0; timeout < MAX_IN_TIMEOUT; timeout++ )
	{
		if(funDigitalRead(PD1))
		{
			funPinMode(PD1, GPIO_Speed_50MHz | GPIO_CNF_OUT_PP);
			// RV_WRITE_CSR(CSR_GPIO_OEN_USER, 1);
			// int fastwait = t1coeff / 2;
			// PrecDelay( fastwait );
			return !!(ret);
			// return !!(ret);
		}
	}
	
	// Force high anyway so, though hazarded, we can still move along.
	funPinMode(PD1, GPIO_Speed_50MHz | GPIO_CNF_OUT_PP);
	// RV_WRITE_CSR(CSR_GPIO_OEN_USER, 1);
	return 2;
}

static void MCFWriteReg32(uint8_t command, uint32_t value, uint8_t t1coeff)
{

 	funDigitalWrite(PD1, 1);
	// funPinMode(PD1, GPIO_Speed_50MHz | GPIO_CNF_OUT_PP);

	__disable_irq();
	Send1Bit(t1coeff);
	uint32_t mask;
	for( mask = 1<<6; mask; mask >>= 1 )
	{
		if( command & mask )
			Send1Bit(t1coeff);
		else
			Send0Bit(t1coeff);
	}
	Send1Bit(t1coeff);
	for( mask = 1<<31; mask; mask >>= 1 )
	{
		if( value & mask )
			Send1Bit(t1coeff);
		else
			Send0Bit(t1coeff);
	}
  __enable_irq();
	Delay_Ms(8); // Sometimes 2 is too short.
}

// returns 0 if no error, otherwise error.
// static int MCFReadReg32(uint8_t command, uint32_t * value )
// {

//  	funDigitalWrite(PD1, 1);
// 	funPinMode(PD1, GPIO_Speed_50MHz | GPIO_CNF_OUT_PP);

// 	__disable_irq();
// 	Send1Bit();
// 	int i;
// 	uint32_t mask;
// 	for( mask = 1<<6; mask; mask >>= 1 )
// 	{
// 		if( command & mask )
// 			Send1Bit();
// 		else
// 			Send0Bit();
// 	}
// 	Send0Bit();
// 	uint32_t rval = 0;
// 	for( i = 0; i < 32; i++ )
// 	{
// 		rval <<= 1;
// 		int r = ReadBit();
// 		if( r == 1 )
// 			rval |= 1;
// 		if( r == 2 )
// 		{
// 			__enable_irq();
// 			return -1;
// 		}
// 	}
// 	*value = rval;
// 	__enable_irq();
// 	Delay_Ms(8); // Sometimes 2 is too short.
// 	return 0;
// }