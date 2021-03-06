/**************************************************************************************************
Target Hardware:		PIC24F
Chip resources used:	A2D Module & Pins setup as analog inputs
Purpose:				Scan A2D, perform DSP to increase resolution, and format accordingly

Version History:
v1.0.0	2015-01-10  Craig Comberbach
	Compiler: XC16 v1.11	IDE: MPLABx 2.20	Tool: ICD3	Computer: Intel Core2 Quad CPU 2.40 GHz, 5 GB RAM, Windows 7 64 bit Home Premium SP1
	Added full scanning functionality
	Added format/pre/post/finished function pointers to extend capabilities to encompass non-generic situations
	Added DSP based resolution increasing capabilities (As much as 16-bit can be obtained from the 10-bit A2D module)
v0.0.0	2013-07-18  Craig Comberbach
	Compiler: C30 v3.31	IDE: MPLABx 1.80	Tool: RealICE	Computer: Intel Xeon CPU 3.07 GHz, 6 GB RAM, Windows 7 64 bit Professional SP1
	First version
 **************************************************************************************************/
/*************    Header Files    ***************/
#include "Config.h"
#include "A2D.h"

/************* Semantic Versioning***************/
#if A2D_MAJOR != 1
	#error "A2D.c has had a change that loses some previously supported functionality"
#elif A2D_MINOR != 0
	#error "A2D.c has new features that this code may benefit from"
#elif A2D_PATCH != 0
	#error "A2D.c has had a bug fix, you should check to see that we weren't relying on a bug for functionality"
#endif

/************Arbitrary Functionality*************/
/*************   Magic  Numbers   ***************/
#define	NUMBER_OF_CHANNELS	16
#define SCAN_BUFFER_SIZE	16	//Size of the scan buffer

/*************    Enumeration     ***************/
/*************ArbitraryFunctionality*************/
#define MAX_SCAN_QUEUE_SIZE	16	//Max size of the scan queue

/************* Module Definitions ***************/
#define	STOP_SCAN		AD1CON1bits.ASAM=0	//Stops the scanning of channels
#define	START_SCAN		AD1CON1bits.ASAM=1	//Starts the scanning of channels

/************* Other  Definitions ***************/
/*************  Global Variables  ***************/
volatile char scanningQueue[MAX_SCAN_QUEUE_SIZE]; //The list of channels to scan, as well as the order to scan them in
volatile char scanIsComplete;
volatile char currentQueueElement;
struct A2D_Channel_Attributes
{
	unsigned char bitsOfResolutionIncrease;	//The number of bit of increased resolution (Default is 0 which is 10 bits)
	unsigned int samplesRequired;			//The number of samples required to initate an averaging event (includes number of finished samples to make a final averaged sample)
	unsigned int valueForDSP;				//Used as part of calculation to get the current value, includes what is required for resolution increasing as well as averaging
	unsigned int samplesTaken;				//Current number of samples, in multiples of 16 (because samples are taken in bursts of 16)
	int value;								//The most current averaged value, includes resolution increase if used
	int (*formatPointer)(int);				//Used to specify a function that handles the formating of the averaged value
	void (*preFunction)(int);				//Used to specify a function that activates when a channel starts being scanned (eg for setting a switched pin)
	void (*postFunction)(int);				//Used to specify a function that activates when a channel stops being scanned (eg for resetting a switched pin)
	void (*finishedFunction)(int);			//Used to specify a function that activates when a channel finishes being scanned and a new value is created (eg For setting flags for functions that need to run as soon as a value is determined)
	unsigned long sumOfSamples;				//Sum of all the A2D samples before it undergoes DSP/Averaging
} A2D_Channel[NUMBER_OF_CHANNELS];

/*************Function  Prototypes***************/
int Change_To_Analog(int pin);
int Change_To_Digital(int pin);
int Add_To_Scan(int pin);
int Remove_From_Scan(int pin);
int Find_Next_Queue_Element(int channel);
void __attribute__((__interrupt__, auto_psv)) _ADC1Interrupt(void);

