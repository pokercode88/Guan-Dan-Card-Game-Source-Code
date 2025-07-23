#ifndef __MSG_H_
#define __MSG_H_

#include "Server2ServerMsg.h"

#define TABLE_MAX_PLAYER_NUM	4 //added by chw 1022
#define MAX_LEVEL_NUM			21//9 //chw 1109

enum
{
    //Client 客户端动作消息

    SEND_CARDS_REQ_MSG       = 0x10,    //玩家“出牌”请求，超时自动出最左边的那张牌，或者不要牌
    DEAL_OK_REQ_MSG          = 0x11,    //发牌OK...为了保持客户端同步

    //Server 通知客户端消息（将某玩家的动作告诉所有桌上的玩家,但只有一个玩家处理此消息）
    SEND_CARDS_NOTICE_MSG    = 0x14,    //玩家的“出牌”通知
    GET_PONIT_NOTICE_MSG     = 0x15,

	//GAME_RESULT_SERVER_MSG   = 0x16,
    //Server 发起客户端消息（服务器端发起,告诉桌上所有玩家,所有玩家都处理此消息）
    READY_SERVER_MSG         = 0x17,   //“开始”请求
    DEAL_CARDS_SERVER_MSG    = 0x18,   //“发牌“请求
    SEND_CARDS_SERVER_MSG    = 0x1A,   //”出牌“请求
    GAME_RESULT_SERVER       = 0X1D,   //游戏结束通知
    AUTO_SEND_NOTICE         = 0X1B,   //托管通知, ?自动托管消息
    SEND_END_NOTICE          = 0x20,   //玩家出完牌通知

    
    ACTIVE_ADD_ACCOUNT       = 0X21,    //活动增加积分


	TRIBUTE_CARD_SERVER_MSG  = 0x22	,	//进贡消息

	TRIBUTE_CARD_REQ_MSG		= 0x23,		//进贡牌

	RETURN_TRIBUTE_REQ_MSG   = 0x24,		//进/还恭消息, 广播4个人

	RETURN_SERVER_MSG   = 0x25	,			//进贡消息, 只发给被进贡的玩家

	TRIBUTE_RETURN_CARD_REQ_MSG = 0x26,
	TRIBUTE_RETURN_CARD_NOTICE_MSG = 0x27,
	RETURN_CARD_SERVER_MSG = 0x28,			//还贡消息; 发给进恭的玩家

	LOCK_NOTICE			= 0X29,			//抗贡
	CHANGE_TRIBUTECARD_NOTICE  = 0x30	,//改变牌级通知
	GAME_EXP_NOTICE  = 0x31,	//等级通知
	MSG_C_TRUST = 0x32	,					// 客户端托管消息, 客户端发送过来的托管消息, 客户端主动托管
	MSG_S_TRUST = 0x33	,					//服务端, 回送的托管消息


	GAME_END	= 0x34,

	ADD_PRIZE = 0x35,					//游戏结束赠送奖品券
    PACKET_CARD_NOTICE_MSG   = 0x36,   // 分组牌通知
 
	LOBBYCLIENT_GAMESERVER_BROADCAST_MSG = 41522,

	MSG_C_CARDS_NOTICE_ROBOT = 0x50,	//机器人偷看牌
};

/* chw 1024 begin */
typedef struct GameGuanDanRankRes
{
	HEADER;
	int   iCount;
	GuanDanRankInfo rankInfo[100];
}GameGuanDanRankResDef;
/* chw 1024 end */

/* chw 1104 begin */
//发送语音消息
typedef struct SendMotion
{
	HEADER;
	unsigned char usIndexMotion;  //语音消息类型
	unsigned char usIndexSound;   //语音消息索引
	unsigned char usTableNumExtra; //发送语音消息玩家的座位号
}DefSendMotion;

/********************************客户端请求消息结构体定义*******************************/

typedef struct DealCardsServer
{
	HEADER;
	char		cHandCards[MAX_PLAYER_NUM][MAX_HAND_CARDS_NUM];
	char        cGameNum[MAX_HAND_CARDS_NUM];     //每局的游戏编号
	int			iDisconnect[TABLE_PLAYER_NUM];
	char        cFirstGetPlayer;  //本桌第一个摸牌的玩家
	char        cFirstSendPlayer; //本桌红桃三出牌
	char		cLevelCard;
	int			iTime;
	char		cNowLevelLocal;
	char		cLevel[2];
}DealCardsServerDef;

typedef struct PacketCard
{
	HEADER;
	char		cPacketCards[MAX_PLAYER_NUM]; //每个位置有的可以组队的牌
	char		cExchangePos[2]; //需要交换的两个位置
}PacketCardDef;

typedef struct SendCardsReq
{
	HEADER;
	char          cCards[MAX_HAND_CARDS_NUM];     //出的牌，最大24
	char          cCardNum;       //出的牌数目
	int           cCardType;      //牌型
	int          cCardValue;     //牌值
}SendCardsReqDef;

typedef struct AutoReq
{
	HEADER;
  unsigned short       usTableNumExtra;
  char                 cAutoSend;         //0 非托管  1托管
}AutoReqDef;


typedef struct SendEndNotice
{
	HEADER;
	unsigned short       usTableNumExtra;
	char                 cOlderNum;
}SendEndNoticeDef;


