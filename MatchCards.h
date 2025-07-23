#ifndef __MATCHCARDS_H__
#define __MATCHCARDS_H__

//调用 OnGameMatchCardsMsg 处理该消息类型
const unsigned short GAME_MATCH_CARDS_MSG = 0x9527;

//配牌结构体
typedef struct tagGameMatchCardsMsg
{
    unsigned short usCount;
    char szCards[4096];
}GameMatchCardsMsg, *PGameMatchCardsMsg;

class MatchCards  
{
public:
    MatchCards();

    //从配牌缓冲中获取牌型, 返回值大于0表示获取成功
    long GetMatchCars(char *lpszCards, unsigned short usCount);

    //接收GAME_MATCH_CARDS_MSG消息
    void OnGameMatchCardsMsg(void *buffer, long length);

private:
    char m_szCards[4096];
public:	 //chw 1112
    long m_lCardsCount;
};

/*
使用:
1. 在游戏网络逻辑的OnNetMessage函数中, 判断GAME_MATCH_CARDS_MSG消息类型, 调用 OnGameMatchCardsMsg 处理;
2. 游戏开始函数 OnGameStart 中, 调用 GetMatchCars 获取配牌的牌型
*/

#endif //__MATCHCARDS_H__
