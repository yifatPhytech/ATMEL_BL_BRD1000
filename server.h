#ifndef SERVER_H_
#define SERVER_H_

// number of retries of HTTP request
#define HTTP_RETRIES				3
#define HTTP_PAUSE_BETWEEN_RETRIES	2

// number of pages to download per single request (this is up to RX buffer size)
#define FLASH_PAGES_PER_REQUEST		6

// firmware statuses
#define FW_STATUS_UNKNOWN		0
#define FW_STATUS_READY			1
#define FW_STATUS_PENDING		2
#define FW_STATUS_UPDATING		3
#define FW_STATUS_FAILURE		4

// downloads firmware from the specified url
bool downloadFirmware(unsigned int version);
bool updateFirmwareStatus(byte status);

#endif /* SERVER_H_ */