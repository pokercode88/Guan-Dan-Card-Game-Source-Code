
//系统头文件
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <time.h>
#include <unistd.h>      //sleep()
#include <stdlib.h>      //rand()
#include <algorithm>
#include <math.h>
#include <assert.h>

#include "ServerData.h"
#include "GameLogic.h"
#include "CheckCard.h"
#include "MatchCards.h"
#include "Card.h"
#include "Server2ServerMsg.h"

#include "ActivitySystem.h"
#include "GDLimitPrize.h"
#include "msg_MatchProto.h"
#include "msg_GuandanProto.h"
#include "Cards.h"

#define _NEW_RATE_

using namespace std;

static CIniFile g_gameConfig;   //单个游戏配置

//排序，并格式化数据,用于打印牌
static string FormatAndSortCards(char *cards, int count) {
    string str;
    vector<int> vec; 
    for (int i = 0; i < count; ++i) {
        if (cards[i] != 0) {
            vec.push_back(cards[i]);
        }
    }
    sort(vec.begin(), vec.end());
    for (vector<int>::iterator iter = vec.begin(); iter != vec.end(); ++iter) {
        char tmp[16] = {0};
        sprintf(tmp, "%X ", *iter);
        str += tmp;
    }
    return str;
}

//从小到大排序
void FLGameLogic::SortCardL(vector<char>& vecCard ,char level) {

    for(int a = 0; a < vecCard.size() - 1; ++a) {
        for(int b = a + 1; b < vecCard.size(); ++b) {
            if (GetLogicCard((vecCard[a] & 0x0F), level) > GetLogicCard((vecCard[b] & 0x0F), level)) {
                char temp = vecCard[a];         
                vecCard[a] = vecCard[b];
                vecCard[b] = temp;
            }    
        }
    }
}

FLGameLogic::FLGameLogic() {
    m_bDoOneDay = false;
    m_pAllServerBaseInfo.iMaxTableNum = MAX_TABLE_NUM;
    m_pAllServerBaseInfo.iPlayerNum   = TABLE_PLAYER_NUM;

    CleanAllTableVal();
    srandom(time(NULL));
    m_maCard = NULL;
    //游戏中配置
    ReadGameConfig();
    m_pSiteActivity->AddActivityCallback(new CGdPrizeLimit(this));
    m_iTableID = 0;

    // 不自动准备
    m_pAllServerBaseInfo.bAtuoReady = false;
}

FLGameLogic::~FLGameLogic() {
}

IPlayerNode* FLGameLogic::OnCreatePlayer() {
      return new PlayerNodeDef;
}

//读取游戏配置
void FLGameLogic::ReadGameConfig() {
    g_gameConfig.InitFile("gameinfo.cfg");
    m_iPercent          =  g_gameConfig.GetValueInt("extend","iPercent",0);                    //抽税
    m_iIfOpenExtendGift =  g_gameConfig.GetValueInt("extend","ifopenextend",0);             //是否赠送礼品
    m_iExtendGiftNum    =  g_gameConfig.GetValueInt("extend","extendnum",800);              //赠送数目     
    m_iExtendGapTime    =  g_gameConfig.GetValueInt("extend","gaptime",10);                 //赠送间隔时间 /s           
    m_iMode             =  g_gameConfig.GetValueInt("extend","Mode",0); 
    m_makeCard          =  g_gameConfig.GetValueInt("extend","MakeCard",0); 
    m_maxGameCount      =  g_gameConfig.GetValueInt("extend","MaxGameCount",3); 
    m_cannotPlaySec     =  g_gameConfig.GetValueInt("extend","NotPlayTogetherSec",1800); 
    __log(_ALL, "Function[%s]", "iPercent[%d]ifopenextend[%d]extendnum[%d]gaptime[%d]Mode[%d]makeCard[%d] m_maxGameCount[%d] m_cannotPlaySec[%d]", 
          __FUNCTION__, m_iPercent, m_iIfOpenExtendGift, m_iExtendGiftNum,m_iExtendGapTime, m_iMode, m_makeCard,
          m_maxGameCount, m_cannotPlaySec);

    char cData[512];
    char szSection[10];

    for (int i = 0;i < MAX_LEVEL_NUM + 1 ; ++i) {
        memset(cData,0,sizeof(cData));
        memset(szSection,0,sizeof(szSection));
        sprintf( szSection, "level_%d", i  );
        g_gameConfig.GetValueStr("gamelevel", szSection, cData, 512);
        sscanf(cData, "%d,%d", &m_GameLevel[i].iLevel, &m_GameLevel[i].iEncourage);
    }
}

//发送广播消息
void FLGameLogic::SendBroadcastMsg() {
    char szMsg[512] = {0};
    CIniFile g_cfgBroadcastConfig;
    g_cfgBroadcastConfig.InitFile("broadcast.cfg");
    g_cfgBroadcastConfig.GetValueStr("OnTimeBroadcast", "info", szMsg, 511);
    __log(_DEBUG, "FLGameLogic::ReadBroadcastConfig:", "发送广播消息 len[%d]：%s", strlen(szMsg), szMsg);

    tagDSBroadcastGameSvrMSG msgInfo;
    memset(&msgInfo, 0, sizeof(tagDSBroadcastGameSvrMSG));
    msgInfo.nMessageType = 1;
    msgInfo.iscore = 0;
    msgInfo.nGameID = m_pServerBaseInfo[0].iGameID;
    msgInfo.nServerID = m_pServerBaseInfo[0].iServerID;
    msgInfo.nMsgID = 0;
    msgInfo.nUserID = 0;
    memcpy(msgInfo.szMsgContext, szMsg, sizeof(msgInfo.szMsgContext));
    msgInfo.utime = time(NULL);
    OnSendMsgToInGameClient(m_pServerBaseInfo[0].iRoomID, &msgInfo, LOBBYCLIENT_GAMESERVER_BROADCAST_MSG, sizeof(msgInfo));
}

//清理桌子的信息
void FLGameLogic::CleanAllTableVal() {
    for(int i = 0;i<MAX_ROOM_NUM;i++) {
        for(int j = 0;j < MAX_TABLE_NUM;j++) {
            //清理桌变量
            m_tbItem[i][j].InitTableInfo();
        }
    }
}

ICompeteLogic*  FLGameLogic::GetICompeteLogic() {
    return NULL;
}

//洗牌
void FLGameLogic::CreateCard(TableItemDef *tableItem) {

    char                      cAllCards[GAME_ALL_CARD_NUM];
    for(int i = 0; i < 52; ++i) {
        cAllCards[i] = Card::MakeCardChar((Card::CardType)(( i / 13) % 4), (char)(i % 13 + 1));
        cAllCards[i + 52] =  cAllCards[i];
    }
    cAllCards[104] = 14; //小王
    cAllCards[105] = 14; //小王
    cAllCards[106] = 15; //大王
    cAllCards[107] = 15; //大王

    //洗牌 完全打乱玩家手中的牌
    for(int i = 0; i < GAME_ALL_CARD_NUM * 10; ++i) {
        int iRandNum = random() % GAME_ALL_CARD_NUM;
        int iIndex = i % GAME_ALL_CARD_NUM;
        char temp = cAllCards[iIndex];
        cAllCards[iIndex] = cAllCards[iRandNum];
        cAllCards[iRandNum] = temp;
    }

    if( m_makeCard == 1 )
    {
            if (m_maCard != NULL && m_maCard->m_lCardsCount == (MAX_HAND_CARDS_NUM - 1) * TABLE_PLAYER_NUM)
            {
                    m_maCard->GetMatchCars(cAllCards,108);
            } 
    }
	else
	{
		Gd_AiLogic aiLogic;
		aiLogic.Clear();
		aiLogic.m_makeCardInfo.levelCard = tableItem->m_cLevel;
		for (int i = 0; i < GAME_PLAYER_NUM; i++)
		{
			aiLogic.m_makeCardInfo.bRobotLogin[i] = tableItem->pFactoryPlayers[i]->bRobotLogin;
		}
		if (aiLogic.m_makeCardInfo.bRobotLogin[0] == true && aiLogic.m_makeCardInfo.bRobotLogin[2] == true
			|| aiLogic.m_makeCardInfo.bRobotLogin[1] == true && aiLogic.m_makeCardInfo.bRobotLogin[3] == true)
		{
			aiLogic.MakeCard();
			memset(cAllCards, 0, sizeof(cAllCards));
			for (int i = 0; i < GAME_PLAYER_NUM; i++)
			{
				memcpy(&cAllCards[i*HAND_CARD_NUM], aiLogic.m_makeCardInfo.cards[i], HAND_CARD_NUM);
			}
		}
	}

    //把牌发给玩家
    for (int i = 0; i < TABLE_PLAYER_NUM; ++i) {
        if (tableItem->pPlayers[i]) {
            memcpy(tableItem->pPlayers[i]->cCards, &(cAllCards[i * (MAX_HAND_CARDS_NUM - 1)]), MAX_HAND_CARDS_NUM - 1);
            tableItem->pPlayers[i]->iHandCardNum = MAX_HAND_CARDS_NUM -1;
            tableItem->pPlayers[i]->cCards[MAX_HAND_CARDS_NUM - 1] = 0;
        }
    }
}

//清理桌子的信息
void FLGameLogic::CleanOneTableVal(TableItemDef *tableItem,int iRoomID) {
    //本桌出牌信息清理
    tableItem->ResetTableSendCard(); 
    
    //清理进贡相关 
    tableItem->ResetTableTribute(); 
 
    //结算相关
    tableItem->ResetTableCalculate();

    //底注 
    tableItem->iBaseTimes = m_pServerBaseInfo[0].iBaseTime;
}

//交换桌子的节点
void FLGameLogic::SetTableNodeInfo(FactoryTableItemDef *pTableItem,int iTableNumExtra) {
    if(pTableItem) {
        TableItemDef *tableItem = (TableItemDef*)pTableItem;
        for(int i = 0;i<TABLE_PLAYER_NUM;i++) {
            if(i == iTableNumExtra) {
                if (tableItem->pPlayers[i]) {
                    tableItem->pPlayers[i] = (PlayerNodeDef*)(tableItem->pFactoryPlayers[i]);
                    __log(_DEBUG,"Function[%s]","掉线状态--[%d]--桌号[%d]",
                          __FUNCTION__, tableItem->pPlayers[i]->userNodeInfo.cDisconnectType,tableItem->pPlayers[i]->userNodeInfo.iTableNum);
               }
            }
        }
    }
}

//拷贝PlayerNode
void FLGameLogic::CopyPlayerNode(FactoryPlayerNodeDef *lpDestNode,FactoryPlayerNodeDef *lpSrcNode) {
    GameLogic::CopyFactoryPlayerNode(lpDestNode,lpSrcNode);  //这里现拷贝
    PlayerNodeDef *pNewNode = (PlayerNodeDef *)lpDestNode;
    PlayerNodeDef *pOldNode = (PlayerNodeDef *)lpSrcNode;

    memcpy(pNewNode->cCards,pOldNode->cCards,sizeof(pNewNode->cCards));
    memcpy(pNewNode->cCurSendCard,pOldNode->cCurSendCard,sizeof(pNewNode->cCurSendCard));
    pNewNode->iHandCardNum = pOldNode->iHandCardNum;
    pNewNode->iOverNum   = pOldNode->iOverNum;
    pNewNode->cPass = pOldNode->cPass;
    pNewNode->ExpLevel = pOldNode->ExpLevel;
    pNewNode->iTimeStart = pOldNode->iTimeStart;
    pNewNode->iTimeOutTimes = pOldNode->iTimeOutTimes;
}

//断线重连设置对应的值
int FLGameLogic::SetLoginAnginBuffer(FactoryPlayerNodeDef *pPlayerNode,FactoryTableItemDef *pTableItem,char *pBuffer) {
    int i,j;
    TableItemDef *tableItem = (TableItemDef *)pTableItem;
    PlayerNodeDef *nodeTemp = (PlayerNodeDef *)pPlayerNode;
    if(nodeTemp == NULL) {
         __log(_ERROR,"Function[%s]","nodeTemp is NULL ", __FUNCTION__);
         return 0;
    }

    int iTableNumExtra = nodeTemp->userNodeInfo.usTableNumExtra;
    GameExpLevelNoticeDef GelNotice;
    if (nodeTemp->gamePropInfo.usPropPlayerB5 == 0) {
        nodeTemp->gamePropInfo.usPropPlayerB5 = 1;
    }
    GelNotice.ExpLevel = nodeTemp->gamePropInfo.usPropPlayerB5;//已领取奖励的最高等级

    int baseTime = 0; 
    if (m_iMode == 3) { 
        baseTime = tableItem->GetTableAnte();
    }    
    else {
        baseTime = m_pServerBaseInfo[0].iBaseTime;
    }    
    GelNotice.iAnte = baseTime;

    SendMsgToClient(nodeTemp,GAME_EXP_NOTICE,&GelNotice,sizeof(GameExpLevelNoticeDef));

    PlayerGameInfoDef againGameInfo = {0};
    memcpy(againGameInfo.szGameNum,tableItem->szGameNum,sizeof(againGameInfo.szGameNum));    
    memcpy(againGameInfo.cTableCard,tableItem->cCards,sizeof(againGameInfo.cTableCard));

    againGameInfo.cNowPlayerStatus = pPlayerNode->userNodeInfo.iGameStatus;
    againGameInfo.cGameLevel = tableItem->m_iAddLevel;
    int maxTimeLeft = 0; //进贡还贡的时候用
    for (int k = 0; k < 4 ; ++k) {
        if (maxTimeLeft <tableItem->pPlayers[k]->userNodeInfo.lCheckTimeOut) {
            maxTimeLeft = tableItem->pPlayers[k]->userNodeInfo.lCheckTimeOut;
        }
    }
   
    for(i = 0; i < TABLE_PLAYER_NUM;i++) {
        if ( tableItem->pPlayers[i] == NULL) {
            __log(_DEBUG,"Function[%s]","玩家节点为空 —— 玩家所在位置[%d]", __FUNCTION__, i);
            return 0;
        }
        if(tableItem->pPlayers[i]) {

            againGameInfo.cTribute[i] = tableItem->m_cTribute[i];
            memcpy(againGameInfo.cCurSendCard[i],tableItem->pPlayers[i]->cCurSendCard,MAX_HAND_CARDS_NUM);    

            if(i == iTableNumExtra) {
                memcpy(againGameInfo.cCards[i],tableItem->pPlayers[i]->cCards,MAX_HAND_CARDS_NUM);
            }
            else  {
                memcpy(againGameInfo.cCards[i],tableItem->pPlayers[i]->cCards,MAX_HAND_CARDS_NUM);   
            }
            againGameInfo.cOrderNum[i]  =  tableItem->pPlayers[i]->iOverNum;
            againGameInfo.cPass[i] = tableItem->pPlayers[i]->cPass;
        }

        if (tableItem->pPlayers[i]->userNodeInfo.cDisconnectType != 0) {
            againGameInfo.iDisconnect[i] = 1;
        }
        else {
            againGameInfo.iDisconnect[i] = 0;
        }
    }
    //对于cMaxLevel复用为当前玩家的超时时间,客户端每局第一次出牌时间为40s，其他所有倒计时为30s
    if ((pPlayerNode->userNodeInfo.iGameStatus == GAME_WAIT_RETURNTRIBUTE) || (pPlayerNode->userNodeInfo.iGameStatus == GAME_WAIT_TRIBUTE)) {
        againGameInfo.cMaxLevel = maxTimeLeft - 10; //进贡还贡阶段，时间为还没有进贡或还贡的那个人的时间
    } 
    else {
        againGameInfo.cMaxLevel = tableItem->pPlayers[tableItem->iCurSendPlayer]->userNodeInfo.lCheckTimeOut - 10;
    }
    if (againGameInfo.cMaxLevel < 0) { //小于0
        againGameInfo.cMaxLevel = 0;
    }
    __log(_ALL, "Function[%s]", "userid[%d]deviceType[%d]倒计时[%d]lCheckTimeOut[%d]timeStart[%d]GameStatus[%d]", __FUNCTION__,pPlayerNode->userNodeInfo.iUserID,  pPlayerNode->GetDeviceType(), againGameInfo.cMaxLevel, tableItem->pPlayers[tableItem->iCurSendPlayer]->userNodeInfo.lCheckTimeOut, tableItem->pPlayers[tableItem->iCurSendPlayer]->iTimeStart, pPlayerNode->userNodeInfo.iGameStatus);
    if ((pPlayerNode->userNodeInfo.iGameStatus == GAME_WAIT_RETURNTRIBUTE) || (pPlayerNode->userNodeInfo.iGameStatus == GAME_WAIT_TRIBUTE)) {
        if (pPlayerNode->userNodeInfo.lCheckTimeOut > 0) {
            againGameInfo.iCurSendPlayer      =  pPlayerNode->userNodeInfo.usTableNumExtra;
        }
        else {
            againGameInfo.iCurSendPlayer      = -1 ;
        } 
    }
    else {
        againGameInfo.iCurSendPlayer      = tableItem->iCurSendPlayer;
    }

    againGameInfo.iCurTablePlayer     = tableItem->iCurTablePlayer;
    againGameInfo.iCurCardType      = tableItem->iCurCardType;
    againGameInfo.iCurCardValue     = tableItem->iCurCardValue;
    againGameInfo.cLevel            = tableItem->m_cLevel;
    againGameInfo.cNowLevel[0]        = tableItem->m_cLastLevel[0];
    againGameInfo.cNowLevel[1]        = tableItem->m_cLastLevel[1];

    for (int a = 0; a < 4; ++a) {
        againGameInfo.cOtherTrust[a] = tableItem->bOtherTrust[a];
    }
    if (tableItem->nCustomTableID != INVALID_TABLE_ID) {
        againGameInfo.nTableID = tableItem->nCustomTableID;
        if (pTableItem->szTableNickName) {
            memcpy(againGameInfo.cTableName, pTableItem->szTableNickName, sizeof(againGameInfo.cTableName));
        }
    }

    memcpy(pBuffer,&againGameInfo,sizeof(PlayerGameInfoDef));
    int iGameInfoLen = sizeof(PlayerGameInfoDef);

    return iGameInfoLen;
}

//断线重连的回调函数
void FLGameLogic::CallBackAgainLogin(FactoryPlayerNodeDef *pPlayerNode) {
    int i,j;
    PlayerNodeDef * pNodePlayers = (PlayerNodeDef *)pPlayerNode;
    if(!pNodePlayers) {
        __log(_ERROR, "Function[%s]", "pNodePlayers is NULL", __FUNCTION__);
        return;
    }

    int iRoomID = pNodePlayers->userNodeInfo.iRoomNum;
    int iTableNum = pNodePlayers->userNodeInfo.iTableNum;

    pNodePlayers->userNodeInfo.cDisconnectType = 0;
    pNodePlayers->bKickOut   = false;

    return;
}

//交换桌子上的玩家
void FLGameLogic::SwapTablePlayer(TableItemDef *tableItem, int pos1, int pos2) {
    char tmpPos = tableItem->pPlayers[pos2]->userNodeInfo.usTableNumExtra; 
    PlayerNodeDef *tmpPlayer = tableItem->pPlayers[pos2];

    tableItem->pPlayers[pos2]->userNodeInfo.usTableNumExtra = tableItem->pPlayers[pos1]->userNodeInfo.usTableNumExtra;
    tableItem->pPlayers[pos1]->userNodeInfo.usTableNumExtra = tmpPos;

    tableItem->pPlayers[pos2] = tableItem->pPlayers[pos1];
    tableItem->pPlayers[pos1] = tmpPlayer;

    FactoryPlayerNodeDef* tmpFactoryPlayer = tableItem->pFactoryPlayers[pos2];
    tableItem->pFactoryPlayers[pos2] = tableItem->pFactoryPlayers[pos1];
    tableItem->pFactoryPlayers[pos1] = tmpFactoryPlayer;
}

//团团转交换玩家位置，把需要交换的玩家信息发给客户端
void FLGameLogic::ProcessPacketCard(TableItemDef *tableItem) {
    __log(_ALL, "Function[%s]", "团团转模式",__FUNCTION__);
    //发送分组牌的通知
    PacketCardDef packetCard = {0};
    for(int i = 0; i < TABLE_PLAYER_NUM; ++i) {
        if(tableItem->pPlayers[i] != NULL) {
            for(int j = 0; j < MAX_HAND_CARDS_NUM; ++j) {
                if (tableItem->pPlayers[i]->cCards[j] == 0x33) { //黑桃3
                    packetCard.cPacketCards[tableItem->pPlayers[i]->userNodeInfo.usTableNumExtra] = 0x33;
                }
            }
        }
    }
    if (((packetCard.cPacketCards[0] != 0) && (packetCard.cPacketCards[1] != 0)) || ((packetCard.cPacketCards[2] != 0) && (packetCard.cPacketCards[3] != 0)))  {
        //1、2交换
        packetCard.cExchangePos[0] = 1;
        packetCard.cExchangePos[1] = 2;
        SwapTablePlayer(tableItem, 1, 2);
        __log(_ALL,"Function[%s]","Exchange chair pos1 and pos2,iUserID[%d]TableNumExtra_[%d]Name[%s],iUserID[%d]TableNumExtra_[%d]Name[%s],", 
              __FUNCTION__, tableItem->pPlayers[1]->userNodeInfo.iUserID,tableItem->pPlayers[1]->userNodeInfo.usTableNumExtra,tableItem->pPlayers[1]->szNickName,
             tableItem->pPlayers[2]->userNodeInfo.iUserID,tableItem->pPlayers[2]->userNodeInfo.usTableNumExtra,tableItem->pPlayers[2]->szNickName);
    }
    else if (((packetCard.cPacketCards[1] != 0) && (packetCard.cPacketCards[2] != 0)) || ((packetCard.cPacketCards[3] != 0) && (packetCard.cPacketCards[0] != 0)))  {
        //2、3交换
        packetCard.cExchangePos[0] = 2;
        packetCard.cExchangePos[1] = 3;
        SwapTablePlayer(tableItem, 2, 3);
 
        __log(_ALL,"Function[%s]","Exchange chair pos2 and pos3,iUserID[%d]TableNumExtra_[%d]Name[%s],iUserID[%d]TableNumExtra_[%d]Name[%s],", 
              __FUNCTION__, tableItem->pPlayers[2]->userNodeInfo.iUserID,tableItem->pPlayers[2]->userNodeInfo.usTableNumExtra,tableItem->pPlayers[2]->szNickName,
             tableItem->pPlayers[3]->userNodeInfo.iUserID,tableItem->pPlayers[3]->userNodeInfo.usTableNumExtra,tableItem->pPlayers[3]->szNickName);
    }
    
    for(int i = 0; i < TABLE_PLAYER_NUM; ++i) 
	{
        if(tableItem->pPlayers[i] != NULL) 
		{
            __log(_ALL,"Function[%s]"," iUserID[%d]TableNumExtra[%d]Name[%s]", 
                  __FUNCTION__, tableItem->pPlayers[i]->userNodeInfo.iUserID,tableItem->pPlayers[i]->userNodeInfo.usTableNumExtra,tableItem->pPlayers[i]->szNickName);
            SendMsgToClient(tableItem->pPlayers[i], PACKET_CARD_NOTICE_MSG, &packetCard,sizeof(PacketCardDef));
        }
    }

}

