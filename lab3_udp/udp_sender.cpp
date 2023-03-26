//lab3-1
#include <iostream>
#include <WinSock2.h>
#include <windows.h>
#include <string>
#include <fstream>
#include "help.h"

using namespace std;
#pragma comment(lib, "ws2_32.lib")

u_short myport = 6666; //为了方便，写死
u_short desport = 6667;
DWORD myip = inet_addr("127.0.0.1");
DWORD desip = inet_addr("127.0.0.2");

SOCKET sockSend;
SOCKADDR_IN address_des;
SOCKADDR_IN address_my;
int addlen = sizeof(address_des);

message recvbuff;
message sendbuff;
const clock_t TIMEOUT = 300 * CLOCKS_PER_SEC;

bool init();
bool shake_hand();
bool tp_file();
bool byebye();

int main()
{
    //加载套接字库
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        cout << "error";
        return -1;
    }
    init();
    shake_hand();
    tp_file();
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

    //绑定套接字, 绑定到端口
    bind(sockSend, (SOCKADDR *)&address_my, sizeof(SOCKADDR)); //会返回一个SOCKET_ERROR
    cout << "绑定sock成功\n初始化完成\n";
    return true;
}

bool shake_hand()
{
    //第一次握手,SYN=1
    memset(&sendbuff, '\0', msg_size);
    packdata(&sendbuff, myip, desip, myport, desport, 0, 1, 0, 0, 0, 0, 0, NULL);
    if (sendto(sockSend, (char *)&sendbuff, msg_size, 0, (struct sockaddr *)&address_des, addlen) == -1)
    {
        cout << "send error";
        return false;
    }
    cout << "第1次握手发送成功\n";

    while (1)
    {
        cout << "等待回应\n";
        memset(&recvbuff, '\0', msg_size);
        if (recvfrom(sockSend, (char *)&recvbuff, msg_size, 0, (struct sockaddr *)&address_des, &addlen) == -1)
        {
            cout << "recv error";
            return false;
        }
        if (recvbuff.ACK == 0 || checksum(&recvbuff, msg_size) != 0)
        {
            cout << "接受第2次握手错误,重传握手\n";
            if (sendto(sockSend, (char *)&sendbuff, msg_size, 0, (struct sockaddr *)&address_des, addlen) == -1)
            {
                cout << "send error";
                return false;
            }
            cout << "已发送\n";
        }
        else
        {
            cout << "第二次握手接受成功\n";
            print_msg(&recvbuff);
            break;
        }
    }
    return true;
}

