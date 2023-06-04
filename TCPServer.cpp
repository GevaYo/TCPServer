#include "TCPServer.h"

struct SocketState sockets[MAX_SOCKETS] = { 0 };
int socketsCount = 0;


void runServer()
{
	WSAData wsaData;

	if (NO_ERROR != WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		cout << "Time Server: Error at WSAStartup()\n";
		return;
	}

	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (INVALID_SOCKET == listenSocket)
	{
		cout << "Time Server: Error at socket(): " << WSAGetLastError() << endl;
		WSACleanup();
		return;
	}

	sockaddr_in serverService;
	serverService.sin_family = AF_INET;
	serverService.sin_addr.s_addr = INADDR_ANY;
	serverService.sin_port = htons(TIME_PORT);

	if (SOCKET_ERROR == bind(listenSocket, (SOCKADDR*)&serverService, sizeof(serverService)))
	{
		cout << "Time Server: Error at bind(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}

	if (SOCKET_ERROR == listen(listenSocket, 5))
	{
		cout << "Time Server: Error at listen(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}
	addSocket(listenSocket, LISTEN);

	while (true)
	{
		fd_set waitRecv; // Empty set that will hold sockets that are ready to listen\receive
		FD_ZERO(&waitRecv);
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if ((sockets[i].recv == LISTEN) || (sockets[i].recv == RECEIVE))
				FD_SET(sockets[i].id, &waitRecv);
		}

		fd_set waitSend; // Empty set that will hold sockets that are ready to send response
		FD_ZERO(&waitSend);
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if (sockets[i].send == SEND)
				FD_SET(sockets[i].id, &waitSend);
		}

		int nfd;
		nfd = select(0, &waitRecv, &waitSend, NULL, NULL);
		if (nfd == SOCKET_ERROR)
		{
			cout << "Time Server: Error at select(): " << WSAGetLastError() << endl;
			WSACleanup();
			return;
		}

		for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
		{
			if (FD_ISSET(sockets[i].id, &waitRecv))
			{
				nfd--;
				switch (sockets[i].recv)
				{
				case LISTEN:
					acceptConnection(i);
					break;

				case RECEIVE:
					receiveMessage(i);
					break;
				}
			}
		}

		checkForTimeoutAndHandle();

		for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
		{
			if (FD_ISSET(sockets[i].id, &waitSend))
			{
				nfd--;
				switch (sockets[i].send)
				{
				case SEND:
					sendMessage(i);
					break;
				}
			}
		}
	}

	cout << "Time Server: Closing Connection.\n";
	closesocket(listenSocket);
	WSACleanup();
}

bool addSocket(SOCKET id, int what)
{
	for (int i = 0; i < MAX_SOCKETS; i++)
	{
		if (sockets[i].recv == EMPTY)
		{
			sockets[i].id = id;
			sockets[i].recv = what;
			sockets[i].send = IDLE;
			sockets[i].len = 0;
			sockets[i].timeSinceRequestFullyReceived = time(nullptr);
			socketsCount++;
			return (true);
		}
	}
	return (false);
}

void removeSocket(int index)
{
	sockets[index].recv = EMPTY;
	sockets[index].send = EMPTY;
	socketsCount--;
}

void acceptConnection(int index)
{
	SOCKET id = sockets[index].id;
	struct sockaddr_in from;		// Address of sending partner
	int fromLen = sizeof(from);

	SOCKET msgSocket = accept(id, (struct sockaddr*)&from, &fromLen);
	if (INVALID_SOCKET == msgSocket)
	{
		cout << "Time Server: Error at accept(): " << WSAGetLastError() << endl;
		return;
	}
	cout << "Time Server: Client " << inet_ntoa(from.sin_addr) << ":" << ntohs(from.sin_port) << " is connected." << endl;

	//
	// Set the socket to be in non-blocking mode.
	//
	unsigned long flag = 1;
	if (ioctlsocket(msgSocket, FIONBIO, &flag) != 0)
	{
		cout << "Time Server: Error at ioctlsocket(): " << WSAGetLastError() << endl;
	}

	if (addSocket(msgSocket, RECEIVE) == false)
	{
		cout << "\t\tToo many connections, dropped!\n";
		closesocket(id);
	}
	return;
}