//游戏开始逻辑
void FLGameLogic::CallGameLogicStart(FactoryTableItemDef *pTableItem, int iRoomID)
{
    TableItemDef *tableItem = (TableItemDef*)pTableItem;
    if(NULL == tableItem)
    {
        __log(_ERROR,"FLGameLogic","CallGameLogicStart:tableItem = NULL!iRoomID = [%d]----",iRoomID);
        return;
    }
    
    __log(_DEBUG,"Function[%s]", "------------------------------------本局的游戏编号__[%s]---------------------------", __FUNCTION__, tableItem->szGameNum);
    for(int i = 0; i < TABLE_PLAYER_NUM; ++i) {
        if(tableItem->pFactoryPlayers[i]) {
            tableItem->pPlayers[i] = (PlayerNodeDef*)(tableItem->pFactoryPlayers[i]);

            // 2016-04-29 扣桌费为底注费
            int baseTime = GetBaseTime( *tableItem );
#ifdef _NEW_RATE_
            assert( tableItem->pPlayers[i]->userNodeInfo.iFirstMoney > baseTime );
            tableItem->pPlayers[i]->userNodeInfo.iFirstMoney -= baseTime;
#endif

            __log(_DEBUG,"Function[%s]"," TableNumExtra[%d]Name[%s]OverNum[%d]node[0x%08x] basetime[%d] firstMoney[%lld]", __FUNCTION__, 
                  tableItem->pPlayers[i]->userNodeInfo.usTableNumExtra,tableItem->pPlayers[i]->szNickName ,
                  tableItem->pPlayers[i]->iOverNum, tableItem->pPlayers[i], baseTime, tableItem->pPlayers[i]->userNodeInfo.iFirstMoney );
        }
    }
    
    //清楚桌节点
    CleanPlayerNode(tableItem);
    //清出本桌信息变量
    CleanOneTableVal(tableItem, iRoomID);
    //发牌
    CreateCard(tableItem);  

    tableItem->iCurSendPlayer       = random() % 4;             //随机一个人出牌
    tableItem->iCurTablePlayer      = tableItem->iCurSendPlayer;
    if (tableItem->m_cBanker == -1) {
        tableItem->m_cBanker = tableItem->iCurSendPlayer % 2;
    }
    //发送发牌消息
    SendDealCardMsg(*tableItem);

    //团团转模式，或私人场的团团转桌子
    if (m_iMode == 2 || (m_iMode == 3  && tableItem->m_cMode == 2)) {
        ProcessPacketCard(tableItem);
    }
    
    if (tableItem->pPlayers[0]->iOverNum >0 && tableItem->pPlayers[0]->iOverNum < 5) { //不是第一局，可能有进贡还贡
        //计算大王个数
        int bigGhostCount = 0;
        for (int p = 0;p < TABLE_PLAYER_NUM; ++p) {
            if (tableItem->pPlayers[p]->iOverNum == 4) { //末游两个大王，或者双下的两个人加起来有两个大王（双下的iOverNum都等于4)
                for (int q = 0 ; q < 27 ; ++q) {
                    if (tableItem->pPlayers[p]->cCards[q] == 15) {
                        ++bigGhostCount;
                    }
                }
            }
        }

        if (bigGhostCount == 2) { //抗贡，头游出牌
            for (int y = 0; y < TABLE_PLAYER_NUM; ++y) {
                if (tableItem->pPlayers[y]->iOverNum == 1) {
                    tableItem->iCurSendPlayer = tableItem->pPlayers[y]->userNodeInfo.usTableNumExtra;
                    tableItem->iCurTablePlayer = tableItem->pPlayers[y]->userNodeInfo.usTableNumExtra;
                }
            }
            SendDefiTributeMsg(*tableItem);
            PrepareSendCard(*tableItem);
        }
        else { //准备进贡
            PrepareTribute(*tableItem);
        }
    }
    else { //出牌消息
        PrepareSendCard(*tableItem);
    }

    tableItem->m_iAddLevel = 0;

	CardsNoticeRobotDef cards_to_robot = { 0 };
	for (int i = 0; i < TABLE_PLAYER_NUM; i++)
	{
		PlayerNodeDef* pPlayerNode = (PlayerNodeDef*)tableItem->pPlayers[i];
		if (pPlayerNode)
		{
			for (int j = 0; j < MAX_HAND_CARDS_NUM; j++)
			{
				if (pPlayerNode->cCards[j] > 0)
				{
					cards_to_robot.cHandCards[i][cards_to_robot.iHandCardsNum[i]++] = pPlayerNode->cCards[j]; 	
				}
			}
		}
	}

	for (int i = 0; i < TABLE_PLAYER_NUM; i++)
	{	
		PlayerNodeDef* pPlayerNode = (PlayerNodeDef*)tableItem->pPlayers[i];
		if (pPlayerNode && pPlayerNode->bRobotLogin)
		{
			SendMsgToClient(pPlayerNode, MSG_C_CARDS_NOTICE_ROBOT, &cards_to_robot, sizeof(cards_to_robot));
		}
	}

    return;
}

//发送出牌消息
void FLGameLogic::SendDealCardMsg(TableItemDef &tableItem) {
    DealCardsServerDef msgDealCard = {0};
    memcpy(msgDealCard.cGameNum, tableItem.szGameNum, sizeof(tableItem.szGameNum));
    msgDealCard.cFirstSendPlayer    = tableItem.iCurSendPlayer;
    msgDealCard.cLevelCard          = tableItem.m_cLevel;                        //牌级
    msgDealCard.cLevel[0]           = tableItem.m_cLastLevel[0];                  //牌级
    msgDealCard.cLevel[1]           = tableItem.m_cLastLevel[1];

    for (int i = 0; i < TABLE_PLAYER_NUM; ++i) {
        if(NULL != tableItem.pPlayers[i]) {
            if (tableItem.pPlayers[i]->userNodeInfo.cDisconnectType != 0) {
                msgDealCard.iDisconnect[i] = 1;
            }
            else {
                msgDealCard.iDisconnect[i] = 0;
            }
            for (int j = 0; j < TABLE_PLAYER_NUM; ++j) {
                memset(&(msgDealCard.cHandCards[j]), 0, MAX_HAND_CARDS_NUM);
            }
            memcpy(msgDealCard.cHandCards[i], tableItem.pPlayers[i]->cCards, MAX_HAND_CARDS_NUM - 1);
            msgDealCard.cFirstGetPlayer    = tableItem.pPlayers[i]->userNodeInfo.usTableNumExtra;
            string cards = FormatAndSortCards(tableItem.pPlayers[i]->cCards, MAX_HAND_CARDS_NUM);
            __log(_ALL,"Function[%s]","iUserID[%d]TableNumExtra[%d]Name[%s]cards[%s]", __FUNCTION__, 
                  tableItem.pPlayers[i]->userNodeInfo.iUserID, tableItem.pPlayers[i]->userNodeInfo.usTableNumExtra, tableItem.pPlayers[i]->szNickName, cards.c_str());
            SendMsgToClient(tableItem.pPlayers[i], DEAL_CARDS_SERVER_MSG, &msgDealCard, sizeof(DealCardsServerDef));

            tableItem.pPlayers[i]->userNodeInfo.iGameStatus   = GAME_WAIE_DEAL;
        }
    }
}

//发送抗贡消息
void FLGameLogic::SendDefiTributeMsg(TableItemDef &tableItem) {
    LockNoticeDef msgNotice = {0};
    msgNotice.cIs = 1;
    
    for (int j = 0; j < TABLE_PLAYER_NUM; ++j) {
        memcpy(msgNotice.cHandCards[j], tableItem->pPlayers[j]->cCards, MAX_HAND_CARDS_NUM);
    }
    for (int j = 0 ; j < TABLE_PLAYER_NUM; ++j) {
        SendMsgToClient(tableItem->pPlayers[j], LOCK_NOTICE, &msgNotice, sizeof(LockNoticeDef));
    }
}

//发送出牌消息
void FLGameLogic::PrepareSendCard(TableItemDef &tableItem) {
    SendCardsServerDef msgServer = {0};
    msgServer.usTableNumExtra = tableItem.iCurSendPlayer;
    msgServer.cPass = 1;
    for(int i = 0; i < TABLE_PLAYER_NUM; ++i) {
        if(tableItem->pPlayers[i]) {
            if(i == tableItem->iCurSendPlayer) {
                LockTimeOut(tableItem.pPlayers[i], 55); //客户端是40秒倒计时，再加上发牌5.4秒、换桌2.5秒、处理发牌、换桌、出牌消息延迟3秒，一共10.9秒左右
            }
            tableItem->pPlayers[i]->userNodeInfo.iGameStatus = GAME_WAIT_SEND;   
            tableItem->pPlayers[i]->iOverNum = 0;

            SendMsgToClient(tableItem.pPlayers[i],SEND_CARDS_SERVER_MSG,&msgServer,sizeof(SendCardsServerDef));
        }
    }
    tableItem.pPlayers[tableItem.iCurSendPlayer]->cPass = 1;
    tableItem->bTribute = false;
} 

//准备进贡，设置参数，发送进贡消息
void FLGameLogic::PrepareTribute(TableItemDef &tableItem) {

    TributeCardsServerDef msgTribute = {0};
    msgTribute.iGameLevel = tableItem.m_cLevel;
    for (int i = 0; i < TABLE_PLAYER_NUM; ++i) {
        msgTribute.iOverNum[i] = tableItem.pPlayers[i]->iOverNum;
    }
    for (int l = 0; l < 4; ++l) {
        if (tableItem->pPlayers[l]->iOverNum == 1) {
            tableItem->cSendPlayer1 = tableItem->pPlayers[l]->userNodeInfo.usTableNumExtra;
            if (tableItem->pPlayers[(l + 2) % TABLE_PLAYER_NUM]->iOverNum == 2) { //对家二游，双下
                tableItem->bTribute = false;
                tableItem->m_iGiveNum = 2;
                tableItem->m_iReturnNum = 2;
                tableItem->cSendPlayer2 = tableItem->pPlayers[(l + 2) % 4]->userNodeInfo.usTableNumExtra;
            }
            else {
                tableItem->bTribute = true;
                tableItem->m_iGiveNum = 1;
                tableItem->m_iReturnNum = 1;
            }
            if (tableItem->pPlayers[(l + 1) % TABLE_PLAYER_NUM]->iOverNum == 4) {
                tableItem->cGivePlayer1 = tableItem->pPlayers[(l + 1) % TABLE_PLAYER_NUM]->userNodeInfo.usTableNumExtra;
            } 
            if (tableItem->pPlayers[(l + 2) % TABLE_PLAYER_NUM]->iOverNum == 4) {
                tableItem->cGivePlayer1 = tableItem->pPlayers[(l + 2) % TABLE_PLAYER_NUM]->userNodeInfo.usTableNumExtra;
            } 
            if (tableItem->pPlayers[(l + 3) % TABLE_PLAYER_NUM]->iOverNum == 4) {
                tableItem->cGivePlayer2 = tableItem->pPlayers[(l + 3) % TABLE_PLAYER_NUM]->userNodeInfo.usTableNumExtra;
            } 
            break;
        }
    }
 
    __log(_ALL, "Function[%s]", "giveNum[%d]", __FUNCTION__, tableItem->m_iGiveNum);
 
    for(int i = 0; i < TABLE_PLAYER_NUM; ++i) {
        if(tableItem->pPlayers[i]) {
            tableItem->pPlayers[i]->userNodeInfo.iGameStatus = GAME_WAIT_TRIBUTE;  
            if (tableItem->cGivePlayer1 != -1) {
                LockTimeOut(tableItem->pPlayers[tableItem->cGivePlayer1], 40); 
            }
            if (tableItem->cGivePlayer2 != -1) {
                LockTimeOut(tableItem->pPlayers[tableItem->cGivePlayer2], 40); 
            }
            SendMsgToClient(tableItem->pPlayers[i], TRIBUTE_CARD_SERVER_MSG, &msgTribute, sizeof(TributeCardsServerDef));
        }
    }
}

FactoryTableItemDef * FLGameLogic::GetTableItem(int iRoomID,int iTableNum) {
    if(iRoomID >-1 && iRoomID < MAX_ROOM_NUM && iTableNum >-1 && iTableNum < MAX_TABLE_NUM) {
       return  &m_tbItem[iRoomID][iTableNum];
    }
    else {
       __log(_ERROR,"FLGameLogic","GetTableItem: iRoomID = [%d],iTableNum = [%d] is Error!",iRoomID,iTableNum);
       return NULL;
    }
}

//自动出牌逻辑
void FLGameLogic::AutoHandleGame(FactoryPlayerNodeDef  *pPlayerNode) 
{
    PlayerNodeDef *nodePlayers = (PlayerNodeDef *)pPlayerNode;
    if(!nodePlayers) {
        __log(_ERROR, "Function[%s]", "nodePlayers is NULL", __FUNCTION__);
        return;
    }
    if (!nodePlayers->bRealAuto) {
        __log(_ERROR, "Function[%s]", "bRealAuto is false,return right now", __FUNCTION__);
        return;
    }
    nodePlayers->bRealAuto = false;
 
    int iTableNum = nodePlayers->userNodeInfo.iTableNum;
    int iTableNumExtra = nodePlayers->userNodeInfo.usTableNumExtra;
    int iRoomID = nodePlayers->userNodeInfo.iRoomNum;
    if(iTableNum < 1 || iTableNum > MAX_TABLE_NUM || iRoomID < 1 || iRoomID > MAX_ROOM_NUM || iTableNumExtra < 0 || iTableNumExtra > 3 ||nodePlayers->iHandCardNum == 0) {
        __log(_ALL,"Function[%s]","iRoomID_[%d] iTableNum_[%d] iTableNumExtra_[%d]iHandCardNum[%d]", __FUNCTION__, iRoomID,iTableNum,iTableNumExtra, nodePlayers->iHandCardNum);
        return;
    }

    TableItemDef *tableItem = (TableItemDef *)GetTableItem(iRoomID - 1, iTableNum - 1);
 
    for(int i = 0;i<TABLE_PLAYER_NUM;i++) {
        if(tableItem == NULL || tableItem->pPlayers[i] == NULL) {
            __log(_ERROR,"Function[%s]", "tableItem.pPlayers[%d]=NULL", __FUNCTION__, i);
            return;
        }
    }
    
    __log(_DEBUG,"Function[%s]", "iTableNumExtra= [%d]--iCurTablePlayer[%d]--iCurSendPlayer[%d]--玩家[%s]iHandCardNum[%d]",
          __FUNCTION__, iTableNumExtra,tableItem->iCurTablePlayer,tableItem->iCurSendPlayer,nodePlayers->szNickName, nodePlayers->iHandCardNum);
     
    if(nodePlayers->userNodeInfo.iGameStatus == GAME_WAIT_SEND && tableItem->iCurSendPlayer == iTableNumExtra) 
	{
        if (nodePlayers->cCards[MAX_HAND_CARDS_NUM - 1] != 0) 
		{ //有进贡还贡牌，把牌调整到前面27个位置，因为AI只考虑了27个牌的情况
            for (int i = 0; i < MAX_HAND_CARDS_NUM - 1; ++i) 
			{
                if (nodePlayers->cCards[i] == 0) 
				{
                    for (int j = i; j < MAX_HAND_CARDS_NUM - 1; ++j) 
					{
                        nodePlayers->cCards[j] = nodePlayers->cCards[j + 1];
                    }
                    nodePlayers->cCards[MAX_HAND_CARDS_NUM - 1] = 0;
                    break;
                }
            }
        }

		Gd_AiLogic aiLogic[TABLE_PLAYER_NUM];
		for (int i = 0; i < TABLE_PLAYER_NUM; i++)
		{
			aiLogic[i].Clear();
		}

		CCardType iSelectType;
		int iSelectValue = 0;
		SendCardsReqDef msgSend = { 0 };
		char cCards[MAX_OUT_CARD_NUM] = { 0 };

		int ret = 1;

		if (tableItem->pPlayers[iTableNumExtra]->cPass > 0)
		{
			aiLogic[iTableNumExtra].m_firstCardInfo.Clear();
			for (int i = 0; i < TABLE_PLAYER_NUM; i++)
			{
				std::vector<char> handCard;
				for (int j = 0; j < MAX_HAND_CARDS_NUM; j++)
				{
					PlayerNodeDef* pPlayer = (PlayerNodeDef*)tableItem->pFactoryPlayers[i];
					if (pPlayer->cCards[j] > 0) handCard.push_back(pPlayer->cCards[j]);
				}
				aiLogic[i].SetAIHandCard(handCard, (Heart << 4) | tableItem->m_cLevel);
				if (i != iTableNumExtra)
				{
					aiLogic[iTableNumExtra].m_firstCardInfo.pAILogic[i] = &aiLogic[i];
					aiLogic[iTableNumExtra].m_firstCardInfo.allPlayerCardsNum[i] = (int)aiLogic[i].m_vcHandCards.size();
				}
			}
			aiLogic[iTableNumExtra].m_firstCardInfo.myTableNumExtra = iTableNumExtra;
			aiLogic[iTableNumExtra].PlayFirstCard();

			iSelectType = aiLogic[iTableNumExtra].m_firstCardInfo.cSelectType;
			iSelectValue = aiLogic[iTableNumExtra].m_firstCardInfo.iSelectValue;

			for (int i = 0; i < MAX_OUT_CARD_NUM; i++)
			{
				if (aiLogic[iTableNumExtra].m_firstCardInfo.cSendCard[i]);
				cCards[i] = aiLogic[iTableNumExtra].m_firstCardInfo.cSendCard[i];
			}
		}
		else
		{
			int oldCardValue = CCard::JudgeCradsValue(tableItem->cCards, strlen(tableItem->cCards),
				(CCardType)tableItem->iCurCardType, tableItem->m_cLevel | (2 << 4));

			aiLogic[iTableNumExtra].m_followInfo.Clear();
			aiLogic[iTableNumExtra].m_followInfo.iMyTableNumExtra = iTableNumExtra;
			aiLogic[iTableNumExtra].m_followInfo.iCurTableNumExtra = tableItem->iCurTablePlayer;
			aiLogic[iTableNumExtra].m_followInfo.cCurType = (CCardType)tableItem->iCurCardType;
			aiLogic[iTableNumExtra].m_followInfo.iCurCardValue = oldCardValue;
			for (int i = 0; i < TABLE_PLAYER_NUM; i++)
			{
				if (i != iTableNumExtra)
				{
					aiLogic[iTableNumExtra].m_followInfo.pAILogic[i] = &aiLogic[i];
				}
			}
			for (int i = 0; i < TABLE_PLAYER_NUM; i++)
			{
				std::vector<char> handCard;
				for (int j = 0; j < MAX_HAND_CARDS_NUM; j++)
				{
					PlayerNodeDef* pPlayer = (PlayerNodeDef*)tableItem->pFactoryPlayers[i];
					if (pPlayer->cCards[j] > 0) handCard.push_back(pPlayer->cCards[j]);
				}
				aiLogic[i].SetAIHandCard(handCard, (Heart << 4) | tableItem->m_cLevel);
				
			}
			
			ret = aiLogic[iTableNumExtra].FollowCard();

			if (ret)
			{
				iSelectType = aiLogic[iTableNumExtra].m_followInfo.cSelectType;
				iSelectValue = aiLogic[iTableNumExtra].m_followInfo.iSelectValue;

				for (int i = 0; i < MAX_OUT_CARD_NUM; i++)
				{
					if (aiLogic[iTableNumExtra].m_followInfo.cSendCard[i]);
					cCards[i] = aiLogic[iTableNumExtra].m_followInfo.cSendCard[i];
				}
			}
			
		}

		if (CheckInHandCard(cCards, nodePlayers->cCards) == false)
		{
			int a = 0;
		}
		
		if (ret)
		{
            int temp[MAX_HAND_CARDS_NUM] = {0};
            int iCardsCount = 0; 
            int iType = 0;
            int iNowValue = 0;
              
            for (int i = 0; i < MAX_OUT_CARD_NUM; ++i) {
                if (cCards[i] > 0) {
                    temp[iCardsCount] = cCards[i];
                    msgSend.cCards[msgSend.cCardNum] = cCards[i];
                    ++msgSend.cCardNum;
                    ++iCardsCount;
                }
            }
            if (msgSend.cCardNum != 0) {
                msgSend.cCardType = CheckCard::CheckShape(temp, msgSend.cCardNum, iNowValue, tableItem->m_cLevel, iType);   //判断玩家出的牌型
            } 
            msgSend.cCardValue = iNowValue;
        }
        string cards = FormatAndSortCards(msgSend.cCards, msgSend.cCardNum);
        __log(_ALL, "Function[%s]", "cPass[%d]CurCardType[%0x]CurCardValue[%0x]selectType[%0x]selectValue[%0x]cards[%s]", 
              __FUNCTION__, tableItem->pPlayers[iTableNumExtra]->cPass, tableItem->iCurCardType, tableItem->iCurCardValue, iSelectType, iSelectValue, cards.c_str()); 
        if (msgSend.cCardNum == 0) { //强制出牌
            ForceHandleGame(pPlayerNode);
        }
        else { 
            HandleSendCardsReq(nodePlayers, (void *)&msgSend, sizeof(SendCardsReqDef));
        }
    }
    else {
        ForceHandleGame(pPlayerNode);
    }
    return;
}

