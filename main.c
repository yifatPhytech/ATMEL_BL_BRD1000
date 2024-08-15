#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include "define.h"
#include "vars.h"
#include "clock.h"
#include "modem_manager.h"
#include "server.h"
#include "cli.h"

#define VERSION		"3.6"
// 24/07/2024	change URL of server to "bootloader-staging.herokuapp.com" for testing.
//				3.6 -  for ATMEL 64
// 22/07/2024	change URL of server to "bootloader.dev.phytech.com" for testing. 
//				3.5 - for ATMEL 128
//				3.4 -  for ATMEL 64
// 11.10.2023	downloadFirmware - enlarge timeout waiting for packet from 20 to 40
//				3.3 - for ATMEL 128
//				3.2 -  for ATMEL 64
// 27.09.2023	change turnModemOn - instead of 1000 sec delay - only 1...
//				3.0   change UART0 baudrate to 9600 - for atmega64
//				3.1   change UART0 baudrate to 9600 - for atmega128
//2.1	adjust to atmega128
//		add modem HW power on
//		remove all modem configuration. make only connection commands.
//2.0	FOR Atmega644PA - adjust SW for modem QUECTEL
//1.14	same as 1.13 but for atmega128
//1.13   back to atmega644. change server to connect with.
// 1.12 - replace device from ATMEGA644 to ATMEGA1284P.
//		change memory setting to 0xf000 instead of 0x7000 
// Version changes
// 1.11
//		BL sends it version after update
//		Minimum RSSI changed to 10
// 1.10
//		Removed MODEL_NA
//		Always go to roaming for general GSM modem
//		SVL never goes to roaming
//		Skip full connection process if modem already configured
//		Stop upgrade if RSSI is low then 12
// 1.9
//		BUG: Enlarged timeouts when sending HTTP requests. Timeout in #SCFG= changed to 3 instead of 50
// 1.8
//		FEAT: 	nMaxSenNum in EEPROM moved from the end after eEnableTmpAlert
// 1.7
//		FIX: Verizon modem connection fix
// 1.6e
//		FIX: Endless loop when BL can't access modem or download firmware (now it has 3 retries).
// 1.6
//		BUG: Initial AT command failed in Verizon Modem. Fixed by enlarging ignition delay.
//		FEAT: Removed qsanf verbose outputs
// 1.5
//		BUG: Fixed wakeup procedure (inputs were incorrectly initialized after power down)
//		BUG: Fixed jump to application
// 1.4
//		BUG: Added interrupts table remapping to application when jumping to application
// 1.3
//		FEAT: Prevent bootloader from infinite power down if valid application exists.
//		FEAT: Added retries to upgrade, after UPGRADE_RETRIES retries bootloader checks APP for validity and runs application
//				If application is not valid, keeps upgrading with deep sleep (SEC_UPGRADE_CYCLE) between retries
// 1.2
//		BUG: Fix for PowerDown doesn't shutdown modem
//		FEET: EEPROM variable nMaxSenNum moved to end of structure
// 1.1
//		BUG: Fix for SIMs that requires no roaming

#define _logd(str, ...) logd(str, ## __VA_ARGS__ )
#define _logs(str) logStr(str)
#define _logt(t) logTx(t)

BOOTLOADER_APP_HEADER appHeader;
bool performUpgrade;
bool isAppValid;
unsigned short crc;
byte upgradeRetries;
bool shortModemInit = true;

const char* getVersion() { return VERSION; }

static void (*jump_to_app_reset_vector)(void) = 0x0000;

// Declare your global variables here
void InitProgram( void );
void WakeUpProcedure(bool reset);
void InitPeripherals();
void PowerDown(bool eternally);

// calculates CRC code of the specified buffer
unsigned short CRC16_CCITT(const byte __flash * data, unsigned short length, unsigned short crc)
{
	byte b;

	while (length > 0)
	{
		b = pgm_read_byte(data);
		crc = (crc >> 8) | (crc << 8);
		crc ^= b;
		crc ^= (crc & 0xff) >> 4;
		crc ^= crc << 12;
		crc ^= (crc & 0xff) << 5;

		++data;
		--length;
	}
	
	return crc;
}

void checkAppHeader()
{
	isAppValid = false;
	
	// read app header
	memcpy_P(&appHeader, (const void*)BL_APP_HEADER_ADDRESS, sizeof(appHeader));

	// calculate crc
	crc = CRC16_CCITT((const BYTE __flash*)BL_APP_START_ADDRESS, BL_APP_HEADER_ADDRESS, 0xFFFF);
	
	_logd("APP v: %d, crc: %d, acrc: %d", appHeader._version, appHeader._crc, crc);

	if (appHeader._magic != BL_APP_HEADER_MAGIC)
	{
		return;
	}
	
	if (appHeader._crc != crc)
	{
		return;
	}
	
	isAppValid = true;
}

bool doAppUpgrade()
{
	bool result = false;
	bool bModemOn = false;

	_logs("Start SW Update");

	// switch on modem
	if (turnModemOn(false))
	{
		if (shortModemInit)
			bModemOn = true;
		else
			bModemOn = initModem();
		// initialize modem
		if (bModemOn)
		{
			// connect modem
			if (connectModem(true))
			{
				// update server that we are downloading
				if (updateFirmwareStatus(FW_STATUS_UPDATING))
				{
					// now we can download file
					if (downloadFirmware(blEepromData.versionUpdate))
					{
						// check new firmware for validity
						checkAppHeader();

						result = isAppValid;
					}
				
					// update server with final status
					updateFirmwareStatus(result ? FW_STATUS_READY : FW_STATUS_FAILURE);
				}
			}
		}
	}

	// close modem and leave
	closeModem();

	return result;
}

