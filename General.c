////////////////start of General.c file//////////////
#include "define.h"
#include "stdio.h"
#include "avr/eeprom.h"
#include "avr/pgmspace.h"
#include "clock.h"
#include "cli.h"

#define _logd(str, ...) logd(str, ## __VA_ARGS__ )
#define _logs(str) logStr(str)
#define _logt(t) //logTx(t)

extern int iVoltage;
extern byte btrStatus;
extern bool bExtReset;
extern unsigned int wdtSecs;

AppEepromData appEepromData;
BlEepromData blEepromData;

void LoadEEPROMData()
{
	eeprom_busy_wait();
	eeprom_read_block(&blEepromData, (const void*)BL_EEPROM_DATA_ADDRESS, sizeof(blEepromData));
	eeprom_read_block(&appEepromData, (const void*)APP_EEPROM_DATA_ADDRESS, sizeof(appEepromData));

	// some validity checks
	if (appEepromData.eUseCntrCode > 1)
	{
		appEepromData.eUseCntrCode = 0;
	}

	if (~appEepromData.eLoggerID == 0)
	{
		appEepromData.eLoggerID = 0;
	}
}

void SaveEEPROMData()
{
	eeprom_busy_wait();
	eeprom_write_block(&blEepromData, (void*)BL_EEPROM_DATA_ADDRESS, sizeof(blEepromData));
}

