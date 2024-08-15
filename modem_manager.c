#include "define.h"
#include "clock.h"
#include "cli.h"
#include "modem_manager.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#define _logd(str, ...) logd(str, ## __VA_ARGS__ )
#define _logs(str) logStr(str)
#define _logr(str) logStrRaw(str)
#define _logt(t) logTx(t)

#define MODEL_GSM					0
#define MODEL_4D					1

#define CONDITION_GT0				100

char atCommand[2048];				// output/input buffer for modem requests/responses
int atCommandCount;					// number of bytes in buffer received
int httpBodyStart;					// start of http body
struct OperatorData OprtTbl[10];	// these are cellular operators found
BYTE numOprt = 0;					// total number of cellular operators
BYTE curOprt = 0;					// current cellular operator
BYTE simStatus = 0;					// sim status (present/not present)
BYTE modemModel = MODEL_GSM;		// modem model
unsigned char ICCID[21];			// sim unique ID
unsigned char CellID[6];			// cell id 5 chars + 0
bool isRoaming;						// set when operator roaming is required
BYTE rssi_val;						// quality of signal value
byte socket = 1;					// socket number used for communication

extern bool shortModemInit;

char* getResponse()
{
	return atCommand;
}

char* getHttpBody()
{
	return httpBodyStart < 0 ? NULL : atCommand + httpBodyStart;
}

int getHttpBodyLength()
{
	return httpBodyStart < 0 ? 0 : atCommandCount - httpBodyStart;
}

void turnOnIgnition()
{
    MODEM_3G_IGNITION_ON();
}

void turnOffIgnition()
{
    MODEM_3G_IGNITION_OFF();
}

void modemHwShdn()
{
    MODEM_PWR_DISABLE();	//MODEM_3G_SHUTDOWN_START();
 //   delay_ms(200);
 //   MODEM_3G_SHUTDOWN_STOP();
}

bool isModemOn()
{
    return ((PINC & (1 << 6)) == 0);
}

bool IsOK(char* str)
{
	return qscanf(str, "^OK\r\n%e") == 1;
}

bool IsError(char* str)
{
	return qscanf(str, "^ERROR\r\n%e") == 1;
}

bool IsConnect(char* str)
{
	return qscanf(str, "^CONNECT\r\n%e") == 1;
}

bool IsReady(char* str)
{
	return qscanf(str, "^RDY\r\n%e") == 1;
}

bool isVerizonConnection()
{
    return (modemModel == MODEL_4D);
}

//
// receives one char from modem UART
int modemRx()
{
    BYTE b;
    BYTE status;
    
	// check for character in the buffer
    if ((UCSR0A & RX_COMPLETE) == 0) return -1;
    
    status = UCSR0A;
    b = UDR0;

    // do nothing on error in RX
    if ((status & (FRAMING_ERROR | PARITY_ERROR | DATA_OVERRUN)) != 0)
    {
        return -1;
    }

    return b;
}

#define COMMAND_TYPE_MODEM		0
#define COMMAND_TYPE_HTTP		1

