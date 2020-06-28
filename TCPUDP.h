#include <arpa/inet.h>	// struct sockaddr_in
#include "GameSystem.h"	// PlayerData

#define MAXTEXTLEN	256

int CreateTCPServerSocket(unsigned short port);
int CreateUDPServerSocket(unsigned short port);
int AcceptTCPConnection(int servSock);
void DieWithError(char *errorMessage);
int RecvMesForTCP(char RecvMes[], int clntSock);
void SendMesForTCP(char SendMes[], int clntSock);
int RecvMesForUDP(char RecvMes[], int sock, struct sockaddr_in *ClntAddr);
void SendMesForUDP(char SendMes[], int sock, struct sockaddr_in *ClntAddr, int recvMsgSize);
void StartConnection(int ServPort[]);
void SendPlayer(char SendMes[], int TP, PlayerData PD[]);
void SendAll(char sendBuffer[], PlayerData PD[]);
void SendExceptTarget(char sendBuffer[], int Target, PlayerData PD[]);
void RecvMes(char RecvMes[], int Target, PlayerData PD[], char JudgeStr[]);
int GetTurnPlayerNum(int TP, PlayerData PD[]);