void InitProgram(void)
{
	// Crystal Oscillator division factor: 1
	CLKPR=(1<<CLKPCE);
	CLKPR=(0<<CLKPCE) | (0<<CLKPS3) | (0<<CLKPS2) | (0<<CLKPS1) | (0<<CLKPS0);

	//Direction - 0 = Input, 1= Output
	//State - if direction is Input:
	//                  1 = enable Pull up
	//                  0 = disable Pull up, become tri state
	// Input/Output Ports initialization
	// Port A initialization
	// Function: Bit7=Out Bit6=Out Bit5=Out Bit4=In Bit3=In Bit2=In Bit1=In Bit0=In
	DDRA=(1<<DDA7) | (1<<DDA6) | (1<<DDA5) | (0<<DDA4) | (0<<DDA3) | (0<<DDA2) | (0<<DDA1) | (0<<DDA0);
	// State: Bit7=0 Bit6=0 Bit5=0 Bit4=T Bit3=T Bit2=T Bit1=T Bit0=T
	PORTA=(0<<PORTA7) | (0<<PORTA6) | (0<<PORTA5) | (0<<PORTA4) | (0<<PORTA3) | (0<<PORTA2) | (0<<PORTA1) | (0<<PORTA0);

	// Port B initialization
	// Function: Bit7=In Bit6=In Bit5=In Bit4=In Bit3=In Bit2=In Bit1=out Bit0=Out
	DDRB=(0<<DDB7) | (0<<DDB6) | (0<<DDB5) | (0<<DDB4) | (0<<DDB3) | (0<<DDB2) | (1<<DDB1) | (1<<DDB0);
	// State: Bit7=T Bit6=T Bit5=T Bit4=T Bit3=T Bit2=T Bit1=T Bit0=T
	PORTB=(0<<PB7) | (0<<PB6) | (0<<PB5) | (0<<PB4) | (0<<PB3) | (0<<PB2) | (0<<PB1) | (0<<PB0);

	// Port C initialization
	// Function: Bit7=In Bit6=Out Bit5=Out Bit4=Out Bit3=Out Bit2=Out Bit1=In Bit0=In
	DDRC=(0<<DDC7) | (1<<DDC6) | (1<<DDC5) | (1<<DDC4) | (0<<DDC3) | (1<<DDC2) | (0<<DDC1) | (0<<DDC0);
	// State: Bit7=0 Bit6=0 Bit5=0 Bit4=0 Bit3=0 Bit2=0 Bit1=T Bit0=T
	PORTC=(1<<PC7) | (0<<PC6) | (0<<PC5) | (0<<PC4) | (1<<PC3) | (0<<PC2) | (0<<PC1) | (0<<PC0); //c4=0      c6=1   c3=1

	// Port D initialization
	// Function: Bit7=Out Bit6=Out Bit5=Out Bit4=Out Bit3=In Bit2=In Bit1=In Bit0=In
	DDRD=(0<<DDD7) | (1<<DDD6) | (1<<DDD5) | (1<<DDD4) | (1<<DDD3) | (0<<DDD2) | (1<<DDD1) | (0<<DDD0);
	// State: Bit7=0 Bit6=0 Bit5=0 Bit4=0 Bit3=T Bit2=T Bit1=T Bit0=T
	PORTD=(1<<PD7) | (0<<PD6) | (1<<PD5) | (1<<PD4) | (1<<PD3) | (0<<PD2) | (1<<PD1) | (0<<PD0);

	// Timer/Counter 0 initialization
	// Clock source: System Clock
	// Clock value: 3.600 kHz
	// Mode: Normal top=0xFF
	// OC0A output: Disconnected
	// OC0B output: Disconnected
	// Timer Period: 71.111 ms
	TCCR0A=(0<<COM0A1) | (0<<COM0A0) | (0<<COM0B1) | (0<<COM0B0) | (0<<WGM01) | (0<<WGM00);
	DISABLE_TIMER0();
	TCNT0=0x00;
	OCR0A=0x00;
	OCR0B=0x00;

	// Timer/Counter 1 initialization
	// Clock source: System Clock
	// Clock value: 460.800 kHz
	// Mode: CTC top=OCR1A
	// OC1A output: Disconnected
	// OC1B output: Disconnected
	// Noise Canceler: Off
	// Input Capture on Falling Edge
	// Timer Period: 0.1 s
	// Timer1 Overflow Interrupt: Off
	// Input Capture Interrupt: Off
	// Compare A Match Interrupt: On
	// Compare B Match Interrupt: Off
	TCCR1A=(0<<COM1A1) | (0<<COM1A0) | (0<<COM1B1) | (0<<COM1B0) | (0<<WGM11) | (0<<WGM10);
	TCCR1B=(0<<ICNC1) | (0<<ICES1) | (0<<WGM13) | (1<<WGM12) | (0<<CS12) | (1<<CS11) | (0<<CS10);
	TCNT1H=0x4C;
	TCNT1L=0x00;
	ICR1H=0x00;
	ICR1L=0x00;
	OCR1AH=0xB3;
	OCR1AL=0xFF;
	OCR1BH=0x00;
	OCR1BL=0x00;


	// Timer/Counter 2 initialization
	// Clock source: System Clock
	// Clock value: 3.600 kHz
	// Mode: Normal top=0xFF
	// OC2A output: Disconnected
	// OC2B output: Disconnected
	// Timer Period: 71.111 ms
	ASSR=(0<<EXCLK) | (0<<AS2);
	TCCR2A=(0<<COM2A1) | (0<<COM2A0) | (0<<COM2B1) | (0<<COM2B0) | (0<<WGM21) | (0<<WGM20);
	DISABLE_TIMER2();
	TCNT2=0x00;
	OCR2A=0x00;
	OCR2B=0x00;

	// Timer/Counter 0 Interrupt(s) initialization
	TIMSK0=(0<<OCIE0B) | (0<<OCIE0A) | (0<<TOIE0); // MICHAEL, INTERRUPT WAS DISABLED

	// Timer/Counter 1 Interrupt(s) initialization
	TIMSK1=(0<<ICIE1) | (0<<OCIE1B) | (1<<OCIE1A) | (0<<TOIE1);

	// Timer/Counter 2 Interrupt(s) initialization
	TIMSK2=(0<<OCIE2B) | (0<<OCIE2A) | (0<<TOIE2); // MICHAEL, INTERRUPT WAS DISABLED

	// Analog Comparator initialization
	// Analog Comparator: Off
	ACSR=(1<<ACD) | (0<<ACBG) | (0<<ACO) | (0<<ACI) | (0<<ACIE) | (0<<ACIC) | (0<<ACIS1) | (0<<ACIS0);
	ADCSRB=(0<<ACME);
	DIDR1=0x00;
}

