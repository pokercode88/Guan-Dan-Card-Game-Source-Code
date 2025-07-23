
#ifndef __SERVERDATA_H_
#define __SERVERDATA_H_

#define SERVER_NAME "GD_server"     //折里是游戏服务器名
#define LOG_FILE SERVER_NAME ".log"

enum GameValueError
{
    ERROR_GAME_VALUE_SET_BASEPOINT = 110,    //设置底注
    ERROR_GAME_VALUE_BET,                    //押注
    ERROR_GAME_VALUE_GET_MONEY,              //取钱
    ERROR_GAME_VALUE_SAVE_MONEY,             //存钱
    ERROR_GAME_VALUE_RESULT,                 //游戏结算
    ERROR_GAME_VALUE_USE_PROP,               //消耗道具
    ERROR_GAME_VALUE_OUT_CARD,               //出牌
};

//#define _USE_PC_EXERCISE_


#endif /* __SERVERDATA_H_ */