//强制出牌逻辑
void FLGameLogic::ForceHandleGame(FactoryPlayerNodeDef  *pPlayerNode)
{ 
    int i;
    PlayerNodeDef *nodePlayers = (PlayerNodeDef *)pPlayerNode;
    if(!nodePlayers) {
        __log(_ERROR, "Function[%s]", "nodePlayers is NULL", __FUNCTION__);
        return;
    }
 
    int iTableNum = nodePlayers->userNodeInfo.iTableNum;
    int iTableNumExtra = nodePlayers->userNodeInfo.usTableNumExtra;
    int iRoomID = nodePlayers->userNodeInfo.iRoomNum;
    if(iTableNum < 1 || iTableNum > MAX_TABLE_NUM || iRoomID < 1 || iRoomID > MAX_ROOM_NUM || iTableNumExtra < 0 || iTableNumExtra > 3) {
        __log(_ALL,"Function[%s]","iRoomID_[%d] iTableNum_[%d] iTableNumExtra_[%d]", __FUNCTION__, iRoomID,iTableNum,iTableNumExtra);
        return;
    }

    TableItemDef *tableItem = (TableItemDef *)GetTableItem(iRoomID - 1, iTableNum - 1);
 
    for( i = 0;i<TABLE_PLAYER_NUM;i++) {
        if(tableItem == NULL || tableItem->pPlayers[i] == NULL) {
            __log(_ERROR,"Function[%s]", "tableItem.pPlayers[%d]=NULL", __FUNCTION__, i);
            return;
        }
    }
    
    __log(_DEBUG,"Function[%s]", "iTableNumExtra= [%d]--iCurTablePlayer[%d]--iCurSendPlayer[%d]--玩家[%s]",
          __FUNCTION__, iTableNumExtra,tableItem->iCurTablePlayer,tableItem->iCurSendPlayer,nodePlayers->szNickName);
 
    if ((nodePlayers->userNodeInfo.iGameStatus == GAME_WAIT_RETURNTRIBUTE)) {
        if (iTableNumExtra == tableItem->cSendPlayer1 || iTableNumExtra == tableItem->cSendPlayer2) {
            __log(_DEBUG, "Function[%s]", "玩家[%s]托管还贡iTableNumExtra  = [%d],tableItem->cSendPlayer1 = [%d],tableItem->cSendPlayer2 = [%d]",
                  __FUNCTION__, nodePlayers->szNickName,iTableNumExtra, tableItem->cSendPlayer1, tableItem->cSendPlayer2); 

            TributeOKServerDef msgTC = {0};
            
			Gd_AiLogic aiLogic;
			aiLogic.Clear();
			aiLogic.m_iUserID = nodePlayers->userNodeInfo.iUserID;
			aiLogic.m_huangongInfo.Clear();
			aiLogic.m_huangongInfo.myTableNumExtra = iTableNumExtra;
			if (iTableNumExtra == tableItem->cSendPlayer1)
			{
				aiLogic.m_huangongInfo.dstTableNumExtra = tableItem->cGivePlayer1;
			}
			else if (iTableNumExtra == tableItem->cSendPlayer2)
			{
				aiLogic.m_huangongInfo.dstTableNumExtra = tableItem->cGivePlayer2;
			}

			std::vector<char> vec_cards;
			for (int i = 0; i < MAX_HAND_CARDS_NUM; i++)
			{
				if (nodePlayers->cCards[i] > 0) vec_cards.push_back(nodePlayers->cCards[i]);
			}

			aiLogic.SetAIHandCard(vec_cards, (Heart << 4) | tableItem->m_cLevel);
			
            msgTC.cTribute = aiLogic.HuanGong();

            HandleTributeReturnCard(nodePlayers, (void *)&msgTC,0);
            return;
        }
    }

    if (nodePlayers->userNodeInfo.iGameStatus == GAME_WAIT_TRIBUTE) {
        if (iTableNumExtra == tableItem->cGivePlayer1 || iTableNumExtra == tableItem->cGivePlayer2) {
            __log(_DEBUG, "Function[%s]", "玩家[%s][%s]托管进贡iTableNumExtra  = [%d],tableItem->cGivePlayer1 = [%d],tableItem->cGivePlayer2 = [%d]",
                  __FUNCTION__, nodePlayers->szNickName,nodePlayers->szUserName,iTableNumExtra, tableItem->cGivePlayer1, tableItem->cGivePlayer2); 

            TributeOKServerDef msgTC = {0};
            char cCardTemp[MAX_HAND_CARDS_NUM] = {0};//暂存玩家手中的牌
            char cCards[MAX_HAND_CARDS_NUM]    = {0};
            int  iCardValue,iCardType,iCardIndex;
            memcpy(cCardTemp,nodePlayers->cCards,MAX_HAND_CARDS_NUM);

            std::vector<char> vecCard;//保存手上面
            for(i = 0; i < MAX_HAND_CARDS_NUM; i++) {
                if(cCardTemp[i] != 0) {
                    vecCard.push_back(cCardTemp[i]);
                }
            }

            if(vecCard.size() <= 0) {
                return;
            }

            SortCardL(vecCard,tableItem->m_cLevel);
            int isize = vecCard.size();

            for(i = 0; i<MAX_HAND_CARDS_NUM;i++) {
                if ( ( (vecCard[isize - 1 - i] & 0x0f) == tableItem->m_cLevel ) && ((vecCard[isize - 1 - i] >> 4) == 2)) { 
                    continue;
                }

                for (int h = 0 ; h <= isize ; ++h) {
                    if(cCardTemp[h] == vecCard[isize - 1 - i]) {
                        iCardIndex = h;
                        msgTC.cTribute = cCardTemp[iCardIndex];
                        HandleTributeCard(nodePlayers, (void *)&msgTC,0);                    //自动进贡
                        return;
                    }
                }

            }
        }
    }

    tableItem->pPlayers[iTableNumExtra]->cPass = 0;
    if(nodePlayers->userNodeInfo.iGameStatus == GAME_WAIT_SEND && tableItem->iCurSendPlayer == iTableNumExtra) {
        __log(_ALL,"Function[%s]","玩家ID[%s]玩家手牌数[%d]", __FUNCTION__, nodePlayers->szUserName,nodePlayers->iHandCardNum);
        SendCardsReqDef msgSend = {0};           //如果不是自己主动出牌 为放弃状态

        char cCardTemp1[MAX_HAND_CARDS_NUM] = {0};//暂存玩家手中的牌
        char cCards1[MAX_HAND_CARDS_NUM]    = {0};
        int  iCardValue1,iCardType1,iCardIndex1;
        memcpy(cCardTemp1,nodePlayers->cCards,MAX_HAND_CARDS_NUM);

        std::vector<char> vecCard1;//保存手上面
        for(i = 0; i < MAX_HAND_CARDS_NUM; i++) {
            if(cCardTemp1[i] != 0) {
                vecCard1.push_back(cCardTemp1[i]);
            }
        }

        if(vecCard1.size() <= 0) {
            return;
        }

        SortCardL(vecCard1,tableItem->m_cLevel);
        int isize1 = vecCard1.size();
        
        __log(_ALL, "Function[%s]", "iCurTablePlayer[%d], iTableNumExtra[%d]", __FUNCTION__, tableItem->iCurTablePlayer, iTableNumExtra);
        //出牌
        if(tableItem->iCurTablePlayer == iTableNumExtra || tableItem->iCurTablePlayer == -1) {

            char cCardTemp[MAX_HAND_CARDS_NUM] = {0};//暂存玩家手中的牌
            char cCards[MAX_HAND_CARDS_NUM]    = {0};
            int  iCardValue = 0,iCardType = 0,iCardIndex = 0; 
            memcpy(cCardTemp,nodePlayers->cCards,MAX_HAND_CARDS_NUM);

            std::vector<char> vecCard;//保存手上面
            for(i = 0; i < MAX_HAND_CARDS_NUM; i++) {
                if(cCardTemp[i] != 0) {
                    vecCard.push_back(cCardTemp[i]);
                }
            }
            if(vecCard.size() <= 0) {
                return;
            }

            SortCardL(vecCard,tableItem->m_cLevel);
            int isize = vecCard.size();
            //出最小的牌
            for(i = 0; i<MAX_HAND_CARDS_NUM;i++) {
                if(cCardTemp[i] == vecCard[0]) {
                    cCards[i] = cCardTemp[i];
                    iCardIndex = i;
                    break;
                }
            }
            
            int temp[MAX_HAND_CARDS_NUM] = {0};
            int iCardsCount = 0;

            for(i = 0; i<MAX_HAND_CARDS_NUM; i++) {
                if(cCards[i]) {
                    temp[iCardsCount++] = cCards[i];
                }
            }
            int iType = 0;
            iCardType = CheckCard::CheckShape(temp,iCardsCount,iCardValue,tableItem->m_cLevel, iType);      //判断玩家出的牌型
                        
            msgSend.cCardNum = 1;
            msgSend.cCardType = iCardType;
            msgSend.cCardValue = iCardValue;
            msgSend.cCards[0] = cCardTemp[iCardIndex];
            __log(_ALL,"Function[%s]", "if:msgSend.cCards[0]:%d, iCardType[%d], iCardValue[%d], Card_size[%d]", 
                  __FUNCTION__, msgSend.cCards[0], iCardType, iCardValue, isize);
        }
        else if (isize1 == 1) {
            for(i = 0; i<MAX_HAND_CARDS_NUM;i++) {
                if(cCardTemp1[i] == vecCard1[0]) {
                    cCards1[i] = cCardTemp1[i];
                    iCardIndex1 = i;
                    break;
                }
            }

            int temp[MAX_HAND_CARDS_NUM] = {0};
            int iCardsCount = 0;

            for(i = 0; i<MAX_HAND_CARDS_NUM; i++) {
                if(cCards1[i]) {
                    temp[iCardsCount++] = cCards1[i];
                }
            }
            int iType = 0;
            iCardType1 = CheckCard::CheckShape(temp,iCardsCount,iCardValue1,tableItem->m_cLevel,iType);      //判断玩家出的牌型

            if (tableItem->iCurCardType == 1 && iCardValue1 > tableItem->iCurCardValue) {
                msgSend.cCardNum = 1;
                msgSend.cCardType = iCardType1;
                msgSend.cCardValue = iCardValue1;
                msgSend.cCards[0] = cCardTemp1[iCardIndex1];
            }
            __log(_ALL, "Function[%s]", "elif:msgSend.cCards[0]:%d", __FUNCTION__, msgSend.cCards[0]);
        }

        HandleSendCardsReq(nodePlayers, (void *)&msgSend, sizeof(SendCardsReqDef));
    }
    return;
}

void FLGameLogic::OnGameTimer(long tmTimer) {
    static time_t iOneMinTime  = time(NULL);
    static time_t iTenSecond   = time(NULL);
    static time_t iOneHourTime = time(NULL);
    static time_t iTenMinTime  = time(NULL);

    if(tmTimer - iTenMinTime >= 60) {
        iTenMinTime = tmTimer;
    }
    
    if(tmTimer - iOneMinTime >=600) {
        time_t iTimeTemp1;
        time_t iTimeTemp2;
        iTimeTemp1 = tmTimer-(24*3600);
        iTimeTemp2 = iOneMinTime-(24*3600);

        struct tm  tm_t1,tm_t2;
        tm_t1 = *localtime(&iTimeTemp1);
        tm_t2 = *localtime(&iTimeTemp2);

        if(tm_t1.tm_mday > tm_t2.tm_mday || tm_t1.tm_mon+1 > tm_t2.tm_mon+1 || tm_t1.tm_year > tm_t2.tm_year) {

            if(m_bDoOneDay) {
                OneDayTimer();                          //一天写一次金元宝牌型记录
            }
             m_bDoOneDay = true;
        }
        
       iOneMinTime = tmTimer;
    }
    
    
    if(tmTimer - iOneHourTime >= 3600) {
        iOneHourTime = tmTimer;
    }

    static time_t iChangeTableTime = time(NULL);
    if (tmTimer - iChangeTableTime >= 1) {
        iChangeTableTime = tmTimer;
        if (m_iMode != 3) { //私人场不换桌
            //SwapTableForRealPlayer();
        }
    }
    
    static time_t timeBroadCast = 0;
    if (tmTimer - timeBroadCast >= 1800) {
        timeBroadCast = tmTimer;
        SendBroadcastMsg();
    }
}

//检测超时自动处理 {
void FLGameLogic::TimeCheckTimeOut(long tmTimer) {

    static time_t tenMitTime  = time(NULL);
    if (tmTimer - tenMitTime >= 1) {
        for(int i = 0; i < MAX_ROOM_NUM; i++) {
            for(int j = 0; j< m_pAllServerBaseInfo.iMaxTableNum; j++) {
                TableItemDef *pTableItem = (TableItemDef *)GetTableItem(i,j);
                //此玩家不在游戏中
                if(pTableItem == NULL)  continue;
                                
                if(pTableItem->cPlayStatus != 1) continue;
                                
                for(int k = 0; k < m_pAllServerBaseInfo.iPlayerNum; k++) {
                    //只有玩家已经开始游戏了 如果有客户端时间超时... 将客户踢出 
                    if(pTableItem->pPlayers[k] != NULL 
                       && pTableItem->pPlayers[k]->userNodeInfo.iGameStatus >= GAME_WAIT_TRIBUTE 
                       && pTableItem->pPlayers[k]->userNodeInfo.iGameStatus < GAME_WAIT_END
                       && pTableItem->pPlayers[k]->userNodeInfo.lCheckTimeOut > 0
                       && pTableItem->pPlayers[k]->iHandCardNum > 0) {

                        pTableItem->pPlayers[k]->userNodeInfo.lCheckTimeOut -= 1;
                        if (pTableItem->bOtherTrust[pTableItem->pPlayers[k]->userNodeInfo.usTableNumExtra]) {
                            pTableItem->pPlayers[k]->bRealAuto = true;
                            AutoHandleGame(pTableItem->pPlayers[k]);
                            continue;
                        }
 
                        //本局非首次断线，直接托管
                        if (pTableItem->pPlayers[k]->iTimeOutTimes > 0  && pTableItem->pPlayers[k]->userNodeInfo.cDisconnectType != 0) {
                            pTableItem->pPlayers[k]->bRealAuto = true;
                            AutoHandleGame(pTableItem->pPlayers[k]);
                            continue;
                        }
                        if(pTableItem->pPlayers[k]->userNodeInfo.lCheckTimeOut <= 0 && pTableItem->pPlayers[k]->bKickOut == false) {
                            __log(_ERROR, "Function[%s]:", " szUserName:[%s] 玩家网络超时被踢出！", __FUNCTION__, pTableItem->pPlayers[k]->szUserName);
                            pTableItem->pPlayers[k]->bKickOut = true;
                            HandleCheatSendCard(pTableItem->pPlayers[k], CLINET_TIME_OUT);     
                            // 上面那行可能会执行到游戏结束，调用游戏结束处理过程中会清除断线玩家，导致指针为空
                            if( pTableItem->pPlayers[k]) 
                            {
                                pTableItem->pPlayers[k]->bRealAuto = true;
                                AutoHandleGame(pTableItem->pPlayers[k]);
                                if (pTableItem->pPlayers[k]) {
                                    pTableItem->pPlayers[k]->iTimeOutTimes ++;
                                }
                            }
                        }
                    }
                }                            
            }
        }
        tenMitTime  = tmTimer;
    }
}

//给玩家加锁
void FLGameLogic::LockTimeOut(PlayerNodeDef *pPlayerNode,short sWaitTime) {
    if(pPlayerNode == NULL)  
        return;
    pPlayerNode->userNodeInfo.lCheckTimeOut = sWaitTime;  //这个每个游戏不同  更具时间客户的需要的值延长五秒;  
    pPlayerNode->iTimeStart = sWaitTime;
    __log(_ALL, "Function[%s]", "userid[%d]lCheckTimeOut[%d]timeStart[%d]", __FUNCTION__,pPlayerNode->userNodeInfo.iUserID, pPlayerNode->userNodeInfo.lCheckTimeOut, pPlayerNode->iTimeStart);
}

//给玩家减锁
void FLGameLogic::UnLockTimeOut(PlayerNodeDef *pPlayerNode) {
    if(pPlayerNode == NULL)  return;
         pPlayerNode->userNodeInfo.lCheckTimeOut = 0;
    pPlayerNode->iTimeStart = 0;
    __log(_ALL, "Function[%s]", "userid[%d]lCheckTimeOut[%d]timeStart[%d]", __FUNCTION__,pPlayerNode->userNodeInfo.iUserID, pPlayerNode->userNodeInfo.lCheckTimeOut, pPlayerNode->iTimeStart);
}


void FLGameLogic::CleanPlayerNode(TableItemDef *tableItem) { 
    for(int i = 0; i < TABLE_PLAYER_NUM; ++i) {
        if(tableItem->pPlayers[i] != NULL) {
            tableItem->pPlayers[i]->ClearNode();
        }
    }
}

void FLGameLogic::HandleOtherNetMessage(IPlayerNode *lpPlayerNode, unsigned short type, void *buffer,long length) {
    PlayerNodeDef *pPlayerNode = (PlayerNodeDef *)lpPlayerNode;
    switch(type) {
    case SEND_CARDS_REQ_MSG:
        HandleSendCardsReq(pPlayerNode,buffer, length);
        break;
    case DEAL_OK_REQ_MSG:
        break;
    case TRIBUTE_CARD_REQ_MSG:
        HandleTributeCard(pPlayerNode,buffer,length);
        break;
    case TRIBUTE_RETURN_CARD_REQ_MSG:
        HandleTributeReturnCard(pPlayerNode,buffer,length);
        break;
    case MSG_C_TRUST:
        HandleTrust(pPlayerNode,buffer,length);
        break;
    case GD::GD_GET_PRIVATE_TABLE_LIST_REQ:
        HandleGetPrivateTableListReq(pPlayerNode, buffer, length);
        break;
    case GD::GD_GET_PRIVATE_TABLE_BYKEY_REQ:
        HandleGetPrivateTableByKeyReq(pPlayerNode, buffer, length);
        break;
#if DEBUG
    case GAME_MATCH_CARDS_MSG:
        HandleMatchCard(pPlayerNode,buffer,length);
        break;
#endif
    default:
        break;
    }
}

void FLGameLogic::HandleOtherRadiusNetMessage(IPlayerNode *lpPlayerNode, unsigned short type, void *buffer, long length) {
    switch(type) {
    case GAME_GET_GUANDAN_POINT_RANK_RES:
        HandleGetGuanDanPointRankRes(buffer, length);
        break;
    default:
        break;
    }
}

//获取掼蛋点排行的请求
void FLGameLogic::HandleGetGuanDanPointRankReq(PlayerNodeDef *pPlayerNode, void *pMsgData,long length) {
    if (NULL == pPlayerNode) {
        return;
    }
    GameGuanDanGetPointRankReqDef reqMsg;
    reqMsg.iGameID = m_pServerBaseInfo[0].iGameID;
    reqMsg.ulPlayerNode = (unsigned long)pPlayerNode;
    reqMsg.ulPlayerNodeId = (unsigned long)*pPlayerNode;
    SendData(m_pRadiusServer, GAME_GET_GUANDAN_POINT_RANK_REQ, &reqMsg, sizeof(reqMsg));
    __log(_ALL, "Function[%s]", "向认证服务器发送掼蛋排名请求", __FUNCTION__);

}

//获取掼蛋点排行的应答
void FLGameLogic::HandleGetGuanDanPointRankRes(void *buffer, long length) {
    GameGuanDanGetPointRankResDef *pRankRes = (GameGuanDanGetPointRankResDef *)buffer;
    FactoryPlayerNodeDef *pPlayerNode = (FactoryPlayerNodeDef *)pRankRes->ulPlayerNode;
    
    if (pPlayerNode && pRankRes->ulPlayerNodeId == *pPlayerNode) {
        __log(_ALL, "Function[%s]", "获取掼蛋玩家排名成功", __FUNCTION__);
        GameGuanDanRankResDef resMsg;
        resMsg.iCount = pRankRes->iCount;
        memcpy(&resMsg.rankInfo, pRankRes->rankInfo, sizeof(resMsg));
        SendMsgToClient(pPlayerNode, GAME_GET_GUANDAN_POINT_RANK_RES, &resMsg, sizeof(GameGuanDanRankResDef));
    }
}

//检查牌是否在手里
bool FLGameLogic::CheckInHandCard(const char cSendCard[],const char cHandCard[]) {
    for (int i = 0; i < MAX_OUT_CARD_NUM; ++i) {
        if (cSendCard[i] == 0) {
            continue;
        }
        bool found = false;
        for (int j = 0; j < MAX_HAND_CARDS_NUM; ++j) {
            if(cSendCard[i] == cHandCard[j]) {
                found = true;
                break;
            }
        }
        if(!found) {
            __log(_ERROR, "Function[%s]", "cSendCard[%0x] is not in hands", __FUNCTION__, cSendCard[i]);
            return false;
        }
    }
    return true;
}

//配牌消息处理
void FLGameLogic::HandleMatchCard(PlayerNodeDef *pPlayerNode,void *pMsgData,long length) {
    if ( NULL == m_maCard) {
        m_maCard = new MatchCards;
    }
    
    if (NULL != m_maCard) {
        GameMatchCardsMsg *pMatch = (GameMatchCardsMsg *)((char *)pMsgData + sizeof(MsgHeader));
        m_maCard->OnGameMatchCardsMsg(pMatch,length);
        __log(_ALL, "Function[%s]", "length[%d], pMatch->usCount[%d]",  __FUNCTION__, length, pMatch->usCount);
        for (int i = 0; i < length && i < pMatch->usCount; i++) {
            __log(_ALL, "Function[%s]","card[%2d]", __FUNCTION__, pMatch->szCards[i]);
            if (i % 10 == 26) {
                __log(_ALL, "Function[%s]","i % 10 == 26", __FUNCTION__);
            }
        }
    }
}