void InitPeripherals()
{
    // USART0 initialization
    // Communication Parameters: 8 Data, 1 Stop, No Parity
    // USART0 Receiver: On
    // USART0 Transmitter: On
    // USART0 Mode: Asynchronous
    // USART0 Baud Rate: 19200
    UCSR0A=(0<<RXC0) | (0<<TXC0) | (0<<UDRE0) | (0<<FE0) | (0<<DOR0) | (0<<UPE0) | (0<<U2X0) | (0<<MPCM0);
    DISABLE_UART0();
    UCSR0C=(0<<UMSEL01) | (0<<UMSEL00) | (0<<UPM01) | (0<<UPM00) | (0<<USBS0) | (1<<UCSZ01) | (1<<UCSZ00) | (0<<UCPOL0);
    UBRR0H=0x00;
    UBRR0L=0x17;//0x0B;

    // USART1 initialization
    // Communication Parameters: 8 Data, 1 Stop, No Parity
    // USART1 Receiver: On
    // USART1 Transmitter: On
    // USART1 Mode: Asynchronous
    // USART1 Baud Rate: 38400 x2
    UCSR1A=(0<<RXC1) | (0<<TXC1) | (0<<UDRE1) | (0<<FE1) | (0<<DOR1) | (0<<UPE1) | (1<<U2X1) | (0<<MPCM1);
    DISABLE_UART1();
    UCSR1C=(0<<UMSEL11) | (0<<UMSEL10) | (0<<UPM11) | (0<<UPM10) | (0<<USBS1) | (1<<UCSZ11) | (1<<UCSZ10) | (0<<UCPOL1);
    UBRR1H=0x00;
    UBRR1L=0x0B;

    // ADC initialization
    // ADC Clock frequency: 28.800 kHz
    // ADC Voltage Reference: 2.56V, cap. on AREF
    // ADC Auto Trigger Source: ADC Stopped
    // Digital input buffers on ADC0: On, ADC1: On, ADC2: On, ADC3: On, ADC4: On
    // ADC5: Off, ADC6: Off, ADC7: Off
    DIDR0=(1<<ADC7D) | (1<<ADC6D) | (1<<ADC5D) | (1<<ADC4D) | (1<<ADC3D) | (1<<ADC2D) | (0<<ADC1D) | (1<<ADC0D);
    ADMUX = ADC_VREF_TYPE;
    ADCSRA=(0<<ADEN) | (0<<ADSC) | (0<<ADATE) | (0<<ADIF) | (0<<ADIE) | (1<<ADPS2) | (1<<ADPS1) | (1<<ADPS0);
    ADCSRB=(0<<ADTS2) | (0<<ADTS1) | (0<<ADTS0);

    // SPI initialization
    // SPI disabled
    SPCR=(0<<SPIE) | (0<<SPE) | (0<<DORD) | (0<<MSTR) | (0<<CPOL) | (0<<CPHA) | (0<<SPR1) | (0<<SPR0);

    // TWI initialization
    // TWI disabled
    TWCR=(0<<TWEA) | (0<<TWSTA) | (0<<TWSTO) | (0<<TWEN) | (0<<TWIE);

	// MICHAEL: this will ensure that RTC will not wake up bootloader
	// External Interrupt(s) initialization
	// INT0: Off
	// INT1: Off
	// INT2: Off
	// Interrupt on any change on pins PCINT0-7: Off
	// Interrupt on any change on pins PCINT8-15: Off
	// Interrupt on any change on pins PCINT16-23: Off
	// Interrupt on any change on pins PCINT24-31: Off
	EICRA=(0<<ISC21) | (0<<ISC20) | (0<<ISC11) | (0<<ISC10) | (0<<ISC01) | (0<<ISC00);
	EIMSK=(0<<INT2) | (0<<INT1) | (0<<INT0);
	PCICR=(0<<PCIE3) | (0<<PCIE2) | (0<<PCIE1) | (0<<PCIE0);
}

