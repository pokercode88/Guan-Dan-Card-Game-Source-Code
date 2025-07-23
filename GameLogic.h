#ifndef __WSK_GAMELOGIC_H_
#define __WSK_GAMELOGIC_H_

#include <stdio.h>
#include "FactoryGameLogic.h"
#include "TableItem.h"
#include "PlayerNode.h"
#include "msg.h"
#include <vector>
#include "MatchCards.h"
#include "gd_robotai.h"

#define MAX_TABLE_NUM             125/*100*/       //每个房间最大桌子数目  //100-125 chw 1123

#define GAME_WAIE_DEAL            GAME_HAVE_READY + 1     
#define GAME_WAIT_TRIBUTE         GAME_HAVE_READY + 2         //等待进贡
#define GAME_WAIT_RETURNTRIBUTE   GAME_HAVE_READY + 3          //等待还贡
#define GAME_WAIT_SEND            GAME_HAVE_READY + 4         //等待出牌
#define GAME_WAIT_END             GAME_HAVE_READY + 5         //等待游戏结束 

#define GAME_WAIT_RESERVE_ONE     GAME_HAVE_READY + 6          //预留1
#define GAME_WAIT_RESERVE_TWO     GAME_HAVE_READY + 7          //预留2)

class Gd_AiLogic;
typedef struct GameLevel
{
    int iLevel;//等级
    int iEncourage;//奖励
}GameLevelDef;

class FLGameLogic : public GameLogic
{
public:
    FLGameLogic(void);
    ~FLGameLogic(void);

    virtual IPlayerNode* OnCreatePlayer();

    //在CopyPlayerNode中首先要调用CopyFactoryPlayerNod
    virtual void CopyPlayerNode(FactoryPlayerNodeDef *lpDestNode,FactoryPlayerNodeDef *lpSrcNode);
    //桌指针 赋值
    virtual void SetTableNodeInfo(FactoryTableItemDef *pTableItem,int iTableNumExtra);

    virtual void CallBackSitDownSuccess(FactoryPlayerNodeDef *pPlayerNode);
    
    //掉线重入游戏数据转发
    virtual int SetLoginAnginBuffer(FactoryPlayerNodeDef *pPlayerNode,FactoryTableItemDef *pTableItem,char *pBuffer);
    //玩家根节点拷贝

    //游戏开始: 当最后一个人加入并准备后进行;
    virtual void CallGameLogicStart(FactoryTableItemDef *pTableItem,int iRoomID);

    //恢复玩家自己主逻辑
    virtual void CallBackAgainLogin(FactoryPlayerNodeDef *pPlayerNode);

    //玩家离开桌子游戏中回调
    virtual void CallBackTablePlayerLeave(FactoryPlayerNodeDef *pPlayerNode,FactoryTableItemDef *pTableItem,int iTableNumExtra);

    //取得玩家桌信息
    virtual FactoryTableItemDef *GetTableItem(int iRoomNum,int iTableNum);

    //游戏中玩家掉线 自动出牌 打牌处理函数
    virtual void AutoHandleGame(FactoryPlayerNodeDef  *pPlayerNode);

    //游戏中逻辑消息接受入口
    virtual void HandleOtherNetMessage(IPlayerNode *lpPlayerNode, unsigned short type, void *buffer,long length);

    //验证服务器的其他消息
    virtual void HandleOtherRadiusNetMessage(IPlayerNode *lpPlayerNode, unsigned short type, void *buffer,long length);
    void HandleGetGuanDanPointRankReq(PlayerNodeDef *pPlayerNode, void *pMsgData,long length);
    void HandleGetGuanDanPointRankRes(void *buffer, long length);

    //强制出牌
    virtual void ForceHandleGame(FactoryPlayerNodeDef  *pPlayerNode);
    
    void SendXmlMsg(PlayerNodeDef *pPlayerNode, unsigned short type, const IMessage &msg);
    //游戏中定时器
    virtual void OnGameTimer(long tmTimer);

    virtual void TimeCheckTimeOut(long tmTimer);
   
    virtual ICompeteLogic* GetICompeteLogic();

    virtual void IsTableValidateResult(FactoryTableItemDef *pTableItem);
    
    void SwapTableForRealPlayer();
    int GetTableIdleNum(FactoryTableItemDef *pTableItem);
    int GetTableIdlePos(FactoryTableItemDef *pTableItem);

    int GetRandIndex();
    
private:
    void HandleSendCardsReq(PlayerNodeDef *pPlayerNode, void *pMsgData,long length);    //玩家出牌请求
    void HandleCheatSendCard(PlayerNodeDef *nodePlayers,unsigned char cKickOutType);                               //非法操作
    void HandleMatchCard(PlayerNodeDef *pPlayerNode,void *pMsgData,long length);//配牌消息处理

    void HandleTributeCard(PlayerNodeDef *pPlayerNode,void *pMsgData,long length);//玩家进贡