//玩家出牌请求
void FLGameLogic::HandleSendCardsReq(PlayerNodeDef *pPlayerNode, void *pMsgData,long length) {
    if(!pPlayerNode || !pMsgData) {
         __log(_ERROR, "Function[%s]", "pPlayerNode or pMsgData is NULL", __FUNCTION__);
         return;
    }
    int             iNowType            = 0;
    int             iNowValue           = 0;
    int             iRoomID             = pPlayerNode->userNodeInfo.iRoomNum;
    int             iTableNum           = pPlayerNode->userNodeInfo.iTableNum;
    char            iTableNumExtra      = pPlayerNode->userNodeInfo.usTableNumExtra;
    TableItemDef    *tableItem          = (TableItemDef *)GetTableItem(iRoomID - 1, iTableNum - 1);
 

    //检查房号、桌号、座位号
    if (iRoomID <= 0  || iRoomID > MAX_ROOM_NUM || iTableNum < 1 || iTableNum > MAX_TABLE_NUM ||  iTableNumExtra < 0 || iTableNumExtra > 3 
       || tableItem == NULL || iTableNumExtra != tableItem->iCurSendPlayer) {
        __log(_ERROR, "Function[%s]","非法的出牌玩家UserID[%d]CurSendPlayer[%d]RoomID[%d] TableNum[%d] TableNumExtra[%d]",
              __FUNCTION__, pPlayerNode->userNodeInfo.iUserID, tableItem->iCurSendPlayer, iRoomID, iTableNum, iTableNumExtra);
        HandleCheatSendCard(pPlayerNode, ILLege_SEND_CARD);
        return;
    }
 
    int             iOldValue           = tableItem->iCurCardValue;
    int             iOldType            = tableItem->iCurCardType;

    SendCardsReqDef *msgSC              = (SendCardsReqDef*)pMsgData;
    //判断消息内容的合法性
    if (sizeof(SendCardsReqDef) != length || msgSC->cCardNum < 0 || strlen(msgSC->cCards) != msgSC->cCardNum) {
        __log(_ERROR, "Function[%s]", "UserID[%d]SendCardsReqDef.length[%d]length[%d]strlen(msgSC->cCards)[%d]msgSC->cCardNum[%d]", 
              __FUNCTION__, pPlayerNode->userNodeInfo.iUserID, sizeof(SendCardsReqDef), length, strlen(msgSC->cCards), msgSC->cCardNum);
        HandleCheatSendCard(pPlayerNode, ILLege_SEND_CARD);
        return;
    }

    string sendCards = FormatAndSortCards(msgSC->cCards, msgSC->cCardNum);
    __log(_ALL,"Function[%s]","iUserID[%d]TableNumExtra[%d]cards[%s]msgSC->cCardNum[%d]", 
          __FUNCTION__, pPlayerNode->userNodeInfo.iUserID, pPlayerNode->userNodeInfo.usTableNumExtra, sendCards.c_str(), msgSC->cCardNum);
    //判断所出的牌是否是玩家手中牌
    if(CheckInHandCard(msgSC->cCards, pPlayerNode->cCards) == false) {
        string handCards = FormatAndSortCards(pPlayerNode->cCards, MAX_HAND_CARDS_NUM);
        __log(_ERROR,"Function[%s]","iUserID[%d]TableNumExtra[%d]出的牌手中没有handCards[%s]sendCards[%s]", 
              __FUNCTION__, pPlayerNode->userNodeInfo.iUserID, pPlayerNode->userNodeInfo.usTableNumExtra, handCards.c_str(), sendCards.c_str());
        HandleCheatSendCard(pPlayerNode, ILLege_SEND_CARD);
        return;
    }

    //验证牌型、牌值
    if (msgSC->cCardNum != 0) {
        int temp[MAX_HAND_CARDS_NUM] = {0};
        int iCardsCount = 0; 
        int iType = 0;
        for(int i = 0; i < msgSC->cCardNum; ++i) {
            if(msgSC->cCards[i]) {
                temp[iCardsCount++] = msgSC->cCards[i];
            }    
        }  
        iNowType = CheckCard::CheckShape(temp, msgSC->cCardNum, iNowValue, tableItem->m_cLevel, iType);   //判断玩家出的牌型
        if(iNowType <= 0 || iNowType != msgSC->cCardType || iNowValue != msgSC->cCardValue) {
            __log(_ERROR,"Function[%s]","UserID[%d]牌型牌值错误iNowType[%0x]iNowValue[%0x]msgSC->iCardType[%0x]msgSC->iCardValue[%0x]",
                __FUNCTION__, pPlayerNode->userNodeInfo.iUserID, iNowType, iNowValue, msgSC->cCardType, msgSC->cCardValue);
            HandleCheatSendCard(pPlayerNode,ILLege_SEND_CARD);
            return;
        }
    } 

    bool bOK = false;
    if((iTableNumExtra == tableItem->iCurTablePlayer) ||(iNowType <= 0)||(iNowValue <= 0)||(iOldValue <= 0 )|| (iOldType <= 0) //上家或本家不出 
        ||(iOldType == iNowType && iNowType < TYPE_BOMB_CARD  && iNowValue > iOldValue) //上家跟本家牌型一样，都不出炸弹，本家牌值大于上家牌值
        ||(iOldType < TYPE_BOMB_CARD && iNowType >= TYPE_BOMB_CARD) //上一家不是炸弹，本家出炸弹
        ||(iOldType >= TYPE_BOMB_CARD && iNowType == iOldType && iNowValue > iOldValue)//都出炸弹，炸弹类型相同，本家牌值比上一家大
        ||(iOldType >= TYPE_BOMB_CARD && iNowType > iOldType)) //都出炸弹，本家炸弹类型比上一家炸弹要大
    {
        bOK = true;
    }
    if (false == bOK) {
        __log(_DEBUG, "Function[%s]","UserID[%d]sendCards[%s]iOldType[%0x]iOldValue[%0x]iNowType[%0x]iNowValue[%0x]", 
              __FUNCTION__, pPlayerNode->userNodeInfo.iUserID, sendCards.c_str(), iOldType, iOldValue, iNowType, iNowValue);
        HandleCheatSendCard(pPlayerNode, ILLege_SEND_CARD);
        return;
    }

    memset(tableItem->pPlayers[iTableNumExtra]->cCurSendCard, 0, MAX_HAND_CARDS_NUM);
    memcpy(tableItem->pPlayers[iTableNumExtra]->cCurSendCard, msgSC->cCards, MAX_HAND_CARDS_NUM);
    string handCards = FormatAndSortCards(pPlayerNode->cCards, MAX_HAND_CARDS_NUM);
    __log(_DEBUG,"Function[%s]","iUserID[%d]TableNumExtra[%d]手牌张数[%d]handCards[%s]sendCards[%s]iCurCardType[%0x]iCurCardValue[%0x]", __FUNCTION__, pPlayerNode->userNodeInfo.iUserID, pPlayerNode->userNodeInfo.usTableNumExtra, pPlayerNode->iHandCardNum, handCards.c_str(), sendCards.c_str(), tableItem->iCurCardType, tableItem->iCurCardValue);
 
    //移除打出去的牌
    for(int i = 0; i < msgSC->cCardNum; ++i) {
        if(msgSC->cCards[i] > 0) {
            for(int j = 0; j < MAX_HAND_CARDS_NUM; ++j) {
                if(pPlayerNode->cCards[j] == msgSC->cCards[i]) {
                    pPlayerNode->cCards[j] = 0;
                    pPlayerNode->iHandCardNum--;
                    break;
                }
            }
        }
    }

    __log(_ALL,"Function[%s]玩家手牌张数","UserID[%d]HandCardNum[%d]玩家ID[%s]出牌type[%0x]value[%0x]sendCards[%s]",
          __FUNCTION__, pPlayerNode->userNodeInfo.iUserID, pPlayerNode->iHandCardNum, pPlayerNode->szUserName, iNowType, msgSC->cCardValue, sendCards.c_str());

    if (TYPE_SINGLE_SAMESEQUENCE_CARDS <= iNowType) 
        ++tableItem->m_cBombNumber;            //同花顺(包括)以上翻倍
    //防止整数越界
    if (tableItem->m_cBombNumber > 16) {
        tableItem->m_cBombNumber = 16;
    }

    if (msgSC->cCardNum == 0) {
        ++tableItem->m_iPassCount;
    }
    else {
        memset(tableItem->cCards, 0, strlen(tableItem->cCards));
        memcpy(tableItem->cCards, msgSC->cCards, strlen(msgSC->cCards));
        tableItem->iCurTablePlayer = iTableNumExtra; 
        tableItem->m_iPassCount = 0;
        tableItem->iCurCardType  = msgSC->cCardType;
        tableItem->iCurCardValue = msgSC->cCardValue;
    }
    tableItem->pPlayers[iTableNumExtra]->cPass = 0;

	CardsNoticeRobotDef cards_to_robot = { 0 };
	for (int i = 0; i < TABLE_PLAYER_NUM; i++)
	{
		PlayerNodeDef* pPlayerNode = (PlayerNodeDef*)tableItem->pPlayers[i];
		if (pPlayerNode)
		{
			for (int j = 0; j < MAX_HAND_CARDS_NUM; j++)
			{
				if (pPlayerNode->cCards[j] > 0)
				{
					cards_to_robot.cHandCards[i][cards_to_robot.iHandCardsNum[i]++] = pPlayerNode->cCards[j]; 	
				}
			}
		}
	}

	for (int i = 0; i < TABLE_PLAYER_NUM; i++)
	{	
		PlayerNodeDef* pPlayerNode = (PlayerNodeDef*)tableItem->pPlayers[i];
		if (pPlayerNode && pPlayerNode->bRobotLogin)
		{
			SendMsgToClient(pPlayerNode, MSG_C_CARDS_NOTICE_ROBOT, &cards_to_robot, sizeof(cards_to_robot));
		}
	}
 
    {//广播出牌消息
        SendCardsNoticeDef msgNotice = {0};
        memcpy(msgNotice.cSendCards, msgSC->cCards, MAX_HAND_CARDS_NUM); //所出的牌（数组全0为Pass）
        msgNotice.cTableNumExtra = iTableNumExtra;
        msgNotice.cCardNum   = msgSC->cCardNum;
        msgNotice.iCardType = msgSC->cCardType;
        msgNotice.iCardValue = msgSC->cCardValue;
        msgNotice.iBombNum = tableItem->m_cBombNumber;
 
        for(int i = 0; i < TABLE_PLAYER_NUM; ++i) 
		{
            if(tableItem->pPlayers[i]) {
                __log(_ALL, "Function[%s]", "SEND_CARDS_NOTICE_MSG UserID[%d]", __FUNCTION__, tableItem->pPlayers[i]->userNodeInfo.iUserID);
                SendMsgToClient(tableItem->pPlayers[i], SEND_CARDS_NOTICE_MSG, &msgNotice, sizeof(SendCardsNoticeDef));
            }
        }
    }
    //统计本桌结束人数以及结束顺序
    if (AccumulatOverNum(pPlayerNode, *tableItem)) {
        OnGameEnd(pPlayerNode, *tableItem);
    }
    else {
        UnLockTimeOut(pPlayerNode);            //时间解锁
        CallNextSendCard(*tableItem);           //调用下一个玩家出牌  
    }
    return;
}

//计算打完牌的顺序
bool FLGameLogic::AccumulatOverNum(PlayerNodeDef *pPlayerNode,TableItemDef &tableItem) {
    tableItem.iOverCount = 0;
    for(int i = 0; i< TABLE_PLAYER_NUM; ++i) {
        if (tableItem->pPlayers[i] == NULL) {
            __log(_DEBUG,"Function[%s]","玩家节点为空 —— 玩家所在位置[%d]", __FUNCTION__, i);
            return false;
        }
        else {
            if(tableItem.pPlayers[i]->iHandCardNum <= 0) {
                tableItem.iOverCount++;
            }
        }
    }
    if(pPlayerNode->iHandCardNum <= 0 && pPlayerNode->iOverNum == 0) {
        if(tableItem.iOverCount == 1) {
            pPlayerNode->iOverNum = 1;          //通知发送玩家出完的顺序
            __log(_ALL, "Function[%s]", "UserID[%d]iOverNum[%d]", __FUNCTION__, pPlayerNode->userNodeInfo.iUserID, pPlayerNode->iOverNum);
        }
        else if(tableItem.iOverCount == 2){
            pPlayerNode->iOverNum = 2;
            __log(_ALL, "Function[%s]", "UserID[%d]iOverNum[%d]", __FUNCTION__, pPlayerNode->userNodeInfo.iUserID, pPlayerNode->iOverNum);
            for(int i = 0; i < TABLE_PLAYER_NUM; ++i) {
                if(tableItem.pPlayers[i] && tableItem.pPlayers[i]->iHandCardNum <= 0){
                    if (tableItem.pPlayers[(i + 2) % TABLE_PLAYER_NUM]->iHandCardNum <= 0) {
                        for(int j = 0; j < TABLE_PLAYER_NUM; ++j) {
                            if(tableItem->pPlayers[j] && tableItem->pPlayers[j]->iOverNum != 1 &&  tableItem->pPlayers[j]->iOverNum != 2) {
                                tableItem->pPlayers[j]->iOverNum = 4;
                                __log(_ALL, "Function[%s]", "UserID[%d]iOverNum[%d]", 
                                __FUNCTION__, tableItem->pPlayers[j]->userNodeInfo.iUserID, tableItem->pPlayers[j]->iOverNum);
                            }
                        }
                        return true;
                    }
                    break;
                }
            }
        }
        else if(tableItem.iOverCount == 3) {
            pPlayerNode->iOverNum = 3;
            __log(_ALL, "Function[%s]", "UserID[%d]iOverNum[%d]", __FUNCTION__, pPlayerNode->userNodeInfo.iUserID, pPlayerNode->iOverNum);
            for(int i = 0; i < TABLE_PLAYER_NUM; ++i) {
                if(tableItem->pPlayers[i]) {
                    if(tableItem->pPlayers[i]->iOverNum != 1 &&  tableItem->pPlayers[i]->iOverNum != 2 &&  tableItem->pPlayers[i]->iOverNum != 3) {
                        tableItem->pPlayers[i]->iOverNum = 4;
                         __log(_ALL, "Function[%s]", "UserID[%d]iOverNum[%d]", 
                               __FUNCTION__, tableItem->pPlayers[i]->userNodeInfo.iUserID, tableItem->pPlayers[i]->iOverNum);
                        return true;
                    }
                }
            }
        }
        
        SendEndNoticeDef  msgSendEnd = {0};
        msgSendEnd.usTableNumExtra   = pPlayerNode->userNodeInfo.usTableNumExtra;
        msgSendEnd.cOlderNum         = pPlayerNode->iOverNum;
        
        for(int j = 0; j < TABLE_PLAYER_NUM; ++j) {
            if(tableItem.pPlayers[j]) {
                SendMsgToClient(tableItem.pPlayers[j], SEND_END_NOTICE, &msgSendEnd, sizeof(SendEndNoticeDef)); 
            }
        }
    }
    return false;
}

//获取下一个打牌玩家
void FLGameLogic::CallNextSendCard(TableItemDef &tableItem) {
    for (int i = 0 ; i < TABLE_PLAYER_NUM; ++i) {
        if (tableItem.pPlayers[i] == NULL) {
            __log(_DEBUG,"Function[%s]","玩家节点为空 —— 玩家所在位置[%d]", __FUNCTION__, i);
            return;
        }
    }

    //下一家
    tableItem.iCurSendPlayer = (++tableItem.iCurSendPlayer) % TABLE_PLAYER_NUM;
    //如果下家出完牌，则再下家
    if(tableItem.pPlayers[tableItem.iCurSendPlayer]->iHandCardNum <= 0) {
        ++tableItem->m_iPassCount;
        tableItem.iCurSendPlayer = (++tableItem.iCurSendPlayer) % TABLE_PLAYER_NUM;
    }
    //如果再下家出完牌，则再下一家 
    if(tableItem.pPlayers[tableItem.iCurSendPlayer]->iHandCardNum <= 0) {
        ++tableItem->m_iPassCount;
        tableItem.iCurSendPlayer = (++tableItem.iCurSendPlayer) % TABLE_PLAYER_NUM;
    }

    SendCardsServerDef msgServer = {0};
    if (tableItem->m_iPassCount >= 3) {
        msgServer.cPass = 1;
        if (tableItem.pPlayers[tableItem.iCurTablePlayer]->iHandCardNum <= 0 && 
            tableItem->pPlayers[(tableItem.iCurTablePlayer + 2) % TABLE_PLAYER_NUM]->iOverNum == 0) {
                tableItem.iCurSendPlayer = (tableItem.iCurTablePlayer + 2) % TABLE_PLAYER_NUM;
        }
        tableItem.iCurTablePlayer = tableItem.iCurSendPlayer;
        tableItem.pPlayers[tableItem.iCurSendPlayer]->cPass = 1;
    }
    msgServer.usTableNumExtra = tableItem.iCurSendPlayer;
    memset(tableItem.pPlayers[tableItem.iCurSendPlayer]->cCurSendCard, 0, MAX_HAND_CARDS_NUM);

    for(int i = 0; i < TABLE_PLAYER_NUM; ++i) {
        if(tableItem.pPlayers[i]->userNodeInfo.usTableNumExtra == tableItem.iCurSendPlayer) {
            LockTimeOut(tableItem.pPlayers[i], 40);
             __log(_ALL,"Function[%s]","UserID[%d]HandCardNum[%d]是否本轮首次出牌[%d]",
                   __FUNCTION__, tableItem.pPlayers[i]->userNodeInfo.iUserID, tableItem.pPlayers[i]->iHandCardNum, msgServer.cPass);
        }
        SendMsgToClient(tableItem->pPlayers[i], SEND_CARDS_SERVER_MSG, &msgServer, sizeof(SendCardsServerDef));
   }
}

//获取打过牌级level后的奖品券的奖励
int FLGameLogic::GetPrizeTicketNumAfterPass(const int &level) {
    int arr6Prize[3]    = {1, 2, 3};
    int arr10Prize[3]   = {3, 4, 5};
    int arrAPrize[3]    = {5, 6, 7};
    int iPrizeTicketNum = 0;
    switch (level) {
        case 6:
            iPrizeTicketNum = arr6Prize[GetRandIndex()];
            break;
        case 10:
            iPrizeTicketNum = arr10Prize[GetRandIndex()];
            break;
        case 1:
            iPrizeTicketNum = arrAPrize[GetRandIndex()];
            break;
        default:
            break;
    }
    return iPrizeTicketNum;
}

//设置N连胜的奖品券奖励
void FLGameLogic::SetPrizeTicketAfterContinuousWinning(TableItemDef &tableItem) {
    for (int i = 0; i < TABLE_PLAYER_NUM; ++i) {
        if (tableItem->pPlayers[i]->iOverNum == 1) {
            ++tableItem->pPlayers[i]->m_ucFirstPlayer;                        //连续头游戏次数累计; 将其它三家清零
            tableItem->pPlayers[(i + 1) % TABLE_PLAYER_NUM]->m_ucFirstPlayer = 0;
            tableItem->pPlayers[(i + 2) % TABLE_PLAYER_NUM]->m_ucFirstPlayer = 0;
            tableItem->pPlayers[(i + 3) % TABLE_PLAYER_NUM]->m_ucFirstPlayer = 0;
            int iPrizeTicket = 0; 
            if (3 <= tableItem->pPlayers[i]->m_ucFirstPlayer) {
                iPrizeTicket = 1 + (tableItem->pPlayers[i]->m_ucFirstPlayer - 3) * 2;
                if (15 < iPrizeTicket) {
                    iPrizeTicket = 0;                    //防刷, 连赢超过10局, 不送, 防止出错
                    __log(_WARN, "Function[%s]", "userID[%d]连胜超过10局，不再送奖品券", __FUNCTION__, tableItem->pPlayers[i]->userNodeInfo.iUserID);
                }
                tableItem->pPlayers[i]->userNodeInfo.iRDPrizeNum += iPrizeTicket;
            }
            __log(_DEBUG, "Function[%s]", "userID[%d]赠送奖品券[%d],连胜次数[%d]", 
                  __FUNCTION__, tableItem->pPlayers[i]->userNodeInfo.iUserID, iPrizeTicket, tableItem->pPlayers[i]->m_ucFirstPlayer);
        }
    } 
}

//设置打过牌级后的奖品券的奖励
void FLGameLogic::SetPrizeTicketAfterPass(TableItemDef &tableItem) {
    //加上打过级数后的奖品券
    if (m_pServerBaseInfo[0].iBaseTime < 100) {
        __log(_ALL, "Function[%s]", "iBaseTime[%d] less than 100,不送奖品券", __FUNCTION__, m_pServerBaseInfo[0].iBaseTime);
        return;
    }
    for (int i = 0; i < TABLE_PLAYER_NUM; ++i) {
        if (tableItem->pPlayers[i]->iOverNum != 1) {
            continue;
        }
        int iPrizeTicketNum1 = 0; 
        int iPrizeTicketNum2 = 0;
        if ((tableItem->m_cLastLevel[i % 2] <= 6) && (6 < tableItem->m_cLevel) && (tableItem->m_cLevel <= 10)) { //打过6,没过10
            iPrizeTicketNum1 = GetPrizeTicketNumAfterPass(6);
            iPrizeTicketNum2 = GetPrizeTicketNumAfterPass(6);
        }
        else if ((tableItem.m_cLastLevel[i % 2] <= 10) && ((10 < tableItem.m_cLevel) || (1 == tableItem->m_cLevel))) { //打过10
            iPrizeTicketNum1 = GetPrizeTicketNumAfterPass(10);
            iPrizeTicketNum2 = GetPrizeTicketNumAfterPass(10);
        }
        else if ((tableItem.m_cLastLevel[i % 2] != 0) && (2 == tableItem->m_cLevel)) { //打过1(只能是打到2)
            iPrizeTicketNum1 = GetPrizeTicketNumAfterPass(1);
            iPrizeTicketNum2 = GetPrizeTicketNumAfterPass(1);
        }
        if (tableItem->pPlayers[i]->m_iPrizeCountPerDay >= 50) {
            __log(_DEBUG, "Function[%s]", "UserID[%d] PrizeCountPerDay is over 50!", __FUNCTION__, tableItem->pPlayers[i]->userNodeInfo.iUserID);
            iPrizeTicketNum1 = 0;
        }
        if (tableItem->pPlayers[(i + 2) % 4]->m_iPrizeCountPerDay >= 50) {
            __log(_DEBUG, "Function[%s]", "UserID[%d] PrizeCountPerDay is over 50!", __FUNCTION__, tableItem->pPlayers[(i+2)%4]->userNodeInfo.iUserID);
            iPrizeTicketNum2 = 0;
        }
        tableItem->pPlayers[i]->m_iPrizeCountPerDay += iPrizeTicketNum1;
        tableItem->pPlayers[i]->userNodeInfo.iRDPrizeNum += iPrizeTicketNum1;
        tableItem->pPlayers[(i + 2) % TABLE_PLAYER_NUM]->m_iPrizeCountPerDay += iPrizeTicketNum2;
        tableItem->pPlayers[(i + 2) % TABLE_PLAYER_NUM]->userNodeInfo.iRDPrizeNum += iPrizeTicketNum2;
        tableItem->m_cPrizeTicketNum[i] = iPrizeTicketNum1;
        tableItem->m_cPrizeTicketNum[(i + 2) % TABLE_PLAYER_NUM] = iPrizeTicketNum1;
    }
}

