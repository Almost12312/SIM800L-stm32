/*
 * SIM800L.cpp
 *
 *  Created on: Jan 16, 2024
 *      Author: dmitry
 */
#define CHARACTER_CARRIAGE_SIZE 1 	// character it's means '\r" - this is 1 in char
#define CHARACTER_NULL_SIZE 1		// character it's means '\n" - this is 1 in char

#define STANDART_DELAY 200
#define AWAIT_RESPONSE_DELAY 3000

#define AT_SIZE 2
#define AT_SIZE_SEND AT_SIZE + CHARACTER_NULL_SIZE
#define AT_SIZE_READ 8
#define AT_DELAY_SEND STANDART_DELAY
#define AT_DELAY_READ STANDART_DELAY

#define CMEE_0_STATUS_ERROR_SIZE 7
#define CMEE_SIZE 9
#define CMEE_SIZE_SEND CMEE_SIZE + CHARACTER_NULL_SIZE
#define CMEE_SIZE_READ CMEE_SIZE_SEND + CMEE_0_STATUS_ERROR_SIZE // 17
#define CMEE_DELAY_SEND STANDART_DELAY
#define CMEE_DELAY_READ STANDART_DELAY

#define SAPBR_SIZE 13
#define SAPBR_GPRS_DELAY_SEND STANDART_DELAY
#define SAPBR_GPRS_DELAY_READ STANDART_DELAY
#define SAPBR_GRPS_SIZE_READ 99 // TODO: it's for error reporting level 2 (cmee=2)

#define SAPBR_APN_DELAY_SEND STANDART_DELAY
#define SAPBR_APN_DELAY_READ STANDART_DELAY
#define SAPBR_APN_SIZE_READ 70 // TODO: it's for error reporting level 2 (cmee=2)

#define SAPBR_USER_DELAY_SEND STANDART_DELAY
#define SAPBR_USER_DELAY_READ STANDART_DELAY
#define SAPBR_USER_SIZE_READ 71 // TODO: 69 it's for error reporting level 2 (cmee=2)

#define SAPBR_PASSWORD_DELAY_SEND STANDART_DELAY
#define SAPBR_PASSWORD_DELAY_READ STANDART_DELAY
#define SAPBR_PASSWORD_SIZE_READ 70 // TODO: 69 it's for error reporting level 2 (cmee=2)

#define SAPBR_GETSTATUS_DELAY_SEND STANDART_DELAY
#define SAPBR_GETSTATUS_DELAY_READ STANDART_DELAY
#define SAPBR_GETSTATUS_SIZE_READ 70

#define SAPBR_STARTGPRS_DELAY_SEND 2000
#define SAPBR_STARTGPRS_DELAY_READ 10000
#define SAPBR_STARTGPRS_SIZE_READ 50

#define SAPBR_CLOSEBEARER_DELAY_SEND 2000
#define SAPBR_CLOSEBEARER_DELAY_READ 2000
#define SAPBR_CLOSEBEARER_SIZE_READ 50

#define SAPBR_CLOSEGPRS_DELAY_SEND 2000
#define SAPBR_CLOSEGPRS_DELAY_READ 2000
#define SAPBR_CLOSEGPRS_SIZE_READ 22
#define SAPBR_CLOSEGPRS_SIZE_SEND 11

#define HTTPINIT_SIZE 11
#define HTTPINIT_DELAY_SEND STANDART_DELAY + 100
#define HTTPINIT_DELAY_READ STANDART_DELAY
#define HTTPINIT_SIZE_READ 50
#define HTTPINIT_SIZE_SEND HTTPINIT_SIZE + CHARACTER_NULL_SIZE

#define HTTPPARA_SIZE 12
#define HTTPPARA_DELAY_SEND STANDART_DELAY
#define HTTPPARA_DELAY_READ STANDART_DELAY

#define HTTPPARA_CID_SIZE 7
#define HTTPPARA_CID_SIZE_READ HTTPPARA_SIZE + 50
#define HTTPPARA_CID_DELAY_READ STANDART_DELAY
#define HTTPPARA_CID_DELAY_READ STANDART_DELAY

#define HTTPPARA_URL_SIZE 7
#define HTTPPARA_URL_READ_DELAY STANDART_DELAY

#define HTTPSSL_SIZE 13
#define HTTPSSL_DELAY_SEND STANDART_DELAY
#define HTTPSSL_DELAY_READ STANDART_DELAY

#define HTTPDATA_SIZE 12
#define HTTPDATA_SIZE_SEND_RESPONSE 10

#define HTTPCONTYPEJSON_SIZE 28