// send raw string to modem
sbyte sendRaw(char *bufToSend, int timeout, fnParseResponse parseResponse, byte commandType)
{
    unsigned short tick;
    byte b;
    sbyte lines;
    int prevLine;
    int i;
	bool parse;
	bool httpHeadersReceived;
    
    // output what is to be sent
	_logt('>');
	_logr(bufToSend);

    WATCHDOG_RESET();

    // transmit to local modem port
    while ((b = *bufToSend++) != 0)
    {
        // wait for UART's shift register to finish sending byte
        while(( UCSR0A & DATA_REGISTER_EMPTY)==0) { asm volatile("nop"); };
        
		UDR0 = b;
		//while(( UCSR0A & (1<<TXC0))==0);
    }
    
    tick = clock_getTicks();
	timeout *= 10;
	atCommand[0] = 0;
    atCommandCount = 0;
    lines = 0;
    prevLine = 0;
	httpHeadersReceived = false;
    
    // acquire entire response in local buffer
    while (1)
    {
        WATCHDOG_RESET();

		// check for timeout
		if (clock_tickDiff(tick) >= timeout)
		{
			if (commandType == COMMAND_TYPE_MODEM)
			{
				return -1;
			}

			break;
		}

        i = modemRx();
        if (i < 0)
        {       
            continue;
        }

		b = (byte)i;
        
        // prevent overflow
        if (atCommandCount >= sizeof(atCommand) - 1)
        {
            atCommandCount = 0;
        }             
        
        // append to buffer
        atCommand[atCommandCount++] = b;
        atCommand[atCommandCount] = 0;
                         
		if (commandType == COMMAND_TYPE_HTTP && httpHeadersReceived)
		{
			// for HTTP timeout indicates a time between chars
			tick = clock_getTicks();
			timeout = 4;	//2;
		}

        if (b != '\n')
        {
	        continue;
        }
          
        // skip empty lines
        if (atCommandCount - prevLine <= 2)
        {
			httpHeadersReceived = true;
			tick = clock_getTicks();
			timeout = 4;	//2;
        }
		else if (commandType == COMMAND_TYPE_MODEM)
		{
			// for modem command wait until OK or ERROR response
			if (IsOK(atCommand + prevLine))
			{
				break;
			}
            
			if (IsError(atCommand + prevLine))
			{
				_logs("<ERROR");
				return -1;
			}

			if (IsConnect(atCommand + prevLine))
			{
				break;
			}
			
			if (IsReady(atCommand + prevLine))
				break;
		}

        // remember pointer to this line
        prevLine = atCommandCount;
    }
    
    prevLine = 0;
    i = 0;
	httpBodyStart = -1;
    
    // now parse response
    while (i < atCommandCount)
    {
        // find end of line
        if (atCommand[i++] != '\n')
        {
            continue;
        }
        
        // we found end of line, parse it
        b = atCommand[i];
        atCommand[i] = 0;
		parse = false;
            
		_logt('<');
        _logr(atCommand + prevLine);
            
		// remember start of message body
		if (commandType == COMMAND_TYPE_HTTP)
		{
			if ((i - prevLine) <= 2)
			{
				atCommand[i] = b;
				httpBodyStart = i;
				atCommand[prevLine] = 0;
				break;
			}

			parse = true;
		}
		else if (commandType == COMMAND_TYPE_MODEM)
		{
			if ((i - prevLine) > 2 && !IsOK(atCommand + prevLine) && !IsConnect(atCommand + prevLine))
			{
				parse = true;
			}
		}

		if (parse)
		{
			if (parseResponse != NULL)
			{
				if (parseResponse(atCommand + prevLine))
				{
					_logs(":)");
					lines++;
				}
				else
				{
					_logs(":(");
				}
			}
		}
        
        atCommand[i] = b;
            
        // remember pointer to this line
        prevLine = i;
    }        
         
    _logd("=> %d", lines);
    return lines;
}

// send raw AT string to modem
sbyte sendAT(char *bufToSend, int timeout, fnParseResponse parseResponse)
{
	return sendRaw(bufToSend, timeout, parseResponse, COMMAND_TYPE_MODEM);
}

//send string to modem
sbyte sendAT1(const char *bufToSend, int timeout, fnParseResponse parseResponse, ...)
{
	va_list args;

	va_start(args, parseResponse);
	qprintfv(atCommand, bufToSend, args);
	va_end(args);

    return sendAT(atCommand, timeout, parseResponse);
}

sbyte sendAT2(const char *bufToSend, int timeout, int retries, int delay, int condition, fnParseResponse parseResponse, ...)
{
    int i;
    sbyte r;
	va_list args;	
    
    for (i = 0; i < retries; i++)
    {
		va_start(args, parseResponse);
		qprintfv(atCommand, bufToSend, args);
		va_end(args);

        r = sendAT(atCommand, timeout, parseResponse);
        if (condition == CONDITION_GT0)
        {
            if (r > 0)
            {
                return r;
            }
        }
        else if (r == condition)
        {
            return r;
        }
        
        delay_secs(delay);
    }
    
    return -1;
}

sbyte sendHTTP(const char *bufToSend, int timeout, fnParseResponse parseHeaders, ...)
{
	va_list args;
	va_start(args, parseHeaders);
	qprintfv(atCommand, bufToSend, args);
	va_end(args);

	return sendRaw(atCommand, timeout, parseHeaders, COMMAND_TYPE_HTTP);
}

bool BLSPARE turnModemOn(bool cli)
{
    byte retries;

    // turn on modem            
    for (retries = 0; retries < 3; retries++)
    {
		_logd("Setting modem on. R %d", retries + 1);

        if (!isModemOn() || retries > 0 || cli)
        { 
			_logs("Ignite modem");
			MODEM_PWR_ENABLE();
			delay_secs(1);
            turnOnIgnition();
            delay_secs(SEC_4_GSM_IGNITION);
            turnOffIgnition();
			delay_secs(15);
        }

_logd("after ignition");
		//sendAT("", 15, NULL);
		_logd("Modem is %d", isModemOn());

		if (cli)
		{
			return isModemOn();
		}
    
        if (isModemOn())
        {
			delay_secs(1);

            if (sendAT2("AT\r\n", 3, 3, 2, 0, NULL) == 0)
            {
                return true;
            }
        }
        
        modemHwShdn();
        delay_secs(1);
    }
    
    return false;
}