void receiveMessage(int index)
{
	SOCKET msgSocket = sockets[index].id;

	int len = sockets[index].len;																				// Reading data from the socket
	int bytesRecv = recv(msgSocket, &sockets[index].buffer[len], sizeof(sockets[index].buffer) - len, 0);		//

	if (SOCKET_ERROR == bytesRecv)
	{
		cout << "TCP Server: Error at recv(): " << WSAGetLastError() << endl;
		closesocket(msgSocket);
		removeSocket(index);
		return;
	}
	if (bytesRecv == 0) // connection died
	{
		closesocket(msgSocket);
		removeSocket(index);
		return;
	}
	else
	{
		sockets[index].timeSinceRequestFullyReceived = time(nullptr);
		sockets[index].buffer[len + bytesRecv] = '\0'; //add the null-terminating to make it a string
		sockets[index].len += bytesRecv;
		cout << "TCP Server: Recieved: " << bytesRecv << endl << "\nRequest: " << endl << "------------\n" << &sockets[index].buffer[len] << endl;


		if (sockets[index].len > 0) // Buffer isn't empty
		{
			sockets[index].send = SEND;
		}
	}

}

void sendMessage(int index)
{
	int bytesSent = 0;
	//char sendBuff[255];

	SOCKET msgSocket = sockets[index].id;
	HTTPRequest requestInfo = parseInfoFromSocket(index);
	string responseMsg = generateHTTPResponseFromSocket(requestInfo);
	cout << "Response:\n" << responseMsg.data() << endl << "Received: " << responseMsg.length() << endl;

	//bytesSent = send(msgSocket, sendBuff, (int)strlen(sendBuff), 0);
	bytesSent = send(msgSocket, responseMsg.data(), responseMsg.length(), 0);
	if (SOCKET_ERROR == bytesSent)
	{
		cout << "Time Server: Error at send(): " << WSAGetLastError() << endl;
		return;
	}
	//cout << "Time Server: Sent: " << bytesSent << "\\" << responseMsg.length() << " bytes of \" \n" << responseMsg << "\n";
	//cout << "Time Server: Sent: " << bytesSent << "\\" << strlen(sendBuff) << " bytes of \"" << sendBuff << "\" message.\n";

	sockets[index].send = IDLE;
}

void checkForTimeoutAndHandle()
{
	double timeSinceLastRequest;

	for (int i = 1; i < socketsCount; i = ++i)
	{
		timeSinceLastRequest = difftime(time(nullptr), sockets[i].timeSinceRequestFullyReceived);
		if (timeSinceLastRequest > TIMEOUT_MAX)
		{
			cout << "Request in socket: " << i << " Timed-out after: \n" << timeSinceLastRequest;
			closesocket(sockets[i].id);
			removeSocket(i);
		}
	}
}