#define HTTPACTION_SIZE 15
#define HTTPACTION_SIZE_SEND HTTPACTION_SIZE + 1
#define HTTPACTION_DELAY_SEND STANDART_DELAY

#define HTTPTERM_SIZE 11
#define HTTPTERM_SIZE_SEND HTTPTERM_SIZE + 1
#define HTTPTERM_SIZE_READ HTTPTERM_SIZE + 50
#define HTTPTERM_DELAY_SEND STANDART_DELAY
#define HTTPTERM_DELAY_READ STANDART_DELAY


#include "SIM800L.h"
#include "stm32f1xx_hal_uart.h"
#include "stm32f1xx_hal_gpio.h"


SIM800L::SIM800L(UART_HandleTypeDef* huart, GPIO_TypeDef *GPIOx, uint16_t pinNumber, const char* url) {
	this->huart = huart;
	this->isReceive = true;
	this->isSend = true;
	this->jsonDownloadSecondsDelay = 1;
	this->errorCounter = 0;

	this->reset.GPIOx = GPIOx;
	this->reset.pin = pinNumber;
	this->isHTTPInit = false;

	this->lastJson.json[0] = '\n';
	this->lastJson.size = 0;

	char apn[] = "\"m.tinkoff.ru\"";
	for (uint8_t i = 0; i < sizeof (apn) / sizeof (apn[0]); ++i) {
		this->settings.apn[i] = apn[i];
	}
	this->settings.apn_size = 15;

	char nullChar[] = "\"\"";
	for (uint8_t i = 0; i < sizeof (nullChar) / sizeof (nullChar[0]); ++i) {
		this->settings.pwd[i] = nullChar[i];
		this->settings.user[i] = nullChar[i];
	}
	this->settings.pwd_size = 3;
	this->settings.user_size = 3;

	this->initUrl();
	uint8_t i;
	for (i = 0; i < url[i]; ++i) {
		this->url.url[i] = url[i];
	}
	this->url.size = i+1;

	for(uint8_t i = 0; i < SIM_ARR_SIZE; ++i) {
		this->rx_buffer[i] = 0;
	}

	this->status.Session = GPRSSessionStatus::NotOpened;
	this->status.Status = GPRSSessionStatus::NotOpened;
}

bool SIM800L::simReadyState() {
	this->readIT(99);
	this->sendIT("AT+CREG?\n", 9);

	bool status = this->waitReceiveLess(STANDART_DELAY);
	if (status == false) {
		status = this->retry("AT+CREG?\n", 9, 99, STANDART_DELAY);
	}

//	status = this->parseCPINReady();

	return status;
}

bool SIM800L::AT() {
	this->readIT(AT_SIZE_READ);
	this->sendIT("AT\n", AT_SIZE_SEND);
//	this->sendIT("AT\n", AT_SIZE_SEND);

	bool status = this->waitReceiveLess(STANDART_DELAY);
	if (status == false) {
//		this->readIT(99);
//			this->sendIT("AT\n", AT_SIZE_SEND);
		status = this->retry("AT\n", AT_SIZE_SEND, AT_SIZE_READ, STANDART_DELAY);
	}

	return this->parseOK(AT_DELAY_READ);
}

// return shit
bool SIM800L::initGPRS() {
	char params[16 + CHARACTER_NULL_SIZE] = "\"CONTYPE\",\"GPRS\"";
	uint8_t usefulTXSize = (sizeof(params) / sizeof(params[0])) - CHARACTER_NULL_SIZE;

	uint8_t size = SAPBR_SIZE + usefulTXSize + CHARACTER_NULL_SIZE;
	char command[size] = "AT+SAPBR=3,1,";
	this->sendBearerCredentials(command, params, usefulTXSize);

	this->readIT(SAPBR_GRPS_SIZE_READ);
	this->sendIT(command, size);

	bool status = this->waitReceive(STANDART_DELAY);
	if (status == false) {
		this->retry(command, size, SAPBR_GRPS_SIZE_READ, STANDART_DELAY);
	}

	return this->parseOK(SAPBR_GRPS_SIZE_READ);
}

bool SIM800L::setGPRSAPN(const char APN[], uint8_t apn_size) {
	uint8_t startIndex = 6;
	uint8_t paramSize = startIndex + apn_size - CHARACTER_NULL_SIZE;

	char params[paramSize] = "\"APN\",";
	uint8_t usefulTXSize = (sizeof(params) / sizeof(params[0]));

	uint8_t size = SAPBR_SIZE + usefulTXSize + CHARACTER_NULL_SIZE;
	char command[size] = "AT+SAPBR=3,1,";

	for (uint8_t i = startIndex; i < size; ++i) {
		uint8_t x = i - startIndex;
		if (APN[x] != '\0') {
			params[i] = APN[x];
		}
	}

	this->sendBearerCredentials(command, params, usefulTXSize);

	this->readIT(SAPBR_APN_SIZE_READ + 29);
	this->sendIT(command, size);

	bool status = this->waitReceiveLess(STANDART_DELAY);
	if(status == false) {
		this->retry(command, size, SAPBR_APN_SIZE_READ, STANDART_DELAY);
	}

	return this->parseOK(SAPBR_APN_SIZE_READ);
}