bool parseQSSResponse(char* response)
{
    if (qscanf(response, "+QSIMSTAT: %b,%b", NULL, &simStatus) == 2)
    {
        return true;
    }
    
    return false;
}

/*bool parseModemModelResponse(char* response)
{
    if (qscanf(response, "LE910-SVL%e") == 1)
    {
        modemModel = MODEL_4D;
    }
    else {
        modemModel = MODEL_GSM;
    }
    
    return true;
}*/

bool parseCCIDResponse(char* response)
{
    byte n = 0;
    
    // find begining of SIM num
    while ((*response < '0' || *response > '9') && *response != 0) response++;

    // check for vaild response    
    if (*response == 0) return false;
           
    // copy sim number
    while (*response >= '0' && *response <= '9' && *response != 0 && n < sizeof(ICCID) - 1)
    {
        ICCID[n++] = *response++;
    }

    // pad with start     
    for (; n < sizeof(ICCID) - 1; n++)
    {
        ICCID[n] = '*';
    }
    
    ICCID[n] = 0;

    return true;
}

bool parseCOPSResponse(char* response)
{
    BYTE status;
    BYTE accTech;
    char mccMncStr[10];
    char sOprt[10];
    byte i;
	byte foundCops;
        
    if (qscanf(response, "+COPS: %p", &response) != 1)
    {
	    return false;
    }

	foundCops = 0;

	while (*response == '(')
	{
		if (qscanf(response, "(%b,\"%s\",\"%s\",\"%s\",%b)%p", &status, NULL, &sOprt, &mccMncStr, &accTech, &response) != 5)
		{
			break;
		}

		// make sure no such operator already added and there is a place for operators
		if (numOprt < sizeof(OprtTbl) / sizeof(OprtTbl[0]))
		{
			for (i = 0; i < numOprt; i++)
			{
				if (strcmp(OprtTbl[i].MccMncStr, mccMncStr) == 0)
				{
					goto foundDuplicate;
				}
			}
        
			OprtTbl[numOprt].Status = status;
			OprtTbl[numOprt].AccTech = accTech;
			strcpy(OprtTbl[numOprt].MccMncStr, mccMncStr);
        
			numOprt++;
			foundCops++;
		}

	foundDuplicate:
		// we expect comma at the end of single COP definition
		if (*response != ',')
		{
			break;
		}

		response++;
	}
    
    return foundCops > 0;
}

bool sendOperatorSelection()
{
    char* copMccMnc;
	char tmpMccMnc[10];
    
    if (appEepromData.eUseCntrCode == 1)
    {
		qprintf(tmpMccMnc, "%#%#",
			appEepromData.eMobileCntryCode, sizeof(appEepromData.eMobileCntryCode) - 1,
			appEepromData.eMobileNetCode, sizeof(appEepromData.eMobileNetCode) - 1);
		copMccMnc = tmpMccMnc;
    }
    else
    {
		copMccMnc = OprtTbl[curOprt].MccMncStr;
    }

    return sendAT1("AT+COPS=1,2,\"%s\"\r\n", 2, NULL, copMccMnc) == 0;    
}

bool parseXREGResponse(char* response, char* fmt)
{
	BYTE mode;
	BYTE stat;
	
	if (qscanf(response, fmt, &mode, &stat) != 2)
	{
		return false;
	}
	
	return (stat == 1 || stat == 5);
}

bool parseCREGResponse(char* response)
{
	return parseXREGResponse(response, "+CREG: %b,%b");
}

bool parseCGREGResponse(char* response)
{
	return parseXREGResponse(response, "+CGREG: %b,%b");
}

bool parseCEREGResponse(char* response)
{
	return parseXREGResponse(response, "+CEREG: %b,%b");
}

bool parseMONIResponse(char* response)
{
    char tmp[10];
	byte i;

	while (response[0] != 0 && (response[0] != 'I' || response[1] != 'd' || response[2] != ':')) response++;

	if (response[0] == 0) return false;
	
	if (qscanf(response, "Id:%s ", tmp) != 1)
	{
		return false;
	}

	for (i = 0; i < sizeof(CellID) - 1; i++)
	{
		CellID[i] = (tmp[i] != 0) ? tmp[i] : '*';
	}

	CellID[i] = 0;
	return true;
}