//设置下一个牌级
void FLGameLogic::SetNextGameLevel(TableItemDef &tableItem) {
    for (int i = 0; i < TABLE_PLAYER_NUM; ++i) {
        if (tableItem->pPlayers[i]->iOverNum != 1) {
            continue;
        }
        if (tableItem->m_cBanker == i % 2) { //庄家是自家
            if (1 == tableItem->m_cLastLevel[i % 2]) { //上次是A，打过A，从2开始
                tableItem->m_cLevel = 2;
                tableItem->m_cPlayAceCount[i % 2] = 0; 
            }
            else {
                if (tableItem->m_cLastLevel[i % 2] + tableItem->m_iAddLevel > 13) {  //超过13(K), 必须打1(A) 
                    tableItem->m_cLevel = 1;
                    tableItem->m_cPlayAceCount[i % 2] = 1; 
                }
                else {
                    tableItem->m_cLevel = tableItem->m_cLastLevel[i % 2] + tableItem->m_iAddLevel;
                }
            }
        }
        else { //庄家是别家
            if (1 == tableItem->m_cLastLevel[i % 2]) { //上次是A，继续打A
                tableItem->m_cLevel = 1;
                ++tableItem->m_cPlayAceCount[i % 2]; 
            }
            else {
                if (tableItem->m_cLastLevel[i % 2] + tableItem->m_iAddLevel > 13) {  //超过13(K), 必须打1(A) 
                    tableItem->m_cLevel = 1;
                    tableItem->m_cPlayAceCount[i % 2] = 1; 
                }
                else {
                    tableItem->m_cLevel = tableItem->m_cLastLevel[i % 2] + tableItem->m_iAddLevel;
                }
            }
            tableItem->m_cBanker = (tableItem->m_cBanker + 1) % 2;
        }
        break; 
    }
} 

void FLGameLogic::OnGameEnd(PlayerNodeDef *pPlayerNode, TableItemDef &tableItem) {
    if (pPlayerNode == NULL) {
        __log(_ERROR,"Function[%s]","pPlayerNode = NULL", __FUNCTION__);
        return;
    }

    GameResultServerDef msgGameRtServer;
    memset(&msgGameRtServer,0,sizeof(GameResultServerDef));
    for(int i = 0; i < TABLE_PLAYER_NUM; i++) {
        if(tableItem.pPlayers[i] == NULL) {
            __log(_ERROR, "Function[%s]","tableItem.pPlayers[%d] = NULL ", __FUNCTION__, i);
            return;
        }
        UnLockTimeOut(tableItem.pPlayers[i]);
        tableItem->bOtherTrust[i] = false;
        tableItem->pPlayers[i]->userNodeInfo.iGameStatus = GAME_WAIT_END;
        memcpy(msgGameRtServer.CHandCard[i], tableItem->pPlayers[i]->cCards, MAX_HAND_CARDS_NUM);
        msgGameRtServer.cTableNumExtra[i] = tableItem->pPlayers[i]->userNodeInfo.usTableNumExtra;
        msgGameRtServer.iOrderNum[i] = tableItem.pPlayers[i]->iOverNum;
    }
    
    for (int i = 0; i < TABLE_PLAYER_NUM;++i) {
        //头游, 只找头游, 然后算出其它玩家情况
        if (tableItem->pPlayers[i]->iOverNum == 1) {

            //设置增加的积分和经验
            tableItem->bTribute = true;
            if (tableItem->pPlayers[(i + 2) % 4]->iOverNum == 2) {
                tableItem->m_iAddLevel = 6;
                tableItem->bTribute = false; 
            }
            else if (tableItem->pPlayers[(i + 2) % 4]->iOverNum == 3) {
                tableItem->m_iAddLevel = 4;
            }
            else if (tableItem->pPlayers[(i + 2) % 4]->iOverNum == 4) {
                tableItem->m_iAddLevel = 2;
            }
            tableItem->m_iAddExp = tableItem->m_iAddLevel;

            //给自己和对家加上荣誉点
            tableItem->pPlayers[i]->SetMatchScoreFromGame(tableItem.m_iAddExp);
            tableItem->pPlayers[(i + 2) % TABLE_PLAYER_NUM]->SetMatchScoreFromGame(tableItem.m_iAddExp);
            tableItem->pPlayers[(i + 1) % TABLE_PLAYER_NUM]->SetMatchScoreFromGame(-tableItem.m_iAddExp);
            tableItem->pPlayers[(i + 3) % TABLE_PLAYER_NUM]->SetMatchScoreFromGame(-tableItem.m_iAddExp);

            //加上惯蛋点
            tableItem->pPlayers[i]->userTaskInfo.iTaskID = tableItem->m_iAddLevel;
            tableItem->pPlayers[(i + 2) %TABLE_PLAYER_NUM]->userTaskInfo.iTaskID = tableItem->m_iAddLevel;
            tableItem->pPlayers[(i + 1) %TABLE_PLAYER_NUM]->userTaskInfo.iTaskID = -tableItem->m_iAddLevel;
            tableItem->pPlayers[(i + 3) %TABLE_PLAYER_NUM]->userTaskInfo.iTaskID = -tableItem->m_iAddLevel;

            //设置下一局的等级
            SetNextGameLevel(tableItem);

            if (m_iMode != 3) {
                //给连胜玩家加上奖品券
                SetPrizeTicketAfterContinuousWinning(tableItem);

                //加上打过级数后的奖品券
                SetPrizeTicketAfterPass(tableItem);
                msgGameRtServer.iPrizeTicket[i] = tableItem->m_cPrizeTicketNum[i];
                msgGameRtServer.iPrizeTicket[(i + 2) % TABLE_PLAYER_NUM] = tableItem->m_cPrizeTicketNum[(i + 2) % TABLE_PLAYER_NUM]; 
                msgGameRtServer.iPrizeTicket[(i + 1) % TABLE_PLAYER_NUM] = 0;
                msgGameRtServer.iPrizeTicket[(i + 3) % TABLE_PLAYER_NUM] = 0;
            }

            //计算积分
            int baseTime = 0;
            if (m_iMode == 3) {
                baseTime = tableItem->GetTableAnte();
            }
            else {
                baseTime = m_pServerBaseInfo[0].iBaseTime;
            }
            int iTaskMoney  = (int)pow(2, tableItem.m_cBombNumber) * baseTime * tableItem->m_iAddLevel;
            int iGameMoney1 = iTaskMoney;
            int iGameMoney2 = iTaskMoney;
            int iExp1       = tableItem->m_iAddExp;
            int iExp2       = tableItem->m_iAddExp; 


             //一家掉线, 承担所有扣分 
            if ((0 != tableItem.pPlayers[(i + 1) % 4]->userNodeInfo.cDisconnectType) && (0 == tableItem.pPlayers[(i + 3) % 4]->userNodeInfo.cDisconnectType)) {
                iGameMoney1 += iGameMoney2;
                iGameMoney2 = 0;
                iExp1 += iExp1;
                iExp2 = 0;
            }
             //一家掉线, 承担所有扣分 
            else if ((0 != tableItem.pPlayers[(i + 3) % 4]->userNodeInfo.cDisconnectType) && (0 == tableItem.pPlayers[(i + 1) % 4]->userNodeInfo.cDisconnectType)) {
                iGameMoney2 += iGameMoney1;
                iGameMoney1 = 0;
                iExp2 += iExp2;
                iExp1 = 0;
            }

            //破产
            if(tableItem->pPlayers[(i + 1) % 4]->userNodeInfo.iFirstMoney < iGameMoney1) {
                __log(_ALL, "Function[%s]", "userID[%d]破产iFirstMoney[%d]iGameMoney[%d]", 
                      __FUNCTION__, tableItem->pPlayers[(i + 1) % 4]->userNodeInfo.iUserID, tableItem->pPlayers[(i + 1) % 4]->userNodeInfo.iFirstMoney, iGameMoney1);
                iGameMoney1 = tableItem->pPlayers[(i + 1) % 4]->userNodeInfo.iFirstMoney;
                msgGameRtServer.bBankrupt[(i + 1) % 4] = true;
            }
            if(tableItem->pPlayers[(i + 3) % 4]->userNodeInfo.iFirstMoney < iGameMoney2) {
                __log(_ALL, "Function[%s]", "userID[%d]破产iFirstMoney[%d]iGameMoney[%d]", 
                      __FUNCTION__, tableItem->pPlayers[(i + 3) % 4]->userNodeInfo.iUserID, tableItem->pPlayers[(i + 3) % 4]->userNodeInfo.iFirstMoney, iGameMoney2);
                iGameMoney2 = tableItem->pPlayers[(i + 3) % 4]->userNodeInfo.iFirstMoney;
                msgGameRtServer.bBankrupt[(i + 3) % 4] = true; 
            }

            if (1 > iTaskMoney) iTaskMoney = 1;                     //防错
            if (1000000000 < iTaskMoney) iTaskMoney = 1000000000;   //防错 一局输赢限制在10亿之内
            
            iTaskMoney = (iGameMoney1 + iGameMoney2) / 2;

            __log(_ALL,"Function[%s]", "UserID[%d]BombNum[%d]iBaseTime[%d]Level[%d]iTaskMoney[%d]m_iPercent[%d]Ante[%d]FirstMoney1[%lld]iGameMoney1[%d]FirstMoney2[%lld]iGameMoney2[%d]", __FUNCTION__, tableItem.pPlayers[i]->userNodeInfo.iUserID, tableItem->m_cBombNumber, m_pServerBaseInfo[0].iBaseTime, tableItem.m_iAddLevel, iTaskMoney, m_iPercent, m_pServerBaseInfo[0].iBaseTime, tableItem->pPlayers[(i + 1) % TABLE_PLAYER_NUM]->userNodeInfo.iFirstMoney, iGameMoney1, tableItem->pPlayers[(i + 3) % TABLE_PLAYER_NUM]->userNodeInfo.iFirstMoney, iGameMoney2);

#ifdef _NEW_RATE_
            msgGameRtServer.iTableMoney =  baseTime;
#else
            msgGameRtServer.iTableMoney =  iTaskMoney * m_iPercent / 100;                           //桌费
#endif
            int count = 1;
            for (int j = i; count <= 2; j += 2, ++count) {
                tableItem.pPlayers[j % TABLE_PLAYER_NUM]->userNodeInfo.iRDGameExpValue  += tableItem.m_iAddExp;            //经验累计
                tableItem.pPlayers[j % TABLE_PLAYER_NUM]->userNodeInfo.llRDWinMoney     += iTaskMoney;                     //奖励积分
                tableItem.pPlayers[j % TABLE_PLAYER_NUM]->userNodeInfo.llRDMoney        += iTaskMoney;                     //记费
                tableItem.pPlayers[j % TABLE_PLAYER_NUM]->userNodeInfo.iRDWinNum++;                                        //赢局, 累计
#ifndef _NEW_RATE_
                tableItem.pPlayers[j % TABLE_PLAYER_NUM]->userNodeInfo.iRDGameRoyalty   = iTaskMoney * m_iPercent / 100;   //抽税 后台记录，赢方会在后台减去抽税部分
#endif
                tableItem.pPlayers[j % TABLE_PLAYER_NUM]->userTaskInfo.cTableNum        = tableItem.m_iAddExp;             //经验值, 当局
                tableItem.pPlayers[j % TABLE_PLAYER_NUM]->userNodeInfo.iGameMultiple    = tableItem.m_iAddLevel;           //升级数
                msgGameRtServer.iExtraNum[j % TABLE_PLAYER_NUM]                         = tableItem.m_iAddExp;
                msgGameRtServer.iMoneyResult[j % TABLE_PLAYER_NUM]                      = iTaskMoney;
                msgGameRtServer.iAmountResult[j % TABLE_PLAYER_NUM]                     = iTaskMoney - msgGameRtServer.iTableMoney;
            }

            tableItem.pPlayers[(i + 1) % TABLE_PLAYER_NUM]->userNodeInfo.iRDLostNum++;
            if ((tableItem.pPlayers[(i + 1) % TABLE_PLAYER_NUM]->userNodeInfo.iGameExpValue - iExp1) <= 0) {
                tableItem.pPlayers[(i + 1) % TABLE_PLAYER_NUM]->userNodeInfo.iRDGameExpValue = 0;
            }
            else {
                tableItem.pPlayers[(i + 1) % TABLE_PLAYER_NUM]->userNodeInfo.iRDGameExpValue -= iExp1;
            }

            tableItem.pPlayers[(i + 1) % TABLE_PLAYER_NUM]->userNodeInfo.llRDMoney -= iGameMoney1;
            msgGameRtServer.iAmountResult[(i + 1) % TABLE_PLAYER_NUM]               = -iGameMoney1;                      
#ifdef _NEW_RATE_
            msgGameRtServer.iAmountResult[(i + 1) % TABLE_PLAYER_NUM]               -= msgGameRtServer.iTableMoney;
#endif
 
            tableItem.pPlayers[(i + 1) % TABLE_PLAYER_NUM]->userNodeInfo.iRDLostMoney = -iGameMoney1;
            tableItem.pPlayers[(i + 1) % TABLE_PLAYER_NUM]->userTaskInfo.cTableNum = -iExp1;
            tableItem.pPlayers[(i + 1) % TABLE_PLAYER_NUM]->userNodeInfo.iGameMultiple = tableItem->m_cLevel;

            tableItem.pPlayers[(i + 3) % TABLE_PLAYER_NUM]->userNodeInfo.iRDLostNum++;
            if ((tableItem.pPlayers[(i + 3) % TABLE_PLAYER_NUM]->userNodeInfo.iGameExpValue - iExp2) <= 0) {
                tableItem.pPlayers[(i + 3) % TABLE_PLAYER_NUM]->userNodeInfo.iRDGameExpValue = 0;
            }
            else {
                tableItem.pPlayers[(i + 3) % TABLE_PLAYER_NUM]->userNodeInfo.iRDGameExpValue -= iExp2;
            }

            tableItem.pPlayers[(i + 3) % TABLE_PLAYER_NUM]->userNodeInfo.llRDMoney -= iGameMoney2;
            msgGameRtServer.iAmountResult[(i + 3) % TABLE_PLAYER_NUM]               = -iGameMoney2;                      
#ifdef _NEW_RATE_
            msgGameRtServer.iAmountResult[(i + 3) % TABLE_PLAYER_NUM]               -= msgGameRtServer.iTableMoney;
#endif
 
            tableItem.pPlayers[(i + 3) % TABLE_PLAYER_NUM]->userNodeInfo.iRDLostMoney = -iGameMoney2;
            tableItem.pPlayers[(i + 3) % TABLE_PLAYER_NUM]->userTaskInfo.cTableNum = -iExp2;
            tableItem.pPlayers[(i + 3) % TABLE_PLAYER_NUM]->userNodeInfo.iGameMultiple = tableItem->m_cLevel;

            msgGameRtServer.iExtraNum[(i + 1) % TABLE_PLAYER_NUM] = -iExp1;
            msgGameRtServer.iMoneyResult[(i + 1)% TABLE_PLAYER_NUM] = tableItem.pPlayers[(i + 1)% TABLE_PLAYER_NUM]->userNodeInfo.llRDMoney;
            msgGameRtServer.iExtraNum[(i + 3)%TABLE_PLAYER_NUM] = -iExp2;
            msgGameRtServer.iMoneyResult[(i + 3)%TABLE_PLAYER_NUM]  = tableItem.pPlayers[(i + 3)%TABLE_PLAYER_NUM]->userNodeInfo.llRDMoney;

            //此时第一个出牌玩家掉线，代理出牌 
            for(int r = 0; r < TABLE_PLAYER_NUM; ++r) {
                if(tableItem->pPlayers[r] != NULL) {
                    if (tableItem->pPlayers[r]->userNodeInfo.cDisconnectType != 0) {
                        /*赢家掉线不做处罚 */
                        msgGameRtServer.cDisconnectType[r] = 0;
                        if(tableItem->pPlayers[i]->iOverNum == 1 || tableItem->pPlayers[(i + 2) % TABLE_PLAYER_NUM]->iOverNum == 1) {
                            continue;
                        }
                        if ((tableItem.pPlayers[r]->userNodeInfo.iGameExpValue - 8 + tableItem.pPlayers[r]->userNodeInfo.iRDGameExpValue) <= 0) {
                            tableItem.pPlayers[r]->userNodeInfo.iRDGameExpValue = -tableItem.pPlayers[r]->userNodeInfo.iGameExpValue;
                        }
                        else {
                            tableItem.pPlayers[r]->userNodeInfo.iRDGameExpValue -= 8;
                        }
                        msgGameRtServer.iExtraNum[r] -= 8;
                    }
                    else {
                        msgGameRtServer.cDisconnectType[r] = 1;
                    }
                }
            }
            tableItem->m_cLastLevel[i % 2] = tableItem->m_cLevel;
        }
    }
    msgGameRtServer.cLevel[0] = tableItem->m_cLastLevel[0];
    msgGameRtServer.cLevel[1] = tableItem->m_cLastLevel[1];
    if (m_iMode == 2 || (m_iMode == 3 && tableItem->m_cMode == 2)) {
        msgGameRtServer.cLevel[0] = 2;   
        msgGameRtServer.cLevel[1] = 2;   
    }

    int baseTime = GetBaseTime(tableItem);
    for (int i = 0; i < TABLE_PLAYER_NUM; ++i) {
        if (tableItem.pPlayers[i] != NULL) {

            if(tableItem.pPlayers[i]->userNodeInfo.cDisconnectType != 0 || m_iMode == 2 || ( m_iMode == 3 && tableItem->m_cMode == 2)) {
                for (int k = 0; k < TABLE_PLAYER_NUM; ++k) {
                    tableItem.pPlayers[k]->iOverNum = 0;
                }
                if (tableItem.pPlayers[i]->userNodeInfo.cDisconnectType != 0) {
                    tableItem->m_cBanker = -1;
                }
                tableItem->m_cLevel = 2;
                tableItem.m_cLastLevel[0] = 2;
                tableItem.m_cLastLevel[1] = 2;
                tableItem->m_cTribute[0] = 0;
                tableItem->m_cTribute[1] = 0;
                tableItem->m_cTribute[2] = 0;
                tableItem->m_cTribute[3] = 0;
                tableItem->m_cReturnTribute[0] = 0;
                tableItem->m_cReturnTribute[1] = 0;
                tableItem->m_cReturnTribute[2] = 0;
                tableItem->m_cReturnTribute[3] = 0;
                break;
            }
        }
    }

    for(int i = 0; i < TABLE_PLAYER_NUM; ++i) {
        if (tableItem.pPlayers[i]) {
#ifdef _NEW_RATE_
            // 2016-04-29 全部扣桌费 , 还回锁住的预扣费, 交由系统扣
            tableItem.pPlayers[i]->userNodeInfo.iRDGameRoyalty += baseTime;
            tableItem.pPlayers[i]->userNodeInfo.iFirstMoney += baseTime;
#endif

            __log(_DEBUG, "Function[%s]", "i[%d]UserID[%d]TableExtraNum[%d],iPrizeNum[%d] iRDGameRoyalty[%d] fristmoney[%lld] basetime[%d] GameRet[%d=%d-%d]", 
                  __FUNCTION__, i, tableItem.pPlayers[i]->userNodeInfo.iUserID, tableItem.pPlayers[i]->userNodeInfo.usTableNumExtra, msgGameRtServer.iPrizeTicket[i],
                tableItem.pPlayers[i]->userNodeInfo.iRDGameRoyalty,
                tableItem.pPlayers[i]->userNodeInfo.iFirstMoney, baseTime,
                msgGameRtServer.iAmountResult[i], msgGameRtServer.iMoneyResult[i],
                msgGameRtServer.iTableMoney );

            SendMsgToClient(tableItem.pPlayers[i],GAME_RESULT_SERVER_MSG,&msgGameRtServer,sizeof(GameResultServerDef));

            if ( NeedBroad(msgGameRtServer.iMoneyResult[i]) ) {
                    char msg[512] = {0};
                    int len = snprintf(msg, sizeof(msg), "恭喜玩家[%s]在掼蛋游戏中单局获得[%ld]积分，真乃神人也!",
                                    tableItem.pPlayers[i]->szNickName[0] ? tableItem.pPlayers[i]->szNickName : tableItem.pPlayers[i]->szUserName, msgGameRtServer.iMoneyResult[i]);
                    if (0 < len)  {
                            if (true == SendBroadcastMsgToDispatchServer(tableItem.pPlayers[i], msg, len+1, msgGameRtServer.iMoneyResult[i]))  {
                                    __log(_DEBUG, SERVER_NAME, "Function[%s]line[%d] UserID[%d] afterTax[%d] Send broadcast success", 
                                                    __FUNCTION__, __LINE__, tableItem.pPlayers[i]->userNodeInfo.iUserID,msgGameRtServer.iMoneyResult[i]);
                                    tagDSBroadcastGameSvrMSG msgInfo;
                                    memset(&msgInfo, 0, sizeof(tagDSBroadcastGameSvrMSG));
                                    msgInfo.nMessageType = 1;
                                    msgInfo.iscore = msgGameRtServer.iMoneyResult[i];
                                    msgInfo.nGameID = m_pServerBaseInfo[0].iGameID;
                                    msgInfo.nServerID = m_pServerBaseInfo[0].iServerID;
                                    msgInfo.nMsgID = 0;
                                    msgInfo.nUserID = 0;
                                    memcpy(msgInfo.szMsgContext, msg, sizeof(msgInfo.szMsgContext));
                                    msgInfo.utime = time(NULL);
                                    OnSendMsgToInGameClient(m_pServerBaseInfo[0].iRoomID, &msgInfo, LOBBYCLIENT_GAMESERVER_BROADCAST_MSG, sizeof(msgInfo));
                            } 
                            else 
                            {
                                    __log(_ERROR, SERVER_NAME, "Function[%s]line[%d] UserID[%d] afterTax[%d]Send broadcast fail",
                                                    __FUNCTION__, __LINE__, tableItem.pPlayers[i]->userNodeInfo.iUserID,msgGameRtServer.iMoneyResult[i]);
                            }
                    }
            }
            GameAuthenGDPointUpdate update;
            update.userid = tableItem.pPlayers[i]->userNodeInfo.iUserID;
            update.gdPoint = tableItem.pPlayers[i]->userTaskInfo.iTaskID;
            update.inmatch = tableItem.pPlayers[i]->IsInMatch() ? 1 : 0;
            GameProvider::SendData(m_pRadiusServerGate, GAME_AUTHEN_GD_UPDATE_USER_GDPOINT_REQ ,&update, sizeof(GameAuthenGDPointUpdate));
        }
    }
   
    // 2016-06-15 增加对局次数
    ++tableItem.m_continueGameCount;

    CallBackGameEndStatus(&tableItem);

    for (int m = 0 ; m < 4 ; ++m) {
        if (tableItem.pPlayers[m]) {
            tableItem.pPlayers[m]->userNodeInfo.iGameMultiple = 0;
            tableItem.pPlayers[m]->userTaskInfo.cTableNum = 0;
        }    
    }

    // 2016-06-15 连雪对局超过一定次数，解散桌子，且一段时间内不能在同一桌
    if( m_iMode != 3 && tableItem.m_continueGameCount >= m_maxGameCount )
    {
        // 设置非机器人指定时间内不能匹配到一起
        int userids[TABLE_PLAYER_NUM] = {0};
        int n = 0;
        for(int i = 0; i < TABLE_PLAYER_NUM; ++i) {
            if (tableItem.pPlayers[i] && !tableItem.pFactoryPlayers[i]->bRobotLogin) {
                userids[n++] = tableItem.pFactoryPlayers[i]->userNodeInfo.iUserID;
            }
        }
		if (m_pAntiCheatControl) m_pAntiCheatControl->AddPlayTogetherPlayer(userids, n, m_cannotPlaySec);

        // 解散桌子，机器人断线
        for(int i = 0; i < TABLE_PLAYER_NUM; ++i) {
            if (tableItem.pPlayers[i]) {
#if DEBUG
                __log(_DEBUG, "Function[%s]", "玩家[%d][%d]需要离桌[%d]", __FUNCTION__, 
                    tableItem.pFactoryPlayers[i]->userNodeInfo.iUserID, 
                    tableItem.pFactoryPlayers[i]->bRobotLogin?1:0, 
                    tableItem.pFactoryPlayers[i]->userNodeInfo.iTableNum);
#endif

                if (tableItem.pFactoryPlayers[i]->bRobotLogin) {
                    // 机器人踢出游戏服
                    Close(tableItem.pFactoryPlayers[i]);
                } else {
                    // 玩家踢出桌子
                    FactoryPlayerNodeDef *pPlayerNode = tableItem.pFactoryPlayers[i];
                    int userid = pPlayerNode->userNodeInfo.iUserID;

                    TablePlayerLeaveDef leaveMsg;
                    leaveMsg.cLeaveType = TPL_READY_LEAVE_FOR_CHANGE;
                    leaveMsg.iUserID = userid;
                    __log(_DEBUG, "Function[%s]", "玩家[%d]离桌[%d] 准备重新入桌", __FUNCTION__, leaveMsg.iUserID, tableItem.pFactoryPlayers[i]->userNodeInfo.iTableNum);
                    HandelTableLeaveReq(tableItem.pFactoryPlayers[i],(void *)&leaveMsg,sizeof(leaveMsg));

                    // 玩家重新入桌
                    SitDownReqDef sitMsg = {0};
                    sitMsg.iBindUserID = userid;
                    sitMsg.iTableNum = 0;
                    HandleSitDownReq(pPlayerNode, (void*)&sitMsg, sizeof(sitMsg));
                }
            }
        }
    }
}

