/*

initadc - Initializes ADC and selects specified channel. Puts ADC in sampling mode.
readadc - Ends sampling and starts conversion. Once conversion is done, resumes 
sampling and returns conversion result.

- 12 ADC clocks per conversion
- ADC clock period tAD is set as a multiple of the peripheral clock period, tPB, using
	the ADCS bits in the equation below (peripheral clock divider = 2)
				tAD = 2*tPB(ADCS+1)
				(our tPB = 1/40 MHz = 25 ns)	
- We want our sampling rate to be 1Msps. Sampling rate and ADC clock period are related
	by 
				sampRate = 1/(sampTime + 12*tAD)
	where sampTime is the amount of time necessary for the input to settle before conversion 
	takes place. 

				tAD 	= 2*tPB(ADCS+1)
				50us 	= 2*25ns(ADCS+1)
				ADCS 	= 0
				ADCS 	= ADC Conversion Clock Select bits
				
*/

#include <P32xxxx.h>
#include <plib.h>
#include <sys/attribs.h>

void main(void);
//void isr(void);

void initadc(int channel){
	AD1CHSbits.CH0SA = channel;		//select which channel to sample
	AD1PCFGCLR = 1 << channel;		//configure pin for this channel to analog input
	AD1CON1bits.ON = 1;				//turn ADC on
	AD1CON1bits.SAMP = 1;			//begin sampling
	AD1CON1bits.DONE = 0;			//clear DONE flag, enable conversion
	}

int readadc(void){	
	AD1CON1bits.SAMP = 0;			//end sampling, start conversion
	while(!AD1CON1bits.DONE);		//wait until done converting
	AD1CON1bits.SAMP = 1;			//resume sampling to prepare for next conversion
	AD1CON1bits.DONE = 0;			//clear DONE flag, enable conversion
	return ADC1BUF0;				//return conversion result
	}

int main(void){
	int sample;
	initadc(0);						//AN0
	sample = readadc();
	} 

/*
//Interrupt code (NOT COMPLETE)
void __ISR(_TIMER_1_VECTOR, ipl7) Timer1Handler(void) { //ipl7AUTO
	if(INTCONbits.TMR0IF){
		INTCONbits.TMR0IF = 0; //reset
		ADCON0bits.GO=1; //Start A/D converter
		PORTE=0; // valid bit low
		TMR0H=0xfe; // reset timer
		TMR0L=0x0c;
	}
	else if (PIR1bits.ADIF) {
		PIR1bits.ADIF=0; //reset
		PORTC=ADRESH; //Get Data
		PORTC=PORTC - 128; //Convert to two’s complement
		PORTE=0x01; //valid bit high
	}
}
*/
