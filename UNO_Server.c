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

//#define TEST
#define GAMEONLY

#define MAXPENDING 	5
#define MAXTEXTLEN	256
#define DECKNUM		4 * (1 * 1 + 2 * 9)

#ifndef TEST
#define PLAYERNUM	3
#endif
#ifdef TEST
#define PLAYERNUM	2
#endif
// プレイヤーのデータ等を保管する構造体や列挙体
// 送受信の結果を示す際に用いる列挙体
typedef enum Mestype {
	Chat,
	GM
}MesType;

typedef enum Bool{
	False,
	True
}Bool;

typedef struct CardData
{
	int number;
	char color;
}CardData;

typedef struct Mes
{
	MesType type;
	char text[MAXTEXTLEN];
}Mes;

typedef struct PlayerData
{
	int TurnNumber;
	CardData hand[DECKNUM];
	int handnum;
	Bool Uno_flag;
	int GM_clntnum;
	struct sockaddr_in Chat_Addr;
}PlayerData;

typedef struct deck
{
	CardData Cards[DECKNUM];
	int DeckNum;
}Deck;

// グローバル変数
int g_playernum = 0;
Deck g_deck;
PlayerData g_playerdata[PLAYERNUM];
Deck g_GraveZone;
CardData nul = {0,'n'};
// ターン数
int g_TurnNum = 0;
// 送られてきたGM用メッセージを判定するための変数
char g_target[3][100] = {"Release", "Draw", "Turn"};
int g_changeHandNum = 0;
// チャット用ソケット
int g_ChatSock;
int g_chatMemberNum = 0;
// 二重通信用
int g_servSock;
fd_set g_fds_tcp, g_readfds_tcp, g_fds_udp, g_readfds_udp;
int g_maxfd;
Bool g_SelectSetFlag = True;

// 関数
// 接続用関数群
int CreateTCPServerSocket(unsigned short port);
void CreateUDPServerSocket(unsigned short port);
int AcceptTCPConnection(int servSock);
void AcceptTCPConnectionS(int servSock1);
void DieWithError(char *errorMessage);
// メッセージ送受信関係用関数群
void SendMes(int playernum, Mes sendBuffer);
void SendAll(Mes sendBuffer);
void SendExceptTarget(Mes sendBuffer, int Target);
Mes RecvMes(int playernum, MesType type);
Bool JudgeMes(char mes[], char stand[]);
// ゲーム進行用関数群
void shuffle(void);
void draw(int playernum);
void ReleaseCard(int TP, CardData CD);
void addGraveZone(CardData CD);
CardData GetCardData(char target[], char stand[]);
Bool JudgeEnd(void);
void Game_init(void);
void GameMaster(void);
void DrawGameHandicap(int TP, Mes recvMes);
Bool SkipTurn(int TP, Mes recvMes);
// チャット部分の関数群
void ChatEcho(int signalType);