static void jump_to_app()
{
	DISABLE_INTERRUPTS();

	// switch vectors to application
	MCUCR = (1 << IVCE);
	MCUCR = (0 << IVSEL);

	// reconfigure and reset watchdog
	WATCHDOG_RESET();
	WATCHDOG_ENABLE_STEP1();
	WATCHDOG_ENABLE_STEP2();

	// stop timer
	DISABLE_TIMER1();
	DISABLE_TIMER1_COMPA();

	_logd("Jump to app, MCUSR: %d, WDTCSR: %d, MCUCR: %d", MCUSR, WDTCSR, MCUCR);

	WATCHDOG_RESET();
	jump_to_app_reset_vector();
}

#ifdef Cli
const BYTE BAUD_RATE_HIGH[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
const BYTE BAUD_RATE_LOW[]  = {0x01, 0x03, 0x05, 0x0B, 0x17, 0x2F, 0xBF};
const unsigned long BAUD_RATE_LONG[] = {115200, 57600,  38400 , 19200 , 9600, 4800, 1200};

void setBaudRate(int b)
{
    UBRR0H=BAUD_RATE_HIGH[b];
    UBRR0L=BAUD_RATE_LOW[b];

	_logd("New baud %l", BAUD_RATE_LONG[b]);
}

void cliHandle(char* cmd)
{
	// handle some commands
	switch (cmd[0])
	{
		case 'u':
			doAppUpgrade();
			break;
		case '1':
			turnModemOn(true);
			break;
		case 'i':
			initModem();
			break;
		case 'c':
			connectModem(false);
			break;
		case 'x':
			closeModem();
			break;
		case 'w':
			WakeUpProcedure(false);
			break;
		case 'd':
			downloadFirmware(2);
			break;
		case 'j':
			jump_to_app();
			break;
		case 'b':
			setBaudRate(cmd[1] - '0');
			break;
		case '@':
			// append \r\n to the command
			sendAT1("%s\r\n", 5, NULL, cmd + 1);
			break;
		case '#':
			// append \r\n to the command
			sendAT1("%s\r", 5, NULL, cmd + 1);
			break;
	}
}
#endif

// this is test EEPROM data and not used in code.
// AVR compiler generates .EEP file that can be burned during debug
typedef struct _tagDebugEEPROM
{
	AppEepromData appData;
	unsigned char reserved[BL_EEPROM_DATA_ADDRESS - sizeof(AppEepromData)];
	BlEepromData blData;
} DebugEEPROM;

DebugEEPROM EEMEM debugEEPROM =
{
	.appData = {
		.eLoggerID = 0,//500030,//604224,
		.eAPN = "",
		.eUseCntrCode = 1,
		.eMobileCntryCode = "42#",
		.eMobileNetCode = "503#",
		.eAPN = "APN",
	},
	.blData = {
		.versionUpdate = 0,
	},
};

int main(void)
{
	// switch interrupt vectors to bootloader
	DISABLE_INTERRUPTS();
	MCUCR = (1 << IVCE);
	MCUCR = (1 << IVSEL);
	ENABLE_INTERRUPTS();

	// initial reset
	WATCHDOG_RESET();

	// initialize all IO's and vars
	WakeUpProcedure(true);
	LoadEEPROMData();

    WATCHDOG_ENABLE_STEP1();
    WATCHDOG_ENABLE_STEP2();

	// remember reset reason
	if (MCUSR & ((1<<EXTRF) | (1<<PORF)))
	{
		bExtReset = TRUE;
	}

	_logd("BL v%s rr: %d, pu: %d, id: %l", VERSION, MCUSR, blEepromData.versionUpdate, appEepromData.eLoggerID);

	// check whether we need an upgrade to perform
	performUpgrade = blEepromData.versionUpdate != 0xFFFF && blEepromData.versionUpdate > 0;
	upgradeRetries = 0;
	shortModemInit = isModemOn();
	
	if (shortModemInit)
	{
		_logs("SHORT INIT");
	}

	// check application CRC and information structure
	checkAppHeader();
	
	// no upgrade required and application is valid, jump to it
	if (!performUpgrade && isAppValid)
	{
		jump_to_app();
	}
	
	// if application is invalid but no upgrade is requested, we have to perform upgrade in any way
	if (!isAppValid && !performUpgrade)
	{
		performUpgrade = true;
		// zero version should mean the latest firmware version for this logger
		blEepromData.versionUpdate = 0;
	}
	
	// this is command line API of boot loader
	while (1)
	{
		WATCHDOG_RESET();
		cliTask();

		#ifndef Cli
		// perform upgrade if specified so
		if (performUpgrade)
		{
			// sleep eternally when battery is empty
			if (btrStatus != BTR_FULL)
			{
				#ifdef SleepMode
				closeModem();
				checkAppHeader();
				if (isAppValid)
				{
					jump_to_app();
				}
				else
				{
					PowerDown(true);
				}
				#endif
			}

			if (doAppUpgrade())
			{
				performUpgrade = false;

				// mark that we do not need to upgrade any more
				blEepromData.versionUpdate = 0;
				SaveEEPROMData();

				// now call application
				jump_to_app();
			}
			else
			{
				#ifdef SleepMode
				_logd("Upgrade retry %d failed.", upgradeRetries);

				//if (shortModemInit)
				//{
					//closeModem();
					//shortModemInit = false;
				//}
				//else
				{
					upgradeRetries++;

					if (upgradeRetries >= UPGRADE_RETRIES)
					{
						checkAppHeader();

						if (isAppValid)
						{
							jump_to_app();
						}
					}

					closeModem();
					PowerDown(false);
				}
				#endif
			}

			#ifndef SleepMode
			performUpgrade = false;
			#endif
		}
		#endif
	}

	return 0;
}
