#include <iostream>
#include <thread>
#include <WinSock2.h>
#include <windows.h>
#include "protocol.h"
#include "help.h"
#pragma comment(lib, "ws2_32.lib")
using namespace std;

bool notend = true;
class client
{
private:
    int id;
    int room;
    char name[16]; //发消息时名字放消息头
    SOCKET conn;

public:
    int select_room_and_join();
    int launch();
    int sendmsg();
    int recvmsg();
    int printmsg(PktStruct *msg);
};

int main()
{
    cout<<YOUCHAT_c;
    client myclient = client();
    myclient.launch();
    thread t_recv(client::recvmsg, myclient); //起个线程负责接收
    t_recv.detach();
    thread t_sd(client::sendmsg, myclient); //起个线程负责发送
    t_sd.detach();
    Sleep(500);    //让子弹飞一会
    while (notend) //防止主线程结束
    {
        cout << "I'm sleeping....\n";
        Sleep(30000);
    }
    cout << "exit!\n";
}

int client::launch()
{
    WSADATA wdata;
    WORD version = MAKEWORD(2, 0); //定义socket版本
    if (WSAStartup(version, &wdata))
    {
        cout << "初始化失败\n";
        WSACleanup();
        return -1;
    }

    conn = socket(AF_INET, SOCK_STREAM, 0); // Ipv4,TCP
    if (conn == -1)
    {
        cout << "创建Socket失败";
        WSACleanup();
        return -1;
    }

    sockaddr_in addr;                                   //地址结构体
    addr.sin_family = AF_INET;                          // Ipv4
    addr.sin_port = htons(9999);                        // Port
    addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1"); // addr

    if (connect(conn, (sockaddr *)&addr, sizeof(addr)) == -1)
    {
        cout << "连接服务器失败" << endl;
        WSACleanup();
        return -1;
    }

    select_room_and_join();
    return 0;
}

int client::select_room_and_join()
{
    cout << "连接服务器成功，请按指示操作:输入1:查询房间;0:退出\n";
    int select;
    cin >> select;
    switch (select)
    {
    case 0:
        return 0;
        notend=0;
    case 1:
    {
        //查询房间
        char tmp[492];
        PktStruct p = packmsg(LOOKUP_ROOM, -1, 0, tmp);
        if (!SendAll(&conn, (char *)&p, 512))
        {
            cout << "发送客户端数据失败" << endl;
            break;
        }
        cout << "查询房间请求发送成功，等待服务器返回\n";
        char buf_rec[512];
        if (!RecvAll(&conn, buf_rec, sizeof(buf_rec))) //收到回复(包含id)
        {
            WSACleanup();
            cout << "接收服务器数据失败" << endl;
            break;
        }
        //准备接收的结构体
        PktStruct msg_recv;
        memcpy(&msg_recv, buf_rec, sizeof(buf_rec)); //拷贝buf，转为结构体
        cout << "收到服务器结果：\n";
        id = msg_recv.des;
        cout << "您已获得ID:" << id << endl; //实际上是服务器的socket
        printmsg(&msg_recv);                 //格式化打印
        Sleep(500);
        cout << "请输入要加入的房间id\n";
        cin >> room;
        cout << "你的昵称:\n";
        cin >> name;
        PktStruct p2 = packmsg(JOIN_ROOM, id, room, name);
        if (!SendAll(&conn, (char *)&p2, 512))
        {
            cout << "发送加入请求失败" << endl;
            break;
        }
        cout << "加入房间请求已发送，等待响应\n";
    }
        char buf_rec2[512];
        if (!RecvAll(&conn, buf_rec2, sizeof(buf_rec2)))
        {
            WSACleanup();
            cout << "接收服务器数据失败" << endl;
            return -1;
        }
        PktStruct msg_recv2;
        cout << "服务器已响应\n";
        memcpy(&msg_recv2, buf_rec2, sizeof(buf_rec2)); //拷贝buf，转为结构体
        printmsg(&msg_recv2);
        cout << "加入房间成功，可以开始发消息\n";
        return 0;
    }
    return 0;
}

int client::printmsg(PktStruct *msg)
{
    char head[32];
    string str;
    switch (msg->ctrl)
    {
    case JOIN_ROOM:
    {
        str = "[加入房间]";
        break;
    }
    case MSG_to_all:
    {
        str = "【全局广播】:";
        break;
    }
    case MSG_to_group:
    {
        str = "[群聊]";
        break;
    }
    case MSG_to_sigle:
    {
        str = "[私聊]";
        break;
    }
    default:
        break;
    }

    int hour = msg->timestmp / 10000;
    int minute = (msg->timestmp % 10000) / 100;
    int sec = msg->timestmp % 100;
    cout << str << hour << ':' << minute << ':' << sec << ':' << msg->MsgBuf << endl;
    return 0;
}

int client::recvmsg()
{
    while (notend)
    {
        char buf_rec2[512];
        if (!RecvAll(&conn, buf_rec2, sizeof(buf_rec2)))
        {
            WSACleanup();
            cout << "接收服务器数据失败" << endl;
            return -1;
        }
        PktStruct msg_recv2;
        memcpy(&msg_recv2, buf_rec2, sizeof(buf_rec2)); //拷贝buf，转为结构体
        printmsg(&msg_recv2);
    }
    return 0;
}

int client::sendmsg()
{
    cout << "[command]客户端命令---0:私聊---1:群聊---2:关机---3:help---4:查询在线用户---5:全局广播(VIP功能)\n";
    while (notend)
    {
        int command;
        cin >> command;
        switch (command)
        {
        case 0:
        {
            char buf_rec[492];
            cout << "[私聊]输入你要联系的ID:";
            int pid;
            cin >> pid;
            cout << "[私聊]输入消息:";
            cin >> buf_rec;
            PktStruct p = packmsg(MSG_to_sigle, id, pid, buf_rec);
            SendAll(&conn, (char *)&p, 512);
            break;
        }
        case 2:
            notend = 0;
            cout<<"已退出";
            break;
        case 1:
        {
            char buf_rec[492];
            cout << "[群聊]输入消息：";
            cin >> buf_rec;
            PktStruct p = packmsg(MSG_to_group, id, room, buf_rec);
            SendAll(&conn, (char *)&p, 512);
            break;
        }
        case 3:
            cout << "[command]客户端命令---0:私聊---1:群聊---2:关机---3:help---4:查询在线用户---5:全局广播(VIP功能)\n(每次最多100字O o 。.!)\n";
            break;
        case 4:
        {
            cout << "查询中....\n";
            char buf_rec[492];
            PktStruct p = packmsg(LOOKUP_member, id, room, buf_rec);
            SendAll(&conn, (char *)&p, 512);
            break;
        }
        case 5:{
            cout<<"您正在使用VIP全局广播(限时体验)...\n";
            char buf_rec[492];
            cout << "[全局]输入消息：";
            cin >> buf_rec;
            PktStruct p = packmsg(MSG_to_all, id, 0, buf_rec);
            SendAll(&conn, (char *)&p, 512);
            break;
        }
        default:
            cout << "错误，请重试\n";
        }
        printf("操作完成，当前房间：%d ，昵称：%s,id:%d\n", room, name,id);
        cout << "--------------------------\n";
    }

    return 0;
}