void A2D_Routine(void)
{
	int buffer;

	if(scanIsComplete)
	{
		//Reset for next time
		scanIsComplete = 0;

		//Add all of the samples into the raw variable
		for(buffer = 0; buffer < NUMBER_OF_CHANNELS; buffer++)
			A2D_Channel[scanningQueue[currentQueueElement]].sumOfSamples += *(&ADC1BUF0 + buffer);

		//Increment the number of samples read in (we just took 16 samples in a burst, hence the +16)
		A2D_Channel[scanningQueue[currentQueueElement]].samplesTaken += SCAN_BUFFER_SIZE;

		//Check if we are ready for DSP/averaging
		if(A2D_Channel[scanningQueue[currentQueueElement]].samplesTaken >= A2D_Channel[scanningQueue[currentQueueElement]].samplesRequired)
		{
			//Perform the DSP/averaging
			A2D_Channel[scanningQueue[currentQueueElement]].sumOfSamples /= A2D_Channel[scanningQueue[currentQueueElement]].valueForDSP; //Create average DSP value

			//Apply formats externaly if required
			if(*A2D_Channel[scanningQueue[currentQueueElement]].formatPointer == NO_FORMATING)
				A2D_Channel[scanningQueue[currentQueueElement]].value = (int)A2D_Channel[scanningQueue[currentQueueElement]].sumOfSamples;
			else
				A2D_Channel[scanningQueue[currentQueueElement]].value = A2D_Channel[scanningQueue[currentQueueElement]].formatPointer((int)A2D_Channel[scanningQueue[currentQueueElement]].sumOfSamples);

			//House keeping - Reset the counter and storage variable
			A2D_Channel[scanningQueue[currentQueueElement]].samplesTaken = 0;
			A2D_Channel[scanningQueue[currentQueueElement]].sumOfSamples = 0;

			//Perform the new reading function action if applicable
			if(*A2D_Channel[scanningQueue[currentQueueElement]].finishedFunction != NO_FINISHED_FUNCTION)
				A2D_Channel[scanningQueue[currentQueueElement]].finishedFunction(scanningQueue[currentQueueElement]);
		}

		//Remove the current channel from the scanning
		Remove_From_Scan(scanningQueue[currentQueueElement]);

		//Move onto the next channel to be scanned
		currentQueueElement = Find_Next_Queue_Element(currentQueueElement);

		//Add the next channel to the scan
		Add_To_Scan(scanningQueue[currentQueueElement]);
	}

	//Perform the beginning of scan action if applicable
	if(*A2D_Channel[scanningQueue[currentQueueElement]].preFunction != NO_PREFUNCTION)
		A2D_Channel[scanningQueue[currentQueueElement]].preFunction(scanningQueue[currentQueueElement]);

	//(Re)start the Automatic scanning of the Analog ports
	START_SCAN;

	return;
}

int Find_Next_Queue_Element(int channel)
{
	int nextChannel = channel;

	//Advance to the next valid queue item - Do while used to ensure it runs at least once
	do
	{
		++nextChannel;
		if(nextChannel >= MAX_SCAN_QUEUE_SIZE)
			nextChannel = 0;
	}
	while((scanningQueue[nextChannel] == -1) && (nextChannel != channel));//Find the next channel to scan, or we end up back where we started

	return nextChannel;
}

void __attribute__((__interrupt__, auto_psv)) _ADC1Interrupt(void)
{
	//Clear interrupt flag
	IFS0bits.AD1IF = 0;

	//Temporarily turn off automatic scanning until we figure out what to do with our current samples
	STOP_SCAN;

	//Perform the end of scan action if applicable
	if(*A2D_Channel[scanningQueue[currentQueueElement]].postFunction != NO_POSTFUNCTION)
		A2D_Channel[scanningQueue[currentQueueElement]].postFunction(scanningQueue[currentQueueElement]);

	//Let the A2D routine know that we have finished
	scanIsComplete = 1;

	return;
}

