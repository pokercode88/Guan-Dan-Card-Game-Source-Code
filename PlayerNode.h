#ifndef __PLAYERNODE_H_
#define __PLAYERNODE_H_

#include "FactoryPlayerNode.h"
#include "CheckCard.h"
#define TABLE_PLAYER_NUM          4       //每桌最大的玩家数目
#define GAME_ALL_CARD_NUM         108      //总共的牌的张数

class PlayerNodeDef : public FactoryPlayerNodeDef
{
public:
    PlayerNodeDef() {
        m_ucFirstPlayer = 0;
        m_iPrizeCountPerDay = 0;
    }
    ~PlayerNodeDef()
    {}

    virtual PlayerNodeDef* operator->() {return this;}
    
    //玩家手中牌状况
    char cCards[MAX_HAND_CARDS_NUM];            //扑克牌状况
    char cCurSendCard[MAX_HAND_CARDS_NUM];      //当前局的牌
    char cPass;
    
    int iHandCardNum;           //玩家手中牌的张数
    int iOverNum;               //玩家打完牌的顺序; 头游, 二游, 三游, 末游

    int ExpLevel;
    unsigned char    m_ucFirstPlayer;        //连续头游戏次数
    int m_iPrizeCountPerDay;                //每天获得奖品券计数

    int iTimeOutTimes;                      //断线次数
    int iTimeStart;                         //我的倒计时
    bool bRealAuto;                         //为1的时候，执行自动出牌的逻辑，为0的时候，在自动出牌函数里面直接返回。

    virtual void ResetGameNode() //创建桌子新节点时清一次
    {
        ClearNode();
        iOverNum   =  0;
    }
    
    void ClearNode()   //每桌开始都要调用的
    {
      memset(cCards,0x00,sizeof(cCards));
      memset(cCurSendCard,0x00,sizeof(cCurSendCard));
      
      iHandCardNum  = 0;
      cPass            = 0;
      ExpLevel        = 0;
      
      userNodeInfo.lCheckTimeOut = 0;
      iTimeOutTimes = 0;    
      iTimeStart    = 0;
      bRealAuto     = false;
    }
    
    
};
#endif /* _PLAYERNODE_H_ */