void BLSPARE WakeUpProcedure(bool reset)
{
	InitProgram();
    InitPeripherals();
    
    ENABLE_UART0();
    ENABLE_TIMER1_COMPA();

    #ifdef DebugMode
	// make sure UART1 muxed to debug output
    PORTD = (PORTD & ~0x30) | (2 << 4);

    ENABLE_UART1();
    #endif //DebugMode

    delay_ms(50);
    _logs("Wakeup");

    MeasureBatt();

    if (iVoltage > BTR_UPDATE_LIMIT)
        btrStatus = BTR_FULL;
    else
        btrStatus = BTR_EMPTY; 

	_logs("Enable WD");

	MCUSR &= ~(1<<WDRF);
	/* Write logical one to WDCE and WDE */
	/* Keep old prescaler setting to prevent unintentional time-out */
	WDTCSR |= (1<<WDCE) | (1<<WDE);
	/* Turn off WDT */
	WDTCSR = 0x00;

    // Watchdog Timer initialization
    // Watchdog Timer Prescaler: OSC/1024k
    // Watchdog Timer interrupt: Off
    WATCHDOG_RESET();
    WATCHDOG_ENABLE_STEP1();
    WATCHDOG_ENABLE_STEP2();

	_logd("End wakeup: MCUSR %d", MCUSR);
}