void A2D_Initialize(void)
{
	int channel;

	//Initialize scan queue
	for(channel = 0; channel < MAX_SCAN_QUEUE_SIZE; ++channel)
		scanningQueue[channel] = -1;//Unassigned

	//AD1 Interrupt
	IFS0bits.AD1IF = 0;				//0 = Interrupt request has not occurred
	IEC0bits.AD1IE = 1;				//1 = Interrupt request is enabled

	//A/D Input Scan Select Register (Low)
	AD1CSSL = 0;					//0 = Analog channel omitted from input scan

	//A/D Port Configuration Register
	AD1PCFG = ~0;					//1 = Pin for corresponding analog channel is configured in Digital mode; I/O port read is enabled

	//A/D Input Select Register
	AD1CHSbits.CH0SA = 0b0111;		//0111 = AVDD
	AD1CHSbits.CH0NA = 0;			//0 = Channel 0 negative input is VR-
	AD1CHSbits.CH0SB = 0b0111;		//0111 = AVDD
	AD1CHSbits.CH0NB = 0;			//0 = Channel 0 negative input is VR-

	//A/D Control Register 3
	AD1CON3bits.ADCS = 0b111111;	//111111 = 64 * TCY
	AD1CON3bits.SAMC = 0b11111;		//11111 = 31 TAD
	AD1CON3bits.ADRC = 1;			//1 = A/D internal RC clock

	//A/D Control Register 2
	AD1CON2bits.ALTS = 0;			//0 = Always uses MUX A input multiplexer settings
	AD1CON2bits.BUFM = 0;			//0 = Buffer is configured as one 16-word buffer (ADC1BUFn<15:0>)
	AD1CON2bits.SMPI = 0b1111;		//1111 = Interrupts at the completion of conversion for each 16th sample/convert sequence
	//AD1CON2bits.BUFS is Read Only
	AD1CON2bits.CSCNA = 1;			//1 = Scan inputs
	AD1CON2bits.OFFCAL = 0;			//0 = Converts to get the actual input value
	AD1CON2bits.VCFG = 0;			//0 = Vr+ = AVDD, Vr- = AVSS

	//A/D Control Register 1
//	AD1CON1bits.DONE is Read Only
//	AD1CON1bits.SAMP is Read Only
	AD1CON1bits.ASAM = 0;		//0 = Sampling begins when SAMP bit is set
	AD1CON1bits.SSRC = 0b111;	//111 = Internal counter ends sampling and starts conversion (auto-convert)
	AD1CON1bits.FORM = 0;		//0 = Integer (0000 00dd dddd dddd)
	AD1CON1bits.ADSIDL = 1;		//1 = Discontinue module operation when device enters Idle mode
	AD1CON1bits.ADON = 1;		//1 = A/D Converter module is operating

	return;
}

int Add_To_Scan(int pin)
{
	//Range checking
	if(pin >= NUMBER_OF_CHANNELS)
		return 0;//Failure

	//Choose the correct pin
	pin = 1 << pin;

	//Turn off the module, changing a CSSL bit with it on can lead to issues
	AD1CON1bits.ADON = 0;

	//Set the pin to be scanned
	AD1CSSL |= pin;

	//Turn the module back on
	AD1CON1bits.ADON = 1;

	return 1;//Success
}

int Remove_From_Scan(int pin)
{
	//Range checking
	if(pin >= NUMBER_OF_CHANNELS)
		return 0;//Failure

	//Choose the correct pin
	pin = 1 << pin;

	//Turn off the module, changing a CSSL bit with it on can lead to issues
	AD1CON1bits.ADON = 0;

	//Remove the pin from being scanned
	AD1CSSL &= ~pin;

	//Turn the module back on
	AD1CON1bits.ADON = 1;

	return 1;//Success
}

int Change_To_Analog(int pin)
{
	//Range checking
	if(pin >= NUMBER_OF_CHANNELS)
		return 0;//Failure

	//Choose the correct pin
	pin = 1 << pin;

	//Set the pin(s) to digital
	AD1PCFG &= ~pin;

	return 1;//Success
}

