// lab3-2
#include <iostream>
#include <WinSock2.h>
#include <windows.h>
#include <string>
#include <fstream>
#include "help.h"

using namespace std;
#pragma comment(lib, "ws2_32.lib")

u_short myport = 6667; // 为了方便，写死
u_short desport = 6666;
DWORD myip = inet_addr("127.0.0.2");
DWORD desip = inet_addr("127.0.0.1");

SOCKET sockSend;
SOCKADDR_IN address_des;
SOCKADDR_IN address_my;
int addlen = sizeof(address_des);

message recvbuff;
message sendbuff;
const double TIMEOUT = 3 * CLOCKS_PER_SEC;

const int my_window_size = 5; // 累计重传的大小

bool init();
bool shake_hand();
bool rec_file();
bool byebye();

int main()
{
    // 加载套接字库
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        cout << "error";
        return -1;
    }
    init();
    shake_hand();
    rec_file();
    byebye();
    return 0;
}

bool init()
{
    sockSend = socket(AF_INET, SOCK_DGRAM, 0); // ipv4,udp
    if (sockSend == -1)
    {
        cout << "创建Socket失败";
        WSACleanup();
        return false;
    }
    printf("创建socket成功\n");

    address_des.sin_addr.S_un.S_addr = desip;
    address_des.sin_family = AF_INET;
    address_des.sin_port = desport;

    address_my.sin_addr.S_un.S_addr = myip;
    address_my.sin_family = AF_INET;
    address_my.sin_port = myport;

    // 绑定套接字, 绑定到端口
    bind(sockSend, (SOCKADDR *)&address_my, sizeof(SOCKADDR)); // 会返回一个SOCKET_ERROR
    cout << "绑定sock成功\n初始化完成\n";
    return true;
}

bool shake_hand()
{
    cout << "等待握手请求...\n";
    while (1)
    {
        memset(&recvbuff, '\0', msg_size);
        if (recvfrom(sockSend, (char *)&recvbuff, msg_size, 0, (struct sockaddr *)&address_des, &addlen) == -1)
        {
            cout << "recv error";
            return false;
        }
        else
            cout << "收到第一次握手请求\n";
        if (recvbuff.SYN == 0 || checksum(&recvbuff, msg_size) != 0)
        {
            cout << "收到错误，重新请求对方发送第一次握手\n";
            memset(&sendbuff, '\0', msg_size);
            packdata(&sendbuff, myip, desip, myport, desport, 0, 1, 0, 0, 0, 0, 0, NULL);
            if (sendto(sockSend, (char *)&sendbuff, msg_size, 0, (struct sockaddr *)&address_des, addlen) == -1)
            {
                cout << "send error";
                return false;
            }
            else
                cout << "已重传;";
        }
        else
        {
            cout << "第一次握手请求正常，准备回复第二次握手请求\n";
            memset(&sendbuff, '\0', msg_size);
            packdata(&sendbuff, myip, desip, myport, desport, 1, 1, 0, 0, 0, 0, 0, NULL);
            if (sendto(sockSend, (char *)&sendbuff, msg_size, 0, (struct sockaddr *)&address_des, addlen) == -1)
            {
                cout << "send error";
                return false;
            }
            else
                cout << "第二次握手发送成功\n";
            break;
        }
    }
    return true;
}