bool SIM800L::setGPRSUser(const char USER[], uint8_t user_size) {
	uint8_t startIndex = 7;
	uint8_t paramSize = startIndex + user_size - CHARACTER_NULL_SIZE;

	char params[paramSize] = "\"USER\",";
	uint8_t usefulTXSize = (sizeof(params) / sizeof(params[0]));

	uint8_t size = SAPBR_SIZE + usefulTXSize + CHARACTER_NULL_SIZE;
	char command[size] = "AT+SAPBR=3,1,";

	for (uint8_t i = startIndex; i < size; ++i) {
		uint8_t x = i - startIndex;
		if (USER[x] == '\0') {
			break;
		}

		params[i] = USER[x];
	}

	this->sendBearerCredentials(command, params, usefulTXSize);

	this->readIT(SAPBR_USER_SIZE_READ);
	this->sendIT(command, size);

	bool status = this->waitReceiveLess(STANDART_DELAY);
	if (status == false) {
		this->retry(command, size, SAPBR_USER_SIZE_READ, STANDART_DELAY);
	}

	return this->parseOK(SAPBR_USER_SIZE_READ);
}

bool SIM800L::setGPRSPassword(const char PASSWORD[], uint8_t password_size) {
	uint8_t startIndex = 6;
	uint8_t paramSize = startIndex + password_size - CHARACTER_NULL_SIZE;

	char params[paramSize] = "\"PWD\",";
	uint8_t usefulTXSize = (sizeof(params) / sizeof(params[0]));

	uint8_t size = SAPBR_SIZE + usefulTXSize + CHARACTER_NULL_SIZE;
	char command[size] = "AT+SAPBR=3,1,";

	for (uint8_t i = startIndex; i < size; ++i) {
		uint8_t x = i - startIndex;
		if (PASSWORD[x] == '\0') {
			break;
		}

		params[i] = PASSWORD[x];
	}

	this->sendBearerCredentials(command, params, usefulTXSize);

	this->readIT(SAPBR_PASSWORD_SIZE_READ);
	this->sendIT(command, size);

	bool status = this->waitReceiveLess(STANDART_DELAY);
	if (status == false) {
		this->retry(command, size, SAPBR_PASSWORD_SIZE_READ, STANDART_DELAY); // TODO: can stuck here
	}

	return this->parseOK(SAPBR_PASSWORD_SIZE_READ);
}

/*
 * if connection is opened it's return true
 *
 * TODO: make middleware if opened
 */
bool SIM800L::startGPRS() {
	if (this->checkGPRSStarted()) {
		return true;
	}

	char msg[SAPBR_SIZE] = "AT+SAPBR=1,1";
	msg[SAPBR_SIZE-1] = '\n';

	this->readIT(SAPBR_STARTGPRS_SIZE_READ);
	this->sendIT(msg, SAPBR_SIZE);
//	this->bearerSend("1,1", SAPBR_STARTGPRS_DELAY_SEND);

	bool status = this->waitReceiveLong(1000); 					// wait success send
	bool isError = this->parseError(SAPBR_STARTGPRS_SIZE_READ);
	if(isError) { return false; }

	this->readIT(10);											// wait open
	status = this->waitReceiveLong(85000);
	if(status == false) {
		this->retry(msg, SAPBR_SIZE, SAPBR_STARTGPRS_SIZE_READ, 85000);
	}

	bool isOk = this->parseOK(SAPBR_STARTGPRS_SIZE_READ);
	if (!isOk) { return false; }

	bool opened = false;
	while (opened != true) {
		opened = this->getGPRSStatus();
		HAL_Delay(100);
	}

	return opened;
}

