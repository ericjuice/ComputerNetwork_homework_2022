// lab3-2
#include <iostream>
#include <WinSock2.h>
#include <windows.h>
#include <string>
#include <fstream>
#include <queue>
#include <thread>
#include <mutex>
#include "help.h"

using namespace std;
#pragma comment(lib, "ws2_32.lib")

u_short myport = 6666; // 为了方便，写死
u_short desport = 6667;
DWORD myip = inet_addr("127.0.0.1");
DWORD desip = inet_addr("127.0.0.2");

SOCKET sockSend;
SOCKADDR_IN address_des;
SOCKADDR_IN address_my;
int addlen = sizeof(address_des);

message recvbuff;
message sendbuff;
const clock_t TIMEOUT = 3 * CLOCKS_PER_SEC;

static int window_base = 0;           // 滑动窗口底部序号
static const int window_size = 30;    // 滑动窗口大小,nowindex-base<window_size
static int now_index = 0;             // 当前传的文件序号
static bool tp_file_end_flag = false; // 全局信号，通知文件传输完成
static int resend_signal = 0;         // 全局信号，通知如何重传窗口
queue<message> window_queue;          // queue.size()<=window_size
mutex mutex_for_queue;
ifstream f_send;

bool init();
bool shake_hand();
bool tp_file();
bool byebye();
void send_file(int group_size, int size);
void recv_ack(int group_size);

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

    // 绑定套接字, 绑定到端口
    bind(sockSend, (SOCKADDR *)&address_my, sizeof(SOCKADDR)); // 会返回一个SOCKET_ERROR
    cout << "绑定sock成功\n初始化完成\n";
    return true;
}

bool shake_hand()
{
    // 第一次握手,SYN=1
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
    // string fpath = "D:\\MSvscodeprojects\\c_projects\\Cpt_ntw\\jw\\lab3_gobackN_udp\\testfile\\" + filename;
    string fpath = ".\\testfile\\" + filename;
    f_send.open(fpath, ios::in | ios::binary);
    if (!f_send.is_open())
    {
        cout << "null file\n";
        return false;
    }
    int size = 0;
    f_send.seekg(0, f_send.end); // 计算大小
    size = f_send.tellg();
    f_send.seekg(0, f_send.beg);

    int group_size = size / BUF_SIZE + (size % BUF_SIZE == 0 ? 0 : 1);
    // 分组，并且向上取整
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
    // 此时总长度是size,分组个数是group_size,除了最后一组,每组大小是buf_size

    thread send_thr(send_file, group_size, size);
    send_thr.detach();
    thread recv_thr(recv_ack, group_size);
    recv_thr.detach();

    while (!tp_file_end_flag)
        Sleep(20); // 防止父线程结束引发子线程结束
    Sleep(200);    // 再睡200毫秒，可能还没cout完
    return true;
}

void recv_ack(int group_size)
{
    u_long mode_yes = 1; // 非阻塞，否则recv函数会一直等待
    ioctlsocket(sockSend, FIONBIO, &mode_yes);
    clock_t start = clock(); // start timer
    while (!tp_file_end_flag)
    {
        if (clock() - start > TIMEOUT)
        {
            // 一直没有收到ack，重传当前队列的所有
            cout << "time out\n";
            resend_signal = window_base; // 通知发送线程从base开始重传
            start = clock();             // reset timer
            continue;
        }

        memset(&recvbuff, '\0', msg_size);
        if (recvfrom(sockSend, (char *)&recvbuff, msg_size, 0, (struct sockaddr *)&address_des, &addlen) == -1)
        {
            if (errno == 0 || errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN)
                continue; // do noting,这是非阻塞的正常报错
            cout << "recv error:" << errno;
            fprintf(stderr, "fopen: %s", strerror(errno));
            return;
        }
        start = clock(); // 不管收到对的错的都先重置计时器
        print_msg(&recvbuff);
        if (recvbuff.ACK == 0) // ACK==0表示对面回复了错误的包
        {
            printf("第%d个文件分组错误重传....\n", recvbuff.ack);
            resend_signal = recvbuff.ack;
            printf("已通知发送线程从第%d个文件分组重传....\n", recvbuff.ack);
        }
        else // 收到正确回复，把base向前移动
        {
            printf("收到ack=%d,移动窗口...\n", recvbuff.ack);
            mutex_for_queue.lock();
            for (int i = recvbuff.ack - window_base; i >= 0; i--)
                window_queue.pop();
            window_base = recvbuff.ack + 1;
            printf("移动完成，当前队列大小：%d,当前base:%d\n", window_queue.size(), window_base);
            mutex_for_queue.unlock();
            if (recvbuff.ack == group_size) // 收到最后一个包，结束
                tp_file_end_flag = true;
        }
    }
    u_long mode_zs = 0; // 恢复阻塞
    ioctlsocket(sockSend, FIONBIO, &mode_zs);
}

