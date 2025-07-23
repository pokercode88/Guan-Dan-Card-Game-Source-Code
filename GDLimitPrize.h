#ifndef __ACTIVITY_GUAN_DAN_H__
#define __ACTIVITY_GUAN_DAN_H__

#include "ActivitySystem.h"
#include "FactoryGameLogic.h"
#include "FactoryPlayerNode.h"
#include "FactoryTableItem.h"
#include "Odao_Client_Msg.h"
#include "GameLogic.h"

/************************************************************************/
/*
							惯蛋奖品券限制2015092601 进度表字段使用说明
lpUserActivityProgress

iGameCoin:                    赠送奖品券数量
iCoinTicket：              
iActivityTicket:           
iLotteryTicket:	           
iPrizeTicket:			   
iProgress1:				   
iProgress2:			       
iUpdateTime:               

iGameCoin:                 每天获得奖品券数量
iPrizeTicket:              11, 概率
iPrizeTicketUpperLimit:    12, 概率
iCoinTicket:               13, 概率
iCoinTicketUpperLimit:     21, 概率
iActivityTicket:		   22, 概率
iActivityTicketUpperLimit: 23, 概率
LotteryTicket:             31, 概率                                                   
LotteryTicketUpperLimit:   32, 概率 
iCondition1:               33, 概率                                                   
iCondition2:                
iConditon3:                 
iConditon4:                 
iConditon5：                

*/
/************************************************************************/

//HEADER;
 


class CGdPrizeLimit : public IActivityCallback
{
public:
    CGdPrizeLimit(FLGameLogic *pServer)
	{
		m_lpGameLogicSink = pServer;
		m_bOpen = FALSE;
	}

    //检查文件开关
    virtual void CheckOpen()
    {
		if (m_fileConifg.InitFile("GuanDanActivity.cfg"))
			m_bOpen = m_fileConifg.GetValueInt("activity", "limitprize", 0);
		else
			m_bOpen = FALSE;
    }

    //活动是否开启
    virtual BOOL IsOpen()
    {
        return m_bOpen;
    }

    //返回接口需要处理的活动类型
    virtual long GetActivityTypeID()
    {
        return 20150926;
    }
    
	//处理消息包
	virtual void OnActivityMessage(long lType, void *lpBuffer, long length,
		FactoryPlayerNodeDef *lpServerUserItem, FactoryTableItemDef *lpTableFrame,
		const DBSiteActivityConfig* const lpSiteActivityConfig,
		UserActivityProgress *lpUserActivityProgress, BOOL &bUpdate)		
	{
		if (FALSE == m_bOpen) return;
		if (2015092601 != lpUserActivityProgress->iActivityID) return;

		if (ADD_PRIZE != lType) return;

		tm			lastTime = *localtime((time_t *)&lpUserActivityProgress->iUpdateTime);
		time_t		nowTime = time(NULL);
		tm			*ptime = localtime(&nowTime);

		if (lastTime.tm_yday != ptime->tm_yday)
		{
			lpUserActivityProgress->iGameCoin = 0;
			lpUserActivityProgress->iUpdateTime = (int)nowTime;
			bUpdate = TRUE;
		}
		if (lpUserActivityProgress->iGameCoin >= lpSiteActivityConfig->iGameCoin) return;	//达到上限
		int					iPrizeNum = 0, iAddPoint = 0, iNumTp = lpSiteActivityConfig->iGameCoin - lpUserActivityProgress->iGameCoin;

		iAddPoint = random() % 100;
		if (1 == length)
		{
			if (iAddPoint < lpSiteActivityConfig->iPrizeTicket)
				iPrizeNum = 1;
			else if (iAddPoint < lpSiteActivityConfig->iPrizeTicket + lpSiteActivityConfig->iPrizeTicketUpperLimit)
				iPrizeNum = 2;
			else if (iAddPoint < lpSiteActivityConfig->iPrizeTicket + lpSiteActivityConfig->iPrizeTicketUpperLimit + lpSiteActivityConfig->iCoinTicket)
				iPrizeNum = 3;
		}
		else if (2 == length)
		{
			if (iAddPoint < lpSiteActivityConfig->iCoinTicketUpperLimit)
				iPrizeNum = 3;
			else if (iAddPoint < lpSiteActivityConfig->iCoinTicketUpperLimit + lpSiteActivityConfig->iActivityTicket)
				iPrizeNum = 4;
			else if (iAddPoint < lpSiteActivityConfig->iCoinTicketUpperLimit + lpSiteActivityConfig->iActivityTicket + lpSiteActivityConfig->iActivityTicketUpperLimit)
				iPrizeNum = 5;
		}
		else if (3 == length)
		{
			if (iAddPoint < lpSiteActivityConfig->iLotteryTicket)
				iPrizeNum = 7;
			else if (iAddPoint < lpSiteActivityConfig->iLotteryTicket + lpSiteActivityConfig->iLotteryTicketUpperLimit)
				iPrizeNum = 8;
			else if (iAddPoint < lpSiteActivityConfig->iLotteryTicket + lpSiteActivityConfig->iLotteryTicketUpperLimit + lpSiteActivityConfig->iCondition1)
				iPrizeNum = 9;
		}
		__log(_DEBUG, "CGdPrizeLimit::OnActivityMessage", "userid[%d], iPrizeNum[%d], length[%d], point[%d], num[%d]",
			lpServerUserItem->userNodeInfo.iUserID, iPrizeNum, length, iAddPoint, iNumTp);
		if (0 < iPrizeNum)
		{
			if (iNumTp < iPrizeNum) iPrizeNum = iNumTp;
			lpUserActivityProgress->iGameCoin += iPrizeNum;
			lpServerUserItem->userNodeInfo.iRDPrizeNum += iPrizeNum;
			bUpdate = TRUE;
		}
	}