//对于非法出牌的处理
void FLGameLogic::HandleCheatSendCard(PlayerNodeDef * pPlayerNode,unsigned char cKickOutType) {
    if(!pPlayerNode) {
        return;
    }
    TableItemDef *pTableItem = (TableItemDef *)GetTableItem(pPlayerNode->userNodeInfo.iRoomNum - 1, pPlayerNode->userNodeInfo.iTableNum - 1);
    if (pTableItem) {
        __log(_ERROR,"Function[%s]","UserName[%s]UserID[%d]GameStatus[%d]iCurTablePlayer[%d]node[0x%08x]", 
              __FUNCTION__, pPlayerNode->szUserName,pPlayerNode->userNodeInfo.iUserID,pPlayerNode->userNodeInfo.iGameStatus, pTableItem->iCurTablePlayer, pPlayerNode);
        if (pPlayerNode->userNodeInfo.cDisconnectType == 0) { //如果已经掉线了，不再close
            KickOutPlayServer(pPlayerNode,cKickOutType);             //游戏中非法出牌 
            Close(pPlayerNode);
        }
    }
    else {
        __log(_ERROR, "Function[%s]UserID[%d]table is null", __FUNCTION__, pPlayerNode->userNodeInfo.iUserID);
    }
    ForceHandleGame(pPlayerNode);
}

void FLGameLogic::OneDayTimer() {
    for (int i = 0; i < MAX_ROOM_NUM; ++i) {
        for (int j = 0; j < MAX_TABLE_NUM; ++j) {
            TableItemDef *pTableItem = (TableItemDef *)GetTableItem(i, j);
            
            for (int k = 0; k < TABLE_PLAYER_NUM; ++k) {
                if (pTableItem && pTableItem->pPlayers[k]) {
                    pTableItem->pPlayers[k]->m_iPrizeCountPerDay = 0;
                }
            }
        }
    }
}

//获取牌的大小值
int FLGameLogic::GetLogicCard(char card, char trumps) {
    if (1 == card) {
        return 14;
    }
    else if(card == trumps) {
        return 16;
    }
    else if (14 == card) {
        return 18;
    }
    else if (15 == card) {
        return 19;
    }
    return card;
}

//发送进贡还贡的广播
void FLGameLogic::SendTributeNotice(TableItemDef &tableItem, const unsigned short &usTableNumExtra, const char &cTribute) {
    ReturnTributeServerDef RTserver;
    RTserver.cLocal = tableItem->pPlayers[0]->userNodeInfo.iGameStatus;
    RTserver.cTribute =  cTribute;
    RTserver.usTableNumExtra = usTableNumExtra;
    __log(_ALL, "Function[%s]", "游戏状态[%d]usTableNumExtra[%d]进贡/还贡牌[%0x]", __FUNCTION__, RTserver.cLocal, usTableNumExtra, cTribute);
    for (int n = 0; n < TABLE_PLAYER_NUM; ++n) {
        SendMsgToClient(tableItem.pPlayers[n], RETURN_TRIBUTE_REQ_MSG, &RTserver, sizeof(ReturnTributeServerDef));
    }
}

//发送得到进贡和还贡的牌的通知
void FLGameLogic::SendTributeReturn(PlayerNodeDef *pPlayerNode, const unsigned short &usTableNumExtra, const char &cTribute, const int &messageType) {
    __log(_ALL, "Function[%s]", "UserID[%d]usTableNumExtra[%d]得到进贡/还贡牌[%0x]", __FUNCTION__, pPlayerNode->userNodeInfo.iUserID, usTableNumExtra, cTribute);
    pPlayerNode->cCards[MAX_HAND_CARDS_NUM - 1] = cTribute;
    ReturnDef RT;
    RT.cTribute =  cTribute;
    RT.usTableNumExtra = usTableNumExtra;
    SendMsgToClient(pPlayerNode, messageType, &RT, sizeof(ReturnDef));
}

//接收到进贡牌的消息的处理
void FLGameLogic::HandleTributeCard(PlayerNodeDef *pPlayerNode,void *pMsgData,long length) {

    TableItemDef *tableItem = (TableItemDef *)GetTableItem(pPlayerNode->userNodeInfo.iRoomNum - 1, pPlayerNode->userNodeInfo.iTableNum - 1);
    char iTableNumExtra = pPlayerNode->userNodeInfo.usTableNumExtra;
    TributeOKServerDef *msgTC = (TributeOKServerDef*)pMsgData;

    for (int m = 0; m < 4; ++m) {
        if (tableItem == NULL || tableItem->pPlayers[m] == NULL) {
            __log(_DEBUG,"Function[%s]","玩家节点为空 —— 玩家所在位置[%d]", __FUNCTION__, m);
            return;
        }
    }

    //不是进贡环节
    if (pPlayerNode->userNodeInfo.iGameStatus != GAME_WAIT_TRIBUTE) {
        __log(_ERROR, "Function[%s]", "roomNum[%d]tableNum[%d]当前不是进贡环节poker[%d]UserID[%d]TableNumExtra[%d]GivePlayer1[%d]GivePlayer2[%d]", __FUNCTION__, pPlayerNode->userNodeInfo.iRoomNum, pPlayerNode->userNodeInfo.iTableNum, msgTC->cTribute, pPlayerNode->userNodeInfo.iUserID, iTableNumExtra, tableItem->cGivePlayer1, tableItem->cGivePlayer2);
        HandleCheatSendCard(pPlayerNode, ILLege_SEND_CARD);
        return;
    } 
    //不该他进贡
    if (tableItem->cGivePlayer1 != iTableNumExtra && tableItem->cGivePlayer2 != iTableNumExtra) {
        __log(_ERROR, "Function[%s]", "roomNum[%d]tableNum[%d]poker[%d]进贡玩家UserID[%d]TableNumExtra[%d]不需要进贡GivePlayer1[%d]GivePlayer2[%d]", __FUNCTION__, pPlayerNode->userNodeInfo.iRoomNum, pPlayerNode->userNodeInfo.iTableNum, msgTC->cTribute, pPlayerNode->userNodeInfo.iUserID, iTableNumExtra, tableItem->cGivePlayer1, tableItem->cGivePlayer2);
        HandleCheatSendCard(pPlayerNode, ILLege_SEND_CARD);
        return;
    }
    //进贡牌为主牌
    if (msgTC->cTribute == (tableItem->m_cLevel | (2 << 4) )){
        __log(_ERROR, "Function[%s]", "roomNum[%d]tableNum[%d]进贡牌为主牌[%d]UserID[%d]TableNumExtra[%d]GivePlayer1[%d]GivePlayer2[%d]", __FUNCTION__, pPlayerNode->userNodeInfo.iRoomNum, pPlayerNode->userNodeInfo.iTableNum, msgTC->cTribute, pPlayerNode->userNodeInfo.iUserID, iTableNumExtra, tableItem->cGivePlayer1, tableItem->cGivePlayer2);
        HandleCheatSendCard(pPlayerNode, ILLege_SEND_CARD);
        return;
    }
    bool exit = false;
    for(int j = 0; j < MAX_HAND_CARDS_NUM; ++j) {
        //减去进贡的牌
        if(pPlayerNode->cCards[j] == msgTC->cTribute) {
            pPlayerNode->cCards[j] = 0;
            exit = true;
            break;
        }
    }
    //进贡牌不存在
    if (!exit) {
        __log(_ERROR, "Function[%s]", "roomNum[%d]tableNum[%d]进贡牌不存在[%d]UserID[%d]TableNumExtra[%d]GivePlayer1[%d]GivePlayer2[%d]", __FUNCTION__, pPlayerNode->userNodeInfo.iRoomNum, pPlayerNode->userNodeInfo.iTableNum, msgTC->cTribute, pPlayerNode->userNodeInfo.iUserID, iTableNumExtra, tableItem->cGivePlayer1, tableItem->cGivePlayer2);
        HandleCheatSendCard(pPlayerNode, ILLege_SEND_CARD);
        return;
    }

    tableItem->m_cTribute[iTableNumExtra] = msgTC->cTribute;                    //记录进贡的牌
    UnLockTimeOut(pPlayerNode);
    __log(_DEBUG, "Function[%s]", "玩家[%s]进贡的牌[%0x]UserID[%d]TableNumExtra[%d]GivePlayer1[%d]GivePlayer2[%d]giveNum[%d]",__FUNCTION__, pPlayerNode->szNickName, msgTC->cTribute, pPlayerNode->userNodeInfo.iUserID, iTableNumExtra, tableItem->cGivePlayer1, tableItem->cGivePlayer2, tableItem->m_iGiveNum);

    //某玩家进贡后就不需要再进贡了
    if (tableItem->cGivePlayer1 == iTableNumExtra) {
        tableItem->cGivePlayer1 = -1;
        --tableItem->m_iGiveNum;
    }

    if (tableItem->cGivePlayer2 == iTableNumExtra) {
        tableItem->cGivePlayer2 = -1;
        --tableItem->m_iGiveNum;
    }

    if (tableItem->m_iGiveNum == 0) {
        for (int m = 0; m < TABLE_PLAYER_NUM; ++m) {
            tableItem->pPlayers[m]->userNodeInfo.iGameStatus = GAME_WAIT_RETURNTRIBUTE;  
        }
    }
    SendTributeNotice(*tableItem, iTableNumExtra, msgTC->cTribute);
 
    //单贡
    if (tableItem->bTribute) {
        for (int m = 0; m < TABLE_PLAYER_NUM; ++m) {
            if (tableItem->pPlayers[m]->iOverNum == 1) {
                SendTributeReturn(tableItem->pPlayers[m], iTableNumExtra, msgTC->cTribute, RETURN_SERVER_MSG);
                LockTimeOut(tableItem->pPlayers[m], 40);
                break;
            }
        }
    }
    else { //双贡
        if (tableItem->m_iGiveNum == 0) { //玩家进贡完了
            int maxCardIndex = 0;
            int maxCard = 0;
            int minCardIndex = 0;
            int minCard = 20; //最大牌
            for (int i = 0; i < TABLE_PLAYER_NUM; ++i) {
                if ((tableItem->m_cTribute[i]!= 0) && (GetLogicCard(tableItem->m_cTribute[i] & 0x0F, tableItem->m_cLevel) >= maxCard)) {
                    maxCard = tableItem->m_cTribute[i];
                    maxCardIndex = tableItem->pPlayers[i]->userNodeInfo.usTableNumExtra;
                }
                if ((tableItem->m_cTribute[i]!= 0) && (GetLogicCard(tableItem->m_cTribute[i] & 0x0F, tableItem->m_cLevel) < minCard)) {
                    minCard = tableItem->m_cTribute[i];
                    minCardIndex = tableItem->pPlayers[i]->userNodeInfo.usTableNumExtra;
                }
                __log(_ALL, "Function[%s]", "roomNum[%d]tableNum[%d]进贡玩家UserID[%d]位置[%d]cTributeCard[%0x]", __FUNCTION__, pPlayerNode->userNodeInfo.iRoomNum, 
                      pPlayerNode->userNodeInfo.iTableNum, tableItem->pPlayers[i]->userNodeInfo.iUserID, tableItem->pPlayers[i]->userNodeInfo.usTableNumExtra, 
                      tableItem->m_cTribute[i]);
            }
            //大牌给头游玩家，小牌给二游玩家    
            for (int m = 0; m < 4 ; ++m) {
                if (tableItem->pPlayers[m]->iOverNum == 1)  {
                    SendTributeReturn(tableItem->pPlayers[m], maxCardIndex, tableItem->m_cTribute[maxCardIndex], RETURN_SERVER_MSG);
                    LockTimeOut(tableItem->pPlayers[m], 40); 
                }
                else if (tableItem->pPlayers[m]->iOverNum == 2) {
                    SendTributeReturn(tableItem->pPlayers[m], minCardIndex, tableItem->m_cTribute[minCardIndex], RETURN_SERVER_MSG);
                    LockTimeOut(tableItem->pPlayers[m], 40);
                }
            }
        }
    }
}

//接收到还贡牌消息的处理
void FLGameLogic::HandleTributeReturnCard(PlayerNodeDef *pPlayerNode,void *pMsgData,long length) {
    int  iRoomID   = pPlayerNode->userNodeInfo.iRoomNum;
    int  iTableNum = pPlayerNode->userNodeInfo.iTableNum;
    char iTableNumExtra = pPlayerNode->userNodeInfo.usTableNumExtra;
    TableItemDef *tableItem = (TableItemDef *)GetTableItem(iRoomID - 1, iTableNum - 1);
     
    for (int m = 0 ; m < TABLE_PLAYER_NUM; ++m) {
        if (tableItem == NULL || tableItem->pPlayers[m] == NULL) {
            __log(_DEBUG,"Function[%s]","玩家节点为空 —— 玩家所在位置[%d]", __FUNCTION__, m);
            return;
        }
    }

    TributeOKServerDef *msgTC = (TributeOKServerDef*)pMsgData;
 
    //还贡时, 大于10、主牌不许作为还贡牌, 不考虑全部牌都大于10的情况
    if (((msgTC->cTribute & 0x0F) == 1) || ((msgTC->cTribute & 0x0F) > 10) || ((msgTC->cTribute & 0xF0) == tableItem->m_cLevel)) {
        __log(_ERROR, "Function[%s]", "userid[%d]还的牌太大或为主牌[%d]", __FUNCTION__, pPlayerNode->userNodeInfo.iUserID, msgTC->cTribute);
        HandleCheatSendCard(pPlayerNode, ILLege_SEND_CARD);
        return;
    }
    //不是还贡环节
    if (pPlayerNode->userNodeInfo.iGameStatus != GAME_WAIT_RETURNTRIBUTE) {
        __log(_ERROR, "Function[%s]", "roomNum[%d]tableNum[%d]当前不是还贡环节poker[%d]TableNumExtra[%d]SendPlayer1[%d]SendPlayer2[%d]", __FUNCTION__, pPlayerNode->userNodeInfo.iRoomNum, pPlayerNode->userNodeInfo.iTableNum, msgTC->cTribute, iTableNumExtra, tableItem->cSendPlayer1, tableItem->cSendPlayer2);
        HandleCheatSendCard(pPlayerNode, ILLege_SEND_CARD);
        return;
    } 
    //不该他还贡
    if (tableItem->cSendPlayer1 != iTableNumExtra && tableItem->cSendPlayer2 != iTableNumExtra) {
        __log(_ERROR, "Function[%s]", "roomNum[%d]tableNum[%d]poker[%d]还贡玩家TableNumExtra[%d]不需要还贡SendPlayer1[%d]SendPlayer2[%d]", __FUNCTION__, pPlayerNode->userNodeInfo.iRoomNum, pPlayerNode->userNodeInfo.iTableNum, msgTC->cTribute, iTableNumExtra, tableItem->cSendPlayer1, tableItem->cSendPlayer2);
        HandleCheatSendCard(pPlayerNode, ILLege_SEND_CARD);
        return;
    }
    bool exit = false;
    for(int j = 0; j < MAX_HAND_CARDS_NUM; ++j) {
        //减去还贡的牌
        if(pPlayerNode->cCards[j] == msgTC->cTribute) {
            pPlayerNode->cCards[j] = 0;
            exit = true;
            break;
        }
    }
    //还贡牌不存在
    if (!exit) {
        __log(_ERROR, "Function[%s]", "roomNum[%d]tableNum[%d]还贡牌不存在[%0x]TableNumExtra[%d]GivePlayer1[%d]GivePlayer2[%d]", __FUNCTION__, pPlayerNode->userNodeInfo.iRoomNum, pPlayerNode->userNodeInfo.iTableNum, msgTC->cTribute, iTableNumExtra, tableItem->cGivePlayer1, tableItem->cGivePlayer2);
        HandleCheatSendCard(pPlayerNode, ILLege_SEND_CARD);
        return;
    }

    tableItem->m_cReturnTribute[iTableNumExtra] = msgTC->cTribute;
    __log(_DEBUG, "Function[%s]", "玩家[%s]UserID[%d]还贡的牌[%0x]bTribute[%d]", __FUNCTION__, pPlayerNode->szNickName, pPlayerNode->userNodeInfo.iUserID, msgTC->cTribute, tableItem->bTribute);

    if (tableItem->cSendPlayer1 == iTableNumExtra) {
        tableItem->cSendPlayer1 = -1;
        --tableItem->m_iReturnNum;
    }
    else if (tableItem->cSendPlayer2 == iTableNumExtra) {
        tableItem->cSendPlayer2 = -1;
        --tableItem->m_iReturnNum;
    }

    UnLockTimeOut(pPlayerNode);
    SendTributeNotice(*tableItem, iTableNumExtra, msgTC->cTribute);
 
    if (tableItem->bTribute) { //单贡，末游出牌
        for (int m = 0; m < 4; ++m) {
            if (tableItem->pPlayers[m]->iOverNum == 4) {
                SendTributeReturn(tableItem->pPlayers[m], iTableNumExtra, msgTC->cTribute, RETURN_CARD_SERVER_MSG);
                tableItem->iCurSendPlayer = tableItem->pPlayers[m]->userNodeInfo.usTableNumExtra;
                tableItem->iCurTablePlayer = tableItem->pPlayers[m]->userNodeInfo.usTableNumExtra;
            }
        }
        PrepareSendCard(*tableItem);
    }
    else { //双贡
        if (tableItem->m_iReturnNum == 0) { //还贡完了
            int maxCardIndex = 0;
            int maxCard = 0;
            int minCardIndex = 0;
            int minCard = 20; //最大牌
            for (int i = 0; i < TABLE_PLAYER_NUM; ++i) {
                //找到进贡牌的大小，iOverNum=1的还进大贡的人，iOverNum=2的还进小贡的人
                if ((tableItem->m_cTribute[i] != 0) && (GetLogicCard(tableItem->m_cTribute[i] & 0x0F, tableItem->m_cLevel) >= maxCard)) {
                    maxCard = tableItem->m_cTribute[i];
                    maxCardIndex = tableItem->pPlayers[i]->userNodeInfo.usTableNumExtra;
                }
                if ((tableItem->m_cTribute[i] != 0) && (GetLogicCard(tableItem->m_cTribute[i] & 0x0F, tableItem->m_cLevel) < minCard)) {
                    minCard = tableItem->m_cTribute[i];
                    minCardIndex = tableItem->pPlayers[i]->userNodeInfo.usTableNumExtra;
                }
                __log(_ALL, "Function[%s]", "roomNum[%d]tableNum[%d]还贡玩家[%d]位置[%d]cTributeCard[%0x]", __FUNCTION__, pPlayerNode->userNodeInfo.iRoomNum, 
                      pPlayerNode->userNodeInfo.iTableNum, tableItem->pPlayers[i]->userNodeInfo.iUserID, tableItem->pPlayers[i]->userNodeInfo.usTableNumExtra, 
                      tableItem->m_cReturnTribute[i]);
            }

            int lastFirstTableNumExtra = 0; //上局的头游 
            for (int m = 0; m < 4; ++m) {
                if (tableItem->pPlayers[m]->iOverNum == 1) {
                    lastFirstTableNumExtra = tableItem->pPlayers[m]->userNodeInfo.usTableNumExtra;
                    SendTributeReturn(tableItem->pPlayers[maxCardIndex], tableItem->pPlayers[m]->userNodeInfo.usTableNumExtra, 
                                      tableItem->m_cReturnTribute[tableItem->pPlayers[m]->userNodeInfo.usTableNumExtra], RETURN_CARD_SERVER_MSG);
                }
                if (tableItem->pPlayers[m]->iOverNum == 2) {
                    SendTributeReturn(tableItem->pPlayers[minCardIndex], tableItem->pPlayers[m]->userNodeInfo.usTableNumExtra,
                                      tableItem->m_cReturnTribute[tableItem->pPlayers[m]->userNodeInfo.usTableNumExtra], RETURN_CARD_SERVER_MSG);
                }
            }
            //进贡牌一样大，头游上家先出牌
            if (GetLogicCard(minCard,tableItem->m_cLevel) == GetLogicCard(maxCard, tableItem->m_cLevel)) {
                tableItem->iCurSendPlayer = (lastFirstTableNumExtra + 3) % TABLE_PLAYER_NUM;
                tableItem->iCurTablePlayer = (lastFirstTableNumExtra + 3) % TABLE_PLAYER_NUM;
            }
            else { //否则进贡牌大的出牌
                tableItem->iCurSendPlayer = maxCardIndex;
                tableItem->iCurTablePlayer = maxCardIndex;
            }
            PrepareSendCard(*tableItem);
        }
    }
}