#ifdef SleepMode
void PowerDown(bool eternally)
{
    _logs(eternally ? "PowerDown Forever" : "PowerDown");
	delay_ms(100);

    bExtReset = FALSE;
	
    // Port A initialization
    // Function: Bit7=Out Bit6=Out Bit5=Out Bit4=In Bit3=In Bit2=In Bit1=In Bit0=In
    DDRA=(1<<DDA7) | (1<<DDA6) | (1<<DDA5) | (0<<DDA4) | (0<<DDA3) | (0<<DDA2) | (0<<DDA1) | (0<<DDA0);
    // State: Bit7=0 Bit6=0 Bit5=0 Bit4=T Bit3=T Bit2=T Bit1=T Bit0=T
    PORTA=(0<<PORTA7) | (0<<PORTA6) | (0<<PORTA5) | (0<<PORTA4) | (0<<PORTA3) | (0<<PORTA2) | (0<<PORTA1) | (0<<PORTA0);

    // Port B initialization
    // Function: Bit7=Out Bit6=Out Bit5=Out Bit4=In Bit3=In Bit2=In Bit1=Out Bit0=In
    DDRB = ((0<<DDB7) | (0<<DDB6) | (0<<DDB5) | (0<<DDB4) | (0<<DDB3) | (0<<DDB2) |(1<<DDB1)  | (1<<DDB0));
    // State: Bit7=0 Bit6=0 Bit5=0 Bit4=P Bit3=T Bit2=T Bit1=0 Bit0=T
    PORTB = ((0<<PORTB7) | (0<<PORTB6) | (0<<PORTB5) | (0<<PORTB4) | (0<<PORTB3)  | (0<<PORTB2) | (0<<PORTB1) | (0<<PORTB0));
	
    // Port C initialization
    // Function: Bit7=Out Bit6=Out Bit5=Out Bit4=Out Bit3=Out Bit2=Out Bit1=Out Bit0=Out
    DDRC=(0<<DDC7) | (1<<DDC6) | (1<<DDC5) | (1<<DDC4) | (0<<DDC3) | (1<<DDC2) | (1<<DDC1) | (1<<DDC0);
    // State: Bit7=0 Bit6=0 Bit5=0 Bit4=0 Bit3=0 Bit2=0 Bit1=1 Bit0=1
    PORTC=(0<<PORTC7) | (0<<PORTC6) | (0<<PORTC5) | (0<<PORTC4) | (0<<PORTC3) | (0<<PORTC2) | (1<<PORTC1) | (1<<PORTC0);
	
    // Port D initialization
    // Function: Bit7=In Bit6=Out Bit5=Out Bit4=Out Bit3=In Bit2=In Bit1=Out Bit0=In
    DDRD=(0<<DDD7) | (1<<DDD6) | (1<<DDD5) | (1<<DDD4) | (1<<DDD3) | (0<<DDD2) | (1<<DDD1) | (0<<DDD0);
    // State: Bit7=0 Bit6=0 Bit5=0 Bit4=0 Bit3=0 Bit2=T Bit1=0 Bit0=T
    PORTD=(0<<PORTD7) | (0<<PORTD6) | (1<<PORTD5) | (0<<PORTD4) | (0<<PORTD3) | (0<<PORTD2) | (0<<PORTD1) | (0<<PORTD0);
	
    DISABLE_UART1();
    DISABLE_TIMER1_COMPA();

	if (!eternally)
	{
		// Watchdog Timer initialization
		// Watchdog Timer Prescaler: OSC/1024k
		// Watchdog timeout action: Interrupt
		MCUSR |= (1<<WDRF);
		WATCHDOG_RESET();
		WDTCSR |= (1 << WDCE) | (1<<WDE);
		WDTCSR = (1 << WDIF) | (1 << WDIE) | (1 << WDP3) | (0 << WDCE) | (0 << WDE) | (0 << WDP2) | (0 << WDP1) | (1 << WDP0);
	}
	else
	{
		/* Clear WDRF in MCUSR */
		MCUSR &= ~(1<<WDRF);
		/* Write logical one to WDCE and WDE */
		/* Keep old prescaler setting to prevent unintentional time-out */
		WDTCSR |= (1<<WDCE) | (1<<WDE);
		/* Turn off WDT */
		WDTCSR = 0x00;

	}

	// wait for very long timeout
	for (wdtSecs = 0; wdtSecs < SEC_UPGRADE_CYCLE; )
	{
		//if (PORTA & (1 << PORTA5))
		//{
		//	PORTA &= ~((1<<PORTA7) | (1<<PORTA6) | (1<<PORTA5));
		//}
		//else
		//{
		//	PORTA |= (1<<PORTA7) | (1<<PORTA6) | (1<<PORTA5);
		//}

		SMCR |= 4;
		SMCR |= 1;

		asm volatile("sleep");
		asm volatile("nop;nop;"); 		//wakeup from sleep mode
	}

	WakeUpProcedure(false);

	_logs("PowerDown end");
}
#endif

int qscanf(const char* str, const char* fmt, ...)
{
	char c;
	char p;
	char t;
	byte count;
	int tempb;
	bool qverbose;
	va_list arg;
	union
	{
		byte* b;
		char* c;
		int* d;
		const char** p;
	} ptr;

	p = 0;
	count = 0;
	va_start(arg, fmt);
	//qverbose = *fmt != '^';
	qverbose = false;

	if (qverbose)
	{
		_logs("qscanf");
		_logs(str);
		_logs(fmt);
	}

	if (*fmt == '^')
	{
		fmt++;
	}

	while ((c = *fmt++) != 0)
	{
		if (qverbose) _logt(c);

		switch (c)
		{
			case '%':
			t = *fmt++;
			
			switch (t)
			{
				case 'p':
					ptr.p = va_arg(arg, const char**);
					if (ptr.p) *ptr.p = str;

				case 'e':
					count++;
					break;

				case 'b':
				case 'd':
				{
					tempb = 0;
					while ((c = *str) != 0)
					{
						if (c < '0' || c > '9') break;

						tempb *= 10;
						tempb += c - '0';
						str++;
					}

					if (t == 'b')
					{
						ptr.b = va_arg(arg, byte*);
						if (ptr.b) *ptr.b = (BYTE)tempb;
					}
					else
					{
						ptr.d = va_arg(arg, int*);
						if (ptr.d) *ptr.d = tempb;
					}
					
					count++;
					break;
				}
				case 's':
				{
					ptr.c = va_arg(arg, char*);
					// p contains previous symbol, find string between these symbols
					p = *fmt;
					do
					{
						c = *str;
						if (c == p || c == 0)
						{
							break;
						}

						str++;
						
						if (ptr.c) *ptr.c++ = c;
					} while (c != 0);
					
					if (ptr.c) *ptr.c = 0;
					count++;
					break;
				}
			}
			break;
			
			default:
			if (c != *str)
			{
				if (qverbose)  _logt('<');
				return count;
			}

			p = c;
			str++;
		}
	}
	
	va_end(arg);

	if (qverbose)
	{
		_logt('*');
		_logt('\r');
		_logt('\n');
	}
	
	return count;
}

