#include "p.h"

char* convert_Request_to_string(struct ParsedRequest* req)
{
	/* Set headers */
	ParsedHeader_set(req, "Host", req->host);
	ParsedHeader_set(req, "Connection", "close");

	int iHeadersLen = ParsedHeader_headersLen(req);

	char* headersBuf;

	headersBuf = (char*)malloc(iHeadersLen + 1);

	if (headersBuf == NULL)
	{
		fprintf(stderr, " Error in memory allocation  of headersBuffer ! \n");
		exit(1);
	}

	ParsedRequest_unparse_headers(req, headersBuf, iHeadersLen);
	headersBuf[iHeadersLen] = '\0';

	int request_size = strlen(req->method) + strlen(req->path) + strlen(req->version) + iHeadersLen + 4;

	char* serverReq;

	serverReq = (char*)malloc(request_size + 1);

	if (serverReq == NULL)
	{
		fprintf(stderr, " Error in memory allocation for serverrequest ! \n");
		exit(1);
	}

	serverReq[0] = '\0';
	strcpy(serverReq, req->method);
	strcat(serverReq, " ");
	strcat(serverReq, req->path);
	strcat(serverReq, " ");
	strcat(serverReq, req->version);
	strcat(serverReq, "\r\n");
	strcat(serverReq, headersBuf);

	free(headersBuf);

	return serverReq;
}

SOCKET createServerSocket(char* pcAddress, char* pcPort) {
	struct addrinfo ahints;
	struct addrinfo* paRes;

	SOCKET iSockfd;

	/* Get address information for stream socket on input port */
	memset(&ahints, 0, sizeof(ahints));
	ahints.ai_family = AF_UNSPEC;
	ahints.ai_socktype = SOCK_STREAM;
	ahints.ai_protocol = IPPROTO_TCP;
	if (getaddrinfo(pcAddress, pcPort, &ahints, &paRes) != 0)
	{
		fprintf(stderr, " Error in server address format ! \n");
		exit(1);
	}

	/* Create and connect */
	if ((iSockfd = socket(paRes->ai_family, paRes->ai_socktype, paRes->ai_protocol)) < 0)
	{
		fprintf(stderr, " Error in creating socket to server ! \n");
		exit(1);
	}
	if (connect(iSockfd, paRes->ai_addr, paRes->ai_addrlen) < 0)
	{
		fprintf(stderr, " Error in connecting to server ! \n");
		exit(1);
	}

	/* Free paRes, which was dynamically allocated by getaddrinfo */
	freeaddrinfo(paRes);

	return iSockfd;
}

void writeToserverSocket(const char* buff_to_server, SOCKET sockfd, int buff_length)
{

	string temp;

	temp.append(buff_to_server);

	int totalsent = 0;

	int senteach;

	while (totalsent < buff_length)
	{
		if ((senteach = send(sockfd, (buff_to_server + totalsent), buff_length - totalsent, 0)) < 0)
		{
			fprintf(stderr, " Error in sending to server ! \n");
			exit(1);
		}
		totalsent += senteach;
	}
}

void writeToclientSocket(const char* buff_to_server, SOCKET sockfd, int buff_length)
{

	string temp;

	temp.append(buff_to_server);

	int totalsent = 0;

	int senteach;

	while (totalsent < buff_length)
	{
		if ((senteach = send(sockfd, (buff_to_server + totalsent), buff_length - totalsent, 0)) < 0)
		{
			fprintf(stderr, " Error in sending to client ! \n");
			exit(1);
		}
		totalsent += senteach;

	}

}

void writeToClient(SOCKET Clientfd, SOCKET Serverfd) {
	int iRecv;
	char buf[resLEN];

	while ((iRecv = recv(Serverfd, buf, resLEN, 0)) > 0)
	{
		//cout << buf << endl;
		writeToclientSocket(buf, Clientfd, iRecv);         // writing to client	    
		memset(buf, 0, sizeof buf);
	}

	/* Error handling */
	if (iRecv < 0)
	{
		fprintf(stderr, "Yo..!! Error while recieving from server ! \n");
		exit(1);
	}
}

