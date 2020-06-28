#include <stdio.h>		// printf()
#include <arpa/inet.h>	// struct sockaddr_in
#include <errno.h>
#include "TCPUDP.h"		// RecvMesForUDP(), SendMesForUDP()
#include "GameSystem.h"	// PLAYERNUM
#include "Chat.h"		// 本体

#define DEBUG

void ChatEcho(int servSock){
	int sock = servSock;
	int recvMsgSize;
	unsigned int ServAddrLen;
	static struct sockaddr_in SendAddr[PLAYERNUM];
	struct sockaddr_in RecvAddr;
	char Buffer[MAXTEXTLEN];
	int playernum = 0;

	do{
		recvMsgSize = RecvMesForUDP(Buffer, sock, &RecvAddr);
		if (recvMsgSize >= 0){
			if (playernum < PLAYERNUM){
				printf("Handling client %s\n", inet_ntoa(RecvAddr.sin_addr));
				printf("Buffer = %s\n", Buffer);
				SendAddr[playernum] = RecvAddr;
				printf("player%d = %s\n", playernum, inet_ntoa(SendAddr[playernum].sin_addr));
				snprintf(Buffer, MAXTEXTLEN-1, "OK!");
				if (sendto(sock, Buffer, recvMsgSize, 0, (struct sockaddr *) &RecvAddr, sizeof(RecvAddr)) != recvMsgSize)
					DieWithError("sendto() sent a different number of bytes than expected");
				printf("addPLAYERNUM\n");
				playernum++;
			}else{
				for (int i = 0; i < PLAYERNUM; ++i)
				{
					printf("Handling client %d = %s\n", i, inet_ntoa(SendAddr[i].sin_addr));
					printf("%s\n", Buffer);
					if (sendto(sock, Buffer, recvMsgSize, 0, (struct sockaddr *) &SendAddr[i], sizeof(SendAddr[i])) != recvMsgSize)
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
	/*
	int recvMsgSize;
	unsigned int cliAddrLen, ServAddrLen;
	static struct sockaddr_in SendAddr[PLAYERNUM];
	struct sockaddr_in RecvAddr;
	char Buffer[MAXTEXTLEN];
	int nowPlayerNum = 0;

	do{
		printf("ChatEcho(): RecvNow...(sock=%d)\n", sock);
		recvMsgSize = RecvMesForUDP(Buffer, sock, &RecvAddr);
		printf("chat:1:%d\n", sock);
		if (recvMsgSize >= 0){
			if (nowPlayerNum < PLAYERNUM){
				#ifdef DEBUG
				printf("GetMes = %s\n", Buffer);
				#endif
				SendAddr[nowPlayerNum] = RecvAddr;
				printf("player%d = %s\n", nowPlayerNum, inet_ntoa(SendAddr[nowPlayerNum].sin_addr));
				printf("2:%d\n", sock);
				SendMesForUDP("OK!", sock, &SendAddr[nowPlayerNum]);
				#ifdef DEBUG
				printf("addMAXPlayerNum\n");
				#endif
				nowPlayerNum++;
			}else{
				for (int i = 0; i < PLAYERNUM; ++i)
				{
					#ifdef DEBUG
					printf("Targetclient%d(IPAddr:%s)\n", i, inet_ntoa(SendAddr[i].sin_addr));
					#endif
					SendMesForUDP(Buffer, sock, &SendAddr[i]);
				}
			}
		}
	}while(recvMsgSize >= 0);
	*/
}