bool SIM800L::closeGPRS() {
	if (this->checkGPRSClosed()) {
		return true;
	}

	char msg[SAPBR_SIZE] = "AT+SAPBR=0,1";
	msg[SAPBR_SIZE-1] = '\n';

	this->readIT(SAPBR_CLOSEBEARER_SIZE_READ + 40);
	this->sendIT(msg, SAPBR_SIZE);
//	this->bearerSend("0,1", SAPBR_CLOSEBEARER_DELAY_SEND);


	bool status = this->waitReceive(1000); 							// wait trash
	bool isError = this->parseError(SAPBR_CLOSEBEARER_SIZE_READ);
	if(isError) { return false; }

	this->readIT(10);												// wait close
	status = this->waitReceiveLong(65000);
	if (status == false) {
		this->retry(msg, SAPBR_SIZE, SAPBR_CLOSEBEARER_SIZE_READ + 40, 65000);
	}

	bool isBearerClosed = this->parseOK(SAPBR_CLOSEBEARER_SIZE_READ);
	if (!isBearerClosed) { return false; }

	this->readIT(SAPBR_CLOSEGPRS_SIZE_READ + 70);
	this->sendIT("AT+CIPSHUT\n", SAPBR_CLOSEGPRS_SIZE_SEND);

	status = this->waitReceive();
	if (status == false) {
		this->retry(msg, SAPBR_SIZE, SAPBR_CLOSEBEARER_SIZE_READ + 40, 65000);
	}


	bool isSusccess = this->parseCloseGPRSStatus();
	if (!isSusccess) { return false; }

	return this->getGPRSStatus();
}

bool SIM800L::setGPRSSettings() {
	bool status = false;
	status = this->setGPRSAPN(this->settings.apn, this->settings.apn_size);
	status = this->setGPRSUser(this->settings.user, this->settings.user_size);
	status = this->setGPRSPassword(this->settings.pwd, this->settings.pwd_size);

	return status;
}

bool SIM800L::getGPRSStatus() {
	char msg[SAPBR_SIZE] = "AT+SAPBR=2,1";
	msg[SAPBR_SIZE-1] = '\n';

	this->readIT(SAPBR_GETSTATUS_SIZE_READ);
	this->sendIT(msg, SAPBR_SIZE);


	bool status = this->waitReceiveLess(100);
	if (status == false) {
		this->retry(msg, SAPBR_SIZE, SAPBR_GETSTATUS_SIZE_READ, STANDART_DELAY);
	}

	this->parseAndSetGPRSStatus();

	return this->checkGPRSStarted();

}

// TODO: make middleware. Ask AT-request to module is http inited or not
bool SIM800L::initHTTP() {
	char msg[HTTPINIT_SIZE + CHARACTER_NULL_SIZE] = "AT+HTTPINIT";
	msg[HTTPINIT_SIZE_SEND - CHARACTER_NULL_SIZE] = '\n';

	this->readIT(HTTPINIT_SIZE_READ);
	this->sendIT(msg, HTTPINIT_SIZE_SEND);

	bool status = this->waitReceive(1000);
	if (status == false) {
		this->retry(msg, HTTPINIT_SIZE_SEND, HTTPINIT_SIZE_READ, 1000);
	}


	return this->parseOK(HTTPINIT_SIZE_READ);
}

bool SIM800L::setHTTPIdentifier() {
	char params[HTTPPARA_CID_SIZE + CHARACTER_NULL_SIZE] = "\"CID\",1";
	uint8_t usefulTXSize = (sizeof(params) / sizeof(params[0]) - CHARACTER_NULL_SIZE);

	uint16_t size = HTTPPARA_SIZE + usefulTXSize;
	char command[size + CHARACTER_NULL_SIZE] = "AT+HTTPPARA=";
	this->sendHTTPPARACredentials(command, params, usefulTXSize);

	this->readIT(HTTPPARA_SIZE + 50); // TODO: replace to define
	this->sendIT(command, size + CHARACTER_NULL_SIZE);

	bool status = this->waitReceiveLess(STANDART_DELAY);
	if (status == false) {
		this->retry(command, size + CHARACTER_NULL_SIZE, HTTPPARA_SIZE + 50, STANDART_DELAY);
	}

	return this->parseOK(HTTPPARA_CID_SIZE_READ);
}

bool SIM800L::setHTTPURL(const char URL[], uint16_t url_size) {
	uint32_t paramSize = HTTPPARA_URL_SIZE + url_size;
	char params[paramSize + CHARACTER_NULL_SIZE] = "\"URL\",\"";

	for (uint16_t i = HTTPPARA_URL_SIZE; i < paramSize; ++i) {
			uint16_t x = i - HTTPPARA_URL_SIZE;
			if (params[x] == '\0') {
				break;
			}

			params[i] = URL[x];
		}

	params[paramSize-1] = '\"';
	uint16_t usefulTXSize = (sizeof(params) / sizeof(params[0])) - CHARACTER_NULL_SIZE;

	uint16_t size = HTTPPARA_SIZE + usefulTXSize + CHARACTER_NULL_SIZE;
	char command[size + CHARACTER_NULL_SIZE] = "AT+HTTPPARA=";
	this->sendHTTPPARACredentials(command, params, usefulTXSize);

	this->readIT(99); // TODO: replace to define
	this->sendIT(command, size);

	bool status = this->waitReceiveLess(200);
	if (status == false) {
//		this->readIT(99);
//		this->sendIT(command, size);
//
//		status = this->waitReceiveLess(200);
		this->retry(command, size, 99, 200); // TODO: can stuck here
	}

	return this->parseOK(99);
}