//玩家坐下成功的回调
void FLGameLogic::CallBackSitDownSuccess(FactoryPlayerNodeDef *pPlayerNode) {
    GameLogic::CallBackSitDownSuccess(pPlayerNode);
    PlayerNodeDef *nodeTemp = (PlayerNodeDef *)pPlayerNode;
    if (nodeTemp == NULL) {
        return;
    }
    int  iRoomID   = nodeTemp->userNodeInfo.iRoomNum;
    int  iTableNum = nodeTemp->userNodeInfo.iTableNum;
    char iTableNumExtra = nodeTemp->userNodeInfo.usTableNumExtra;
    TableItemDef *tableItem = (TableItemDef *)GetTableItem(iRoomID - 1, iTableNum - 1);

    if (tableItem == NULL) {   
        return;
    } 

    for (int i = 0; i < 4; i++) {
        if (tableItem && tableItem->pPlayers[i]) {
            __log(_DEBUG,"Function[%s]"," TableNumExtra[%d]Name[%s]Over[%d]",
                  __FUNCTION__, tableItem->pPlayers[i]->userNodeInfo.usTableNumExtra,tableItem->pPlayers[i]->szNickName ,tableItem->pPlayers[i]->iOverNum);
        }
    }
    GameExpLevelNoticeDef GelNotice;
    if (nodeTemp->gamePropInfo.usPropPlayerB5 == 0) {
        nodeTemp->gamePropInfo.usPropPlayerB5 = 1;
    }
    GelNotice.ExpLevel = nodeTemp->gamePropInfo.usPropPlayerB5;//已领取奖励的最高等级

    int baseTime = 0; 
    if (m_iMode == 3) { 
        baseTime = tableItem->GetTableAnte();
    }    
    else {
        baseTime = m_pServerBaseInfo[0].iBaseTime;
    }    
    GelNotice.iAnte = baseTime;

    SendMsgToClient(nodeTemp,GAME_EXP_NOTICE,&GelNotice,sizeof(GameExpLevelNoticeDef));

    if (m_iMode == 3) { //有人入桌，广播给没有游戏的玩家
        GD::GDPrivateNumExtraSitDownNtf notice;
        notice.tableID = tableItem->nCustomTableID;
        notice.numExtra = iTableNumExtra;
        notice.avatarID = nodeTemp->userNodeInfo.iMobileSpecialAvatar;
        notice.nickName = nodeTemp->szNickName;
        vector<unsigned long> vecPlayer;
        m_pServerBaseInfo[0].m_mapPlayerList.GetPlayerList(0, vecPlayer);
        for (size_t i = 0; i < vecPlayer.size(); ++i) {
            FactoryPlayerNodeDef *pTmpPlayer = (FactoryPlayerNodeDef *)vecPlayer[i];
            if (pTmpPlayer && pTmpPlayer->userNodeInfo.iGameStatus <= GAME_HAVE_SIT) {   
                SendXmlMsg((PlayerNodeDef*)pTmpPlayer, GD::GD_PRIVATE_NUM_EXTRA_SITDOWN_NTF, notice);
            }
        }
    }
   
    // 2016-06-15 连续对局次数归0
    tableItem->m_continueGameCount = 0;
}

//这里是处理拆桌,回调清理桌中玩家节点
void FLGameLogic::CallBackTablePlayerLeave(FactoryPlayerNodeDef *pPlayerNode, FactoryTableItemDef *pTableItem, int iTableNumExtra) {

    TableItemDef *pTable = (TableItemDef*)pTableItem;
    __log(_ERROR, "Function[%s]", "UserID[%d]zNickName[%s]iHowmuchPlayer[%d]", 
          __FUNCTION__, pPlayerNode->userNodeInfo.iUserID, pPlayerNode->szNickName, pTable->iHowmuchPlayer);

    for (int i = 0 ; i < 4 ; ++i) {
        if (pTable->pPlayers[i]) {
            pTable->pPlayers[i]->iOverNum = 0;
            pTable->bOtherTrust[i] = 0;
        }
    }

    if(pTable->pPlayers[iTableNumExtra]) {
        pTable->pPlayers[iTableNumExtra] = NULL;
    }
    
 
    bool allRobot = true;
    for(int i = 0; i < TABLE_PLAYER_NUM; ++i) {
        if(pTable->pFactoryPlayers[i] && (!pTable->pFactoryPlayers[i]->bRobotLogin)) {
            allRobot = false;
            break;
        }
    }

    if (allRobot && (pPlayerNode->userNodeInfo.iGameStatus > GAME_WAIT_END || pPlayerNode->userNodeInfo.iGameStatus < GAME_WAIE_DEAL)) {
        for(int i = 0; i < TABLE_PLAYER_NUM; ++i) {
            if(pTable->pFactoryPlayers[i] && pTable->pFactoryPlayers[i]->bRobotLogin) {
                HandleCheatSendCard ((PlayerNodeDef*)pTable->pFactoryPlayers[i], ROOM_HAVE_CLOSED);
            }
        }
    }
    if (m_iMode == 3) { //有人离开，广播给没有游戏的玩家
        GD::GDPrivateNumExtraLeaveNtf notice;
        notice.tableID = pTable->nCustomTableID;
        notice.numExtra = iTableNumExtra;
        vector<unsigned long> vecPlayer;
        m_pServerBaseInfo[0].m_mapPlayerList.GetPlayerList(0, vecPlayer);
        for (size_t i = 0; i < vecPlayer.size(); ++i) {
            FactoryPlayerNodeDef *pTmpPlayer = (FactoryPlayerNodeDef *)vecPlayer[i];
            if (pTmpPlayer && pTmpPlayer->userNodeInfo.iGameStatus <= GAME_HAVE_SIT) {   
                SendXmlMsg((PlayerNodeDef*)pTmpPlayer, GD::GD_PRIVATE_NUM_EXTRA_LEAVE_NTF, notice);
            }
        }
    }
    pTable->ResetTableSendCard(); 
    pTable->ResetTableCalculate();
    pTable->ResetTableTribute(); 
    pTable->ResetTableLevel();
   
    // 2016-06-15
    pTable->m_continueGameCount = 0;
}

//接收到托管消息的处理
void FLGameLogic::HandleTrust(PlayerNodeDef *pPlayerNode,void *pMsgData,long length) {
    int  iRoomID   = pPlayerNode->userNodeInfo.iRoomNum;
    int  iTableNum = pPlayerNode->userNodeInfo.iTableNum;
    char iTableNumExtra = pPlayerNode->userNodeInfo.usTableNumExtra;
    TableItemDef *tableItem = (TableItemDef *)GetTableItem(iRoomID - 1, iTableNum - 1);
    if( !tableItem )
    {
        __log(_ERROR, "Function[%s]", "UserID[%d]", __FUNCTION__, pPlayerNode->userNodeInfo.iUserID );
        return;
    }
         
    CUserTrust *msgTrust = (CUserTrust*)pMsgData;
    tableItem->bOtherTrust[iTableNumExtra] = msgTrust->bTrust;

    CUserTrust msgTrustNotice = {0};
    msgTrustNotice.nTrustUser = iTableNumExtra;
    msgTrustNotice.bTrust = msgTrust->bTrust; 
    for (int i = 0 ; i < TABLE_PLAYER_NUM; ++i) {
        if (tableItem && tableItem->pPlayers[i]) {
            SendMsgToClient(tableItem->pPlayers[i],MSG_S_TRUST,&msgTrustNotice,sizeof(CUserTrust));
        }
    }
    __log(_ALL, "Function[%s]", "UserID[%d]托管[%d]", __FUNCTION__, pPlayerNode->userNodeInfo.iUserID, msgTrust->bTrust);
}

//对结算的异常的处理
void FLGameLogic::IsTableValidateResult(FactoryTableItemDef *pTableItem) {
    for (int m = 0 ; m < 4 ; ++m) {
        if ( pTableItem->pFactoryPlayers[m] == NULL) {
            __log(_DEBUG,"Function[%s]","玩家节点为空, 玩家所在位置[%d]", __FUNCTION__,  m);
            return;
        }
    }

    for (int i = 0; i < 4 ; ++i) {
        __log(_ALL,"IsTableValidateResult::llRDMoney[%d,%d]...", __FUNCTION__, i, pTableItem->pFactoryPlayers[i]->userNodeInfo.llRDMoney);
        if (pTableItem->pFactoryPlayers[i]->userNodeInfo.llRDMoney > 1000000100) {
            pTableItem->pFactoryPlayers[i]->userNodeInfo.llRDMoney = 0;
            __log(_ERROR, "FLGameLogic", "IsTableValidateResult: 玩家[%s]结算异常",pTableItem->pFactoryPlayers[i]->szNickName);
            char cData[512]; 
            memset(cData,0,sizeof(cData));
            sprintf(cData, "【%s】在【掼蛋】游戏【结算阶段】金币异常", pTableItem->pFactoryPlayers[i]->szUserName);
        }
    }
}

//交换玩家，可以让非机器人玩家尽量到一桌
void FLGameLogic::SwapTableForRealPlayer() {
#if DEBUG
    __log(_ALL, "Function[%s]","", __FUNCTION__);    
#endif

    for (int i = 0; i < MAX_ROOM_NUM; ++i) {
        vector<FactoryPlayerNodeDef *> vcPlayer;
        for (int j = 0; j < MAX_TABLE_NUM; ++j) {
            FactoryTableItemDef *pTableItem = GetTableItem(i, j);
            if (pTableItem == NULL || pTableItem->iHaveReadyNum >= TABLE_PLAYER_NUM) {
                continue;
            }

            for (int k = 0; k < TABLE_PLAYER_NUM; ++k) {
                if (pTableItem->pFactoryPlayers[k]) {
                    if (!pTableItem->pFactoryPlayers[k]->bRobotLogin) {
                        vcPlayer.push_back(pTableItem->pFactoryPlayers[k]);
                    }
                }
            }    
        }
        
        int iSize = vcPlayer.size();
#if DEBUG
         __log(_ALL, "Function[%s]", "size[%d]", __FUNCTION__, iSize);
#endif

        if (iSize <= 1) {
            break;
        }
        
        for (int j = 0; j < iSize;) {
            FactoryPlayerNodeDef * pPlayerNode = vcPlayer[j];
            int iRoomID = pPlayerNode->userNodeInfo.iRoomNum;
            int iTableID = pPlayerNode->userNodeInfo.iTableNum;
            FactoryTableItemDef *pTableItem = GetTableItem(iRoomID - 1, iTableID - 1);

            int iIdleNum = GetTableIdleNum(pTableItem);
            __log(_ALL, "Function[%s]", "j[%d]IdleNum[%d]", __FUNCTION__, j, iIdleNum);

            if (iIdleNum <= 0) {
                break;
            }
            for (int k = j + 1; k <= j + iIdleNum && k < iSize; ++k) {
                FactoryPlayerNodeDef * pPlayerNodeTmp = vcPlayer[k];
                //玩家原来就在该桌子上
                if (iRoomID == pPlayerNodeTmp->userNodeInfo.iRoomNum && iTableID == pPlayerNodeTmp->userNodeInfo.iTableNum) {
                    continue;
                }
                TablePlayerLeaveDef leaveMsg;
                leaveMsg.cLeaveType = TPL_READY_LEAVE_FOR_CHANGE;
                leaveMsg.iUserID = pPlayerNodeTmp->userNodeInfo.iUserID;
                HandelTableLeaveReq(pPlayerNodeTmp,(void *)&leaveMsg,sizeof(leaveMsg));
                __log(_ALL, "Function[%s]", "玩家[%d]已离桌[%d]", __FUNCTION__, leaveMsg.iUserID, pPlayerNodeTmp->userNodeInfo.iTableNum);

                SitDownReqDef sitMsg;
                sitMsg.iBindUserID = pPlayerNodeTmp->userNodeInfo.iUserID;
                sitMsg.iTableNum = iTableID;
                sitMsg.userPointLimit.iMinPoint = 0;
                sitMsg.userPointLimit.iMaxPoint = 0;
                int iPos = GetTableIdlePos(pTableItem);
                if (iPos == -1)
                {
                    j = k;
                    break;
                }
                sitMsg.usTableNumExtra = iPos;
                HandleSitDownReq(pPlayerNodeTmp,(void *)&sitMsg,sizeof(sitMsg));
                __log(_ALL, "Function[%s]", "玩家[%d]换桌[%d]成功", __FUNCTION__, leaveMsg.iUserID, pPlayerNodeTmp->userNodeInfo.iTableNum);
            }
            j += iIdleNum + 1;
        }
    }
}

//踢掉机器人，返回机器人所在位置
int FLGameLogic::GetTableIdlePos( FactoryTableItemDef *pTableItem ) {
    if (NULL == pTableItem) {
        return -1;
    }
    __log(_ALL, "Function[%s]", "", __FUNCTION__);
 
    for (int i = 0; i < TABLE_PLAYER_NUM; ++i) {
        if (pTableItem->pFactoryPlayers[i] && pTableItem->pFactoryPlayers[i]->bRobotLogin == true) {
            TablePlayerLeaveDef leaveMsg;  //剔除机器人
            leaveMsg.cLeaveType = TPL_GAME_ReSULT_LEAVE;
            leaveMsg.iUserID = pTableItem->pFactoryPlayers[i]->userNodeInfo.iUserID;
            __log(_DEBUG, "GetTableIdlePos", "自动换桌踢掉机器人！");
            HandelTableLeaveReq(pTableItem->pFactoryPlayers[i],(void *)&leaveMsg,sizeof(leaveMsg));
        }
        if (NULL == pTableItem->pFactoryPlayers[i]) {
            return i;
        }
    }
    return -1;
}

//获取非机器人玩家数量
int FLGameLogic::GetTableIdleNum (FactoryTableItemDef *pTableItem) {
    if (NULL == pTableItem) {
        return -1;
    }
    int iNum = 0;
    for (int i = 0; i < TABLE_PLAYER_NUM; ++i) {
        if (pTableItem->pFactoryPlayers[i] && pTableItem->pFactoryPlayers[i]->bRobotLogin == false) {
            ++iNum;
        }
    }
    return TABLE_PLAYER_NUM - iNum;
}

int FLGameLogic::GetRandIndex() {
    int index = 0;
    int iRand = rand() % 100;
    if(iRand >= 95 && iRand < 100) {
        index = 2;
    }
    else if (iRand >= 75 && iRand < 95) {
        index = 1;
    }
    else {
        index = 0;
    }
    return index;
}

//更新比赛数据到比赛服务
void FLGameLogic::UpdateMatchDataToMatchServer(FactoryTableItemDef *pTableItem) {
    // 注意这里是更新了玩家的比赛积分
    // 在比赛服务器上会检查比赛和游戏服的匹配关系，
    // 只有真的匹配才会去参与排名并更新到db中,
    // 这里只是一个内存值

    map<int, servermatch::GameMatchUpdateUserRankExReq> mapUpdateMatchData;
    for (int i = 0; i < m_pAllServerBaseInfo.iPlayerNum; ++i) {
        if (!pTableItem->pFactoryPlayers[i]) {
            continue;
        }    
        if (IsOpenMatch() && pTableItem->pFactoryPlayers[i]->IsInMatch()) {
            if (mapUpdateMatchData.find(pTableItem->pFactoryPlayers[i]->m_matchId) == mapUpdateMatchData.end()) {
                mapUpdateMatchData[pTableItem->pFactoryPlayers[i]->m_matchId].id = pTableItem->pFactoryPlayers[i]->m_matchId;
            }    
            pbproto::GameRankUserInfo rankUserInfo;
            pTableItem->pFactoryPlayers[i]->m_matchScore += pTableItem->pFactoryPlayers[i]->m_getMatchScore;
            if (pTableItem->pFactoryPlayers[i]->m_matchScore < 0) { //总分不能为负
                pTableItem->pFactoryPlayers[i]->m_matchScore = 0;
                if (pTableItem->pFactoryPlayers[i]->m_getMatchScore < 0) { //总分为0了，获取的分数改为0,否则排名的时候计算老的分数会有问题
                    pTableItem->pFactoryPlayers[i]->SetMatchScoreFromGame(0);
                }
            }
            rankUserInfo.userid = pTableItem->pFactoryPlayers[i]->userNodeInfo.iUserID;
            rankUserInfo.nickname = pTableItem->pFactoryPlayers[i]->szNickName;
            rankUserInfo.getScore = pTableItem->pFactoryPlayers[i]->m_getMatchScore;
            rankUserInfo.totalScore = pTableItem->pFactoryPlayers[i]->m_matchScore;
            rankUserInfo.oldrank = 0; 
            rankUserInfo.newrank = 0; 
            rankUserInfo.fromserver = m_pServerBaseInfo[0].iServerID;
            mapUpdateMatchData[pTableItem->pFactoryPlayers[i]->m_matchId].users.push_back(rankUserInfo);
            __log(_DEBUG, "Function[%s]", "update match progress. user[%d] match[%llu]getScore[%d]totalScore[%d]fromserver[%d]",
                  __FUNCTION__,rankUserInfo.userid, pTableItem->pFactoryPlayers[i]->m_matchId, rankUserInfo.getScore, rankUserInfo.totalScore,rankUserInfo.fromserver);
        }    
    }    
    for (map<int, servermatch::GameMatchUpdateUserRankExReq>::iterator iter =  mapUpdateMatchData.begin(); iter != mapUpdateMatchData.end(); ++iter) {
        SendMsgToMatch(servermatch::GAME_MATCH_UPDATE_USER_RANK_EX_REQ, iter->second);
    } 
}

//私人场加入请求的处理
void  FLGameLogic::HandlePhoneJoinTableReq(IPlayerNode *lpPlayerNode, void *pMsg, unsigned short Len) {
    FactoryPlayerNodeDef * pPlayerNode = (FactoryPlayerNodeDef *)lpPlayerNode;

    if (pPlayerNode == NULL) {
        MsgHeader * msg = (MsgHeader *)pMsg;
        __log(_ERROR, "Function[%s]:", "pPlayerNode = NULL...Message Type = %08x", __FUNCTION__,  msg->type);
        return;
    }

    char *msg = (char*)pMsg + sizeof(MsgHeader); 
    int length = Len - sizeof(MsgHeader);   
    GD::GDJoinPrivateTableReq joinPrivateTableReq;
    if (joinPrivateTableReq.ParseFromArray ((const char *)msg, length) != length) {    
        __log(_ERROR, "Function[%s]", "length is not match !length[%d], node[0x%08x] UserID[%d]",
             __FUNCTION__, length, pPlayerNode, pPlayerNode->userNodeInfo.iUserID);
        return;
    }    

    int iRoomID = pPlayerNode->userNodeInfo.iRoomNum;    //begin from 1
    if (iRoomID<=0 || iRoomID>MAX_ROOM_NUM) {
        __log(_ERROR, "Function[%s]:", "房间索引非法: iUserID[%s], iRoomID[%d]", __FUNCTION__, pPlayerNode->userNodeInfo.iUserID, iRoomID);
        return;
    }

    //如果不是手机端的手动开桌房间
    if (m_pServerBaseInfo[iRoomID-1].cSitDownType != SITDOWN_TYPE_PLAYER_CHOOSE || 
        pPlayerNode->cLoginType != LOGIN_TYPE_PHONE_CLIENT) {
        __log(_ERROR, "Function[%s]:", "不是手机端的手动开桌房间iUserID[%d]SitDownType[%d]LoginType[%d]", 
              __FUNCTION__, pPlayerNode->userNodeInfo.iUserID, m_pServerBaseInfo[iRoomID-1].cSitDownType, pPlayerNode->cLoginType);
        return;
    }

    if(pPlayerNode->userNodeInfo.iGameStatus > GAME_WAIT_DESK) {
        __log(_ERROR, "Function[%s]:", "状态大于GAME_WAIT_DESK, iUserID[%d]iGameStatus[%d]",
              __FUNCTION__, pPlayerNode->userNodeInfo.iUserID, pPlayerNode->userNodeInfo.iGameStatus);
        return;
    }

    SitDownResDef  msgSitDownRes;
    memset(&msgSitDownRes, 0, sizeof(SitDownResDef));

    GD::GDJoinPrivateTableRsp joinPrivateRsp;

    FactoryTableItemDef *pTableItem = NULL;
    
    // 1、确定座位
    bool bFoundTable = false;
    int usTableNumExtra = -1;
    int iTableNum = -1;
    for (int i = 0; i < m_pAllServerBaseInfo.iMaxTableNum; ++i) {

        pTableItem = GetTableItem(iRoomID - 1, i);
        if (!pTableItem) continue;  

        if (joinPrivateTableReq.tableID == 0) { //快速加入
            if (!pTableItem->bTableLock && pTableItem->nCustomTableID != INVALID_TABLE_ID) {
                bool bFind = false;
                for (int j = 0; j < TABLE_PLAYER_NUM; ++j) {
                    if (m_iMode == 3 && pTableItem->pFactoryPlayers[j] == NULL) {
                        iTableNum = i + 1;
                        usTableNumExtra = j;
                        bFoundTable = true;
                        bFind = true;
                        break;
                    }
                }
                if (bFind) {
                    break;
                }
            }
        }
        else if (pTableItem->nCustomTableID == joinPrivateTableReq.tableID) {
            bFoundTable = true;
            iTableNum = i + 1;
            bool bFind = false;
            for (int j = 0; j < TABLE_PLAYER_NUM; ++j) {
                if (m_iMode == 3 && ((TableItemDef*)pTableItem)->m_cMode == 1) {
                    if(pTableItem->pFactoryPlayers[joinPrivateTableReq.tableNumExtra] == NULL) {
                        usTableNumExtra = joinPrivateTableReq.tableNumExtra;
                        bFind = true;
                        break;
                    }
                    else {
                        msgSitDownRes.cError = SDR_TABLENUMEXTRA_ERROR;
                        goto done;
                    }
                } 
                else if (m_iMode == 3 && ((TableItemDef*)pTableItem)->m_cMode == 2) { //团团转不能选位置
                    if(pTableItem->pFactoryPlayers[j] == NULL) {
                        usTableNumExtra = j;
                        bFind = true;
                        break;
                    }
                }
            }
            if (!bFind) {
                msgSitDownRes.cError = SDR_TABLENUMEXTRA_ERROR;
                goto done;
            }
            break;
        }
    }
    if (!bFoundTable) {
        msgSitDownRes.cError = SDR_TABLENUM_ERROR;
        goto done;
    }

    // 2、验证桌子密码
    if (pTableItem->bTableLock) {
        if (strcmp(joinPrivateTableReq.tablePasswd.c_str(), pTableItem->szTablePasswd) != 0) {
            msgSitDownRes.cError = SDR_TABLE_PASSWD_ERROR;
            goto done;
        }
    }

    // 3、验证身上积分是否满足底注
    if (pTableItem->GetTableAnte() * m_pServerBaseInfo[iRoomID - 1].iBaseTime > pPlayerNode->userNodeInfo.iFirstMoney) {
        msgSitDownRes.cError = SDR_TABLE_NO_ENOUGH_MONEY;
        goto done;
    }
done:
    //4、错误返回
    if (msgSitDownRes.cError != 0) {
        SendMsgToClient(pPlayerNode, SITDOWN_RES_MSG, &msgSitDownRes, sizeof(SitDownResDef));
    }    
    else {
    // 5、处理坐下
        joinPrivateRsp.mode = ((TableItemDef*)pTableItem)->m_cMode;
        joinPrivateRsp.tableID = pTableItem->nCustomTableID;
        joinPrivateRsp.tableName = pTableItem->szTableNickName;
        SendXmlMsg((PlayerNodeDef*)pPlayerNode, GD::GD_JOIN_PRIVATE_TABLE_RSP, joinPrivateRsp);

        pPlayerNode->userNodeInfo.iTableNum = iTableNum;
        pPlayerNode->userNodeInfo.usTableNumExtra = usTableNumExtra;
        DealSitDownReq(pTableItem, pPlayerNode, NULL, MSG_CHANGE_SITDOWN);  
    }
    __log(_DEBUG, "Function[%s]:", "TableID[%d]usTableNumExtra[%d]ErrorNum[%d]UserID[%d]reqPasswd[%s]", __FUNCTION__, 
          joinPrivateTableReq.tableID, joinPrivateTableReq.tableNumExtra, msgSitDownRes.cError, pPlayerNode->userNodeInfo.iUserID, joinPrivateTableReq.tablePasswd.c_str());
    return;
}