bool parseCSQResponse(char* response)
{
	BYTE rssi;
	BYTE ber;
	
	if (qscanf(response, "+CSQ: %b,%b", &rssi, &ber) != 2)
	{
		return false;
	}

	rssi_val = rssi;
	
	return true;
}

bool parseContextStatesResponse(char* response)
{
	BYTE cid;
	BYTE status;
	
	if (qscanf(response, "#SGACT: %b,%b", &cid, &status) != 2)
	{
		return false;
	}

	if (cid != socket)
	{
		return false;
	}
	
	return status == 1;
}

bool initModem()
{
    _logs("INIT");

	rssi_val = 0;
    
    // query SIM status
/*    if (sendAT("AT+QSIMSTAT?\r\n", 2, parseQSSResponse) != 1)
    {
        return false;
    }
    
    if (simStatus != 1)
    {                
        return false;
    }*/
    
    // query modem model
/*    if (sendAT("AT+GMM\r\n", 2, parseModemModelResponse) < 0)
    {
        return false;
    }
  */  
    // get modem CCID
/*    if (sendAT("AT#CCID\r\n", 3, parseCCIDResponse) != 1)
    {
        return false;
    }
*/
	// skip other initialization on modem init and assume it is initialized
	if (shortModemInit)
	{
		return true;
	}

    // detect cellular operator automatically
    if (sendAT("AT+COPS=0\r\n", 3, NULL) < 0)
    {
        return false;
    }

	// for general GSM modem always go to roaming
    isRoaming = (appEepromData.eUseCntrCode == 0) && !isVerizonConnection();

	curOprt = 0;
	numOprt = 0;

    if (appEepromData.eUseCntrCode == 0 && isRoaming)
    {
		//SUB_TASK_INIT_MODEM_COPS_LST

		// get operators
		if (sendAT2("AT+COPS=?\r\n", 90, 15, 10, CONDITION_GT0, parseCOPSResponse) <= 0)
		{
			return false;
		}
		    
		// no operators found
		if (numOprt == 0)
		{
			return false;
		}
    }

    _logs("INIT OK");
	return true;
}

/*static bool SendPDPCntDef()
{
	socket = isVerizonConnection() ? 3 : 1;

	return sendAT1("AT+CGDCONT=%b,\"IP\",\"%#\"\r\n", 2, NULL, socket, appEepromData.eAPN, sizeof(appEepromData.eAPN) - 1) == 0;
}*/

static bool SendStartDial()
{
	int retry;

	for (retry = 0; retry <= 3; retry++)
	{
		//if (sendAT1("AT#SD=%b,0,80,\"%s\"\r\n", 120, NULL, socket, FIRMWARE_HOST_URL) >= 0)
		if (sendAT1("AT+QIOPEN=1,0,\"TCP\",\"%s\",80,0,2\r\n", 40, NULL ,FIRMWARE_HOST_URL) >= 0)
		{
			return true;
		}

		delay_secs(1);
	}

	return false;
}

