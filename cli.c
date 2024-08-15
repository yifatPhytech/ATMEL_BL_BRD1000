#include <avr/io.h>
#include "define.h"

#ifdef Cli

char cli_buf[100];
byte cli_count = 0;

void cliHandle(char* cmd);

void BLSPARE cliTx(char c)
{
	while ((UCSR1A & DATA_REGISTER_EMPTY)==0);
	UDR1 = c;
}

void BLSPARE cliTask()
{
	char b;
	BYTE status;
	
	// check that we have some RX on DBG uart
	if ((UCSR1A & RX_COMPLETE) == 0) return;
	
	// get char from
	status = UCSR1A;
	b = UDR1;

	// do nothing on error in RX
	if ((status & (FRAMING_ERROR | PARITY_ERROR | DATA_OVERRUN)) != 0)
	{
		return;
	}
	
	if (b == '\r' || b == '\n')
	{
		if (cli_count > 0)
		{
			cliTx('\r');
			cliTx('\n');
			
			cliHandle(cli_buf);
			cli_count = 0;
			cli_buf[0] = 0;
		}
	}
	else
	{
		if (cli_count == 0 && b == 32)
		{
			return;
		}
		
		if (b == 8) // backspace
		{
			if (cli_count > 0)
			{
				cliTx(b);
				cliTx(' ');
				cliTx(b);
				
				cli_count--;
			}
		}
		else if (cli_count < sizeof(cli_buf))
		{
			cliTx(b);
			cli_buf[cli_count++] = b;
		}
		
		cli_buf[cli_count] = 0;
	}
}

#endif