    //根据活动配置, 初始化玩家活动进度; bUpdate表示在玩家离开时, 是否需要写数据库
	virtual void InitializeActivity(FactoryPlayerNodeDef *lpServerUserItem, const DBSiteActivityConfig* const lpSiteActivityConfig, UserActivityProgress *lpUserActivityProgress, BOOL &bUpdate)
	{
		if (FALSE == m_bOpen) return;
        if (2015092601 != lpUserActivityProgress->iActivityID) return;

		tm			lastTime = *localtime((time_t *)&lpUserActivityProgress->iUpdateTime);
		time_t		nowTime = time(NULL);
		tm			*ptime = localtime(&nowTime);

		if (lastTime.tm_yday != ptime->tm_yday)
		{
			lpUserActivityProgress->iGameCoin = 0;
			lpUserActivityProgress->iUpdateTime = (int)nowTime;
			bUpdate = TRUE;
		}
    }

    //处理活动进度; bUpdate表示在玩家离开时, 是否需要写数据库
	virtual void OnActivityProgress(FactoryPlayerNodeDef *pPlayerNode, FactoryTableItemDef *lpTableFrame, const DBSiteActivityConfig* const lpSiteActivityConfig, UserActivityProgress *lpUserActivityProgress, BOOL &bUpdate)
    {
        // todo
    }

    //玩家离开时, 写数据库保存玩家活动进度
    virtual void OnWriteProgress(UserActivityProgress *lpUserActivityProgress)
    {
        m_lpGameLogicSink->UpdateActivityProgress(lpUserActivityProgress);
    }

	//活动关闭通知
	virtual void OnActivityClosed(const DBSiteActivityConfig*  const lpSiteActivityConfig){}

	//全服事件处理
	virtual void OnGlobalActivityMessage(long lType, void *lpBuffer, long length, const DBSiteActivityConfig* const lpSiteActivityConfig, multimap<int, UserActivityProgressEx>& mapUserActivityProgress){}

	//释放接口
    virtual void Release()
    {
        delete this;
    }

    virtual void OnReconnectActivity(FactoryPlayerNodeDef *lpServerUserItem, const DBSiteActivityConfig* const lpSiteActivityConfig, UserActivityProgress *lpUserActivityProgress, BOOL &bUpdate)
    {
    }
    virtual IActivityCallback* Clone(GameLogic *game)
    { 
        return new CGdPrizeLimit((FLGameLogic*)game);
    }

private:
    FLGameLogic					*m_lpGameLogicSink;
    CIniFile					m_fileConifg;
    BOOL						m_bOpen;
};

#endif //__ACTIVITY_GUAN_DAN_H__