int main(int argc, char *argv[])
{
	int servSock;
	unsigned short ServPort1, ServPort2;

	#ifdef GAMEONLY
	if (argc != 2)
	{
		fprintf(stderr, "Usage: %s <Server Port1>\n", argv[0]);
		exit(1);
	}

	srand((unsigned int)time(NULL));

	ServPort1 = atoi(argv[1]);

	g_servSock = CreateTCPServerSocket(ServPort1);
	AcceptTCPConnectionS(g_servSock);
	#endif
	#ifndef GAMEONLY
	if (argc != 3)
	{
		fprintf(stderr, "Usage: %s <Server Port1(GM)> <Server Port2(Chat)>\n", argv[0]);
		exit(1);
	}

	srand((unsigned int)time(NULL));

	ServPort1 = atoi(argv[1]);
	ServPort2 = atoi(argv[2]);

	g_servSock = CreateTCPServerSocket(ServPort1);
	AcceptTCPConnectionS(g_servSock);
	CreateUDPServerSocket(ServPort2);
	FD_ZERO(&g_readfds_tcp);
	//FD_ZERO(&g_readfds_udp);
	FD_SET(g_servSock, &g_readfds_tcp);
	//FD_SET(g_ChatSock, &g_readfds_udp);
	/*
	if (g_servSock > g_ChatSock){
		g_maxfd = g_servSock;
	}else{
		g_maxfd = g_ChatSock;
	}
	*/
	g_maxfd = g_servSock;
	#endif
	/*
	for (int i = 0; i < g_playernum; ++i)
	{
		
	}
	*/
		
	printf("Main(): AllPlayerAccept!\n");
	printf("Main(): GameStart!!\n");

	GameMaster();
	
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

void CreateUDPServerSocket(unsigned short port){
	struct sockaddr_in echoServAddr;
	struct sigaction handler;


	if ((g_ChatSock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		DieWithError("socket() failed");

	memset(&echoServAddr, 0, sizeof(echoServAddr));
	echoServAddr.sin_family = AF_INET;
	echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	echoServAddr.sin_port = htons(port);

	if (bind(g_ChatSock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0)
		DieWithError("bind() failed");

	handler.sa_handler = ChatEcho;
	if(sigfillset(&handler.sa_mask)<0)
		DieWithError("sigfillset() failed");
	handler.sa_flags=0;

	if (sigaction(SIGIO, &handler, 0) < 0)
		DieWithError("sigaction() failed for SIGIO");

	if (fcntl(g_ChatSock, F_SETOWN, getpid()) < 0)
		DieWithError("Unable to set process owner to us");

	if (fcntl(g_ChatSock, F_SETFL, O_NONBLOCK | FASYNC) < 0)
		DieWithError("Unable to put client sock into nonblocking/async mode");
}

void AcceptTCPConnectionS(int servSock1){
	int GM_clntSock;

	for (int i = 0; i < PLAYERNUM; ++i)
	{
		GM_clntSock = AcceptTCPConnection(servSock1);

		g_playerdata[g_playernum].GM_clntnum = GM_clntSock;
		g_playernum++;
		printf("main(): EnterThisRoom %d/%d(sock = %d)\n", g_playernum, PLAYERNUM, GM_clntSock);
	}
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

void SendMes(int playernum, Mes sendBuffer)
{
	int recvMsgSize, clntSock;
	char sendMes[MAXTEXTLEN];
	
	for (int i = 0; i < g_playernum; ++i)
	{
		if (g_playerdata[i].TurnNumber == playernum){
			switch(sendBuffer.type){
			case GM:
				clntSock = g_playerdata[i].GM_clntnum;
				printf("GM server->player%d(clntnum=%d):", playernum, clntSock);
				break;
			case Chat:
				//clntSock = g_playerdata[playernum].Chat_clntnum;
				//printf("Chat server->player%d:", playernum);
				break;
			}
			break;
		}
	}
	memset(sendMes, 0, MAXTEXTLEN);
	snprintf(sendMes, MAXTEXTLEN-1, "%s;", sendBuffer.text);

	recvMsgSize = strlen(sendMes);
	if (send(clntSock, sendMes, recvMsgSize, 0) != recvMsgSize)
		DieWithError("send() failed");
	printf("%s\n", sendMes);
}

void SendAll(Mes sendBuffer){
	int TP;
	for(int i = 0; i < g_playernum; i++){
		TP = g_playerdata[i].TurnNumber;
		SendMes(TP, sendBuffer);
	}
}

void SendExceptTarget(Mes sendBuffer, int Target){
	int TP;
	for(int i = 0; i < g_playernum; i++){
		TP = g_playerdata[i].TurnNumber;
		if (TP == Target)	continue;
		SendMes(TP, sendBuffer);
	}
}

Mes RecvMes(int playernum, MesType type)
{
	char recvBuffer[MAXTEXTLEN];
	Mes recvMes;
	char tmp[2];
	int clntSock;
	Bool EndFlag = True;

	for (int i = 0; i < g_playernum; ++i)
	{
		if (g_playerdata[i].TurnNumber == playernum){
			switch (type){
			case GM:
				clntSock  = g_playerdata[i].GM_clntnum;
				printf("RecvMes From player%d(clntSock:%d)\n", playernum, clntSock);
				break;
			case Chat:
				//clntSock  = g_playerdata[playernum].Chat_clntnum;
			break;
			}
		}
	}

	memset(recvBuffer, 0, MAXTEXTLEN);

	#ifndef GAMEONLY
	memcpy(&g_fds_tcp, &g_readfds_tcp, sizeof(fd_set));
	select(g_maxfd+1, &g_fds_tcp, NULL, NULL, NULL);
	if (FD_ISSET(g_servSock, &g_fds_tcp)){
	#endif
		int recvMsgSize = recv(clntSock, recvBuffer, MAXTEXTLEN, 0);
		if ( recvMsgSize < 0)
			DieWithError("recv() failed");
		printf("%d\n", recvMsgSize);
		strcpy(recvMes.text, recvBuffer);
		//printf("recvMes %s\n", recvMes.text);
		
		if (strcmp(recvMes.text, "end") == 0){
			for (int i = 0; i < g_playernum; ++i)
			{
				close(g_playerdata[i].GM_clntnum);
				//close(g_playerdata[i].Chat_clntnum);
			}
			printf("recvMes: Client send End\n");
			exit(1);
		}
		for (int i = 0; i < 3; ++i)
		{
			int strnum = strlen(g_target[i]);
			if ((strncmp(recvMes.text, g_target[i], strnum) == 0 && type == GM) || type == Chat)
			{
				EndFlag = False;
				break;
			}
		}
		if (EndFlag){
			for (int i = 0; i < g_playernum; ++i)
			{
				close(g_playerdata[i].GM_clntnum);
				//close(g_playerdata[i].Chat_clntnum);
			}
			printf("GetMes(player%d->server): %s\n", playernum, recvMes.text);
			printf("Emergency: EndConnection!\n");
			exit(1);
		}
		
		switch(type){
			case GM:
				printf("GM player%d->server:", playernum);
				recvMes.type = GM;
				break;
			case Chat:
				printf("Chat player%d->server:", playernum);
				recvMes.type = Chat;
				break;
		}
		printf("%s\n", recvMes.text);

		return recvMes;
	#ifndef GAMEONLY
	}else{
		exit(1);
		recvMes.type = GM;
		snprintf(recvMes.text, MAXTEXTLEN-1, "NULL");
		return recvMes;
	}
	#endif
}

Bool JudgeMes(char mes[], char stand[]){
	int targetNum = strlen(stand);

	if (strncmp(mes, stand, targetNum) == 0)
		return True;
	else
		return False;
}

void shuffle(void){
	int i,j,k;
	CardData tmp;
	int shuffle_num = 10000;

	for (i = 0; i < shuffle_num; i++){
		j = rand()%g_deck.DeckNum;
		k = rand()%g_deck.DeckNum;
		tmp = g_deck.Cards[j];
		g_deck.Cards[j] = g_deck.Cards[k];
		g_deck.Cards[k] = tmp;
	}
}
void draw(int playernum){
	Mes sendMes;
	int HN = g_playerdata[playernum].handnum;
	int TP = g_playerdata[playernum].TurnNumber;

	g_deck.DeckNum--;
	
	// プレイヤーの手札を更新
	g_playerdata[playernum].handnum++;
	g_playerdata[playernum].hand[HN] = g_deck.Cards[g_deck.DeckNum];
	//printf("draw(): DrawCard%c%d\nPlayer%d'sHand = %d\n", g_deck.Cards[g_deck.DeckNum].color, g_deck.Cards[g_deck.DeckNum].number, playernum, g_playerdata[playernum].handnum++);

	// 送るメッセージの書き込み
	sendMes.type = GM;
	snprintf(sendMes.text, MAXTEXTLEN-1, "myHand:%c%d", g_playerdata[playernum].hand[HN].color, g_playerdata[playernum].hand[HN].number);
	
	// デッキの中身を実際にnul状態に変更
	g_deck.Cards[g_deck.DeckNum] = nul;

	// メッセージの送信
	SendMes(TP, sendMes);
}

void Game_init(void){
	int init_hand_num = 7;
	int flag;
	// カードの種類(0~9)
	int cardsNumber = 10;
	// カードの色の種類(4or5)
	int colorsNumber = 4;
	char color;
	// 同じカードは2枚(0に限り1枚)
	int samenumber = 2;
	Mes sendMes;
	int TP;

	g_deck.DeckNum = 0;

	for (int i = 0; i < colorsNumber; ++i)
	{
		switch (i){
			case 0:
				color = 'r';
				break;
			case 1:
				color = 'b';
				break;
			case 2:
				color = 'g';
				break;
			case 3:
				color = 'y';
				break;
		}
		for (int j = 0; j < cardsNumber; ++j)
		{
			// 0だけは1枚なので、j=0の時には1枚入れたらbreakする。
			if (j == 0){
				g_deck.Cards[g_deck.DeckNum].number = j;
				g_deck.Cards[g_deck.DeckNum].color = color;
				g_deck.DeckNum++;
				continue;
			}
			for (int k = 0; k < samenumber; ++k)
			{
				g_deck.Cards[g_deck.DeckNum].number = j;
				g_deck.Cards[g_deck.DeckNum].color = color;
				g_deck.DeckNum++;
			}
		}
	}

	printf("Game_init():CardSet Complete\n");

	#ifndef TEST
	for (int i = 0; i < g_playernum; ++i)
	{
		flag = 1;
		while (flag){
			g_playerdata[i].TurnNumber = rand()%g_playernum;
			if (i == 0){
				g_playerdata[i].TurnNumber = 0;
				break;
			}
			for (int j = 0; j < i; ++j)
			{
				if (g_playerdata[j].TurnNumber == g_playerdata[i].TurnNumber)
				{
					flag = 1;
					break;
				}else
				{
					flag = 0;
				}
			}
		}
		printf("Game_init():Player%d is TurnPlayer%d(sock=%d)\n", i, g_playerdata[i].TurnNumber, g_playerdata[i].GM_clntnum);
	}
	#endif
	#ifdef TEST
	g_playerdata[0].TurnNumber = 0;
	g_playerdata[1].TurnNumber = 1;
	#endif
	for (int i = 0; i < g_playernum; ++i)
	{
		sendMes.type = GM;
		snprintf(sendMes.text, MAXTEXTLEN-1, "myturn:%d", g_playerdata[i].TurnNumber);
		SendMes(g_playerdata[i].TurnNumber, sendMes);
	}

	printf("Game_init():DecideTurnPlayer Complete\n");

	shuffle();

	printf("Game_init():Shuffle Complete\n");

	for (int i = 0; i < g_playernum; ++i)
	{
		g_playerdata[i].handnum = 0;
	}

	for (int i = 0; i < init_hand_num; ++i)
	{
		for (int j = 0; j < g_playernum; ++j)
		{
			draw(j);
		}
	}

	printf("Game_init():Draw Complete\n");


	// 全てのクライアントにフィールド場の情報を提供
	g_GraveZone.DeckNum = 0;
	g_deck.DeckNum--;
	g_GraveZone.Cards[g_GraveZone.DeckNum] = g_deck.Cards[g_deck.DeckNum];
	g_deck.Cards[g_deck.DeckNum] = nul;
	g_GraveZone.DeckNum++;

	sendMes.type = GM;
	snprintf(sendMes.text, MAXTEXTLEN-1, "Grave:%c%d", g_GraveZone.Cards[g_GraveZone.DeckNum-1].color, g_GraveZone.Cards[g_GraveZone.DeckNum-1].number);
	SendAll(sendMes);

	printf("Game_init():GraveZoneSet Complete\n");

	sendMes.type = GM;
	for (int i = 0; i < g_playernum; ++i)
	{
		TP = g_playerdata[i].TurnNumber;
		for (int j = 0; j < g_playernum ; ++j)
		{
			if (j == i) continue;
			snprintf(sendMes.text, MAXTEXTLEN-1, "othersturn:%d", g_playerdata[j].TurnNumber);
			SendMes(TP, sendMes);
		}
	}

	sendMes.type = GM;
	for (int i = 0; i < g_playernum; ++i)
	{
		for (int j = 0; j < g_playernum; ++j)
		{
			if (j == i)
			{
				continue;
			}
			snprintf(sendMes.text, MAXTEXTLEN-1, "othershand:%d", g_playerdata[j].handnum);
			SendMes(i, sendMes);
		}
	}

	printf("Game_init():OtherPlayerInfomationNotice Complete\n");
}


void ReleaseCard(int TP, CardData CD){
	Mes sendMes;
	
	for (int i = 0; i < g_playerdata[TP].handnum; i++){
		printf("ReleaseCard(): TargetCard=>%c%d\n",g_playerdata[TP].hand[i].color,g_playerdata[TP].hand[i].number);
		if (g_playerdata[TP].hand[i].color == CD.color && g_playerdata[TP].hand[i].number == CD.number)
		{
			printf("ReleaseCard(): ReleaseCard=>%c%d\n", CD.color, CD.number);
			for (int j = i+1; j < g_playerdata[TP].handnum; ++j)
			{
				g_playerdata[TP].hand[j-1] = g_playerdata[TP].hand[j];
			}
			g_playerdata[TP].handnum -= 1;
			break;
		}
	}
	g_changeHandNum--;
}

void addGraveZone(CardData CD){
	Mes sendMes;
	CardData GZ = CD;

	g_GraveZone.Cards[g_GraveZone.DeckNum] = GZ;
	g_GraveZone.DeckNum++;
	sendMes.type = GM;
	snprintf(sendMes.text, MAXTEXTLEN-1, "Grave:%c%d", GZ.color, GZ.number);
	SendAll(sendMes);
}

CardData GetCardData(char target[], char stand[]){
	CardData ret;
	int targetNum = strlen(stand);

	ret.color = target[targetNum+2];
	ret.number = atoi(&target[targetNum+3]);

	printf("GetCardData(): GetCard: %c%d\n", ret.color, ret.number);

	return ret;
}

Bool JudgeEnd(void){
	int TP;
	for (int i = 0; i < g_playernum; ++i)
	{
		printf("JudgeEnd(): PlayerData[%d].handnum = %d\n", i, g_playerdata[i].handnum);
		if(g_playerdata[i].handnum == 0){
			Mes sendMes;
			TP = g_playerdata[i].TurnNumber;
			sendMes.type = GM;
			snprintf(sendMes.text, MAXTEXTLEN-1, "turn:end");
			SendAll(sendMes);
			snprintf(sendMes.text, MAXTEXTLEN-1, "win:0");
			SendMes(TP, sendMes);
			snprintf(sendMes.text, MAXTEXTLEN-1, "lose:0");
			SendExceptTarget(sendMes, TP);
			return True;
		}
	}
	return False;
}

void DrawGameHandicap(int TP, Mes recvMes){
	char *ret;
	int targetPlayer;
	Mes sendMes;

	if ((ret = strchr(recvMes.text, ':')) != NULL){
		targetPlayer = atoi(ret+1);
		if (targetPlayer != -1 && targetPlayer == TP){
			draw(TP);
			g_changeHandNum++;
		}else{
			sendMes.type = GM;
			snprintf(sendMes.text, MAXTEXTLEN-1, "n0");
			SendMes(TP, sendMes);
		}
	}
}

Bool SkipTurn(int TP, Mes recvMes){
	int result = atoi(&recvMes.text[strlen(g_target[2])+1]);
	Mes sendMes;

	sendMes.type = GM;
	if (result == 1){
		printf("SkipTurn(): Player%d'sTurnStart(result:%d)\n", TP, result);
		snprintf(sendMes.text, MAXTEXTLEN-1, "Turn:1");
	}
	else{
		printf("SkipTurn(): SkipPlayer%d'sTurn(result:%d)\n", TP, result);
		snprintf(sendMes.text, MAXTEXTLEN-1, "Turn:0");
	}
	SendExceptTarget(sendMes, TP);
	return result;
}

void GameMaster(void){
	int TP;
	CardData Card;
	Mes sendMes, recvMes;

	// スタート時の初期化
	Game_init();

	printf("GameMaster():Game_init() Complete\n");

	// ターンの開始
	while (1){
		g_changeHandNum = 0;
		// ターン数の通知
		TP = g_TurnNum % g_playernum;
		sendMes.type = GM;
		snprintf(sendMes.text, MAXTEXTLEN-1, "turn:%d", g_TurnNum);
		SendAll(sendMes);
		// 出せるカードがあるかを受信。なければ一枚カードを引かせる。(g_target[1] = "Draw")
		recvMes = RecvMes(TP, GM);
		while (!JudgeMes(recvMes.text, g_target[1])){
			recvMes = RecvMes(TP, GM);
		}
		DrawGameHandicap(TP, recvMes);
		// カードを一枚引いた上で出せるカードがなければターンをスキップさせる。(g_target[2] = "Turn")
		recvMes = RecvMes(TP, GM);
		while (!JudgeMes(recvMes.text, g_target[2])){
			recvMes = RecvMes(TP, GM);
		}
		if (SkipTurn(TP,recvMes)){
			recvMes = RecvMes(TP, GM);
			while(!JudgeMes(recvMes.text, g_target[0])){
				recvMes = RecvMes(TP, GM);
			}
			Card = GetCardData(recvMes.text, g_target[0]);
			ReleaseCard(TP, Card);
			addGraveZone(Card);
		}
		sendMes.type = GM;
		snprintf(sendMes.text, MAXTEXTLEN-1, "othershandChage:%d", g_changeHandNum);
		SendExceptTarget(sendMes, TP);
		if (JudgeEnd())	break;
		g_TurnNum++;
	}
	for (int i = 0; i < g_playernum; ++i)
	{
		close(g_playerdata[i].GM_clntnum);
	}
}

void ChatEcho(int signalType){
	int recvMsgSize;
	unsigned int NewAddrLen;
	static struct sockaddr_in echoClntAddr[PLAYERNUM];
	struct sockaddr_in echoNewAddr;
	char MessageBuffer[MAXTEXTLEN];

	do{
		NewAddrLen = sizeof(echoNewAddr);
		
		//memcpy(&g_fds_udp, &g_readfds_udp, sizeof(fd_set));
		//select(g_maxfd+1, &g_fds_udp, NULL, NULL, NULL);
		//if (FD_ISSET(g_ChatSock, &g_fds_udp)){
			if ((recvMsgSize = recvfrom(g_ChatSock, MessageBuffer, MAXTEXTLEN, 0, (struct sockaddr *) &echoNewAddr, &NewAddrLen)) < 0){
				if (errno != EWOULDBLOCK)
					DieWithError("recvfrom() failed");
			}else{
				if (g_chatMemberNum < PLAYERNUM){
					printf("Handling client %s\n", inet_ntoa(echoNewAddr.sin_addr));
					printf("MessageBuffer = %s\n", MessageBuffer);
					echoClntAddr[g_chatMemberNum] = echoNewAddr;
					printf("player%d = %s\n", g_chatMemberNum, inet_ntoa(echoClntAddr[g_chatMemberNum].sin_addr));
					snprintf(MessageBuffer, MAXTEXTLEN-1, "OK!");
					if (sendto(g_ChatSock, MessageBuffer, recvMsgSize, 0, (struct sockaddr *) &echoNewAddr, sizeof(echoNewAddr)) != recvMsgSize)
						DieWithError("sendto() sent a different number of bytes than expected");
					printf("addMAXg_chatMemberNum\n");
					g_chatMemberNum++;
				}else{
					//if (sendto(g_ChatSock, MessageBuffer, recvMsgSize, 0, (struct sockaddr *) &echoNewAddr, sizeof(echoNewAddr)) != recvMsgSize)
					//	DieWithError("sendto() sent a different number of bytes than expected");
					for (int i = 0; i < PLAYERNUM; ++i)
					{
						printf("Handling client %d = %s\n", i, inet_ntoa(echoClntAddr[i].sin_addr));
						//snprintf(MessageBuffer, MAXTEXTLEN-1,"You are Player%d!", i+1);
						printf("%s\n", MessageBuffer);
						if (sendto(g_ChatSock, MessageBuffer, recvMsgSize, 0, (struct sockaddr *) &echoClntAddr[i], sizeof(echoClntAddr[i])) != recvMsgSize)
							DieWithError("sendto() sent a different number of bytes than expected");
					}
				}
			}
		//}
		//count++;
		//if (count >= 10)
		//{
		//	printf("%d\n", count);
		//	break;
		//}
	}while(recvMsgSize >= 0);
}