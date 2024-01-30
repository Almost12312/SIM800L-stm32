/*
 * Sim800L.h
 *
 *  Created on: Jan 16, 2024
 *      Author: dmitry
 */

#ifndef INC_SIM800L_H_
#define INC_SIM800L_H_

#define MAX_ERRORS_FOR_RESTART 10

#include "stm32f1xx.h"

#define SIM_ARR_SIZE 200

enum HTTPStatuses {
	NotAwaited,
	Sended,
	ErrorWhileSending,
	Success,
	BadRequest,
	UndefinedError,
	Error600,
};

typedef struct SIMPin {
	GPIO_TypeDef* GPIOx;
	uint8_t pin;
} SIMPin;

struct GPRSStatus {
	uint8_t Session;
	uint8_t Status;
};

struct BaseUrl {
	char url[50];
	uint8_t size;
};

struct GPRSSetting {
	char apn[30];
	uint8_t apn_size;
	char user[3];
	uint8_t user_size;
	char pwd[3];
	uint8_t pwd_size;
};

struct URLParam {
	char dataSize[4];
	uint8_t sizeLength;
	uint8_t seconds;
};

struct Json {
	char json[SIM_ARR_SIZE];
	uint8_t size;
};

enum GPRSSessionStatus {
	NotOpened = '0',
	Opened = '1',
	Closing = '2',
	Closed = '3'
};

enum GPRSConnectionStatus {
	NotConnect = '0',
	Connected = '1',
	PassingConnected = '2',
	GPRS = '3',
};

enum HTTPActions {
	GET = '0',
	POST = '1',
};

class SIM800L {
public:
	UART_HandleTypeDef* huart;

	bool isReceive;
	bool isSend;

	SIMPin reset;
	bool isHTTPInit;

	uint8_t rx_buffer[SIM_ARR_SIZE];

	GPRSStatus status;
	GPRSSetting settings;
	uint8_t jsonDownloadSecondsDelay;

	Json lastJson;
	BaseUrl url; // TODO: realize

	uint8_t errorCounter;

	SIM800L(UART_HandleTypeDef* huart, GPIO_TypeDef *GPIOx, uint16_t pinNumber, const char* url);

	bool simReadyState();

	bool AT();

	bool initGPRS(); 													// work
	bool setGPRSAPN(const char APN[], uint8_t apn_size); 				// work
	bool setGPRSUser(const char USER[], uint8_t user_size); 			// work
	bool setGPRSPassword(const char PASSWORD[], uint8_t password_size);	// work
	bool startGPRS();													// work
	bool closeGPRS(); 													// work
	bool setGPRSSettings();

	bool getGPRSStatus();										// work

	bool initHTTP(); 											// work
	bool setHTTPIdentifier();									// work
	bool setHTTPURL(const char URL[], uint16_t url_size);		// work. AT command for setting url
	bool setURL();												// work. set SIM800L.url property
	bool setContentTypeJson();									// work
	bool setHTTPDATA(URLParam& u);								// work
	bool setHTTPSSL();											// work
	bool defaultHTTPSettings();									// work

	bool downloadHTTPCredentials(const char params[], uint8_t usefulTXSize);	// work
	void checkDownload();														// not work
	bool downloadJson(const char json[], uint16_t json_size);					// work
	bool downloadTemp(int32_t temp);

	HTTPStatuses sendHTTP(HTTPActions a);										// work
	HTTPStatuses resendJson();
	bool closeHTTP(); 															// work

	bool readHTTPResponse();													// work
	/*
	 * level it is string number.
	 *
	 * WARNING!
	 * Dont use it. Lib is not adapted for this func
	 */
	bool setErrorLevel(const char* level);

	bool resetModule();
	uint8_t parseUint(char buffer[], uint32_t target, uint8_t parseSize);
private:
	void initUrl();

	void sendHTTPPARACredentials(char command[], const char params[], uint8_t usefulTXSize);
	/*
	 * usefulParamsSize it's means:
	 * how many new bytes this "const char params[]" added to send tx
	 *
	 * see SIM800L::SetGPRS() to understand
	 */
	void sendBearerCredentials(char command[], const char params[], uint8_t usefulTXSize);
//	void bearerSend(const char bearerSetting[], uint16_t delay);

	bool parseOK(uint16_t size);
	bool parseError(uint16_t size);
	bool parseDOWNLOAD(uint16_t size);
	void parseAndSetGPRSStatus();
	bool parseCloseGPRSStatus();
	bool parse200();
	bool parse600();
	bool parse400();


	bool parseCPINReady();
	bool parseCallReady();
	bool parseSMSReady();

	bool checkGPRSStarted();
	bool checkGPRSClosed();
	bool checkErrors();

	void read(uint16_t size, uint16_t delayMS);
	void send(const char* msg, uint16_t size, uint16_t delayMS);
	void readIT(uint16_t size);
	void sendIT(const char* msg, uint16_t size);

	bool retry(const char* msg, uint16_t size, uint16_t sizeReceive, uint32_t timeoutMS = 0, bool isOnce = false);

	bool waitReceive(uint16_t timeoutMS = 0);
	bool waitReceiveLong(uint32_t timeoutMS = 0);
	bool waitReceiveLess(uint8_t timeoutMS = 0);

};


#endif /* INC_SIM800L_H_ */
