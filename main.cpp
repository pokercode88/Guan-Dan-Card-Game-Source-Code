#include "ServerData.h"
#include "GameLogic.h"

//系统头文件
#include<sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

void handler(int sigal)
{
}

int main(int argc, char *argv[])
{
    InitializeLog(CONFIG_FILE, NULL);

    __log(_DEBUG,"main","************staring:%sv1.0.0.7**************\r\n", SERVER_NAME);

    IGameProvider *lpGameProvider = new FLGameLogic();

    if(0 == lpGameProvider->Start())
    {
        signal(SIGTERM, handler);
        pause();
    }

    delete lpGameProvider;

    __log(_DEBUG, "main", "%s has been exit !\r\n", argv[0]);
    return 0;
}