typedef struct SendCardsServer
{
	HEADER;
	unsigned short       usTableNumExtra;	//当前出牌的玩家
	char cPass;								//1, 为先手出牌, 0, 跟牌
	char cFirst;							//?
	char cSecond;							//?
}SendCardsServerDef;


typedef struct SendCardsNotice
{
	HEADER;
	unsigned short  cTableNumExtra; 
	char cSendCards[MAX_HAND_CARDS_NUM];
	char cSelfSendCards[MAX_HAND_CARDS_NUM];
	char cCardNum;
	int iCardType;
	int iCardValue;
	int	iBombNum;						//炸弹数
	char bIfLocal;
}SendCardsNoticeDef, *PSendCardsNoticeDef;	//出牌消息结构体

typedef struct PlayerGameInfo
{
	char                        szGameNum[MAX_HAND_CARDS_NUM];      //游戏编号
	char                        cCards[TABLE_PLAYER_NUM][MAX_HAND_CARDS_NUM];      //每个人手里的牌 
	char                        cTableCard[MAX_HAND_CARDS_NUM];     //当前桌上牌
	char                        cCurSendCard[TABLE_PLAYER_NUM][MAX_HAND_CARDS_NUM];//记录当前所出的牌
	char                        cOrderNum[TABLE_PLAYER_NUM];       //完成的顺序


	int							iDisconnect[TABLE_PLAYER_NUM];


	int                         iCurSendPlayer;     //当前出牌玩家
	int                         iCurTablePlayer;    //当前出桌上牌的玩家 


	int                         iCurCardType;       //当前的局的牌型
	int                         iCurCardValue;      //当前局的最大牌值

	char						cLevel;

	char						cPass[TABLE_PLAYER_NUM];

	char						cMaxLevel;

	char						cNowLevelLocal;
	char						cNowLevel[2];
	char						cOtherTrust[4];
	char						cNowPlayerStatus;
	char						cGameLevel;

	char						cTribute[4];
	char						cSelfTribute[4];
    char                        cTableName[64];
    int                         nTableID;
}PlayerGameInfoDef;

typedef struct GameResultServer
{
	HEADER;
	char        cResultType;
	char        CHandCard[MAX_PLAYER_NUM][MAX_HAND_CARDS_NUM];
	int         iMoneyResult[MAX_PLAYER_NUM];
	int         iAmountResult[MAX_PLAYER_NUM];
	int         iOrderNum[MAX_PLAYER_NUM];      //玩家排名
	int         iOverNum[MAX_PLAYER_NUM];       //玩家完成顺
	int         iExtraNum[MAX_PLAYER_NUM];      //客户端显示的掼蛋点

	int         iPrizeTicket[MAX_PLAYER_NUM];   //赠送奖品券数量

	int         iFinalTimes;                  //最终倍率
	int         iTableMoney;                  //桌费,系统一共扣除的

	char		cAddLevel;						//增加的级数
	char		cTableNumExtra[MAX_PLAYER_NUM];
	char		cNowLevelLocal;
	char		cLevel[2];

	char		cDisconnectType[MAX_PLAYER_NUM];
	bool        bBankrupt[MAX_PLAYER_NUM];       //chw 1117玩家是否已经输光身上的积分
	
}GameResultServerDef;

//让客户端进贡
typedef struct TributeCardsServer
{
	HEADER;
	int iOverNum[MAX_PLAYER_NUM];		//玩家上一局, 得第几名, 当双下时, 有两个玩家为4
	int iGameLevel;						//上一局升几级
} TributeCardsServerDef;

//客户端进贡或者还贡
typedef struct TributeOKServer
{
	HEADER;
	char cTribute;
} TributeOKServerDef;

//进贡或还贡的通知,群发
typedef struct ReturnTributeServer
{
	HEADER;
	unsigned short       usTableNumExtra;	//进贡/还贡的玩家
	char cTribute;							//进贡的牌
	char cLocal;							//进贡时为-1; 还贡时, 为进恭的玩家, 还恭的牌给谁
} ReturnTributeServerDef;

//单发, 进贡时,发送给被进贡的玩家; 还贡消息时, 只发给进贡的玩家
typedef struct Return
{
	HEADER;
	unsigned short       usTableNumExtra;		//进/还恭的玩家
	char cTribute;								//进/还贡的牌
} ReturnDef;

typedef struct LockNotice
{
	HEADER;
	char		cHandCards[MAX_PLAYER_NUM][MAX_HAND_CARDS_NUM];	//当前玩家的牌
	char			cFirstGetPlayer;	//有多少个玩家, 改数据, 可以不用考虑使用
	char		cIs;					//1, 抗贡
}LockNoticeDef;							//抗恭消息

typedef struct GameExpLevelNotice
{
	HEADER;
	char	ExpLevel;
	int		iAnte;				//当局底注
}GameExpLevelNoticeDef;			//等级同步

typedef struct tagUserTrust
{
	HEADER;
	int nTrustUser;
	bool bTrust;
} CUserTrust;


typedef struct tagCardsNoticeRobot
{
	HEADER;
	
	char		cHandCards[MAX_PLAYER_NUM][MAX_HAND_CARDS_NUM - 1];	//当前玩家的牌
	int			iHandCardsNum[MAX_PLAYER_NUM];//当前玩家的牌数
}CardsNoticeRobotDef;


#endif /* __MSG_H_ */