HTTPRequest parseInfoFromSocket(int index)
{
	HTTPRequest receivedRequest;
	string method, body, endPoint, queryParams, headers;
	eRequestType type;
	string requestInBuffer(sockets[index].buffer, sockets[index].len);
	size_t endOfFullRequest = requestInBuffer.find("\r\n\r\n");

	if (endOfFullRequest != string::npos) {
		string fullRequest = requestInBuffer.substr(0, endOfFullRequest);
		body = requestInBuffer.substr(endOfFullRequest + 4);

		// Extract HTTP method, endpoint, and query parameters from fullRequest
		size_t methodEnd = fullRequest.find(' ');
		if (methodEnd != string::npos) { // No Valid Method
			method = fullRequest.substr(0, methodEnd);
			type = getRequestType(method);
			size_t endpointEnd = fullRequest.find(' ', methodEnd + 1);

			if (endpointEnd != string::npos) {
				endPoint = fullRequest.substr(methodEnd + 1, endpointEnd - methodEnd - 1);
				size_t queryParamsStart = fullRequest.find('?');

				if (queryParamsStart != string::npos) { // There are Query Params
					queryParams = endPoint.substr(endPoint.find('?') + 1);
					endPoint = endPoint.substr(0, endPoint.find('?'));
					//receivedRequest.queryParams = fullRequest.substr(queryParamsStart, queryParamsEnd - queryParamsStart);
					size_t langStart = receivedRequest.queryParams.find("lang=");

					if (langStart != string::npos) // There's 'lang='
					{
						size_t queryParamsEnd = fullRequest.find(' ', queryParamsStart + 1);
						headers = fullRequest.substr(queryParamsEnd + 1, endOfFullRequest - queryParamsEnd - 1);
					}
					else
					{
						type = eRequestType::INVALID;
					}
				}
				else // No Query Params
				{
					queryParams = "";
					headers = fullRequest.substr(endpointEnd + 1, endOfFullRequest - endpointEnd - 1);
				}
			}
		}
	}

	receivedRequest.type = type;
	receivedRequest.headers = headers;
	receivedRequest.endPoint = endPoint;
	receivedRequest.queryParams = queryParams;
	receivedRequest.body = body;
	receivedRequest.len = to_string(requestInBuffer.length());

	clearRequestFromBuffer(index, requestInBuffer.size());
	return receivedRequest;
}

eRequestType getRequestType(string methodName)
{
	eRequestType requestTypeAsEnum;

	if (methodName == "GET")
	{
		requestTypeAsEnum = eRequestType::GET;
	}
	else if (methodName == "POST")
	{
		requestTypeAsEnum = eRequestType::POST;
	}
	else if (methodName == "PUT")
	{
		requestTypeAsEnum = eRequestType::PUT;
	}
	else if (methodName == "DELETE")
	{
		requestTypeAsEnum = eRequestType::DEL;
	}
	else if (methodName == "TRACE")
	{
		requestTypeAsEnum = eRequestType::TRACE;
	}
	else if (methodName == "HEAD")
	{
		requestTypeAsEnum = eRequestType::HEAD;
	}
	else if (methodName == "OPTIONS")
	{
		requestTypeAsEnum = eRequestType::OPTIONS;
	}
	else
	{
		requestTypeAsEnum = eRequestType::INVALID;
	}

	return requestTypeAsEnum;
}

void clearRequestFromBuffer(int i, int bytesToRemove)
{
	string temp;
	memcpy(sockets[i].buffer, sockets[i].buffer + bytesToRemove, sizeof(sockets[i].buffer) - bytesToRemove);
	sockets[i].len -= bytesToRemove;
}

string generateHTTPResponseFromSocket(HTTPRequest requestInfo)
{
	string response;

	switch (requestInfo.type) {
	case eRequestType::GET:
	{
		response = GETMethod(requestInfo);
		break;
	}
	case eRequestType::POST:
	{
		response = POSTMethod(requestInfo);
		break;
	}
	case eRequestType::HEAD:
	{
		response = HEADMethod(requestInfo);
		break;
	}
	case eRequestType::TRACE:
	{
		response = TRACEMethod(requestInfo);
		break;
	}
	case eRequestType::PUT:
	{
		response = PUTMethod(requestInfo);
		break;
	}
	case eRequestType::OPTIONS:
	{
		response = OPTIONSMethod(requestInfo);
		break;
	}
	case eRequestType::DEL:
	{
		response = DELETEMethod(requestInfo);
		break;
	}
	default:
	{
		string statusCode = BAD_REQUEST;
		string body = "Method Isn't Supported";
		response = VERSION + statusCode + "\r\n"
			+ CONTENT_TYPE + "text/plain" + "\r\n"
			+ CONTENT_LEN + to_string(body.length())
			+ "\r\n\r\n"
			+ body + "\r\n";
		break;
	}

	return response;
	}
}