bool SIM800L::setURL() {
	return this->setHTTPURL(this->url.url, this->url.size);
}

bool SIM800L::setHTTPSSL() {
	char msg[HTTPSSL_SIZE] = "AT+HTTPSSL=1";
	msg[HTTPSSL_SIZE-1] = '\n';

	this->readIT(HTTPSSL_SIZE + 30);
	this->sendIT(msg, HTTPSSL_SIZE);

	bool status = this->waitReceiveLess(STANDART_DELAY);
	if (status == false) {
		this->retry(msg, HTTPSSL_SIZE, HTTPSSL_SIZE + 30, STANDART_DELAY);
	}

	return this->parseOK(HTTPSSL_SIZE + 30);
}

bool SIM800L::setHTTPDATA(URLParam& u) {
	uint16_t size = HTTPDATA_SIZE + u.sizeLength + 1 + 4; // + 1 it's is u.seconds real length
	char command[size + CHARACTER_NULL_SIZE] = "AT+HTTPDATA=";

	uint8_t startPos = HTTPDATA_SIZE;
	for (uint16_t i = HTTPPARA_SIZE; i < size - 3; ++i) {
		uint16_t x = i - HTTPPARA_SIZE;

		if (u.dataSize[x] != '\0') {
			command[startPos] = u.dataSize[x];
			startPos++;
		}
	}

	command[HTTPDATA_SIZE + u.sizeLength] = ',';
	command[HTTPDATA_SIZE + u.sizeLength + 1] = (u.seconds + '0');

	for (uint8_t i = 0; i < 4; ++i ) {
		uint8_t x = HTTPDATA_SIZE + u.sizeLength + 2 + i;
		command[x] = '0';
	}

	command[size] = '\n';

	this->readIT(size + HTTPDATA_SIZE_SEND_RESPONSE + 40);
	this->sendIT(command, size + CHARACTER_NULL_SIZE);

	bool status = this->waitReceiveLess(200);
	if (status == false) {
		this->retry(command, size + CHARACTER_NULL_SIZE, size + HTTPDATA_SIZE_SEND_RESPONSE + 40, 200);
	}

	return this->parseDOWNLOAD(size + 80);
}

void SIM800L::checkDownload() {
	this->readIT(99);
	this->sendIT("AT+HTTPDATA=?\n", HTTPDATA_SIZE + 2);

	this->waitReceive();
}

bool SIM800L::downloadJson(const char json[], uint16_t json_size) {
	URLParam u;
	u.seconds = this->jsonDownloadSecondsDelay;
	for (uint8_t i = 0; i < 4; ++i) {
		u.dataSize[i] = 0;
	}

	for (uint8_t i = 0; i < json_size - CHARACTER_NULL_SIZE; ++i) {
		this->lastJson.json[i] = json[i];
	}
	this->lastJson.size = json_size;

	uint8_t effected = this->parseUint(u.dataSize, json_size - CHARACTER_NULL_SIZE, 4);
	u.sizeLength = effected;

	bool status = this->setHTTPDATA(u);
	status = this->downloadHTTPCredentials(json, json_size - CHARACTER_NULL_SIZE);

	return status;
}

bool SIM800L::downloadTemp(int32_t temp) {
	char json[] = "{\"temp\": \"+90000\"}\n";
	uint8_t size = sizeof(json) / sizeof(json[0]);;

	if(temp < 0) {
		json[10] = '-';
		temp *= -1;
	}


	char charTemp[5];
	this->parseUint(charTemp, temp, 5);
	for (uint8_t i = 0; i < sizeof(charTemp) / sizeof(charTemp[0]); ++i) {
		if (charTemp[i] == '\0') {
			charTemp[i] = '0';
		}
	}

	for (uint8_t i = 11; i < 11 + 5; ++i) {
		uint8_t x = i - 11;
		json[i] = charTemp[x];
	}


	return this->downloadJson(json, size - CHARACTER_NULL_SIZE);
}