bool rec_file()
{
    cout << "等待文件信息\n";
    while (1)
    {
        memset(&recvbuff, '\0', msg_size);
        if (recvfrom(sockSend, (char *)&recvbuff, msg_size, 0, (struct sockaddr *)&address_des, &addlen) == -1)
        {
            cout << "recv error";
            return false;
        }
        else
            cout << "收到文件信息\n";
        if (recvbuff.seq != 0 || checksum(&recvbuff, msg_size) != 0)
        {
            cout << "收到错误，重新请求对方发送\n";
            memset(&sendbuff, '\0', msg_size);
            packdata(&sendbuff, myip, desip, myport, desport, 0, 0, 0, 0, 0, 0, 0, NULL);
            if (sendto(sockSend, (char *)&sendbuff, msg_size, 0, (struct sockaddr *)&address_des, addlen) == -1)
            {
                cout << "send error";
                return false;
            }
            else
                cout << "已重传\n";
        }
        else
        {
            cout << "文件正常,回复ack\n";
            memset(&sendbuff, '\0', msg_size);
            packdata(&sendbuff, myip, desip, myport, desport, 1, 0, 0, 0, 0, 0, 0, NULL);
            if (sendto(sockSend, (char *)&sendbuff, msg_size, 0, (struct sockaddr *)&address_des, addlen) == -1)
            {
                cout << "send error";
                return false;
            }
            else
                cout << "ack发送成功\n";
            break;
        }
    }
    cout << "正在创建本地文件\n";
    char filename[20];
    memcpy(filename, recvbuff.msg, recvbuff.filesize);
    // string fpath = "D:\\MSvscodeprojects\\c_projects\\Cpt_ntw\\jw\\lab3_gobackN_udp\\recvfile\\";
    string fpath = ".\\recvfile\\";
    fpath.append(filename);
    ofstream f_recv;
    f_recv.open(fpath, ofstream::out | ios::binary);
    if (!f_recv.is_open())
    {
        cout << "null file\n";
        return false;
    }
    int group_size = recvbuff.msgnumber;
    if (f_recv.is_open())
    {
        cout << "创建成功,文件名:" << filename << endl;
        cout << "分组数:" << group_size << endl;
    }
    cout << "等待传输\n";
    const clock_t start_time = clock(); // 计时
    //-------------------------------------准备完成，正式接受文件

    for (int i = 1; i <= group_size; i++) // i表示希望recv包的seq
    {
        printf("第%d个文件分组接受中....\n", i);
        // client不需要 timer
        int max_err_time = 20;
        while (1)
        {
            max_err_time--;
            memset(&recvbuff, '\0', msg_size);
            if (recvfrom(sockSend, (char *)&recvbuff, msg_size, 0, (struct sockaddr *)&address_des, &addlen) == -1)
            {
                cout << "recv error";
                return false;
            }
            else
                print_msg(&recvbuff);

            if (recvbuff.seq != i || checksum(&recvbuff, sizeof(recvbuff)) == 0)
            {
                if (max_err_time == 0)
                {
                    cout << "重传次数过多，失败\n";
                    return false;
                }
                printf("收到错误，重新请求对方发送\n");
                memset(&sendbuff, '\0', msg_size);
                packdata(&sendbuff, myip, desip, myport, desport, 0, 0, 0, 0, i, 0, 0, NULL); // 回复ack=i，表示我要第i个报文
                if (sendto(sockSend, (char *)&sendbuff, msg_size, 0, (struct sockaddr *)&address_des, addlen) == -1)
                {
                    printf("send error");
                    return false;
                }
                else
                    printf("已请求重传\n");
                continue;
            }
            else
            {
                printf("文件验证正常\n");
                //===========================================================随机丢包测试
                if (clock() % 50 == 51) // 根据时间随机丢包
                {
                    printf("==========文件正常，故意丢包\n");
                    printf("==========回复ACK=0,ack=%d!!!\n", i);
                    memset(&sendbuff, '\0', msg_size);
                    packdata(&sendbuff, myip, desip, myport, desport, 0, 0, 0, 0, i, 0, 0, NULL); // ack=i,表示已经收到第i个
                    if (sendto(sockSend, (char *)&sendbuff, msg_size, 0, (struct sockaddr *)&address_des, addlen) == -1)
                    {
                        printf("send error");
                        return false;
                    }
                    printf("==========故意的ack发送成功!!!\n");
                    continue;
                }
                if (i % my_window_size == 0 || i == group_size) // 每隔几次才ack，或者最后一个
                {
                    printf("回复ack=%d!!!\n", i);
                    memset(&sendbuff, '\0', msg_size);
                    packdata(&sendbuff, myip, desip, myport, desport, 1, 0, 0, 0, i, 0, 0, NULL); // ack=i,表示已经收到第i个
                    if (sendto(sockSend, (char *)&sendbuff, msg_size, 0, (struct sockaddr *)&address_des, addlen) == -1)
                    {
                        cout << "send error";
                        return false;
                    }
                    else
                        printf("ack发送成功!!!\n");
                }
                f_recv.write(recvbuff.msg, recvbuff.filesize);
                printf("文件写入完成\n");
                break; // next round
            }
        }
    }
    printf("\n传输完成!!!!!\n");
    int tuntulv;
    float totalt;
    const clock_t end_time = clock();
    totalt = (float)(end_time - start_time) / CLOCKS_PER_SEC;
    tuntulv = (group_size * BUF_SIZE) / totalt / 1000;
    printf("发送字节:%d\n组数:%d\n每组大小:%d\n吞吐率:%dkb/s\n总时间:%.5fs\n\n",
           group_size * BUF_SIZE, group_size, BUF_SIZE, tuntulv, totalt);
    f_recv.close();
    return true;
}

bool byebye()
{
    cout << "等待挥手请求...\n";
    while (1)
    {
        memset(&recvbuff, '\0', msg_size);
        if (recvfrom(sockSend, (char *)&recvbuff, msg_size, 0, (struct sockaddr *)&address_des, &addlen) == -1)
        {
            cout << "recv error";
            return false;
        }
        else
            cout << "收到第一次挥手请求\n";
        if (recvbuff.FIN != 1 || checksum(&recvbuff, msg_size) != 0)
        {
            cout << "收到错误，重新请求对方发送第一次挥手\n";
            memset(&sendbuff, '\0', msg_size);
            packdata(&sendbuff, myip, desip, myport, desport, 0, 1, 0, 0, 0, 0, 0, NULL);
            if (sendto(sockSend, (char *)&sendbuff, msg_size, 0, (struct sockaddr *)&address_des, addlen) == -1)
            {
                cout << "send error";
                return false;
            }
            else
                cout << "已重传;";
        }
        else
        {
            cout << "第1次挥手请求正常,准备回复第二次挥手请求\n";
            memset(&sendbuff, '\0', msg_size);
            packdata(&sendbuff, myip, desip, myport, desport, 1, 0, 1, 0, 0, 0, 0, NULL);
            if (sendto(sockSend, (char *)&sendbuff, msg_size, 0, (struct sockaddr *)&address_des, addlen) == -1)
            {
                cout << "send error";
                return false;
            }
            else
                cout << "第二次挥手发送成功\n";
            break;
        }
    }
    closesocket(sockSend);
    WSACleanup();
    cout << "退出成功\n";
    return true;
}