int datafromclient(SOCKET& sockid)
{
	char buf[reqLEN];
	char* req_mess; // Get message from URL
	req_mess = (char*)malloc(reqLEN);
	if (req_mess == NULL)
	{
		fprintf(stderr, "Error in memory allocation ! \n");
		return -1;
	}

	req_mess[0] = '\0';
	int recvdbits = 0;
	int MAXLEN = reqLEN;
	while (strstr(req_mess, "\r\n\r\n") == NULL) // determines end of request
	{
		int recvd = recv(sockid, buf, MAXLEN, 0);
		if (recvd < 0)
		{
			fprintf(stderr, "Error while receiving ! \n");
			return -1;
		}
		else if (recvd == 0)
		{
			printf("Connection closing...\n");
			break;
		}
		else
		{
			recvdbits += recvd;

			/* if total message size greater than our string size, double the string size */

			buf[recvd] = '\0';
			if (recvdbits > MAXLEN)
			{
				MAXLEN *= 2;
				req_mess = (char*)realloc(req_mess, MAXLEN);
				if (req_mess == NULL)
				{
					fprintf(stderr, "Error while receiving ! \n");
					return -1;
				}
			}
		}
		strcat(req_mess, buf);
	}
	struct ParsedRequest* req;
	req = ParsedRequest_create();
	if (ParsedRequest_parse(req, req_mess, strlen(req_mess)) < 0)
	{
		fprintf(stderr, "Error in request message... only http and get with headers are allowed ! \n");
		return -1;
	}
	if (req->port == NULL)
	{
		req->port = (char*)oPORT;
	}

	/* final request to be sent */
	char* browser_req = convert_Request_to_string(req);
	//cout << browser_req << endl;
	SOCKET iServerfd;
	iServerfd = createServerSocket(req->host, req->port);
	writeToserverSocket(browser_req, iServerfd, recvdbits);
	writeToClient(sockid, iServerfd);

	ParsedRequest_destroy(req);

	//closesocket(sockid);   // close the sockets
	closesocket(iServerfd);

	return 0;
}

DWORD WINAPI function_cal(LPVOID arg)
{
	int keycont;
	SOCKET hConnected = *((SOCKET*)arg);
	do
	{
		keycont = (datafromclient(hConnected));
	} while (keycont == 0);
	closesocket(hConnected);
	return 0;
}