//私人场创建桌子的处理
void FLGameLogic::HandlePhoneCreateNewTableReq(IPlayerNode *lpPlayerNode,void *pMsg, unsigned short Len) {

    FactoryPlayerNodeDef * pPlayerNode = (FactoryPlayerNodeDef *)lpPlayerNode;
    if (pPlayerNode == NULL) {
        MsgHeader * msg = (MsgHeader *)pMsg;
        __log(_ERROR, "Function[%s]:", "pPlayerNode = NULL...Message Type = %08x", __FUNCTION__,  msg->type);
        return;
    }

    char *msg = (char*)pMsg + sizeof(MsgHeader); 
    int length = Len - sizeof(MsgHeader);   
    GD::GDCreateNewPrivateTableReq createPrivateNewTableReq;
    if (createPrivateNewTableReq.ParseFromArray ((const char *)msg, length) != length) {    
        __log(_ERROR, "Function[%s]", "length is not match !length[%d], node[0x%08x] UserID[%d]",
             __FUNCTION__, length, pPlayerNode, pPlayerNode->userNodeInfo.iUserID);
        return;
    }    

    int iRoomID = pPlayerNode->userNodeInfo.iRoomNum;    //begin from 1
    if (iRoomID<=0 || iRoomID>MAX_ROOM_NUM) {
        __log(_ERROR, "Function[%s]:", "房间索引非法: iUserID[%s], iRoomID[%d]", __FUNCTION__, pPlayerNode->userNodeInfo.iUserID, iRoomID);
        return;
    }

    //如果不是手机端的手动开桌房间
    if (m_pServerBaseInfo[iRoomID-1].cSitDownType != SITDOWN_TYPE_PLAYER_CHOOSE || 
        pPlayerNode->cLoginType != LOGIN_TYPE_PHONE_CLIENT) {
        __log(_ERROR, "Function[%s]:", "不是手机端的手动开桌房间iUserID[%d]SitDownType[%d]LoginType[%d]", 
              __FUNCTION__, pPlayerNode->userNodeInfo.iUserID, m_pServerBaseInfo[iRoomID-1].cSitDownType, pPlayerNode->cLoginType);
        return;
    }

    if(pPlayerNode->userNodeInfo.iGameStatus > GAME_WAIT_DESK) {
        __log(_ERROR, "Function[%s]:", "状态大于GAME_WAIT_DESK, iUserID[%s]iGameStatus[%d]",
              __FUNCTION__, pPlayerNode->userNodeInfo.iUserID, pPlayerNode->userNodeInfo.iGameStatus);
        return;
    }

    GD::GDCreateNewPrivateTableRsp CreateNewTableRsp;
    CreateNewTableRsp.errorCode = 0;
    int nBaseAnte = m_pServerBaseInfo[iRoomID - 1].iBasePoint;
    int nBaseMultiple = m_pServerBaseInfo[iRoomID - 1].iBaseTime;
    FactoryTableItemDef *pTableItem = NULL;
    int iLastTableNum = pPlayerNode->userNodeInfo.iLastTableID;
    int iTableNum      = -1;
    int iTableNumExtra = -1;

    if (nBaseAnte * nBaseMultiple > pPlayerNode->userNodeInfo.iFirstMoney) {
        CreateNewTableRsp.errorCode = GD::ERR_PRIVATE_NOT_ENOUGH_MONEY;
        goto done;
    }

    if (createPrivateNewTableReq.basePoint < nBaseAnte) {
        CreateNewTableRsp.errorCode = GD::ERR_PRIVATE_TOO_LITTLE_ANTE;
        goto done;
    }

    if (createPrivateNewTableReq.basePoint * nBaseMultiple > pPlayerNode->userNodeInfo.iFirstMoney) {
        CreateNewTableRsp.errorCode = GD::ERR_PRIVATE_TOO_LARGE_ANTE;
        goto done;
    }
    if (createPrivateNewTableReq.mode != 1 && createPrivateNewTableReq.mode != 2) {
        CreateNewTableRsp.errorCode = GD::ERR_PRIVATE_UNKNOW_MODE; //模式不对
        goto done;
    }
 
    //分配一个空桌子
    for (int i = 0; i < m_pAllServerBaseInfo.iMaxTableNum; ++i) {

        pTableItem = GetTableItem(iRoomID - 1, i);
        if (!pTableItem) continue;  

        //不能坐在上次位置 
        if (iLastTableNum == i + 1) continue;
        if (true == GetDifferentTable(pTableItem, pPlayerNode)) continue; 
        if (pTableItem->iHowmuchPlayer == 0) {
            ((TableItemDef*)pTableItem)->m_cMode = createPrivateNewTableReq.mode;
            iTableNum      = i + 1;
            iTableNumExtra = 0;
            break;
        }
    }

    //查找桌子失败
    if (-1 == iTableNum) {
        CreateNewTableRsp.errorCode = GD::ERR_PRIVATE_NO_TABLE;
        goto done;
    }
    //桌子名重复
    for (int i = 0; i < m_pAllServerBaseInfo.iMaxTableNum; ++i) {

       TableItemDef *pTmpTableItem = (TableItemDef*) GetTableItem(iRoomID - 1, i);
        if (!pTmpTableItem) continue;  
        if (string(pTmpTableItem->szTableNickName) == createPrivateNewTableReq.tableName) {
            CreateNewTableRsp.errorCode = GD::ERR_PRIVATE_TABLENAME_EXIST;
            goto done;
        }
    }

done:
    if (CreateNewTableRsp.errorCode == 0) {
        pPlayerNode->userNodeInfo.iTableNum       = iTableNum;
        pPlayerNode->userNodeInfo.usTableNumExtra = iTableNumExtra;
        {
            //生成6位数的TableID
            if (m_iTableID <= 0 || m_iTableID > 1000000) {
                 m_iTableID = (m_pServerBaseInfo[iRoomID - 1].iGameID * 1000) % 1000000;
            }
            ++m_iTableID;
            pTableItem->nCustomTableID = m_iTableID;
            pTableItem->iHostUserID = pPlayerNode->userNodeInfo.iUserID;
            pTableItem->nCustomTableAnte = createPrivateNewTableReq.basePoint;
            memcpy(pTableItem->szTableNickName, createPrivateNewTableReq.tableName.c_str(), sizeof(pTableItem->szTableNickName));
            CreateNewTableRsp.tableID = pTableItem->nCustomTableID;
            if (!createPrivateNewTableReq.tablePasswd.empty()) {
                pTableItem->bTableLock = true;
                memcpy(pTableItem->szTablePasswd, createPrivateNewTableReq.tablePasswd.c_str(), sizeof(pTableItem->szTablePasswd));
            }
            else {
                pTableItem->bTableLock = false;
            }
            __log(_DEBUG, "Function[%s]", "UserID[%d]HostUserID[%d]UserName[%s]nCustomTableID[%d]iRoomID[%d]iTableNum[%d]nCustomTableAnte[%d]iTableNumExtra[%d]", 
                  __FUNCTION__,pPlayerNode->userNodeInfo.iUserID, pTableItem->iHostUserID, pPlayerNode->szUserName,pTableItem->nCustomTableID, iRoomID, iTableNum, 
                  pTableItem->nCustomTableAnte, iTableNumExtra);
        }

        DealSitDownReq(pTableItem, pPlayerNode, NULL, MSG_CHANGE_SITDOWN);
        if (!pPlayerNode->bHaveSitDown) {
            __log(_ERROR, "Function[%s]", "UserID[%d] sitdown failed", __FUNCTION__, pPlayerNode->userNodeInfo.iUserID);
        }
    }
    __log(_DEBUG, "Function[%s]:", "UserID[%d]ErrorNum[%d]mode[%d]tableNickName[%s]TablePasswd[%s]tableBasePoint[%d]FirstMoney[%lld]iBasePoint[%d]iBaseTime[%d]", 
          __FUNCTION__,pPlayerNode->userNodeInfo.iUserID, CreateNewTableRsp.errorCode,createPrivateNewTableReq.mode,  createPrivateNewTableReq.tableName.c_str(), 
          createPrivateNewTableReq.tablePasswd.c_str(), createPrivateNewTableReq.basePoint, pPlayerNode->userNodeInfo.iFirstMoney, 
          m_pServerBaseInfo[iRoomID - 1].iBasePoint,  m_pServerBaseInfo[iRoomID - 1].iBaseTime);

    SendXmlMsg((PlayerNodeDef*)pPlayerNode, GD::GD_CREATE_NEW_PRIVATE_TABLE_RSP, CreateNewTableRsp);
    return;
}

//私人场获取桌子列表的处理
void FLGameLogic::HandleGetPrivateTableListReq(IPlayerNode *lpPlayerNode,void *pMsg, unsigned short Len) {

    FactoryPlayerNodeDef * pPlayerNode = (FactoryPlayerNodeDef *)lpPlayerNode;
    if (NULL == pPlayerNode) return;
    int iRoomID = pPlayerNode->userNodeInfo.iRoomNum;
    if (iRoomID<=0 || iRoomID>MAX_ROOM_NUM) return;

    //如果不是手机端的手动开桌房间
    if (m_pServerBaseInfo[iRoomID-1].cSitDownType != SITDOWN_TYPE_PLAYER_CHOOSE || pPlayerNode->cLoginType != LOGIN_TYPE_PHONE_CLIENT) {    
        __log(_ERROR, "Function[%s]", "不是手机端的手动开桌房间: SitDownType[%d], LoginType[%d]", 
              __FUNCTION__, m_pServerBaseInfo[iRoomID-1].cSitDownType, pPlayerNode->cLoginType);
        return;
    }    
    char *msg = (char*)pMsg + sizeof(MsgHeader); 
    int length = Len - sizeof(MsgHeader);   
    GD::GDGetPrivateTableListReq getPrivateTableListReq;
    if (getPrivateTableListReq.ParseFromArray ((const char *)msg, length) != length) {    
        __log(_ERROR, "Function[%s]", "length is not match !length[%d], node[0x%08x] UserID[%d]",
             __FUNCTION__, length, pPlayerNode, pPlayerNode->userNodeInfo.iUserID);
        return;
    }    

    GD::GDGetPrivateTableListRsp rsp;
    int count = 0;
    for(int i = 0; i < m_pAllServerBaseInfo.iMaxTableNum; ++i)  {    
        TableItemDef *pTableItem = (TableItemDef*) GetTableItem(iRoomID - 1, i);
        if (NULL == pTableItem) continue;
        if (INVALID_TABLE_ID == pTableItem->nCustomTableID) continue;
        if (pTableItem->nCustomTableID <= getPrivateTableListReq.tableID) continue;
        if (count > getPrivateTableListReq.count) {
            break;
        } 
        ++count;
        GD::GDPrivateTableInfoItem tableInfo;
        tableInfo.mode = pTableItem->m_cMode;
        tableInfo.needPasswd = pTableItem->bTableLock;
        tableInfo.tableName = pTableItem->szTableNickName;
        tableInfo.basePoint = pTableItem->GetTableAnte();
        tableInfo.tableID  = pTableItem->nCustomTableID;
        __log(_ALL, "Function[%s]", "table mode[%d]needPasswd[%d]tableName[%s]basePoint[%d]tableID[%d]", 
              __FUNCTION__, tableInfo.mode, tableInfo.needPasswd, tableInfo.tableName.c_str(), tableInfo.basePoint, tableInfo.tableID);
        for (int j = 0; j < TABLE_PLAYER_NUM; ++j) {
            if (pTableItem->pFactoryPlayers[j] != NULL) {
                GD::GDPrivateUserInfoOnTable userInfoOnTable;
                userInfoOnTable.tableNumExtra = pTableItem->pFactoryPlayers[j]->userNodeInfo.usTableNumExtra;
                userInfoOnTable.avatarID = pTableItem->pFactoryPlayers[j]->userNodeInfo.iMobileSpecialAvatar;
                userInfoOnTable.nickName = pTableItem->pFactoryPlayers[j]->szNickName;
                tableInfo.vecUsers.push_back(userInfoOnTable);
            }
        }
        rsp.vecTables.push_back(tableInfo);
    }
    SendXmlMsg((PlayerNodeDef*)pPlayerNode, GD::GD_GET_PRIVATE_TABLE_LIST_RSP, rsp);
    __log(_DEBUG, "Function[%s]", "tablesize[%d]", __FUNCTION__, rsp.vecTables.size());
}

//私人场根据关键词查询桌子的处理
void FLGameLogic::HandleGetPrivateTableByKeyReq(IPlayerNode *lpPlayerNode,void *pMsg, unsigned short Len) {

    FactoryPlayerNodeDef * pPlayerNode = (FactoryPlayerNodeDef *)lpPlayerNode;
    if (NULL == pPlayerNode) return;
    int iRoomID = pPlayerNode->userNodeInfo.iRoomNum;
    if (iRoomID<=0 || iRoomID>MAX_ROOM_NUM) return;

    //如果不是手机端的手动开桌房间
    if (m_pServerBaseInfo[iRoomID-1].cSitDownType != SITDOWN_TYPE_PLAYER_CHOOSE || pPlayerNode->cLoginType != LOGIN_TYPE_PHONE_CLIENT) {    
        __log(_ERROR, "Function[%s]", "不是手机端的手动开桌房间: SitDownType[%d], LoginType[%d]", 
              __FUNCTION__, m_pServerBaseInfo[iRoomID-1].cSitDownType, pPlayerNode->cLoginType);
        return;
    }    

    char *msg = (char*)pMsg + sizeof(MsgHeader); 
    int length = Len - sizeof(MsgHeader);   
    GD::GDGetPrivateTableByKeyReq getPrivateTableByKeyReq;
    if (getPrivateTableByKeyReq.ParseFromArray ((const char *)msg, length) != length) {    
        __log(_ERROR, "Function[%s]", "length is not match !length[%d], node[0x%08x] UserID[%d]",
             __FUNCTION__, length, pPlayerNode, pPlayerNode->userNodeInfo.iUserID);
        return;
    }    
    __log(_DEBUG, "Function[%s]", "UserID[%d]key[%s]", __FUNCTION__, pPlayerNode->userNodeInfo.iUserID, getPrivateTableByKeyReq.key.c_str());
    GD::GDGetPrivateTableByKeyRsp rsp;
    rsp.errorCode = 0;
    for(int i = 0; i < m_pAllServerBaseInfo.iMaxTableNum; ++i)  {    

        TableItemDef *pTableItem = (TableItemDef*)GetTableItem(iRoomID - 1, i);

        if (NULL == pTableItem) 
            continue;
        if (INVALID_TABLE_ID == pTableItem->nCustomTableID) 
            continue;

        string tableNickName = pTableItem->szTableNickName;
        bool found = false;
        if (pTableItem->nCustomTableID == atoi(getPrivateTableByKeyReq.key.c_str()) ||
            tableNickName.find(getPrivateTableByKeyReq.key) != string::npos) {
            found = true;
        }
        if (!found) {
            for (int j = 0 ; j < TABLE_PLAYER_NUM; ++j) {
                if (pTableItem->pPlayers[j] && pTableItem->pPlayers[j]->userNodeInfo.iUserID == pTableItem->iHostUserID ) {
                    string nickName = pTableItem->pPlayers[j]->szNickName;
                    if (nickName.find(getPrivateTableByKeyReq.key) != string::npos) {
                        found = true;
                    }
                    else {
                        break;
                    }
                } 
            }
        }
        if (!found) {
            continue;
        }
        GD::GDPrivateTableInfoItem  table;
        table.mode = ((TableItemDef*)pTableItem)->m_cMode;
        table.needPasswd = pTableItem->bTableLock;
        table.tableName = pTableItem->szTableNickName;
        table.basePoint = pTableItem->GetTableAnte();
        table.tableID = pTableItem->nCustomTableID;
        for (int j = 0; j < TABLE_PLAYER_NUM; ++j) {
            if (pTableItem->pFactoryPlayers[j] != NULL) {
                GD::GDPrivateUserInfoOnTable userInfoOnTable;
                userInfoOnTable.tableNumExtra = pTableItem->pFactoryPlayers[j]->userNodeInfo.usTableNumExtra;
                userInfoOnTable.avatarID = pTableItem->pFactoryPlayers[j]->userNodeInfo.iMobileSpecialAvatar;
                userInfoOnTable.nickName = pTableItem->pFactoryPlayers[j]->szNickName;
                table.vecUsers.push_back(userInfoOnTable);
            }
        }
        rsp.vecTables.push_back(table);
    }
    SendXmlMsg((PlayerNodeDef*)pPlayerNode, GD::GD_GET_PRIVATE_TABLE_BYKEY_RSP, rsp);
    __log(_DEBUG, "Function[%s]", "tablesize[%d]", __FUNCTION__, rsp.vecTables.size());
}
 
//发送xml协议的消息
void FLGameLogic::SendXmlMsg(PlayerNodeDef *pPlayerNode, unsigned short type, const IMessage &msg) {
    if (!pPlayerNode) {
        return;
    }

    const int MAX_PACKET_LEN = 4096;

    int headsz = sizeof(MsgHeader);
    char buf[MAX_PACKET_LEN] = {0};
    int msglen = msg.SerializeToArray( buf+headsz, MAX_PACKET_LEN-headsz );
    if (msglen == IMessage::E_Error) {
        __log(_ERROR, "Function[%s]", "SendMsg failed. SerializeToArray Packet failed.UserID[%d] chair[%d] Disconnect[%d] type[%d]",
              __FUNCTION__, pPlayerNode->userNodeInfo.iUserID, pPlayerNode->userNodeInfo.usTableNumExtra, pPlayerNode->userNodeInfo.cDisconnectType, type);
        return;
    }

    int sz = msglen + headsz;
    if (sz > MAX_PACKET_LEN) {
        __log(_ERROR, "Function[%s]", "SendMsg failed. Packet too large.UserID[%d] chair[%d] Disconnect[%d] type[%d] sz[%d]",
              __FUNCTION__, pPlayerNode->userNodeInfo.iUserID, pPlayerNode->userNodeInfo.usTableNumExtra, pPlayerNode->userNodeInfo.cDisconnectType, type, sz );
        return;
    }

    if(!SendMsgToClient(pPlayerNode, type, buf, sz)) {
        __log( _ERROR, "Function[%s]", "SendMsg failed. SendData error. UserID[%d] chair[%d] Disconnect[%d] type[%d] sz[%d]",
              __FUNCTION__, pPlayerNode->userNodeInfo.iUserID, pPlayerNode->userNodeInfo.usTableNumExtra, pPlayerNode->userNodeInfo.cDisconnectType, type, sz );
    }
    return;
}

int FLGameLogic::GetBaseTime(TableItemDef &tableItem)
{
    int baseTime = 0;
    if (m_iMode == 3) {
        baseTime = tableItem.GetTableAnte();
    }
    else {
        baseTime = m_pServerBaseInfo[0].iBaseTime;
    }
    return baseTime;
}

int FLGameLogic::GetUsePropLimit(FactoryPlayerNodeDef* pPlayerNode)
{
    int iRoomID = pPlayerNode->userNodeInfo.iRoomNum;
    int tableid = pPlayerNode->userNodeInfo.iTableNum;
    TableItemDef *table = (TableItemDef*)GetTableItem( iRoomID-1, tableid-1 );
    if( !table )
    {
        return 0x1FFFFFFF;
    }

    int limit1 = m_pServerBaseInfo[0].iLimitCoin;
    int limit2 = GetBaseTime(*table) * 2;
    return limit1 > limit2 ? limit1 : limit2;
}

bool FLGameLogic::CallBackClientReadyReq(FactoryTableItemDef &table,FactoryPlayerNodeDef &player)
{
    int iRoomID   = player.userNodeInfo.iRoomNum;
    int iTableNum = player.userNodeInfo.iTableNum;
    int iTableNumExtra = player.userNodeInfo.usTableNumExtra;

    if(player.userNodeInfo.iFirstMoney < m_pServerBaseInfo[iRoomID -1].iLimitCoin)
    {
        __log(_ERROR, "GameLogic::HandlelClientReadyReq:", "pPlayerNode[%d] Money is [%ld],no enough!", player.userNodeInfo.iUserID, player.userNodeInfo.iFirstMoney);
        KickOutPlayServer(&player,NO_ENOUGH_MONEY_IN_GAME);
        return false;
    }

    // 私人场特殊判断
    if( m_iMode == 3 )
    {
        TableItemDef *ptable = (TableItemDef*)&table;
        int basetime = GetBaseTime(*ptable); 
        if(player.userNodeInfo.iFirstMoney < basetime * 50 )
        {
            __log(_ERROR, "GameLogic::HandlelClientReadyReq:", "pPlayerNode[%d] Money is [%ld],no enough 2. need= %d*50", 
                player.userNodeInfo.iUserID, player.userNodeInfo.iFirstMoney, basetime );
            KickOutPlayServer(&player,NO_ENOUGH_MONEY_IN_GAME);
            return false;
        }
    }
    return true;
}

