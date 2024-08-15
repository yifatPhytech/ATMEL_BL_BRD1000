//boot_program_page
// Server API
//

#include "modem_manager.h"
#include "avr/interrupt.h"
#include "avr/boot.h"
#include "server.h"
#include "clock.h"

//#define DebugServer
#define _logd(str, ...) logd(str, ## __VA_ARGS__ )
#define _logs(str) logStr(str)
#define _logt(t) logTx(t)

extern const char* getVersion();

typedef void (*fnParseHeader)(const char* header, char* value);

int contentLength;

// taken from AVR example
// IMPORTANT: this function should be in BOOTLOADER flash and not in BLSPARE section
void boot_program_page(uint16_t page, uint8_t *buf, unsigned int chunkLength)
{
	uint16_t i;
	uint8_t sreg;
	uint16_t w;

	// Disable interrupts.

	sreg = SREG;
	cli();
	    
	WATCHDOG_RESET();

	eeprom_busy_wait();

	WATCHDOG_RESET();

	boot_page_erase(page);
	boot_spm_busy_wait();      // Wait until the memory is erased.

	for (i = 0; i < SPM_PAGESIZE; i += 2)
	{
		// Set up little-endian word.
		if (i < chunkLength)
		{
			w = *buf++;
			w += (*buf++) << 8;
		}
		else
		{
			w = 0xFFFF;
		}

		#ifdef DebugServer
		logHexByte(w & 0xff);
		logHexByte((w >> 8) & 0xff);

		if (i % 16 == 0)
		{
			_logt('\r');
			_logt('\n');
		}
		#endif

		boot_page_fill (page + i, w);
	}

	#ifdef DebugServer
	_logt('\r');
	_logt('\n');
	#endif

	WATCHDOG_RESET();

	boot_page_write (page);     // Store buffer in flash page.
	boot_spm_busy_wait();       // Wait until the memory is written.

	WATCHDOG_RESET();

	// Reenable RWW-section again. We need this if we want to jump back
	// to the application after bootloading.

	boot_rww_enable ();

	// Re-enable interrupts (if they were ever enabled).
	SREG = sreg;
}

bool BLSPARE isHTTPOK()
{
	return qscanf(getResponse(), "HTTP/1.1 200 %e") == 1;
}

bool BLSPARE isHTTPPartial()
{
	return qscanf(getResponse(), "HTTP/1.1 206 %e") == 1;
}

bool BLSPARE parseFileInfoHeaders(char* header)
{
	if (qscanf(header, "Content-Length: %d", &contentLength) == 1)
	{
		return true;
	}

	return false;
}

bool BLSPARE getFirmwareLength(unsigned int version)
{
	byte retries;

	for (retries = 0; retries < HTTP_RETRIES; retries++, delay_secs(HTTP_PAUSE_BETWEEN_RETRIES))
	{
		contentLength = 0;

		// first send request to server to get file information
		if (sendHTTP("HEAD /firmware/%d HTTP/1.1\r\nHost: %s\r\nUser-Agent: logger/1.0\r\n\r\n", 20, parseFileInfoHeaders, version, FIRMWARE_HOST_URL) != 1)
		{
			continue;
		}

		// now we can parse response
		if (!isHTTPOK())
		{
			continue;
		}

		// content length is found
		if (contentLength != 0)
		{
			return true;
		}
	}

	return false;
}

bool BLSPARE parseFileChunkHeaders(char* header)
{
	if (qscanf(header, "Content-Length: %d", &contentLength) == 1)
	{
		return true;
	}

	return false;
}

bool flashCmp(const byte* mem, unsigned short flash, unsigned short length)
{
	byte b;

	while (length > 0)
	{
		b = pgm_read_byte(flash);
		if (b != *mem) return false;
		flash++;
		mem++;
		--length;
	}
	
	return true;
}

bool BLSPARE downloadFirmware(unsigned int version)
{
	unsigned int offset;
	unsigned int length;
	unsigned int chunkLength;
	byte retries;
	byte* httpBody;

	// first ask server for firmware file length
	if (!getFirmwareLength(version)) return false;

	// now start getting file by chunks of SPM_PAGESIZE bytes
	offset = 0;
	length = contentLength;

	while (offset < length)
	{
		for (retries = 0; retries < HTTP_RETRIES; retries++, delay_secs(HTTP_PAUSE_BETWEEN_RETRIES))
		{
			chunkLength = (length - offset) > (SPM_PAGESIZE * FLASH_PAGES_PER_REQUEST) ? (SPM_PAGESIZE * FLASH_PAGES_PER_REQUEST) : (length - offset);
			contentLength = 0;

			// first send request to server to get file information
			if (sendHTTP("GET /firmware/%d HTTP/1.1\r\nHost: %s\r\nRange: bytes=%d-%d\r\nUser-Agent: logger/1.0\r\n\r\n", 80, parseFileChunkHeaders,
				version, FIRMWARE_HOST_URL, offset, offset + chunkLength - 1) != 1)
			{
				continue;
			}

			// now we can parse response
			if (!isHTTPPartial())
			{
				continue;
			}

			_logd("Chunk: l %d req %d body %d", contentLength, chunkLength, getHttpBodyLength());
			// check that we received exact number of bytes requested
			if (getHttpBodyLength() != chunkLength)
			{
				continue;
			}

			httpBody = (byte*)getHttpBody();

			// now we can flush buffer to flash
			while (chunkLength > 0)
			{
				contentLength = chunkLength > SPM_PAGESIZE ? SPM_PAGESIZE : chunkLength;

				boot_program_page(offset, httpBody, contentLength);

				offset += contentLength;
				chunkLength -= contentLength;
				httpBody += contentLength;
			}

			break;
		}

		if (retries >= HTTP_RETRIES)
		{
			return false;
		}
	}

	_logs("DF OK");

	//
	// we successfully burned the firmware
	return true;
}

bool BLSPARE updateFirmwareStatus(byte status)
{
	byte retries;

	for (retries = 0; retries < HTTP_RETRIES; retries++, delay_secs(HTTP_PAUSE_BETWEEN_RETRIES))
	{
		contentLength = 0;

		// first send request to server to get file information
		if (sendHTTP("PUT /status/%l/%d/%d/%s HTTP/1.1\r\nHost: %s\r\nUser-Agent: logger/1.0\r\n\r\n", 30, NULL,
			appEepromData.eLoggerID, status, appHeader._version, getVersion(), FIRMWARE_HOST_URL) < 0)
		{
			continue;
		}

		// now we can parse response
		if (!isHTTPOK())
		{
			continue;
		}

		return true;
	}

	return false;
}