    void HandleTributeReturnCard(PlayerNodeDef *pPlayerNode,void *pMsgData,long length);//还贡

    void HandleTrust(PlayerNodeDef *pPlayerNode,void *pMsgData,long length); 

    void CleanPlayerNode(TableItemDef *tableItem);
    void CleanAllTableVal();
    void CleanOneTableVal(TableItemDef *tableItem,int iRoomID);
    //创建牌
    void CreateCard(TableItemDef *tableItem); 

    bool CheckInHandCard(const char cSendCard[], const char cHandCard[]); 
    
    bool AccumulatOverNum(PlayerNodeDef *pPlayerNode,TableItemDef &tableItem);
    
    //发送发牌消息
    void SendDealCardMsg(TableItemDef &tableItem);
    //发送抗贡消息
    void SendDefiTributeMsg(TableItemDef &tableItem);
    //准备出牌
    void PrepareSendCard(TableItemDef &tableItem);
    //准备进贡
    void PrepareTribute(TableItemDef &tableItem);
    //准备还贡
    void PrepareReturnTribute(TableItemDef &tableItem);
    //进贡或还贡的群发通知
    void SendTributeNotice(TableItemDef &tableItem, const unsigned short &usTableNumExtar, const char &cTribute);
    //进贡或还贡返回，返回给被进贡和被还贡的玩家
    void SendTributeReturn(PlayerNodeDef *pPlayerNode, const unsigned short &usTableNumExtar, const char &cTribute, const int &messageType);
    //下一玩家出牌
    void CallNextSendCard(TableItemDef &tableItem);

    //游戏结束
    void OnGameEnd(PlayerNodeDef *pPlayerNode,TableItemDef &tableItem);
    //读取游戏中的配置参数
    void ReadGameConfig();
    //获取逻辑值，用于比较大小
    int GetLogicCard(char card, char trumps);

    void SendBroadcastMsg();   //chw 1127 读取移动端广播消息
    //客户端加锁
    void LockTimeOut(PlayerNodeDef *pPlayerNode,short sWaitTime);
    //客户端减锁
    void UnLockTimeOut(PlayerNodeDef *pPlayerNode);
    //交换玩家位置  
    void SwapTablePlayer(TableItemDef *tableItem, int pos1, int pos2);
    //处理分组的牌
    void ProcessPacketCard(TableItemDef *tableItem);
    //打过6、10、A后的奖品券
    int GetPrizeTicketNumAfterPass(const int &level);
    //连胜的奖品券
    void SetPrizeTicketAfterContinuousWinning(TableItemDef &tableItem);
    //打过牌级的奖品券
    void SetPrizeTicketAfterPass(TableItemDef &tableItem);
    //设置下局的牌级
    void SetNextGameLevel(TableItemDef &tableItem);
    //更新比赛数据到比赛服务
    virtual void UpdateMatchDataToMatchServer(FactoryTableItemDef *pTableItem);

    /*****定时起调用**************************************************/
    void OneDayTimer();
    void TenMinuteTimer();   //十分钟定时器 
    void SortCardL(vector<char>& vecCard,char level);

    /********************************************************私人场**************************************************/
    //创建桌子
    virtual void HandlePhoneCreateNewTableReq(IPlayerNode *lpPlayerNode,void *pMsg, unsigned short Len);
    //加入某桌
    virtual void HandlePhoneJoinTableReq(IPlayerNode *lpPlayerNode,void *pMsg,unsigned short Len);
    //获取桌子列表
    void HandleGetPrivateTableListReq(IPlayerNode *lpPlayerNode,void *pMsg, unsigned short Len);
    //查找桌子
    void HandleGetPrivateTableByKeyReq(IPlayerNode *lpPlayerNode,void *pMsg, unsigned short Len);

public:
    int GetBaseTime(TableItemDef &tableItem);
    virtual int GetUsePropLimit(FactoryPlayerNodeDef* pPlayerNode);
    virtual bool CallBackClientReadyReq(FactoryTableItemDef &table,FactoryPlayerNodeDef &player);

private:
    TableItemDef m_tbItem[MAX_ROOM_NUM][MAX_TABLE_NUM];

    bool m_bDoOneDay;

    GameLevel m_GameLevel[MAX_LEVEL_NUM + 1];
    
    MatchCards *m_maCard;

    int m_iIfOpenCompete;
    
    int m_iIfOpenExtendGift;       //是否赠送礼品
    int m_iExtendGiftNum;          //赠送数目
    int m_iExtendGapTime;          //赠送时间间隔     
    int m_makeCard;

    int m_iPercent;
    
    int m_iMode;                //模式,1经典2团团转3私人场
    int m_iTableID;            //私人场桌子ID

    // 2016-06-15
    int m_maxGameCount;             // 同桌最大连续对局数
    int m_cannotPlaySec;            // 超过上面次数后，多少秒内不能在同桌游戏
};



#endif /* __WSK_GAMELOGIC_H_ */