bool SIM800L::setContentTypeJson() {
	char params[HTTPCONTYPEJSON_SIZE + CHARACTER_NULL_SIZE] = "\"CONTENT\",\"application/json\"";
	uint8_t usefulTXSize = (sizeof(params) / sizeof(params[0]) - CHARACTER_NULL_SIZE);

	uint16_t size = HTTPPARA_SIZE + usefulTXSize;
	char command[size + CHARACTER_NULL_SIZE] = "AT+HTTPPARA=";
	this->sendHTTPPARACredentials(command, params, usefulTXSize);

	this->readIT(HTTPPARA_CID_SIZE_READ); // TODO: replace to define
	this->sendIT(command, size + CHARACTER_NULL_SIZE);

	bool status = this->waitReceiveLess(STANDART_DELAY);
	if (status == false) {
		this->retry(command, size + CHARACTER_NULL_SIZE, HTTPPARA_CID_SIZE_READ + 20, STANDART_DELAY);
	}

	return this->parseOK(HTTPPARA_CID_SIZE_READ);
}

bool SIM800L::downloadHTTPCredentials(const char params[], uint8_t usefulTXSize) {
	this->readIT(99);
	this->sendIT(params, usefulTXSize);

	this->waitReceive();

	return this->parseOK(99);
}

// make enum for statuses
HTTPStatuses SIM800L::sendHTTP(HTTPActions a) {
	uint16_t size = HTTPACTION_SIZE;
	char command[size + CHARACTER_NULL_SIZE] = "AT+HTTPACTION=";
	command[size - 1] = a;

	command[size] = '\n';

	this->readIT(size + 50);
	this->sendIT(command, 16);

	bool isAwait = this->waitReceive(AWAIT_RESPONSE_DELAY); // TODO: make timeout status
	if (isAwait == false) { return HTTPStatuses::NotAwaited; }

	bool isError = this->parseError(22);
	if(isError) { return HTTPStatuses::ErrorWhileSending; }

	bool isSended = this->parseOK(size + 30);
	if (isSended == false) { return HTTPStatuses::UndefinedError; }

	bool is600 = this->parse600();
	if (is600) {
		this->errorCounter += 1;
		this->checkErrors();

		return HTTPStatuses::Error600;
	}

	this->readIT(size + 30);
	this->waitReceive(); // TODO: can get 603 and stuck

	bool code = this->parse200();
	if (code) {
		return HTTPStatuses::Success;
	}

	code = this->parse400();
	if(code) {
		return HTTPStatuses::BadRequest;
	}

	return HTTPStatuses::UndefinedError;
}

HTTPStatuses SIM800L::resendJson() {	// TODO: realize
	bool status = this->defaultHTTPSettings();
	status = this->setHTTPURL(this->url.url, this->url.size);
	status = this->downloadJson(this->lastJson.json, this->lastJson.size);

	return this->sendHTTP(HTTPActions::POST);
}

bool SIM800L::defaultHTTPSettings() {
	bool status = false;
	status = this->setHTTPSSL();
	status = this->setContentTypeJson(); // TODO: can stuckin retry
	status = this->setHTTPIdentifier();
	status = this->setURL();

	return status;
}

bool SIM800L::closeHTTP() {
	char msg[HTTPTERM_SIZE_SEND] = "AT+HTTPTERM";

	this->sendIT("AT+HTTPTERM\n", HTTPTERM_SIZE_SEND);
	this->readIT(HTTPTERM_SIZE_READ);

	bool status = this->waitReceive(1000);
	if (status == false) {
		this->retry(msg, HTTPTERM_SIZE_SEND, HTTPTERM_SIZE_READ, 1000);
	}

	return this->parseOK(HTTPTERM_SIZE_READ);
}

bool SIM800L::readHTTPResponse() {
	this->readIT(199);
	this->sendIT("AT+HTTPREAD\n", 12);

	bool status = this->waitReceive();
	if (status == false) {
		this->retry("AT+HTTPREAD\n", 12, 199, STANDART_DELAY);
	}

	this->waitReceive();

	return this->parseOK(199);
}

void SIM800L::sendHTTPPARACredentials(char command[], const char params[], uint8_t usefulTXSize) {
	uint16_t size = HTTPPARA_SIZE + usefulTXSize;

	for (uint16_t i = HTTPPARA_SIZE; i < size; ++i) {
		uint16_t x = i - HTTPPARA_SIZE;
		if (params[x] == '\0') {
			break;
		}

		command[i] = params[x];
	}

	command[size] = '\n';
}