void send_file(int group_size, int size)
{
    srand((unsigned)time(NULL));
    const clock_t start_time = clock(); // 总计时，不要改动
    window_base = 1;// 初始化滑动窗口

    char tempbuf[BUF_SIZE];
    for (now_index = 1; !tp_file_end_flag;) // index表示包的seq，理论上结束的时候now_index==group_size+1
    {
        if (resend_signal != 0)
        {
            printf("正在从%d组开始错误重传\n", resend_signal); // head>=rs>=base，要从队列的第rs-base个开始重传
            mutex_for_queue.lock();
            queue<message> temp_queue = window_queue; // 拷贝一个临时的
            mutex_for_queue.unlock();
            for (int i = 0; i < resend_signal - window_base; i++) // 把前面的先除去，从要重传的位置再传
                temp_queue.pop();
            for (int i = resend_signal; temp_queue.size() > 0; i++) // 重传队列剩下的所有
            {
                message temp_msg = temp_queue.front();
                temp_queue.pop();
                if (sendto(sockSend, (char *)&temp_msg, msg_size, 0, (struct sockaddr *)&address_des, addlen) == -1)
                {
                    cout << "send error";
                    return;
                }
                printf("第%d个文件分组重传完成....\n", i);
            }
            resend_signal = 0; // 传完了，信号归0
            continue;
        }

        mutex_for_queue.lock();
        if (window_queue.size() == window_size || now_index > group_size) // 队列满了，或者已经传完了，可能需要重传
        {
            Sleep(10);
            mutex_for_queue.unlock();
            continue;
        }
        mutex_for_queue.unlock();

        memset(tempbuf, '\0', BUF_SIZE); // 先清空
        printf("第%d个文件分组传输中....\n", now_index);
        memset(&sendbuff, '\0', msg_size); // 清空并打包
        u_short buff_size =
            ((now_index == group_size) ? (size % BUF_SIZE) : BUF_SIZE); // 是否为最后一组

        f_send.read(tempbuf, buff_size); // 读取文件内容，存入buffer
        packdata(&sendbuff, myip, desip, myport, desport,
                 0, 0, 0, now_index, 0, group_size, buff_size, tempbuf); // 发送第index组
        mutex_for_queue.lock();
        window_queue.push(sendbuff); // 入队
        if (sendto(sockSend, (char *)&sendbuff, msg_size, 0, (struct sockaddr *)&address_des, addlen) == -1)
        {
            cout << "send error";
            return;
        }
        printf("第%d个文件分组发送完成,当前队列大小%d,base:%d....\n", now_index, window_queue.size(), window_base);
        mutex_for_queue.unlock();
        now_index++; // 准备发送下一个
    }
    cout << "\n传输完成!!!\n";
    int tuntulv;
    float totalt;
    const clock_t end_time = clock();
    totalt = (float)(end_time - start_time) / CLOCKS_PER_SEC;
    tuntulv = size / totalt / 1000;
    printf("发送字节:%d\n组数:%d\n每组大小:%d\n吞吐率:%dkb/s\n总时间:%.5fs\n\n",
           group_size * BUF_SIZE, group_size, BUF_SIZE, tuntulv, totalt);
    f_send.close();
}

bool byebye()
{
    // 第一次挥手手,FIN=1
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
