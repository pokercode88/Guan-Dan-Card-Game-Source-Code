#ifndef __TABLEITEM_H_
#define __TABLEITEM_H_

#include "FactoryTableItem.h"
#include "PlayerNode.h"
#include<vector>

class TableItemDef : public FactoryTableItemDef
{
public:
    TableItemDef() {
        m_cBombNumber = 0;
        memset(m_cPlayAceCount, 0, sizeof(m_cPlayAceCount));
        
        m_continueGameCount = 0;
    }

    ~TableItemDef() {
    }

    virtual TableItemDef* operator->() {
        return this;
    }

    PlayerNodeDef               *pPlayers[TABLE_PLAYER_NUM];        //这里和FactoryTableItemDef中的pFactoryPlayers指针值一样.否则在游戏中去改指针强制转换太烦了

    //出牌相关
    char                        cCards[MAX_HAND_CARDS_NUM];         //当前剩余的牌 
    int                         iCurSendPlayer;                     //当前出牌玩家
    int                         iCurTablePlayer;                    //当前桌上出牌的玩家
    int                         iCurCardType;                       //当前局的牌型 
    int                         iCurCardValue;                      //当前局的最大牌值
    int                         m_iPassCount;                       //pass的玩家

    //进贡相关
    bool                        bTribute;                           //是否为单贡
    char                        cGivePlayer1;                       //需要进贡的玩家1
    char                        cGivePlayer2;                       //需要进贡的玩家2
    char                        cSendPlayer1;                       //需要还贡的玩家1
    char                        cSendPlayer2;                       //需要还贡的玩家2
    char                        m_iGiveNum;                         //需要进贡牌个数
    char                        m_iReturnNum;                       //需要还贡牌个数
    char                        m_cTribute[TABLE_PLAYER_NUM];       //进贡牌 
    char                        m_cReturnTribute[TABLE_PLAYER_NUM]; //进贡牌 

    //结算相关
    int                         iOverCount;                         //本桌结束的人数
    int                         m_iAddLevel;                        //当局结束, 升几级
    int                         m_iAddExp;                          //当局结束, 经验值
    char                        m_cPrizeTicketNum[TABLE_PLAYER_NUM];//奖品券数目 
    char                        m_cBombNumber;                      //炸弹倍数

    //牌级相关
    char                        m_cPlayAceCount[2];                 //c记录打A的局数，超过3次则重置为2, 0为0、2玩家的次数，1为1、3玩家的次数
    char                        m_cLastLevel[2];                    //上局打几
    char                        m_cLevel;                           //当局打几: 按点数算; 2, 3..12(Q), 13(K), 1(A)
    char                        m_cBanker;                          //当前是哪两家坐庄

    //模式相关
    char                        m_cMode;                            //模式1经典模式2团团转

    //托管相关
    bool                        bOtherTrust[TABLE_PLAYER_NUM];

    // 4人连续对局的次数
    int                         m_continueGameCount;

    void    ResetTableSendCard() {
        //出牌相关
        memset(cCards, 0, sizeof(cCards));
        iCurSendPlayer      = -1;
        iCurTablePlayer     = -1;
        iCurCardType        = 0;
        iCurCardValue       = 0;
        m_iPassCount        = 0;
    } 

    //计算相关
    void ResetTableCalculate() {
        iOverCount          = 0;
        m_iAddLevel         = 0;
        m_iAddExp           = 0;
        memset(m_cPrizeTicketNum, 0, TABLE_PLAYER_NUM);
        m_cBombNumber       = 0;
    }

    //进贡相关
    void ResetTableTribute() {
        cGivePlayer1        = -1;
        cGivePlayer2        = -1;
        cSendPlayer1        = -1;
        cSendPlayer2        = -1;
        bTribute            = false;
        m_iGiveNum          = 0;
        m_iReturnNum        = 0;
        memset(m_cTribute, 0, TABLE_PLAYER_NUM);
        memset(m_cReturnTribute, 0, TABLE_PLAYER_NUM);
    }

    //牌级相关
    void ResetTableLevel() {
        memset(m_cPlayAceCount, 0, 2); 
        memset(m_cLastLevel, 2, 2); 
        m_cLevel            = 2;
        m_cBanker           = -1;
    }

    virtual void ResetGameTable() {
        for(int i = 0; i < TABLE_PLAYER_NUM; ++i) {
            pPlayers[i] = NULL;
        }
        ResetTableSendCard(); 
        ResetTableCalculate();
        ResetTableTribute(); 
        ResetTableLevel();

        m_cMode             = 0;
        memset(bOtherTrust, 0, TABLE_PLAYER_NUM);
    }
};

#endif /* __TABLEITEM_H_ */
