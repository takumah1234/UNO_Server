#include <stdio.h>		// printf(),snprintf()
#include <sys/socket.h>	// socket(),connect(),send(),recv()
#include <arpa/inet.h>	// sockaddr_in,inet_addr()
#include <stdlib.h>		// malloc()
#include <string.h>		// memset()
#include <unistd.h>		// close()
#include <pthread.h>	// POSIXスレッド
#include "GameSystem.h"	// GameMaster(), PLAYERNUM, PlayerData
#include "Chat.h"		// ChatEcho()
#include "TCPUDP.h"		// 本体

#define MAXPENDING 	5

typedef struct ThreadTCPArgs
{
	int clntSock[PLAYERNUM];
}ThreadTCPArgs;

typedef struct ThreadUDPArgs
{
	int servSock;
}ThreadUDPArgs;

void *ThreadTCPMain(void *arg);
void *ThreadUDPMain(void *arg);

void StartConnection(int ServPort[]){
	int servSockTCP, clntSockTCP[PLAYERNUM];
	int servSockUDP, clntSockUDP;
	unsigned short ServPortTCP;
	unsigned short ServPortUDP;
	pthread_t threadTCPID;
	pthread_t threadUDPID;
	ThreadTCPArgs *threadTCPArgs;
	ThreadUDPArgs *threadUDPArgs;

	ServPortTCP = ServPort[0];
	ServPortUDP = ServPort[1];
	servSockTCP = CreateTCPServerSocket(ServPortTCP);
	servSockUDP = CreateUDPServerSocket(ServPortUDP);
	
	for(;;){
		for (int i = 0; i < PLAYERNUM; ++i)
		{
			clntSockTCP[i] = AcceptTCPConnection(servSockTCP);
		}

		threadTCPArgs = (ThreadTCPArgs *)malloc(sizeof(ThreadTCPArgs));
		if (threadTCPArgs == NULL)
			DieWithError("malloc() failed");
		for (int i = 0; i < PLAYERNUM; ++i)
		{
			threadTCPArgs -> clntSock[i] = clntSockTCP[i];	
		}

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

int RecvMesForTCP(char RecvMes[], int clntSock){
	int recvMsgSize;

	memset(RecvMes, 0, MAXTEXTLEN);

	recvMsgSize = recv(clntSock, RecvMes, MAXTEXTLEN, 0);
	if (recvMsgSize < 0)
		DieWithError("recv() failed");

	printf("RecvMesForTCP(): recv: %s(%d)\n", RecvMes, recvMsgSize);

	return recvMsgSize;
}

void SendMesForTCP(char SendMes[], int clntSock){
	int SendMsgSize;
	int ResultMsgSize;
	char ResultMes[MAXTEXTLEN];

	snprintf(ResultMes, MAXTEXTLEN-1, "%s;", SendMes);

	if (SendMsgSize > MAXTEXTLEN-1){
		printf("SendMesForUDP(): SendMsgSizeOver!(Size = %d)\n", SendMsgSize);
		return;
	}

	SendMsgSize = strlen(ResultMes);
	ResultMsgSize = send(clntSock, ResultMes, SendMsgSize, 0);
	if (SendMsgSize != ResultMsgSize)
		DieWithError("send() failed");

	printf("SendMesForUDP(): send: %s(%d)\n", ResultMes, ResultMsgSize);
}

int RecvMesForUDP(char RecvMes[], int sock, struct sockaddr_in *ClntAddr){
	int recvMsgSize;
	unsigned int ClntAddrLen = sizeof(ClntAddr);

	memset(RecvMes, 0, MAXTEXTLEN);
		
	recvMsgSize = recvfrom(sock, RecvMes, MAXTEXTLEN, 0, (struct sockaddr *) ClntAddr, &ClntAddrLen);
	if (recvMsgSize < 0)
		DieWithError("recvfrom() failed");

	printf("RecvMesForUDP(): recv: %s(%d)\n", RecvMes, recvMsgSize);

	return recvMsgSize;
}

void SendMesForUDP(char SendMes[], int sock, struct sockaddr_in *ClntAddr, int recvMsgSize){
	int ResultMsgSize;

	printf("Test:SendMesForUDP: %d\n", sock);

	if (recvMsgSize > MAXTEXTLEN-1){
		printf("SendMesForUDP(): recvMsgSizeOver!(Size = %d)\n", recvMsgSize);
		return;
	}

	printf("Test:SendMesForUDP: %d, %s, %d, %s, %lu\n", sock, SendMes, recvMsgSize, inet_ntoa(ClntAddr->sin_addr), sizeof(ClntAddr));

	ResultMsgSize = sendto(sock, SendMes, recvMsgSize, 0, (struct sockaddr *) &ClntAddr, sizeof(ClntAddr));
	printf("%d\n", ResultMsgSize);
	if (ResultMsgSize != recvMsgSize)
		DieWithError("sendto() sent a different number of bytes than expected");

	printf("SendMesForUDP(): send: %s(%d)\n", SendMes, ResultMsgSize);
}

void SendPlayer(char SendMes[], int TP, PlayerData PD[]){
	int ClntSock;

	int PDNum = GetTurnPlayerNum(TP, PD);
	
	ClntSock  = PD[PDNum].GM_clntnum;
	printf("SendPlayer(): To player%d\n", TP);

	SendMesForTCP(SendMes, ClntSock);
}

void SendAll(char sendBuffer[], PlayerData PD[]){
	for (int i = 0; i < PLAYERNUM; ++i)
	{
		SendPlayer(sendBuffer, i, PD);
	}
}

void SendExceptTarget(char sendBuffer[], int Target, PlayerData PD[]){
	for (int i = 0; i < PLAYERNUM; ++i)
	{
		if (Target == PD[i].TurnNumber)	continue;
		SendPlayer(sendBuffer, i, PD);
	}
}

void RecvMes(char RecvMes[], int Target, PlayerData PD[], char JudgeStr[]){
	int clntSock;
	Bool EndFlag = True;
	int PDNum = GetTurnPlayerNum(Target, PD);
	
	clntSock  = PD[PDNum].GM_clntnum;
	printf("RecvMes(): From player%d\n", Target);
	
	RecvMesForTCP(RecvMes, clntSock);
	if (strcmp(RecvMes, "end") == 0){
		for (int i = 0; i < PLAYERNUM; ++i)
		{
			close(PD[i].GM_clntnum);
		}
		printf("recvMes: Client send End\n");
		exit(1);
	}
	int strnum = strlen(JudgeStr);
	if ((strncmp(RecvMes, JudgeStr, strnum)) == 0)
		EndFlag = False;
	if (EndFlag){
		printf("GetMes(player%d->server): %s\n", Target, RecvMes);
		printf("Emergency: EndConnection!\n");
		exit(1);
	}
	printf("GM player%d->server:", Target);
	printf("%s\n", RecvMes);
}

int GetTurnPlayerNum(int TP, PlayerData PD[]){
	for (int i = 0; i < PLAYERNUM; ++i)
	{
		if (PD[i].TurnNumber == TP)
		{
			return i;
		}
	}
	return -1;
}

void *ThreadTCPMain(void *threadTCPArgs){
	int clntSock[PLAYERNUM];
	printf("TCPMain()Start\n");

	pthread_detach(pthread_self());

	for (int i = 0; i < PLAYERNUM; ++i)
	{
		clntSock[i] = ((ThreadTCPArgs *) threadTCPArgs) -> clntSock[i];	
	}
	free(threadTCPArgs);

	GameMaster(clntSock);

	return (NULL);
}

void *ThreadUDPMain(void *threadUDPArgs){
	int servSock;
	printf("UDPMain()Start\n");

	pthread_detach(pthread_self());

	servSock = ((ThreadUDPArgs *) threadUDPArgs) -> servSock;
	free(threadUDPArgs);

	ChatEcho(servSock);

	return (NULL);
}