bool tp_file()
{
    string filename;
    cout << "输入文件名\n";
    cin >> filename;
    // debug的时候要改成绝对路径，因为调试器和cpp不在一个地方
    // string fpath = "D:\\MSvscodeprojects\\c_projects\\Cpt_ntw\\jw\\lab3_udp\\testfile\\" + filename;
    string fpath = ".\\testfile\\" + filename;
    ifstream f_send;
    f_send.open(fpath, ios::in | ios::binary);
    if (!f_send.is_open())
    {
        cout << "null file";
        return false;
    }
    int size = 0;
    f_send.seekg(0, f_send.end); //计算大小
    size = f_send.tellg();
    f_send.seekg(0, f_send.beg);

    int group_size = size / BUF_SIZE + (size % BUF_SIZE == 0 ? 0 : 1);
    //分组，并且向上取整
    cout << "获取文件成功,分组数:" << group_size << endl;
    memset(&sendbuff, '\0', msg_size);
    packdata(&sendbuff, myip, desip, myport, desport, 0, 0, 0, 0, 0, group_size, filename.length(), (char *)filename.c_str());
    if (sendto(sockSend, (char *)&sendbuff, msg_size, 0, (struct sockaddr *)&address_des, addlen) == -1)
    {
        cout << "send error";
        return false;
    }
    cout << "文件信息发送成功\n";

    while (1)
    {
        cout << "等待回应\n";
        memset(&recvbuff, '\0', msg_size);
        if (recvfrom(sockSend, (char *)&recvbuff, msg_size, 0, (struct sockaddr *)&address_des, &addlen) == -1)
        {
            cout << "recv error";
            return false;
        }
        print_msg(&recvbuff);
        if (recvbuff.ACK == 0 || checksum(&recvbuff, msg_size) != 0)
        {
            cout << "错误,重传\n";
            if (sendto(sockSend, (char *)&sendbuff, msg_size, 0, (struct sockaddr *)&address_des, addlen) == -1)
            {
                cout << "send error";
                return false;
            }
            cout << "已发送\n";
        }
        else
        {
            cout << "对方ack,准备开始传文件\n";
            break;
        }
    }
    //-----------------------------------准备完成，正式传文件
    //此时总长度是size,分组个数是group_size,除了最后一组,每组大小是buf_size

    srand((unsigned)time(NULL));
    clock_t start;                       //计时的
    const clock_t start_time = clock(); //总计时，不要改动
    u_long mode_yes = 1;                 //非阻塞，否则recv函数会一直等待
    ioctlsocket(sockSend, FIONBIO, &mode_yes);

    char tempbuf[BUF_SIZE];
    for (int i = 1; i < group_size + 1; i++) // i表示包的seq
    {
        memset(tempbuf, '\0', BUF_SIZE); //先清空
        printf("第%d个文件分组传输中....\n", i);
        memset(&sendbuff, '\0', msg_size); //清空并打包
        u_short buff_size =
            ((i == group_size) ? (size % BUF_SIZE) : BUF_SIZE); //是否为最后一组

        f_send.read(tempbuf, buff_size); //读取文件内容，存入buffer

        packdata(&sendbuff, myip, desip, myport, desport,
                 0, 0, 0, i, 0, group_size, buff_size, tempbuf);
        if (sendto(sockSend, (char *)&sendbuff, msg_size, 0, (struct sockaddr *)&address_des, addlen) == -1)
        {
            cout << "send error";
            return false;
        }
        printf("第%d个文件分组发送完成....\n", i);
        start = clock(); // start timer

        while (1)
        {
            if (clock() - start > TIMEOUT)
            {
                printf("第%d个文件分组超时重传....\n", i);
                if (sendto(sockSend, (char *)&sendbuff, msg_size, 0, (struct sockaddr *)&address_des, addlen) == -1)
                {
                    cout << "send error";
                    return false;
                }
                printf("第%d个文件分组重传完成....\n", i);
                start = clock(); // reset timer
            }
            else
            {
                memset(&recvbuff, '\0', msg_size);
                if (recvfrom(sockSend, (char *)&recvbuff, msg_size, 0, (struct sockaddr *)&address_des, &addlen) == -1)
                {
                    if (errno == 0 || errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN)
                        continue; // do noting,这是非阻塞的正常报错
                    cout << "recv error:" << errno;
                    fprintf(stderr, "fopen: %s", strerror(errno));
                    return false;
                }
                else
                    print_msg(&recvbuff);
                if (recvbuff.ACK == 0 || checksum(&recvbuff, msg_size) != 0)
                {
                    printf("第%d个文件分组错误重传....\n", i);
                    if (sendto(sockSend, (char *)&sendbuff, msg_size, 0, (struct sockaddr *)&address_des, addlen) == -1)
                    {
                        cout << "send error";
                        return false;
                    }
                    else
                        printf("第%d个文件分组重传完成....\n", i);
                }
                else
                {
                    start = clock();
                    break; //发下一个包
                }
            }
        }
    }
    u_long mode_zs = 0; //恢复阻塞
    ioctlsocket(sockSend, FIONBIO, &mode_zs);
    cout << "\n传输完成!!!\n";
    int tuntulv;
    float totalt;
    const clock_t end_time=clock();
    totalt=(float)(end_time-start_time)/CLOCKS_PER_SEC;
    tuntulv=size/totalt/1000;
    printf("发送字节:%d\n组数:%d\n每组大小:%d\n吞吐率:%dkb/s\n总时间:%.5fs\n\n",
           group_size*BUF_SIZE, group_size, BUF_SIZE, tuntulv, totalt);
    f_send.close();
    return true;
}

bool byebye()
{
    //第一次挥手手,FIN=1
    memset(&sendbuff, '\0', msg_size);
    packdata(&sendbuff, myip, desip, myport, desport, 0, 0, 1, 0, 0, 0, 0, NULL);
    if (sendto(sockSend, (char *)&sendbuff, msg_size, 0, (struct sockaddr *)&address_des, addlen) == -1)
    {
        cout << "send error";
        return false;
    }
    cout << "第1次挥手发送成功\n";

    while (1)
    {
        cout << "等待回应\n";
        memset(&recvbuff, '\0', msg_size);
        if (recvfrom(sockSend, (char *)&recvbuff, msg_size, 0, (struct sockaddr *)&address_des, &addlen) == -1)
        {
            cout << "recv error";
            return false;
        }
        if (recvbuff.ACK == 0 || checksum(&recvbuff, msg_size) != 0)
        {
            cout << "接受第2次挥手错误,重传\n";
            if (sendto(sockSend, (char *)&sendbuff, msg_size, 0, (struct sockaddr *)&address_des, addlen) == -1)
            {
                cout << "send error";
                return false;
            }
            cout << "已发送\n";
        }
        else
        {
            cout << "第二次挥手接受成功\n";
            print_msg(&recvbuff);
            break;
        }
    }
    closesocket(sockSend);
    WSACleanup();
    cout << "退出成功\n";
    return true;
}