bool modemRegisterNextOperator()
{    
	// skip initialization that could be already performed
	if (!shortModemInit)
	{
		sbyte regstatus = -1;

		if (isRoaming)
		{
			if (curOprt >= numOprt && !appEepromData.eUseCntrCode)
			{
				return false;
			}
	
			// try to select the operator
			if (!sendOperatorSelection())
			{
				return false;
			}
		}

		// perform registration
		delay_secs(10);

		if (!isVerizonConnection())
		{
			//SUB_TASK_INIT_MODEM_REG
			sendAT2("AT+CREG?\r\n", 3, 30, 4, 1, parseCREGResponse);
		}
        
		// validate GPRS registration status

		//SUB_TASK_INIT_MODEM_REG_STAT
		regstatus = sendAT2("AT+CGREG?\r\n", 3, 15, 4, 1, parseCGREGResponse);
        
		if (regstatus != 1)
		{
			return false;
		}

		// if we here, registration is succeeded
                                  
		// SUB_TASK_INIT_MODEM_REG_LTE
		//if (isVerizonConnection())
		//{   
			//if (sendAT2("AT+CEREG?\r\n", 3, 15, 4, 1, parseCEREGResponse) != 1)
			//{
				//return false;
			//}
		//}
    
		// SUB_TASK_INIT_MODEM_GET_COPS
		if (sendAT("AT+COPS?\r\n", 3, NULL) < 0)
		{
			return false;
		}
    
		// SUB_TASK_INIT_MODEM_MONITOR
		//if (sendAT("AT#MONI\r\n", 3, parseMONIResponse) != 1)
		if (sendAT("AT+QNWINFO\r\n", 3, parseMONIResponse)!= 1)
		{
			return false;
		}

		// SUB_TASK_INIT_MODEM_RSSI
		if (sendAT("AT+CSQ\r\n", 3, parseCSQResponse) != 1)
		{
			return false;
		}

		// stop connection is RSSI is too low
		if (rssi_val < 12)
		{
			return false;
		}

//		if (!isVerizonConnection())
		{
			// SUB_TASK_MODEM_CONNECT_ATCH
			if (sendAT2("AT+CGATT=1\r\n", 2, 2, 1, 0, NULL) < 0)
			{
				return false;
			}
		}

		// SUB_TASK_MODEM_CONNECT_PDP_DEF
		//if (!SendPDPCntDef()) 
			//return false;

		// SUB_TASK_MODEM_CONNECT_SETUP1
		//if (sendAT1("AT+CGQMIN=%b,0,0,0,0,0\r\n", 2, NULL, socket) < 0)
		//{
			//return false;
		//}
//
		//// SUB_TASK_MODEM_CONNECT_SETUP2
		//if (sendAT1("AT+CGQREQ=%b,0,0,0,0,0\r\n", 2, NULL, socket) < 0)
		//{
			//return false;
		//}
			
		// SUB_TASK_MODEM_CONNECT_SETUP3
		//if (sendAT1(isVerizonConnection() ? "AT#SCFG=%b,3,1500,90,450,3\r\n" : "AT#SCFG=%b,1,1500,90,450,30\r\n", 2, NULL, socket) < 0)
		//{
			//return false;
		//}

		// deactivate previous PDP context (this releases stuck PDP context)
		sendAT1("AT+CGACT=%b,0\r\n", 10, NULL, socket);

		// SUB_TASK_MODEM_CONNECT_ACTV
		if (sendAT2("AT+CGACT=%b,1\r\n", 10, 10, 1, 0, NULL, socket) < 0)
		{
			return false;
		}
	}
	else
	{
		// SUB_TASK_INIT_MODEM_RSSI
		if (sendAT("AT+CSQ\r\n", 3, parseCSQResponse) != 1)
		{
			return false;
		}

		// stop connection is RSSI is too low
		if (rssi_val < 10)
		{
			return false;
		}

		// check context activation status and break initialization if context is not activated
//		if (sendAT1("AT+CGACT?\r\n", 10, parseContextStatesResponse) != 1)
		//if (sendAT1("AT+CGATT=1\r\n", 2, NULL) != 1)
		//{
			//_logs("failed");
			//return false;
		//}
	}

	// SUB_TASK_MODEM_CONNECT_START_DIAL
	if (!SendStartDial())
	{
		return false;
	}

    return true;
}

bool connectModem(bool cycleOperators)
{
	_logs("CONNECT");

	// we always start from the first operator
	curOprt = 0;

	while (true)
	{
		if (modemRegisterNextOperator())
		{
			break;
		}

		if (!cycleOperators || shortModemInit)
		{
			return false;
		}

		if (curOprt >= numOprt || appEepromData.eUseCntrCode || !isRoaming)
		{
			return false;
		}
	
		curOprt++;
	}

	_logs("CONNECT OK");
	return true;
}

bool closeModem()
{
	byte retry = 0;

	if (!isModemOn())
	{
		return true;
	}

	_logs("CLOSE");

	// try to close data stream if it is opened
	// SUB_TASK_MODEM_CLOSE_EOD
	sendAT("+++\r\n", 1, NULL);
	
	// answer with OK indicates about opened TCP connection, close it
	// SUB_TASK_MODEM_CLOSE_TCP
	//sendAT2("AT#SH=%b\r\n", 1, 2, 1, 0, NULL, socket);
	sendAT("AT+QICLOSE=0\r\n",1, NULL );

	// now close modem
//	sendAT("AT#SHDN\r\n", 1, NULL);
	sendAT("AT+QPOWD\r\n", 1, NULL);

	// wait additional time
	for (retry = 0; retry < 15; retry++)
	{
		if (!isModemOn()) break;

		delay_secs(1);
	}

	// perform hardware shutdown if needed
	if (isModemOn())
	{
		modemHwShdn();
	}

	_logs("CLOSE OK");
	return true;
}
