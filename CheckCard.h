#ifndef __CHECKCARD_H_
#define __CHECKCARD_H_


#define MAX_PLAYER_NUM    4                 //最大玩家数目
#define MAX_HAND_CARDS_NUM 28

#define LITTLE_JOKCER	18 //小王

#define TYPE_SINGLE_CARDS					0x01	//单牌
#define TYPE_PAIR_CARDS						0x20	//对子
#define TYPE_TRIAD_CARDS					0x30	//三张牌
#define TYPE_TRIAD_CARDS_PAIR				0x32	//三带二

#define TYPE_SINGLE_SEQUENCE_CARDS			0x15	//单顺牌(5张)

#define TYPE_PAIR_SEQUENCE_CARDS			0x23	//双顺牌(3对)

#define TYPE_TRIAD_SEQUENCE_CARDS			0x37	//三顺牌(连续2个)


#define TYPE_BOMB_CARD						0x81	//四张炸弹
#define TYPE_BOMB_FIVE						0x82	//五张炸弹
#define TYPE_SINGLE_SAMESEQUENCE_CARDS		0x83	//(同花)单顺牌(5张)
#define TYPE_BOMB_SIX						0x84	//六张炸弹, 天炸
#define	TYPE_BOMB_SEVEN						0x85	//七张炸弹
#define	TYPE_BOMB_EIGHT						0x86	//八张炸弹
#define	TYPE_BOMB_NINE						0x87	//九张炸弹
#define	TYPE_BOMB_TEN						0x88	//十张炸弹

#define TYPE_ROCKET_CARD					0x91	//火箭



class CheckCard
{
    /*
    *判断牌形,arg_num[]为存有牌的数组,arg_num为牌的张数,arg_value用来返回牌的大小,
    *若牌合法,返回牌形,arg_value返回牌形中主要牌的最大一张,
    *若牌不合法,否则返回-1;arg_value返回0;
    *若牌合法  arg_Wsk值-1,不是五十K,0五十K,1为方片wsk 2为草花WSK 3红桃WSK  4为黑桃wsk
    */
public:
	static int CheckShape(int arg_card[], int arg_num, int &arg_value ,char level , int type);
	
	static int CheckCardShape(int arg_card[], int arg_num, int &arg_value,int arg_num_card[],char level,bool isSequence ,bool is = false);
private:
	static int JudgeCardShape(int arg_card[], int arg_num, int &arg_value,int arg_num_card[], char level, int type);

    static void JockerSwitch(int &jocker);

	static int CheckSameSequence(int arg_card[], int arg_num, int &arg_value,int arg_num_card[],char level,bool is = false);

	static int CheckPairSequence(int arg_card[], int arg_num, int &arg_value,int arg_num_card[],char level);

	static int CheckTriadSequence(int arg_card[], int arg_num, int &arg_value,int arg_num_card[],char level);

};

#endif /* __CHECKCARD_H_ */