//void SIM800L::bearerSend(const char bearerSetting[], uint16_t delay) {
//	uint16_t size = SAPBR_SIZE - CHARACTER_NULL_SIZE;
//
//	char command[size + CHARACTER_NULL_SIZE] = "";
//
//	command[9] = bearerSetting[0];
//	command[11] = bearerSetting[2];
//	command[size] = '\n';
//
//	this->send(command, SAPBR_SIZE, delay);
//}
/*
 * usefulTXSize it's means:
 * how many new bytes this "const char params[]" added to send tx
 *
 * see SIM800L::SetGPRS() to understand
 */
void SIM800L::sendBearerCredentials(char command[], const char params[], uint8_t usefulTXSize) {
	uint16_t size = SAPBR_SIZE + usefulTXSize;

	for (uint16_t i = SAPBR_SIZE; i < size; ++i) {
		uint16_t x = i - SAPBR_SIZE;
		if (params[x] == '\0') {
			break;
		}

		command[i] = params[x];
	}

	command[size] = '\n';
}

/*
 * level it is string number.
 *
 * WARNING!
 * Dont use it. Lib is not adapted for this func
 */
bool SIM800L::setErrorLevel(const char* level) {
	char command[CMEE_SIZE_SEND] = "AT+CMEE=2";
	command[CMEE_SIZE] = '\n';
	command[8] = level[0];

	this->readIT(50);
	this->sendIT(command, CMEE_SIZE_SEND);

	bool status = this->waitReceiveLess(100);
	if (status == false) {
		this->retry(command, CMEE_SIZE_SEND, 50, STANDART_DELAY);
	}

	return this->parseOK(50);
}

bool SIM800L::parseCloseGPRSStatus() {
	for (uint16_t i = 0; i < SIM_ARR_SIZE; i++) {
		if (this->rx_buffer[i] == 'S') {
			return true;
		}
	}

	return false;
}

bool SIM800L::parseOK(uint16_t size) {
	bool result = false;
	for (uint16_t i = 0; i < size; i++) {
		if (this->rx_buffer[i] == '\n') {
			uint16_t x = i;

			result = this->rx_buffer[x+1] == 'O';
			if (result) { return true; }
		}
	}

	return result;
}

bool SIM800L::parseError(uint16_t size) {
	bool result = false;
	for (uint16_t i = 0; i < size; i++) {
		if (this->rx_buffer[i] == '\n') {
			uint16_t x = i;

			result = this->rx_buffer[x+1] == 'E';
			if (result) { return true; }
		}
	}

	return result;
}

bool SIM800L::parse200() {
	bool result = false;
	for (uint16_t i = 0; i < SIM_ARR_SIZE; i++) {
		if (this->rx_buffer[i] == ':') {
			uint16_t x = i;

			result = this->rx_buffer[x+4] == '2';
			if (result) { return true; }
		}
	}

	return result;
}

bool SIM800L::parse600() {
	bool result = false;
	for (uint16_t i = 0; i < SIM_ARR_SIZE; i++) {
		if (this->rx_buffer[i] == ':') {
			uint16_t x = i;

			result = this->rx_buffer[x+4] == '6';
			if (result) { return true; }
		}
	}

	return result;
}

bool SIM800L::parse400() {
	bool result = false;
	for (uint16_t i = 0; i < SIM_ARR_SIZE; i++) {
		if (this->rx_buffer[i] == ':') {
			uint16_t x = i;

			result = this->rx_buffer[x+4] == '4';
			if (result) { return true; }
		}
	}

	return result;
}

void SIM800L::parseAndSetGPRSStatus() {
	for (uint8_t i = 0; i < SIM_ARR_SIZE; ++i) {
		if (this->rx_buffer[i] == ':') {
//			char session = ;
//			char status = this->rx_buffer[i + 4];

			this->status.Session = this->rx_buffer[i + 2];
			this->status.Status = this->rx_buffer[i + 4];

			return;
		}
	}
}

bool SIM800L::parseCPINReady() {
	bool result = false;
	for (uint8_t i = 0; i < SIM_ARR_SIZE; ++i) {
		if (this->rx_buffer[i] == 'R') {

			result = this->rx_buffer[i+1] == 'E' && this->rx_buffer[i+2] == 'A' && this->rx_buffer[i+2+7] == 'D';
		}
	}

	return result;
}

bool SIM800L::parseCallReady() {
	bool result = false;
	for (uint8_t i = 0; i < SIM_ARR_SIZE; ++i) {
		if (this->rx_buffer[i] == 'S') {

			result = this->rx_buffer[i+1] == 'M' && this->rx_buffer[i+2] == 'S';
		}
	}

	return result;
}

bool SIM800L::parseSMSReady() {
	bool result = false;

	for (uint8_t i = 0; i < SIM_ARR_SIZE; ++i) {
		if (this->rx_buffer[i] == 'S') {

			result = this->rx_buffer[i+1] == 'A' && this->rx_buffer[i+2] == 'L';
		}
	}

	return result;
}