static byte qprintu(char* str, unsigned long n)
{
	byte d;
	byte c = 0;

	do
	{
		d = n % 10;
		n /= 10;

		*str++ = d + '0';
		c++;
	} while (n != 0);

	return c;
}

static void qrev(char* start, byte count)
{
	char c;

	while (count > 1)
	{
		c = start[0];
		start[0] = start[count - 1];
		start[count - 1] = c;

		count--;
		count--;
		start++;
	}
}

char* qprintfv(char* str, const char* fmt, va_list args)
{
	char c;
	char t;
	char* ps;
	byte len;
	
	while ((c = *fmt++) != 0)
	{
		switch (c)
		{
			case '%':
			t = *fmt++;
			
			switch (t)
			{
				case 'b':
				case 'd':
				case 'l':
				{
					len = qprintu(str, t == 'l' ? va_arg(args, unsigned long) : va_arg(args, unsigned int));
					qrev(str, len);
					str += len;
					break;
				}
				case 's':
				{
					ps = va_arg(args, char*);
					while ((c = *ps++) != 0)
					{
						*str++ = c;
					}
					
					break;
				}
				case '#':
				{
					ps = va_arg(args, char*);
					len = va_arg(args, unsigned int);
					while ((c = *ps++) != 0 && len-- > 0)
					{
						if (c == '#') break;
						*str++ = c;
					}
					
					break;
				}
				//case '%':
				//	*str++ = '%';
				break;
			}
			break;
			
			default:
				*str++ = c;
			break;
		}
	}
	
	*str = 0;
	return str;
}

char* qprintf(char* str, const char* fmt, ...)
{
	va_list args;
	char* r;

	va_start(args, fmt);
	r = qprintfv(str, fmt, args);
	va_end(args);

	return r;
}

#ifdef DebugMode

char debugbuf[100];
const char* hex = "0123456789ABCDEF";

void logTx(char c)
{
	while ((UCSR1A & DATA_REGISTER_EMPTY)==0);
	UDR1 = c;
	//while(( UCSR1A & (1<<TXC1))==0);
}

void logHexByte(byte b)
{
	logTx(hex[(b >> 4) & 0xF]);
	logTx(hex[b & 0xF]);
	logTx(' ');
}

void logStrRaw(const char* str)
{
	while (*str)
	{
		WATCHDOG_RESET();

		while ((UCSR1A & DATA_REGISTER_EMPTY)==0);
		UDR1 = *str++;
		//while(( UCSR1A & (1<<TXC1))==0);
	}
};

void logStr(const char* str)
{
	logStrRaw(str);
	logStrRaw("\r\n");
};

void logd(const char* fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	qprintfv(debugbuf, fmt, args);
	va_end(args);

	logStrRaw(debugbuf);
	logStrRaw("\r\n");
}

#endif //DebugMode