string buildResponse(HTTPRequest requestInfo, string statusCode) // A response Template for all of the methods with little modifications
{
	string responseTemplate;
	string body = requestInfo.body;
	eRequestType methodType = requestInfo.type;

	responseTemplate = VERSION + statusCode + "\r\n"
		+ CONTENT_TYPE + "text/html" + "\r\n"
		+ CONTENT_LEN + requestInfo.len + "\r\n";

	if (methodType == eRequestType::GET || methodType == eRequestType::HEAD)
	{
		if (statusCode != NOT_FOUND && methodType == eRequestType::HEAD)
		{
			body = "";
		}
		responseTemplate += "\r\n\r\n" + body;
	}
	else if (methodType == eRequestType::OPTIONS)
	{
		string allow = "GET, TRACE, PUT, POST, HEAD, DELETE, OPTIONS";
		responseTemplate = VERSION + statusCode + "\r\n"
			+ ALLOW + allow + "\r\n"
			+ CONTENT_LEN + "0" + "\r\n\r\n";
	}
	else if (methodType == eRequestType::TRACE)
	{
		responseTemplate = VERSION + statusCode + "\r\n"
			+ CONTENT_TYPE + "message/http" + "\r\n"
			+ CONTENT_LEN + requestInfo.len
			+ "\r\n\r\n";
	}
	else if (methodType == eRequestType::POST)
	{
		body = "Printed in server's console:\n" + requestInfo.body;
		responseTemplate = VERSION + statusCode + "\r\n"
			+ CONTENT_TYPE + "text/plain" + "\r\n"
			+ CONTENT_LEN + to_string(body.length())
			+ "\r\n\r\n"
			+ body + "\r\n";
	}
	else if (methodType == eRequestType::PUT || methodType == eRequestType::DEL)
	{
		responseTemplate = VERSION + statusCode + "\r\n"
			+ CONTENT_TYPE + "text/plain" + "\r\n";
	}

	return responseTemplate;
}

string GETMethod(HTTPRequest requestInfo)
{
	return getFileContentAndGenerateResponse(requestInfo);
}

string HEADMethod(HTTPRequest requestInfo)
{
	return getFileContentAndGenerateResponse(requestInfo);
}

string extractLang(string queryParams)
{
	string lang;
	if (queryParams.find("lang=fr") != string::npos)
	{
		lang = "fr";
	}
	else if (queryParams.find("lang=he") != string::npos)
	{
		lang = "he";
	}
	else
	{
		lang = "en";
	}

	return lang;
}

string extractContentFromFile(string filePath, string& statusCode)
{
	ifstream file;
	string body;
	file.open(filePath.c_str());
	if (file.is_open())
	{
		string buffer;
		while (getline(file, buffer))
		{
			body += buffer;
		}
		file.close();
		statusCode = OK;
	}
	else
	{
		body = setBodyContentTo404();
		statusCode = NOT_FOUND;
	}

	return body;
}

string setBodyContentTo404()
{
	string errorBody;
	ifstream file;
	string errFileName = "NotFound404.html";
	string URL = ROOT + errFileName;
	file.open(URL);
	if (file.is_open())
	{
		string buffer;
		while (getline(file, buffer))
		{
			errorBody += buffer;
		}
		file.close();
	}

	return errorBody;
}

string getFileContentAndGenerateResponse(HTTPRequest requestInfo) // For GET and HEAD methods which are basically the same
{
	string response, statusCode;
	string lang = extractLang(requestInfo.queryParams);
	string filePathPrefix = "C:\\temp\\";
	string fileNameInRequest = requestInfo.endPoint.substr(1, requestInfo.endPoint.rfind('.') - 1);
	string extension = requestInfo.endPoint.rfind('.') != string::npos
		? requestInfo.endPoint.substr(requestInfo.endPoint.rfind('.'), requestInfo.endPoint.rfind('?') - 1)
		: " ";
	string filePath = filePathPrefix + fileNameInRequest + "-" + lang + extension;
	requestInfo.body = extractContentFromFile(filePath, statusCode);

	response = buildResponse(requestInfo, statusCode);

	return response;
}

string POSTMethod(HTTPRequest requestInfo)
{
	string response, statusCode;
	cout << "The Body of the POST request consists of:\n" << requestInfo.body << endl;
	statusCode = OK;
	response = buildResponse(requestInfo, statusCode);
	return response;
}