int main()
{
	WSADATA wsaData;
	int Result;
	Result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (Result != 0) {
		printf("WSAStartup failed: %d\n", Result);
		return 1;
	}
	//

	//Get proxy server info
	int status;
	struct addrinfo hints, * servinfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	if ((status = getaddrinfo(NULL, iPORT, &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
		WSACleanup();
		return 1;
	}
	//

	//Create listen socket
	SOCKET listenSock = INVALID_SOCKET;
	listenSock = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
	if (listenSock == INVALID_SOCKET)
	{
		freeaddrinfo(servinfo);
		WSACleanup();
		printf("Error at socket(): %ld\n", WSAGetLastError()); //perror("server: socket");
		return 1;
	}
	//

	//Give listen socket ip and port
	if (bind(listenSock, servinfo->ai_addr, servinfo->ai_addrlen) == SOCKET_ERROR)
	{
		freeaddrinfo(servinfo);
		closesocket(listenSock);
		WSACleanup();
		printf("bind failed with error: %d\n", WSAGetLastError()); //perror("server: bind");
		return 1;
	}
	//

	freeaddrinfo(servinfo);//no need anymore

	//Listening in listen socket
	if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR)
	{
		printf("Listen failed with error: %ld\n", WSAGetLastError()); //perror("listen");
		closesocket(listenSock);
		WSACleanup();
		return 1;
	}
	//

	DWORD threadID;
	HANDLE threadStatus;

	while (1)
	{
		/* A browser request starts here */

		SOCKET clientSock = INVALID_SOCKET;
		clientSock = accept(listenSock, NULL, NULL);
		if (clientSock == INVALID_SOCKET)
		{
			printf("accept failed: %d\n", WSAGetLastError());
			closesocket(listenSock);
			WSACleanup();
			return 1;
		}
		//

		//
		/*threadStatus = CreateThread(NULL, 0, function_cal, &listenSock, 0, &threadID);
		if (threadStatus == NULL)
		{
			cout << "Thread Creation Failed & Error No --> " << GetLastError() << endl;
			CloseHandle(threadStatus);
		}*/
		datafromclient(clientSock);
		closesocket(clientSock);
	}
	closesocket(listenSock);
	WSACleanup();
	system("pause");
	return 0;
}

	//const char* c =
	//	"GET http://www.google.com:80/index.html/ HTTP/1.0\r\nContent-Length:"
	//	" 80\r\nIf-Modified-Since: Sat, 29 Oct 1994 19:43:31 GMT\r\n\r\n";

	//int len = strlen(c);
	////Create a ParsedRequest to use. This ParsedRequest
	////is dynamically allocated.
	//ParsedRequest* req = ParsedRequest_create();
	//if (ParsedRequest_parse(req, c, len) < 0) {
	//	printf("parse failed\n");
	//	return -1;
	//}
	//printf("Method:%s\n", req->method);
	//printf("Host:%s\n", req->host);
	//// Turn ParsedRequest into a string. 
	//// Friendly reminder: Be sure that you need to
	//// dynamically allocate string and if you
	//// do, remember to free it when you are done.
	//// (Dynamic allocation wasn't necessary here,
	//// but it was used as an example.)
	//int rlen = ParsedRequest_totalLen(req);
	//char* b = (char*)malloc(rlen + 1);
	//if (ParsedRequest_unparse(req, b, rlen) < 0) {
	//	printf("unparse failed\n");
	//	return -1;
	//}
	//b[rlen] = '\0';
	//// print out b for text request 
	//free(b);

	//// Turn the headers from the request into a string.
	//rlen = ParsedHeader_headersLen(req);
	//char* buf = (char*)malloc(rlen + 1);
	//if (ParsedRequest_unparse_headers(req, buf, rlen) < 0) {
	//	printf("unparse failed\n");
	//	return -1;
	//}
	//buf[rlen] = '\0';
	////print out buf for text headers only 
	//// Get a specific header (key) from the headers. A key is a header field 
	//// such as "If-Modified-Since" which is followed by ":"
	//struct ParsedHeader* r = ParsedHeader_get(req, "If-Modified-Since");
	//printf("Modified value: %s\n", r->value);

	//// Remove a specific header by name. In this case remove
	//// the "If-Modified-Since" header. 
	//if (ParsedHeader_remove(req, "If-Modified-Since") < 0) {
	//	printf("remove header key not work\n");
	//	return -1;
	//}
	//// Set a specific header (key) to a value. In this case,
	////we set the "Last-Modified" key to be set to have as 
	////value  a date in February 2014 

	//if (ParsedHeader_set(req, "Last-Modified", " Wed, 12 Feb 2014 12:43:31 GMT") < 0) {
	//	printf("set header key not work\n");
	//	return -1;
	//}
	//// Check the modified Header key value pair
	//r = ParsedHeader_get(req, "Last-Modified");
	//printf("Last-Modified value: %s\n", r->value);
	//// Call destroy on any ParsedRequests that you
	//// create once you are done using them. This will
	//// free memory dynamically allocated by the proxy_parse library. 
	//ParsedRequest_destroy(req);
	//return 0;

//DWORD WINAPI ThreadFun(LPVOID lpParam)
//{
//	int key = *((int*)lpParam);
//	cout << "Thread " << key << " is running" << endl;
//	return 0;
//}
//
//int main()
//{
//	HANDLE hThread;
//	DWORD threadID;
//	int a = 1;
//	int b = 2;
//	do
//	{
//		hThread = CreateThread(NULL, 0, ThreadFun, &a, 0, &threadID);
//		b++;
//	}while (b < 4);
//	CloseHandle(hThread);
//	return 0;
//}