int Change_To_Digital(int pin)
{
	//Range checking
	if(pin >= NUMBER_OF_CHANNELS)
		return 0;//Failure

	//Choose the correct pin
	pin = 1 << pin;

	//Set the pin(s) to digital
	AD1PCFG |= pin;

	return 1;//Success
}

int A2D_Add_To_Scan_Queue(int channel)
{
	int position = 0;

	//Check if we are within a valid range of channels
	if((channel < 0) || (channel >= NUMBER_OF_CHANNELS))
		return 0;

	//Find the next free scan position
    while((scanningQueue[position] != -1) && (position < MAX_SCAN_QUEUE_SIZE))
        position++;

	//Make it so
	if(position < MAX_SCAN_QUEUE_SIZE)
		scanningQueue[position] = channel;//Make it official
	else
		return 0;

	//So much win!
	return 1;
}

int A2D_Channel_Settings(int channel, enum RESOLUTION desiredResolutionIncrease, int numberOfAverages, int (*formatPointer)(int), void (*preFunction)(int), void (*postFunction)(int), void (*finishedFunction)(int))
{
	unsigned long samplesRequired;

	//Check if we are within a valid range of channels
	if((channel < 0) || (channel >= NUMBER_OF_CHANNELS))
		return 0;

	//Determine the number of samples required
	samplesRequired = numberOfAverages;
	switch(desiredResolutionIncrease)
	{
		case RESOLUTION_10_BIT:
			samplesRequired *= 1;//4^0
			break;
		case RESOLUTION_11_BIT:
			samplesRequired *= 4;//4^1
			break;
		case RESOLUTION_12_BIT:
			samplesRequired *= 16;//4^2
			break;
		case RESOLUTION_13_BIT:
			samplesRequired *= 64;//4^3
			break;
		case RESOLUTION_14_BIT:
			samplesRequired *= 256;//4^4
			break;
		case RESOLUTION_15_BIT:
			samplesRequired *= 1024;//4^5
			break;
		case RESOLUTION_16_BIT:
			samplesRequired *= 4096;//4^6
			break;
		default:
			return 0;
	}

	//Check if we have a valid number of averages (Minimum of 16 and must be a multiple of 16 that is no higher than a 16 bit number)
	if(((samplesRequired % 16) != 0) || (samplesRequired < 16) || (samplesRequired >= 65536))
		return 0;

	//Set values
	A2D_Channel[channel].value = 0;
	A2D_Channel[channel].sumOfSamples = 0;
	A2D_Channel[channel].bitsOfResolutionIncrease = desiredResolutionIncrease;
	A2D_Channel[channel].samplesRequired = (int)samplesRequired;
	A2D_Channel[channel].samplesTaken = 0;
	A2D_Channel[channel].formatPointer = formatPointer;
	A2D_Channel[channel].preFunction = preFunction;
	A2D_Channel[channel].postFunction = postFunction;
	A2D_Channel[channel].finishedFunction = finishedFunction;
	
	//Calculate the DSP and averaging number
	A2D_Channel[channel].valueForDSP = numberOfAverages;
	switch(desiredResolutionIncrease)
	{
		case RESOLUTION_10_BIT:
			break;
		case RESOLUTION_11_BIT:
			A2D_Channel[channel].valueForDSP *= 2;//2^1;
			break;
		case RESOLUTION_12_BIT:
			A2D_Channel[channel].valueForDSP *= 4;//2^2;
			break;
		case RESOLUTION_13_BIT:
			A2D_Channel[channel].valueForDSP *= 8;//2^3;
			break;
		case RESOLUTION_14_BIT:
			A2D_Channel[channel].valueForDSP *= 16;//2^4;
			break;
		case RESOLUTION_15_BIT:
			A2D_Channel[channel].valueForDSP *= 32;//2^5;
			break;
		case RESOLUTION_16_BIT:
			A2D_Channel[channel].valueForDSP *= 64;//2^6;
			break;
	}

	Change_To_Analog(channel);

	//Success!
	return 1;
}

int A2D_Value(int channel)
{
	return A2D_Channel[channel].value; //Return the value - NOTE: it has already been formatted
}
