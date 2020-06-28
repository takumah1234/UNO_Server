#include <stdio.h>			// snprintf(), printf()
#include <stdlib.h>			// rand(),atoi()
#include <string.h>			// memset(),strlen()
#include <unistd.h>			// close()
#include "TCPUDP.h"			// SendPlayer(),SendAll(),SendExceptTarget(),RecvMes()
#include "GameSystem.h"		//本体

#define TEST

typedef struct deck
{
	CardData Cards[DECKNUM];
	int DeckNum;
}Deck;

// 各グローバル変数
Deck g_deck;
PlayerData g_playerdata[PLAYERNUM];
Deck g_GraveZone;
// ターン数
int g_TurnNum = 0;
// カードがない時に用いる
CardData nul = {0,'n'};
// ターンプレイヤーの手札の変化量
int g_changeHandNum = 0;
// 送られてきたGM用メッセージを判定するための変数
char g_target[3][100] = {"Release", "Draw", "Turn"};

// ゲーム進行用関数群
void shuffle(void);
void draw(int TP);
void ReleaseCard(int TP, CardData CD);
void addGraveZone(CardData CD);
CardData GetCardData(char target[], char stand[]);
Bool JudgeEnd(void);
void Game_init(void);
void GameMaster(int clntSock[]);
void DrawGameHandicap(int TP, char recvMes[]);
Bool SkipTurn(int TP, char recvMes[]);

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

