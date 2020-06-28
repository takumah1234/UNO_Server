#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#define MAXTEXTLEN	265
#define MAXPLAYER 2

int sock;
int count = 0;
int PLAYERNUM = 0;

void CreateUDPServerSocket(unsigned short port);
void DieWithError(char *errorMessage);
void ChatEcho(int signalType);

int main(int argc, char *argv[])
{
	unsigned short ServPort;
	char echoBuffer[MAXTEXTLEN];

	if (argc != 2)
	{
		fprintf(stderr, "Usage: %s <Server Port>\n", argv[0]);
		exit(1);
	}

	ServPort = atoi(argv[1]);

	CreateUDPServerSocket(ServPort);
	
	for(;;)
	{
		printf(".\n");
		sleep(3);
	}
}

void CreateUDPServerSocket(unsigned short port){
	struct sockaddr_in echoServAddr, echoClntAddr;
	int recvMesSize;
	char GetNewCliAddr[MAXTEXTLEN];
	struct sigaction handler;


	if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		DieWithError("socket() failed");

	memset(&echoServAddr, 0, sizeof(echoServAddr));
	echoServAddr.sin_family = AF_INET;
	echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	echoServAddr.sin_port = htons(port);

	if (bind(sock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0)
		DieWithError("bind() failed");

	handler.sa_handler = ChatEcho;
	if(sigfillset(&handler.sa_mask)<0)
		DieWithError("sigfillset() failed");
	handler.sa_flags=0;

	if (sigaction(SIGIO, &handler, 0) < 0)
		DieWithError("sigaction() failed for SIGIO");

	if (fcntl(sock, F_SETOWN, getpid()) < 0)
		DieWithError("Unable to set process owner to us");

	if (fcntl(sock, F_SETFL, O_NONBLOCK | FASYNC) < 0)
		DieWithError("Unable to put client sock into nonblocking/async mode");
}

void DieWithError(char *errorMessage)
{
	perror(errorMessage);
	exit(1);
}

void ChatEcho(int signalType){
	int recvMsgSize;
	unsigned int cliAddrLen, ServAddrLen;
	static struct sockaddr_in echoClntAddr[MAXPLAYER];
	struct sockaddr_in echoServAddr;
	char GetNewCliAddr[MAXTEXTLEN];

	do{
		ServAddrLen = sizeof(echoServAddr);
	
		if ((recvMsgSize = recvfrom(sock, GetNewCliAddr, MAXTEXTLEN, 0, (struct sockaddr *) &echoServAddr, &ServAddrLen)) < 0){
			if (errno != EWOULDBLOCK)
				DieWithError("recvfrom() failed");
		}else{
			if (PLAYERNUM < MAXPLAYER){
				printf("Handling client %s\n", inet_ntoa(echoServAddr.sin_addr));
				printf("GetNewCliAddr = %s\n", GetNewCliAddr);
				echoClntAddr[PLAYERNUM] = echoServAddr;
				printf("player%d = %s\n", PLAYERNUM, inet_ntoa(echoClntAddr[PLAYERNUM].sin_addr));
				snprintf(GetNewCliAddr, MAXTEXTLEN-1, "OK!");
				if (sendto(sock, GetNewCliAddr, recvMsgSize, 0, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) != recvMsgSize)
					DieWithError("sendto() sent a different number of bytes than expected");
				printf("addMAXPLAYERNUM\n");
				PLAYERNUM++;
			}else{
				//if (sendto(sock, GetNewCliAddr, recvMsgSize, 0, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) != recvMsgSize)
				//	DieWithError("sendto() sent a different number of bytes than expected");
				for (int i = 0; i < MAXPLAYER; ++i)
				{
					printf("Handling client %d = %s\n", i, inet_ntoa(echoClntAddr[i].sin_addr));
					//snprintf(GetNewCliAddr, MAXTEXTLEN-1,"You are Player%d!", i+1);
					printf("%s\n", GetNewCliAddr);
					if (sendto(sock, GetNewCliAddr, recvMsgSize, 0, (struct sockaddr *) &echoClntAddr[i], sizeof(echoClntAddr[i])) != recvMsgSize)
						DieWithError("sendto() sent a different number of bytes than expected");
				}
			}
		}
		/*
		count++;
		if (count >= 10)
		{
			printf("%d\n", count);
			break;
		}
		*/
	}while(recvMsgSize >= 0);
}