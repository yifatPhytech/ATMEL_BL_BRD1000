#ifndef DEFINE_H
#define DEFINE_H

#include <avr/io.h>
#include <avr/pgmspace.h>
#include "stdio.h"

//optional:
#define DebugMode
//#define Cli
#define SleepMode

typedef unsigned char BYTE;

#ifndef BOOL
#define BOOL char
#endif

#ifndef bool
#define bool char
#endif

#ifndef byte
#define byte unsigned char
#endif

#ifndef sbyte
#define sbyte signed char
#endif

#ifndef NULL
#define NULL    ((void*)0)
#endif

#define TRUE 1
#define FALSE 0
#define true 1
#define false 0

#ifndef __flash
#define __flash
#endif

#ifndef __eeprom
#define __eeprom
#endif

#define BL_APP_START_ADDRESS					0x0
#define BL_APP_END_ADDRESS					    (0xD000u - 1)
#define BL_APP_HEADER_MAGIC						0x4E4D
#define BL_APP_HEADER_ADDRESS					(BL_APP_END_ADDRESS - SPM_PAGESIZE + 1)

#define APP_EEPROM_DATA_ADDRESS					0
#define BL_EEPROM_DATA_ADDRESS					0x400

#define NO_DATA 2

#ifdef DebugMode
#define BLSPARE __attribute__((section(".blspare")))
#else
#define BLSPARE //__attribute__((section(".blspare")))
#endif

//
// this is application eeprom structure. If changed in application it should be changed here!!!
// bootloader uses some values from it
typedef struct _tagAPPEEPROM
{
	unsigned long eLoggerID;
	unsigned char eStartConnectionH;
	unsigned char eConnectionInDay;
	unsigned char eConnectIntervalH;
	int eMinTmp4Alert;
	unsigned char eEnableTmpAlert;
	unsigned short nMaxSenNum;
	unsigned short eTimeZoneOffset;
	byte eRoamingDelay;
	byte eUseCntrCode;
	char eMobileNetCode[5];
	char eMobileCntryCode[5];
	char eIPorURLval1[33];
	char ePORTval1[5];
	char eAPN[33];
} AppEepromData;

//
// this is data shared between application and bootloader
typedef struct _tagBLEEPROM
{
	unsigned short versionUpdate;
} BlEepromData;

extern AppEepromData appEepromData;
extern BlEepromData blEepromData;

void LoadEEPROMData();
void SaveEEPROMData();

// Application Header for boot loader
typedef struct tagBOOTLOADER_APP_HEADER
{
	unsigned short _magic;
	unsigned short _crc;
	unsigned short _version;
} BOOTLOADER_APP_HEADER;

extern BOOTLOADER_APP_HEADER appHeader;

#define MODEM_GSM			1

// if battery voltage less than this value, no upgrade will be performed
#define BTR_UPDATE_LIMIT	3800

#define BTR_FULL			0
#define BTR_EMPTY			1

// ADC consts
#define INT_VREF 				2560	//reference voltage [mV] internal
#define ADC_VREF_TYPE			((1<<REFS1) | (1<<REFS0) | (0<<ADLAR))  //reference voltage [mV] internal

// some UART constants
#define DATA_REGISTER_EMPTY		(1<<UDRE0)
#define RX_COMPLETE				(1<<RXC0)
#define FRAMING_ERROR			(1<<FE0)
#define PARITY_ERROR			(1<<UPE0)
#define DATA_OVERRUN			(1<<DOR0)

#define MUX_CHARGE				0
#define MUX_BATTERY				1

// number of seconds between upgrade cycles
#define SEC_UPGRADE_CYCLE		(16)
// bootloader will try to upgrade this number of times and will return
// to application if upgrade fails and application is valid
// if application is invalid bootloader will keep updating
#define UPGRADE_RETRIES			3

// number of seconds to wait after modem ignition is on
#define SEC_4_GSM_IGNITION		1//10

#define WATCHDOG_ENABLE_STEP1() (WDTCSR=(0<<WDIE) | (1<<WDP3) | (1<<WDCE) | (1<<WDE) | (0<<WDP2) | (0<<WDP1) | (1<<WDP0))
#define WATCHDOG_ENABLE_STEP2() (WDTCSR=(0<<WDIE) | (1<<WDP3) | (0<<WDCE) | (1<<WDE) | (0<<WDP2) | (0<<WDP1) | (1<<WDP0))

