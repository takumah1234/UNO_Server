#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>

#define MAXPENDING 	5
#define MAXTEXTLEN	256

int CreateTCPServerSocket(unsigned short port);
int CreateUDPServerSocket(unsigned short port);
int AcceptTCPConnection(int servSock);
void DieWithError(char *errorMessage);
void HandleTCPClient(int clntSock);
void HandleUDPClient(int servSock);
int RecvMes(char RecvMes[], int clntSock);
void SendMes(char SendMes[], int clntSock);
void *ThreadTCPMain(void *arg);
void *ThreadUDPMain(void *arg);

typedef struct ThreadTCPArgs
{
	int clntSock;
}ThreadTCPArgs;

typedef struct ThreadUDPArgs
{
	int servSock;
}ThreadUDPArgs;

int main(int argc, char *argv[])
{
	int servSockTCP, clntSockTCP, servSockUDP, clntSockUDP;
	unsigned short ServPortTCP, ServPortUDP;
	pthread_t threadTCPID, threadUDPID;
	ThreadTCPArgs *threadTCPArgs;
	ThreadUDPArgs *threadUDPArgs;

	if (argc != 3)
	{
		fprintf(stderr, "Usage: %s <Server Port1> <Server Port2>\n", argv[0]);
		exit(1);
	}

	srand((unsigned int)time(NULL));

	ServPortTCP = atoi(argv[1]);

	ServPortUDP = atoi(argv[2]);

	servSockTCP = CreateTCPServerSocket(ServPortTCP);

	servSockUDP = CreateUDPServerSocket(ServPortUDP);

	for ( ; ; )
	{
		clntSockTCP = AcceptTCPConnection(servSockTCP);

		threadTCPArgs = (ThreadTCPArgs *)malloc(sizeof(ThreadTCPArgs));
		if (threadTCPArgs == NULL)
			DieWithError("malloc() failed");
		threadTCPArgs -> clntSock = clntSockTCP;

		if (pthread_create(&threadTCPID, NULL, ThreadTCPMain, (void *) threadTCPArgs) != 0)
			DieWithError("pthread_create() failed");
		printf("with thread%ld\n", (long int) threadTCPID);

		if ((threadUDPArgs = (ThreadUDPArgs *)malloc(sizeof(ThreadUDPArgs))) == NULL)
			DieWithError("malloc() failed");
		threadUDPArgs -> servSock = servSockUDP;

		if (pthread_create(&threadUDPID, NULL, ThreadUDPMain, (void *) threadUDPArgs) != 0)
			DieWithError("pthread_create() failed");
		printf("with thread%ld\n", (long int) threadUDPID);
	}

	return 0;
}

int CreateTCPServerSocket(unsigned short port){
	int sock;
	struct sockaddr_in echoServAddr;

	if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		DieWithError("socket() failed");

	memset(&echoServAddr, 0, sizeof(echoServAddr));
	echoServAddr.sin_family = AF_INET;
	echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	echoServAddr.sin_port = htons(port);

	if (bind(sock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0)
		DieWithError("bind() failed");

	if (listen(sock, MAXPENDING) < 0)
		DieWithError("listen() failed");

	printf("クライアントを探しています\n");

	return sock;
}

int CreateUDPServerSocket(unsigned short port){
	int clntSock;
	struct sockaddr_in echoClntAddr;
	struct sigaction handler;


	if ((clntSock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		DieWithError("socket() failed");

	memset(&echoClntAddr, 0, sizeof(echoClntAddr));
	echoClntAddr.sin_family = AF_INET;
	echoClntAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	echoClntAddr.sin_port = htons(port);

	if (bind(clntSock, (struct sockaddr *) &echoClntAddr, sizeof(echoClntAddr)) < 0)
		DieWithError("bind() failed");

	return clntSock;
}

int AcceptTCPConnection(int servSock){
	int clntSock;
	struct sockaddr_in echoClntAddr;
	unsigned int clntLen;

	clntLen = sizeof(echoClntAddr);

	if ((clntSock = accept(servSock, (struct sockaddr *) &echoClntAddr, &clntLen)) < 0)
		DieWithError("accept() failed");

	printf("Handling client %s\n", inet_ntoa(echoClntAddr.sin_addr));

	return clntSock;
}

void DieWithError(char *errorMessage)
{
	perror(errorMessage);
	exit(1);
}

void *ThreadTCPMain(void *threadTCPArgs){
	int clntSock;

	pthread_detach(pthread_self());

	clntSock = ((ThreadTCPArgs *) threadTCPArgs) -> clntSock;
	free(threadTCPArgs);

	HandleTCPClient(clntSock);

	return (NULL);
}

void *ThreadUDPMain(void *threadUDPArgs){
	int servSock;

	pthread_detach(pthread_self());

	servSock = ((ThreadUDPArgs *) threadUDPArgs) -> servSock;
	free(threadUDPArgs);

	HandleUDPClient(servSock);

	return (NULL);
}

void HandleTCPClient(int clntSock){
	char echoBuffer[MAXTEXTLEN];
	int recvMsgSize, result;

	recvMsgSize = RecvMes(echoBuffer, clntSock);

	while (recvMsgSize > 0){
		SendMes(echoBuffer, clntSock);

		recvMsgSize = RecvMes(echoBuffer, clntSock);

		if (strcmp(echoBuffer, "end") == 0){
			SendMes(echoBuffer, clntSock);
			printf("end Connect\n");
			break;
		}
	}

	close(clntSock);
}

int RecvMes(char RecvMes[], int clntSock){
	int recvMsgSize;

	memset(RecvMes, 0, MAXTEXTLEN);

	recvMsgSize = recv(clntSock, RecvMes, MAXTEXTLEN, 0);
	if (recvMsgSize < 0)
		DieWithError("recv() failed");

	printf("TCP:recv: %s(%d)\n", RecvMes, recvMsgSize);

	return recvMsgSize;
}

void SendMes(char SendMes[], int clntSock){
	int SendMsgSize = strlen(SendMes);
	int ResultMsgSize;

	ResultMsgSize = send(clntSock, SendMes, SendMsgSize, 0);
	if (SendMsgSize != ResultMsgSize)
		DieWithError("send() failed");

	printf("TCP:send: %s(%d)\n", SendMes, ResultMsgSize);
}

void HandleUDPClient(int servSock){
	int recvMsgSize, ResultMsgSize;
	unsigned int ClntAddrLen;
	struct sockaddr_in echoClntAddr;
	char echoBuffer[MAXTEXTLEN];

	while(1){
		ClntAddrLen = sizeof(echoClntAddr);

		memset(echoBuffer, 0, MAXTEXTLEN);
		
		recvMsgSize = recvfrom(servSock, echoBuffer, MAXTEXTLEN, 0, (struct sockaddr *) &echoClntAddr, &ClntAddrLen);
		if (recvMsgSize < 0)
			DieWithError("recvfrom() failed");

		printf("UDP:recv: %s(%d)\n", echoBuffer, recvMsgSize);

		//printf("Handling client %s\n", inet_ntoa(echoClntAddr.sin_addr));

		ResultMsgSize = sendto(servSock, echoBuffer, recvMsgSize, 0, (struct sockaddr *) &echoClntAddr, sizeof(echoClntAddr));
		if (ResultMsgSize != recvMsgSize)
			DieWithError("sendto() sent a different number of bytes than expected");

		printf("UDP:send: %s(%d)\n", echoBuffer, ResultMsgSize);
	}
}