bool SIM800L::checkGPRSStarted() {
	if(this->status.Session == GPRSSessionStatus::Opened
			&& this->status.Status == GPRSConnectionStatus::Connected)
	{
		return true;
	} else {
		return false;
	}
}

bool SIM800L::checkGPRSClosed() {
	if(this->status.Session == GPRSSessionStatus::Closed
			&& this->status.Status == GPRSConnectionStatus::NotConnect)
	{
		return true;
	} else {
		return false;
	}
}

bool SIM800L::checkErrors() {
	if(this->errorCounter == MAX_ERRORS_FOR_RESTART) {
		// TODO: restart
	}
}

bool SIM800L::parseDOWNLOAD(uint16_t size) {
	bool result = false;
	for (uint16_t i = 0; i < size; i++) {
		if (this->rx_buffer[i] == '\n') {
			uint16_t x = i;

			result = this->rx_buffer[x+1] == 'D';
			if (result) { return true; }
		}
	}

	return result;
}

uint8_t SIM800L::parseUint(char buffer[], uint32_t target, uint8_t parseSize) {
	if(target == 0) {
		buffer[0] = target;
		return 1;
	}

	for (uint8_t i = parseSize - 1; i >= 0; --i) {
		if (target > 10) {
			uint8_t digit = target % 10;

			buffer[i] = digit + '0';
			target = target / 10;
		} else {
			buffer[i] = target + '0';
			return i;
		}
	}
}

void SIM800L::send(const char* msg, uint16_t size, uint16_t delayMS) {
	HAL_UART_Transmit(this->huart, (uint8_t*) msg, size, delayMS);
}

void SIM800L::read(uint16_t size, uint16_t delayMS) {
	HAL_UART_Receive(this->huart, this->rx_buffer, size, delayMS);
}

bool SIM800L::retry(const char* msg, uint16_t sizeSend, uint16_t sizeReceive, uint32_t timeoutMS, bool isOnce) {
	if (isOnce) {
//		this->readIT(sizeReceive);
		this->sendIT(msg, sizeSend);

		return this->waitReceiveLong(timeoutMS);
	}

	while(this->isReceive != true) {
//		this->readIT(sizeReceive);
		this->sendIT(msg, sizeSend);

		this->waitReceiveLong(timeoutMS);
	}

	return true;
}

bool SIM800L::waitReceive(uint16_t timeoutMS) {
	if (timeoutMS == 0) {
		while(this->isReceive != true) {}
		return true;
	}

	uint32_t time = HAL_GetTick();

	while(this->isReceive != true) {
		if (HAL_GetTick() - time > timeoutMS) {
			return false;
		}
	}

	return true;
}

bool SIM800L::waitReceiveLong(uint32_t timeoutMS) {
	if (timeoutMS == 0) {
		while(this->isReceive != true) {}
		return true;
	}

	uint32_t time = HAL_GetTick();

	while(this->isReceive != true) {
		if (HAL_GetTick() - time > timeoutMS) {
			return false;
		}
	}

	return true;
}

bool SIM800L::waitReceiveLess(uint8_t timeoutMS) {
	if (timeoutMS == 0) {
		while(this->isReceive != true) {}
		return true;
	}

	uint32_t time = HAL_GetTick();

	while(this->isReceive != true) {
		if (HAL_GetTick() - time > timeoutMS) {
			return false;
		}
	}

	return true;
}

void SIM800L::readIT(uint16_t size) {
	this->isReceive = false;

	HAL_UARTEx_ReceiveToIdle_IT(this->huart, rx_buffer, size);
}

void SIM800L::sendIT(const char* msg, uint16_t size) {
	this->isSend = false;
	HAL_UART_Transmit_IT(this->huart, (uint8_t*) msg, size);

	while(this->isSend != true) {}
}

void SIM800L::initUrl() {
	for (uint8_t i = 0; i < 50; ++i) {
		this->url.url[i] = '0';
	}
}

bool SIM800L::resetModule() {
	HAL_GPIO_WritePin(this->reset.GPIOx, this->reset.pin, GPIO_PIN_RESET);
	HAL_Delay(1);
	HAL_GPIO_WritePin(this->reset.GPIOx, this->reset.pin, GPIO_PIN_SET);

	HAL_Delay(7000);

	this->AT();

	bool isSimReady = this->simReadyState();
	while(isSimReady == false) {
		HAL_Delay(100);
		isSimReady = this->simReadyState();
	}

	return isSimReady;
}
