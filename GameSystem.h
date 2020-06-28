#define PLAYERNUM	3
#define DECKNUM		4 * (1 * 1 + 2 * 9)

#ifndef GAMESYSTEM_H
#define GAMESYSTEM_H
typedef enum Bool{
	False,
	True
}Bool;

typedef struct CardData
{
	int number;
	char color;
}CardData;

typedef struct PlayerData
{
	int TurnNumber;			// 自身のターン
	CardData hand[DECKNUM];	// 自身の手札
	int handnum;			// 手札の数
	Bool Uno_flag;			// ウノを言ったかどうか(未実装)
	int GM_clntnum;			// 自身のIPアドレス
}PlayerData;
#endif

void GameMaster(int clntSock[]);