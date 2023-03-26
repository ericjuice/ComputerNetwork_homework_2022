#include <iostream>
#include <WinSock2.h>
#include <windows.h>
#include <thread>
#pragma comment(lib,"ws2_32.lib")

using namespace std;

int endflag=1;
void recvmsg(SOCKET myskt);
void sendmsg(SOCKET myskt);

int main()
{

    WSADATA wdata;
    WORD version = MAKEWORD(2, 0); //定义socket版本
    if (WSAStartup(version, &wdata))
    {
        cout << "初始化失败\n";
        WSACleanup();
        return -1;
    }

    SOCKET sock_Serv = socket(AF_INET, SOCK_STREAM, 0); // Ipv4,TCP
    if (sock_Serv == -1)
    {
        cout << "创建Socket失败";
        WSACleanup();
        return -1;
    }

// #define _WINSOCK_DEPRECATED_NO_WARNINGS                 // vs环境下必须定义，否则无法使用inet_addr函数
    sockaddr_in addr;                                   //地址结构体
    addr.sin_family = AF_INET;                          //地址为IPv4协议
    addr.sin_port = htons(9999);                        //端口为9999
    addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1"); //绑定本机的地址
    //绑定
    if (bind(sock_Serv, (sockaddr *)&addr, sizeof(addr)) == -1)
    {
        cout << "绑定地址端口失败";
        WSACleanup();
        return -1;
    }

    if (listen(sock_Serv, 5) == -1) // 5为socket队列最大长度
    {
        cout << "监听Socket失败";
        WSACleanup();
        return -1;
    }

    sockaddr addrCli;
    int len = sizeof(addrCli);
    SOCKET sockConn = accept(sock_Serv, &addrCli, &len);
    if (sockConn == -1)
    {
        cout << "接收客户端连接失败";
        WSACleanup();
        return -1;
    }
    thread t_recvmsg(recvmsg, sockConn);
    thread t_sendmsg(sendmsg, sockConn);
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
    closesocket(sockConn);
    closesocket(sock_Serv);
    WSACleanup();
}

void recvmsg(SOCKET myskt)
{
    while (1)
    {
        char buf_rec[255];
        if (recv(myskt, buf_rec, sizeof(buf_rec), 0) <= 0)
        {
            WSACleanup();
            cout << "接收客户端数据失败" << endl;
            break;
        }
        cout << "来自客户端：" << buf_rec << endl;
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
            cout << "发送服务器数据失败" << endl;
            break;
        }
        else{
            cout<<"\nsuccess\n";
        }
    }
}