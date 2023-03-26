#include <time.h>
#include <WinSock2.h>
#include "protocol.h"

std::string YOUCHAT_c = "                      ___  _             _   \n\
/\\_/\\  ___   _   _   / __\\| |__    __ _ | |_ \n\
\\_ _/ / _ \\ | | | | / /   | '_ \\  / _` || __|\n\
 / \\ | (_) || |_| |/ /___ | | | || (_| || |_ \n\
 \\_/  \\___/  \\__,_|\\____/ |_| |_| \\__,_| \\__|  ----client\n\
                                             \n\n";
std::string YOUCHAT_s = "                      ___  _             _   \n\
/\\_/\\  ___   _   _   / __\\| |__    __ _ | |_ \n\
\\_ _/ / _ \\ | | | | / /   | '_ \\  / _` || __|\n\
 / \\ | (_) || |_| |/ /___ | | | || (_| || |_ \n\
 \\_/  \\___/  \\__,_|\\____/ |_| |_| \\__,_| \\__|  ----server\n\
                                             \n\n";

int gettime()
{
    time_t now = time(0);
    tm ltm;
    now = time(0);
    localtime_s(&ltm, &now);
    return ltm.tm_hour * 10000 + ltm.tm_min * 100 + ltm.tm_sec;
}

bool SendAll(SOCKET *sock, char *buffer, int size)
{
    char *c = buffer;
    while (size > 0)
    {
        int SendSize = send(*sock, buffer, size, 0);
        if (SOCKET_ERROR == SendSize)
            return false;
        size = size - SendSize; //用于循环发送且退出功能
        buffer += SendSize;     //用于计算已发buffer的偏移量
    }
    return true;
}

bool RecvAll(SOCKET *sock, char *buffer, int size)
{
    while (size > 0) //剩余部分大于0
    {
        int RecvSize = recv(*sock, buffer, size, 0);
        if (SOCKET_ERROR == RecvSize)
            return false;
        size = size - RecvSize;
        buffer += RecvSize;
    }
    return true;
}

PktStruct packmsg(int ctrl, int src, int des, char *msg)
{
    PktStruct pkt = {
        .ctrl = ctrl,
        .src = src,
        .des = des,
        .timestmp = gettime(),
        .MsgLen = sizeof(msg)};
    strcpy(pkt.MsgBuf, msg);
    return pkt;
}