#define WATCHDOG_DISABLE() 		(WDTCSR = 0x00) //WDE = 0
#define WATCHDOG_RESET() 		asm volatile("wdr")

#define ENABLE_TIMER0()			(TCCR0B=(0<<WGM02) | (1<<CS02) | (0<<CS01) | (1<<CS00))
#define DISABLE_TIMER0()		(TCCR0B=(0<<WGM02) | (0<<CS02) | (0<<CS01) | (0<<CS00))

#define ENABLE_TIMER1()			(TCCR1B=(0<<ICNC1) | (0<<ICES1) | (0<<WGM13) | (1<<WGM12) | (0<<CS12) | (1<<CS11) | (0<<CS10))
#define DISABLE_TIMER1()		(TCCR1B=(0<<ICNC1) | (0<<ICES1) | (0<<WGM13) | (0<<WGM12) | (0<<CS12) | (0<<CS11) | (0<<CS10))

#define ENABLE_TIMER2()			(TCCR2B=(0<<WGM22) | (1<<CS22) | (1<<CS21) | (1<<CS20))
#define DISABLE_TIMER2()		(TCCR2B=(0<<WGM22) | (0<<CS22) | (0<<CS21) | (0<<CS20))

#define ENABLE_UART0()			(UCSR0B=(0<<RXCIE0) | (0<<TXCIE0) | (0<<UDRIE0) | (1<<RXEN0) | (1<<TXEN0) | (0<<UCSZ02) | (0<<RXB80) | (0<<TXB80))
#define DISABLE_UART0()			(UCSR0B=(0<<RXCIE0) | (0<<TXCIE0) | (0<<UDRIE0) | (0<<RXEN0) | (0<<TXEN0) | (0<<UCSZ02) | (0<<RXB80) | (0<<TXB80))

#define ENABLE_UART1()			(UCSR1B=(0<<RXCIE1) | (0<<TXCIE1) | (0<<UDRIE1) | (1<<RXEN1) | (1<<TXEN1) | (0<<UCSZ12) | (0<<RXB81) | (0<<TXB81))
#define DISABLE_UART1()			(UCSR1B=(0<<RXCIE1) | (0<<TXCIE1) | (0<<UDRIE1) | (0<<RXEN1) | (0<<TXEN1) | (0<<UCSZ12) | (0<<RXB81) | (0<<TXB81))

#define ENABLE_TIMER1_COMPA()   (TIMSK1 |= (1<<OCIE1A))
#define DISABLE_TIMER1_COMPA()  (TIMSK1 &= ~(1<<OCIE1A))

#define ENABLE_ADC()  (ADCSRA=(1<<ADEN) | (0<<ADSC) | (0<<ADATE) | (0<<ADIF) | (0<<ADIE) | (1<<ADPS2) | (1<<ADPS1) | (1<<ADPS0))
#define DISABLE_ADC() (ADCSRA=(0<<ADEN) | (0<<ADSC) | (0<<ADATE) | (0<<ADIF) | (0<<ADIE) | (0<<ADPS2) | (0<<ADPS1) | (0<<ADPS0))

// Modem management macros
#define MODEM_3G_IGNITION_ON() (PORTC |= (1 << 4))
#define MODEM_3G_IGNITION_OFF() (PORTC &= ~(1 << 4))

#define MODEM_3G_SHUTDOWN_START() (PORTC |= (1 << 6))
#define MODEM_3G_SHUTDOWN_STOP() (PORTC &= ~(1 << 6))

#define MODEM_PWR_DISABLE() (PORTD &= ~(1 << 7))	//(PORTD.7 = 0);
#define MODEM_PWR_ENABLE() (PORTD |= (1 << 7))	//(PORTD.7 = 0);


#define DISABLE_INTERRUPTS() asm volatile("cli")
#define ENABLE_INTERRUPTS() asm volatile("sei")

#define HASHTAG     '#'

#ifdef DebugMode
void logTx(char c);
void logStr(const char* str);
void logStrRaw(const char* str);
void logd(const char* fmt, ...);
void logHexByte(byte b);
#else
#define logd(fmt, ...)
#define logTx(c)
#define logStr(s)
#define logStrRaw(s)
#define logHexByte(b)
#endif //DebugMode

void MeasureBatt();

int qscanf(const char* str, const char* fmt, ...);
char* qprintf(char* str, const char* fmt, ...);
char* qprintfv(char* str, const char* fmt, va_list args);

#endif //DEFINE_H