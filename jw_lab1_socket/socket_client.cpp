#include <iostream>
#include <thread>
#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")
using namespace std;

int endflag = 1;
void print_info();
void recvmsg(SOCKET myskt);
void sendmsg(SOCKET myskt);

int main()
{

    WSADATA wdata;
    WORD version = MAKEWORD(2, 2); //定义socket版本
    if (WSAStartup(version, &wdata))
    {
        cout << "初始化失败\n";
        WSACleanup();
        return -1;
    }

    SOCKET sock_Client = socket(AF_INET, SOCK_STREAM, 0); // Ipv4,TCP
    if (sock_Client == -1)
    {
        cout << "创建Socket失败";
        WSACleanup();
        return -1;
    }

    sockaddr_in addr;                                   //地址结构体
    addr.sin_family = AF_INET;                          // Ipv4
    addr.sin_port = htons(9999);                        // Port
    addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1"); // addr

    if (connect(sock_Client, (sockaddr *)&addr, sizeof(addr)) == -1)
    {
        cout << "连接服务器失败" << endl;
        WSACleanup();
        return -1;
    }

    thread t_recvmsg(recvmsg, sock_Client);
    thread t_sendmsg(sendmsg, sock_Client);
    t_recvmsg.detach();
    t_sendmsg.detach();

    while (1)
    {
        Sleep(1000);
        cout<<"wait\n";
        if (endflag == 0){
            cout<<"\nexit\n";
            break;
        }
    }
    closesocket(sock_Client);
    WSACleanup();
}

void print_info()
{
    cout << 1;
}

void recvmsg(SOCKET myskt)
{
    while (1)
    {
        char buf_rec[255];
        if (recv(myskt, buf_rec, sizeof(buf_rec), 0) <= 0)
        {
            WSACleanup();
            cout << "接收服务器数据失败" << endl;
            break;
        }
        cout << "来自服务器：" << buf_rec << endl;
    }
}

void sendmsg(SOCKET myskt)
{
    while (1)
    {   
        char buf_rec[255];
        cin>>buf_rec;
        if(buf_rec[0]=='0')
            endflag=0;
        if (send(myskt, buf_rec, strlen(buf_rec), 0) <= 0)
        {
            WSACleanup();
            cout << "发送客户端数据失败" << endl;
            break;
        }
        else{
            cout<<"\nsuccess\n";
        }
    }
}