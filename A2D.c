/**************************************************************************************************
Authours:				Craig Comberbach
Target Hardware:		PIC24F
Chip resources used:	A2D Module
Code assumptions:
Purpose:				Read the Thermistor inputs

Version History:
v0.0.0	2013-07-18  Craig Comberbach
	Compiler: C30 v3.31	IDE: MPLABx 1.80	Tool: RealICE	Computer: Intel Xeon CPU 3.07 GHz, 6 GB RAM, Windows 7 64 bit Professional SP1
	First version
**************************************************************************************************/
/*************    Header Files    ***************/
#include "Config.h"
#include "A2D.h"

/************* Semantic Versioning***************/
#ifndef A2D_LIBRARY
	#error "You need to include the A2D library for this code to compile"
#elif A2D_MAJOR != 0
	#warning "A2D.c has had a change that loses some previously supported functionality"
#elif A2D_MINOR != 1
	#warning "A2D.c has new features that this code may benefit from"
#elif A2D_PATCH != 0
	#warning "A2D.c has had a bug fix, you should check to see that we weren't relying on a bug for functionality"
#endif

/************Arbitrary Functionality*************/
/*************   Magic  Numbers   ***************/
/*************    Enumeration     ***************/
/***********State Machine Definitions*************/
/*************  Global Variables  ***************/
/*************Function  Prototypes***************/
/************* Device Definitions ***************/
/************* Module Definitions ***************/
/************* Other  Definitions ***************/

void Initialize_A2D(void)
{
	#if defined __PIC24F08KL200__
		//Analog Input Configuration Register
		ANCFGbits.VBGEN		= 0;//0 = Band gap is not available to the A/D Converter

		//A/D Input Scan Select Register
		AD1CSSLbits.CSSL0	= 0;//0 = Analog channel omitted from input scan
		AD1CSSLbits.CSSL1	= 0;//0 = Analog channel omitted from input scan
//		AD1CSSLbits.CSSL2	= Unimplemented on this chip
//		AD1CSSLbits.CSSL3	= Unimplemented on this chip
//		AD1CSSLbits.CSSL4	= Unimplemented on this chip
//		AD1CSSLbits.CSSL6	= Unimplemented on this chip (But not specified as such)
//		AD1CSSLbits.CSSL7	= Unimplemented on this chip (But not specified as such)
//		AD1CSSLbits.CSSL8	= Unimplemented on this chip (But not specified as such)
		AD1CSSLbits.CSSL9	= 0;//0 = Analog channel omitted from input scan
		AD1CSSLbits.CSSL10	= 0;//0 = Analog channel omitted from input scan
//		AD1CSSLbits.CSSL11	= Unimplemented on this chip
//		AD1CSSLbits.CSSL12	= Unimplemented on this chip
		AD1CSSLbits.CSSL13	= 1;//1 = Corresponding analog channel selected for input scan
		AD1CSSLbits.CSSL14	= 1;//1 = Corresponding analog channel selected for input scan
		AD1CSSLbits.CSSL15	= 0;//0 = Analog channel omitted from input scan

		//A/D Input Select Register
		AD1CHSbits.CH0SA	= 6;//0110 = Internal band gap reference (VBG)
		AD1CHSbits.CH0NA	= 0;//0 = Channel 0 negative input is VR
		AD1CHSbits.CH0SB	= 6;//0110 = Internal band gap reference (VBG)
		AD1CHSbits.CH0NB	= 0;//0 = Channel 0 negative input is VR-

		//A/D Control Register 3
//		AD1CON3bits.ADCS	= Using internal RC clock, not main clock
		AD1CON3bits.SAMC	= 13;//01101 = 13 TAD(TAD is the clock period)
		AD1CON3bits.PUMPEN	= 0;//0 = Charge pump for switches is disabled
		AD1CON3bits.EXTSAM	= 0;//0 = A/D is finished sampling
		AD1CON3bits.ADRC	= 1;//1 = A/D internal RC clock

		//A/D Control Register 2
		AD1CON2bits.ALTS	= 0;//0 = Always uses MUX A input multiplexer settings
		AD1CON2bits.SMPI	= 15;//1111= Interrupts at the completion of conversion for each 16th sample/convert sequence
		AD1CON2bits.CSCNA	= 1;//1 = Scan inputs
		AD1CON2bits.OFFCAL	= 0;//0 = Conversions to get the actual input value
		AD1CON2bits.VCFG	= 0;//00 = Vr+ = AVdd, VR- = AVss

		//A/D Control Register 1
//		AD1CON1bits.DONE	= Not used
//		AD1CON1bits.SAMP	= Not used
		AD1CON1bits.ASAM	= 1;//1 = Sampling begins immediately after the last conversion completes; SAMP bit is auto-set
		AD1CON1bits.SSRC	= 7;//111 = Internal counter ends sampling and starts conversion (auto-convert)
		AD1CON1bits.FORM	= 0;//00 = Integer (0000 00dd dddd dddd)
		AD1CON1bits.ADSIDL	= 0;//0 = Continue module operation in Idle mode
		AD1CON1bits.ADON	= 1;//1 = A/D Converter module is operating

	#endif

	return;
}

void A2D_Routine(void)
{
	int A2DValue,samplecount;
	int *ADCPtr;

	while (1)
	{
		A2DValue=0;		   // clear the A2Dvalue
		ADCPtr=&ADC1BUF0;  //intialize the ADC1Buf
		IFS0bits.AD1IF = 0;	//cleat the ADC interupt flag
							//auto start sampling set up in Initialize_A2D
		while (!IFS0bits.AD1IF);//wait for conversion to be done

		AD1CON1bits.ASAM= 0;
		for (samplecount=0;samplecount < 16; samplecount++)
		{
			A2DValue = A2DValue + *ADCPtr++;
		}
		A2DValue = A2DValue >> 4;
}
//		A2DValue = ADC1BUF0;// conversion is complete get ADC value from buffer
//		IFS0bits.AD1IF=0;//clear AD1IF
	


	//return;
}
