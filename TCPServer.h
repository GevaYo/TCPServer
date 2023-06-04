#pragma once
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#ifndef SOCKETS_DEFINED
#define SOCKETS_DEFINED
using namespace std;
#pragma comment(lib, "Ws2_32.lib")
#include <iostream>
#include <winsock2.h>
#include <string.h>
#include <string>
#include <time.h>
#include <fstream>
#define VERSION "HTTP/1.1 "
#define ROOT "C:\\temp\\"
#define CONTENT_TYPE "Content-Type: "
#define CONTENT_LEN "Content-Length: "
#define ALLOW "Allow: "
#define MAX_BUFFER_LEN 1000
#define OK "200 OK"
#define CREATED "201 CREATED"
#define BAD_REQUEST "400 BAD_REQUEST"
#define NOT_FOUND "404 NOT FOUND"
#define INTERNAL_ERROR "500 INTERNAL_ERROR"
#define NOT_IMPLEMENTED "501 NOT_IMPLEMENTED"

enum eRequestType
{
	GET,
	POST,
	PUT,
	DEL,
	TRACE,
	HEAD,
	OPTIONS,
	INVALID
};

typedef struct SocketState
{
	SOCKET id;			// Socket handle
	int	recv;			// Receiving?
	int	send;			// Sending?	
	char buffer[MAX_BUFFER_LEN];
	int len;
	time_t timeSinceRequestFullyReceived;
};

typedef struct HTTPRequest
{
	eRequestType type;
	string headers;
	string endPoint;
	string queryParams;
	string body;
	string len;
};

const int TIME_PORT = 27015;
const int MAX_SOCKETS = 60;
const int EMPTY = 0;
const int LISTEN = 1;
const int RECEIVE = 2;
const int IDLE = 3;
const int SEND = 4;
const int TIMEOUT_MAX = 120;

void runServer();

bool addSocket(SOCKET id, int what);

void removeSocket(int index);

void acceptConnection(int index);

void receiveMessage(int index);

void sendMessage(int index);

void checkForTimeoutAndHandle();

HTTPRequest parseInfoFromSocket(int index);

eRequestType getRequestType(string methodName);

void clearRequestFromBuffer(int i, int bytesToRemove);

string generateHTTPResponseFromSocket(HTTPRequest requestInfo);

string buildResponse(HTTPRequest requestInfo, string statusCode);

string GETMethod(HTTPRequest requestInfo);

string HEADMethod(HTTPRequest requestInfo);

string extractLang(string queryParams);

string extractContentFromFile(string filePath, string& statusCode);

string setBodyContentTo404();

string getFileContentAndGenerateResponse(HTTPRequest requestInfo);

string POSTMethod(HTTPRequest requestInfo);

string TRACEMethod(HTTPRequest requestInfo);

string OPTIONSMethod(HTTPRequest requestInfo);

string PUTMethod(HTTPRequest requestInfo);
bool validateFileName(string fileName);
string createOrModifyFile(HTTPRequest requestInfo, string& statusCode);

string DELETEMethod(HTTPRequest requestInfo);

string deleteFile(HTTPRequest requestInfo, string& statusCode);

bool fileExists(string fileLocation);

#endif