void draw(int TP){
	char sendMes[MAXTEXTLEN];
	int PDNum = GetTurnPlayerNum(TP, g_playerdata);
	int HN = g_playerdata[PDNum].handnum;

	g_deck.DeckNum--;
	
	// プレイヤーの手札を更新
	g_playerdata[PDNum].handnum++;
	g_playerdata[PDNum].hand[HN] = g_deck.Cards[g_deck.DeckNum];
	//printf("draw(): DrawCard%c%d\nPlayer%d'sHand = %d\n", g_deck.Cards[g_deck.DeckNum].color, g_deck.Cards[g_deck.DeckNum].number, PDNum, g_playerdata[PDNum].handnum++);

	// 送るメッセージの書き込み
	snprintf(sendMes, MAXTEXTLEN-1, "myHand:%c%d", g_playerdata[PDNum].hand[HN].color, g_playerdata[PDNum].hand[HN].number);
	
	// デッキの中身を実際にnul状態に変更
	g_deck.Cards[g_deck.DeckNum] = nul;

	// メッセージの送信
	SendPlayer(sendMes, TP, g_playerdata);
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
	char sendMes[MAXTEXTLEN];
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
	for (int i = 0; i < PLAYERNUM; ++i)
	{
		flag = 1;
		while (flag){
			g_playerdata[i].TurnNumber = rand()%PLAYERNUM;
			if (i == 0)	break;
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
	g_playerdata[2].TurnNumber = 2;
	#endif
	for (int i = 0; i < PLAYERNUM; ++i)
	{
		memset(sendMes, 0, MAXTEXTLEN);
		snprintf(sendMes, MAXTEXTLEN-1, "myturn:%d", g_playerdata[i].TurnNumber);
		SendPlayer(sendMes, g_playerdata[i].TurnNumber, g_playerdata);
	}

	printf("Game_init():DecideTurnPlayer Complete\n");

	shuffle();

	printf("Game_init():Shuffle Complete\n");

	for (int i = 0; i < PLAYERNUM; ++i)
	{
		g_playerdata[i].handnum = 0;
	}

	for (int i = 0; i < init_hand_num; ++i)
	{
		for (int j = 0; j < PLAYERNUM; ++j)
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

	memset(sendMes, 0, MAXTEXTLEN);
	snprintf(sendMes, MAXTEXTLEN-1, "Grave:%c%d", g_GraveZone.Cards[g_GraveZone.DeckNum-1].color, g_GraveZone.Cards[g_GraveZone.DeckNum-1].number);
	SendAll(sendMes, g_playerdata);

	printf("Game_init():GraveZoneSet Complete\n");

	for (int i = 0; i < PLAYERNUM; ++i)
	{
		// 自身のターンから一つずつ増やした形で手札を配布すべき?
		TP = g_playerdata[i].TurnNumber;
		for (int j = 0; j < PLAYERNUM ; ++j)
		{
			if (j == i) continue;
			memset(sendMes, 0, MAXTEXTLEN);
			snprintf(sendMes, MAXTEXTLEN-1, "othersturn:%d", g_playerdata[j].TurnNumber);
			SendPlayer(sendMes, TP, g_playerdata);
		}
	}

	for (int i = 0; i < PLAYERNUM; ++i)
	{
		for (int j = 0; j < PLAYERNUM; ++j)
		{
			if (j == i)
			{
				continue;
			}
			memset(sendMes, 0, MAXTEXTLEN);
			snprintf(sendMes, MAXTEXTLEN-1, "othershand:%d", g_playerdata[j].handnum);
			SendPlayer(sendMes, g_playerdata[i].TurnNumber, g_playerdata);
		}
	}

	printf("Game_init():OtherPlayerInfomationNotice Complete\n");
}


void ReleaseCard(int TP, CardData CD){
	char sendMes[MAXTEXTLEN];
	int PDNum = GetTurnPlayerNum(TP, g_playerdata);
	
	for (int i = 0; i < g_playerdata[PDNum].handnum; i++){
		printf("ReleaseCard(): TargetCard=>%c%d\n",g_playerdata[PDNum].hand[i].color,g_playerdata[PDNum].hand[i].number);
		if (g_playerdata[PDNum].hand[i].color == CD.color && g_playerdata[PDNum].hand[i].number == CD.number)
		{
			printf("ReleaseCard(): ReleaseCard=>%c%d\n", CD.color, CD.number);
			for (int j = i+1; j < g_playerdata[PDNum].handnum; ++j)
			{
				g_playerdata[PDNum].hand[j-1] = g_playerdata[PDNum].hand[j];
			}
			g_playerdata[PDNum].handnum -= 1;
			break;
		}
	}
	g_changeHandNum--;
}

void addGraveZone(CardData CD){
	char sendMes[MAXTEXTLEN];
	CardData GZ = CD;

	g_GraveZone.Cards[g_GraveZone.DeckNum] = GZ;
	g_GraveZone.DeckNum++;
	memset(sendMes, 0, MAXTEXTLEN);
	snprintf(sendMes, MAXTEXTLEN-1, "Grave:%c%d", GZ.color, GZ.number);
	SendAll(sendMes, g_playerdata);
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
	char sendMes[MAXTEXTLEN];
	for (int i = 0; i < PLAYERNUM; ++i)
	{
		printf("JudgeEnd(): PlayerData[%d].handnum = %d\n", i, g_playerdata[i].handnum);
		if(g_playerdata[i].handnum == 0){
			TP = g_playerdata[i].TurnNumber;
			memset(sendMes, 0, MAXTEXTLEN);
			snprintf(sendMes, MAXTEXTLEN-1, "turn:end");
			SendAll(sendMes, g_playerdata);
			memset(sendMes, 0, MAXTEXTLEN);
			snprintf(sendMes, MAXTEXTLEN-1, "win:0");
			SendPlayer(sendMes, TP, g_playerdata);
			memset(sendMes, 0, MAXTEXTLEN);
			snprintf(sendMes, MAXTEXTLEN-1, "lose:0");
			SendExceptTarget(sendMes, TP, g_playerdata);
			return True;
		}
	}
	return False;
}

void DrawGameHandicap(int TP, char recvMes[]){
	char *ret;
	int targetPlayer;
	char sendMes[MAXTEXTLEN];

	if ((ret = strchr(recvMes, ':')) != NULL){
		targetPlayer = atoi(ret+1);
		if (targetPlayer != -1 && targetPlayer == TP){
			draw(TP);
			g_changeHandNum++;
		}else{
			memset(sendMes, 0, MAXTEXTLEN);
			snprintf(sendMes, MAXTEXTLEN-1, "n0");
			SendPlayer(sendMes, TP, g_playerdata);
		}
	}
}

Bool SkipTurn(int TP, char recvMes[]){
	int result = atoi(&recvMes[strlen(g_target[2])+1]);
	char sendMes[MAXTEXTLEN];

	if (result == 1){
		printf("SkipTurn(): Player%d'sTurnStart(result:%d)\n", TP, result);
		memset(sendMes, 0, MAXTEXTLEN);
		snprintf(sendMes, MAXTEXTLEN-1, "Turn:1");
	}
	else{
		printf("SkipTurn(): SkipPlayer%d'sTurn(result:%d)\n", TP, result);
		memset(sendMes, 0, MAXTEXTLEN);
		snprintf(sendMes, MAXTEXTLEN-1, "Turn:0");
	}
	SendExceptTarget(sendMes, TP, g_playerdata);
	return result;
}

void GameMaster(int clntSock[]){
	int TP;
	CardData Card;
	char sendMes[MAXTEXTLEN], recvMes[MAXTEXTLEN];

	for (int i = 0; i < PLAYERNUM; ++i)
	{
		g_playerdata[i].GM_clntnum = clntSock[i];
	}

	// スタート時の初期化
	Game_init();

	printf("GameMaster():Game_init() Complete\n");

	// ターンの開始
	while (1){
		g_changeHandNum = 0;
		// ターン数の通知
		TP = g_TurnNum % PLAYERNUM;
		memset(sendMes, 0, MAXTEXTLEN);
		snprintf(sendMes, MAXTEXTLEN-1, "turn:%d", g_TurnNum);
		SendAll(sendMes, g_playerdata);
		// 出せるカードがあるかを受信。なければ一枚カードを引かせる。(g_target[1] = "Draw")
		RecvMes(recvMes, TP, g_playerdata, g_target[1]);
		DrawGameHandicap(TP, recvMes);
		// カードを一枚引いた上で出せるカードがなければターンをスキップさせる。(g_target[2] = "Turn")
		RecvMes(recvMes, TP, g_playerdata, g_target[2]);
		if (SkipTurn(TP,recvMes)){
			RecvMes(recvMes, TP, g_playerdata, g_target[0]);
			Card = GetCardData(recvMes, g_target[0]);
			ReleaseCard(TP, Card);
			addGraveZone(Card);
		}
		memset(sendMes, 0, MAXTEXTLEN);
		snprintf(sendMes, MAXTEXTLEN-1, "othershandChage:%d", g_changeHandNum);
		SendExceptTarget(sendMes, TP, g_playerdata);
		if (JudgeEnd())	break;
		g_TurnNum++;
	}
	for (int i = 0; i < PLAYERNUM; ++i)
	{
		close(g_playerdata[i].GM_clntnum);
	}
	printf("GameMaster():endGame\n");
}