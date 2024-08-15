#ifndef __MODEM_MANAGER_H__
#define __MODEM_MANAGER_H__

#include "define.h"

#define AT_T    '0'
#define VERIZON '1'

//#define FIRMWARE_HOST_URL		"bootloader.phytech.com"  //"bootloader-staging.herokuapp.com"  //"phytech.herokuapp.com"	//"phytech.dev.valigar.co.il"
#define FIRMWARE_HOST_URL		"bootloader-staging.herokuapp.com" //"bootloader.dev.phytech.com"		

struct OperatorData
{
    BYTE Status;
    BYTE AccTech;
    char MccMncStr[10];
};

//
// modem manager API
bool turnModemOn(bool cli);
bool initModem();
bool connectModem(bool cycleOperators);
bool closeModem();
bool isModemOn();
char* getResponse();
char* getHttpBody();
int getHttpBodyLength();
//
// Low level commands interface
typedef bool (*fnParseResponse)(char* response);

sbyte sendAT(char *bufToSend, int timeout, fnParseResponse parseResponse);
sbyte sendAT1(const char *bufToSend, int timeout, fnParseResponse parseResponse, ...);
sbyte sendAT2(const char *bufToSend, int timeout, int retries, int delay, int condition, fnParseResponse parseResponse, ...);
sbyte sendHTTP(const char *bufToSend, int timeout, fnParseResponse parseHeaders, ...);

#endif