string TRACEMethod(HTTPRequest requestInfo)
{
	string statusCode, response;

	statusCode = OK;
	response = buildResponse(requestInfo, statusCode);
	response += "TRACE "
		+ requestInfo.endPoint
		+ requestInfo.queryParams
		+ " "
		+ requestInfo.headers
		+ "\r\n\r\n"
		+ requestInfo.body;
	return response;
}

string OPTIONSMethod(HTTPRequest requestInfo)
{
	string statusCode, response, fullRequest;
	statusCode = OK;
	//fullRequest = requestInfo.headers + requestInfo.body;
	response = buildResponse(requestInfo, statusCode);

	return response;
}

string PUTMethod(HTTPRequest requestInfo)
{
	string statusCode;
	string responseBody = createOrModifyFile(requestInfo, statusCode);
	string response = buildResponse(requestInfo, statusCode);
	response += +CONTENT_LEN + to_string(responseBody.length())
		+ "\r\n\r\n"
		+ responseBody;

	return response;
}

bool validateFileName(string fileName)
{
	bool valid = true;
	size_t fileNameEnd = fileName.length() - 5;
	string suffix = fileName.substr(fileNameEnd);
	if (fileName.length() <= 5 || suffix != ".html")
		valid = false;

	for (size_t i = 0; i < fileNameEnd; ++i)
	{
		char c = fileName[i];
		if (!isalnum(c))
			valid = false;
	}

	return valid;
}

string createOrModifyFile(HTTPRequest requestInfo, string& statusCode)
{
	string responseBody;
	string fileName = requestInfo.endPoint.substr(1);
	if (!validateFileName(fileName))
	{
		statusCode = BAD_REQUEST;
		responseBody = "The file name isn't valid.\nThe requested format is: {file name consists only alphanumeric chars}.html.\n";
	}
	else
	{
		string fileLocation = string(ROOT) + fileName;
		fstream file(fileLocation);

		if (!fileExists(fileLocation)) // file doesn't exist --> create new
		{
			file.open(fileLocation, fstream::out);

			if (file) // File created successfully
			{
				file << requestInfo.body;
				responseBody = "File: " + fileName + "was created in: " + fileLocation + "and its content is: " + requestInfo.body;
				statusCode = CREATED;
			}
			else // Failed to create
			{
				statusCode = INTERNAL_ERROR;
				responseBody = "Failed to create the requested file!";
			}
		}
		else // file exist --> override it
		{
			//file.open(fileLocation, fstream::out);
			file << requestInfo.body;
			responseBody = "File: " + fileName + " exists, and was modified.\n The new content is: " + requestInfo.body;
			statusCode = OK;
		}

		file.close();
	}

	return responseBody;
}

string DELETEMethod(HTTPRequest requestInfo)
{
	string statusCode;
	string responseBody = deleteFile(requestInfo, statusCode);
	string response = buildResponse(requestInfo, statusCode);
	response += +CONTENT_LEN + to_string(responseBody.length())
		+ "\r\n\r\n"
		+ responseBody;

	return response;
}

string deleteFile(HTTPRequest requestInfo, string& statusCode)
{
	string responseBody;
	string fileName = requestInfo.endPoint.substr(1);
	if (!validateFileName(fileName)) // Invalid file name
	{
		statusCode = BAD_REQUEST;
		responseBody = "The file name isn't valid.\nThe requested format is: {file name consists only alphanumeric chars}.html.\n";
	}
	else // valid
	{
		string fileLocation = string(ROOT) + fileName;
		if (!fileExists(fileLocation)) // File doesn't exist
		{
			statusCode = NOT_FOUND;
			responseBody = "The file name you entered wasn't found, therefore, opertaion failed!\n";
		}
		else // File exists
		{
			if (remove(fileLocation.c_str()) == 0) // success
			{
				statusCode = OK;
				responseBody = "File: " + fileName + " was deleted successfully!\n";
			}
			else // File exists but deltetion failed
			{
				statusCode = INTERNAL_ERROR;
				responseBody = "Failed to delete the requested file!";
			}
		}
	}

	return responseBody;
}

bool fileExists(string fileLocation)
{
	ifstream file(fileLocation);
	return file.good();
}


