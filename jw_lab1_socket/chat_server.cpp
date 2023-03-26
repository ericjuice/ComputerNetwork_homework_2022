#include <iostream>
#include <thread>
#include <WinSock2.h>
#include <windows.h>
#include <map>
#include <list>
#include "help.h"
#include "protocol.h"

#pragma comment(lib, "ws2_32.lib")
using namespace std;
bool notend = true; //全局结束信号

struct UserItem
{
    SOCKET *conn; // 8字节
    char name[16];
};
struct RoomItem
{
    int id;
    char name[16];
};

class server
{
private:
    SOCKET skt_server;
    map<SOCKET *, int> userskt_roomid; //一些全局表
    map<int, list<int>> roomid_usersktlist;
    map<int, UserItem> usermap;
    map<int, RoomItem> roommap;
    //当前用户和房间数可以通过map.size()找出

public:
    int launch(); //启动并初始化socket，并开始监听
    int server_ctrl();
    int recvmsg(SOCKET *skt); //接受id用户的消息
    int creat_room();
    int user_selected_room(PktStruct *recvpkt, SOCKET *skt);
    int sendmsg(PktStruct *sendpkt);
};

int main()
{
    cout << YOUCHAT_s;
    server myserver = server();
    myserver.launch();
    cout << "shutdown and exit!\n";
}

int server::launch()
{
    WSADATA wdata;
    WORD version = MAKEWORD(2, 0); //定义socket版本
    if (WSAStartup(version, &wdata))
    {
        cout << "初始化失败\n";
        WSACleanup();
        return -1;
    }
    skt_server = socket(AF_INET, SOCK_STREAM, 0); // Ipv4,TCP
    if (skt_server == -1)
    {
        cout << "创建Socket失败";
        WSACleanup();
        return -1;
    }
    sockaddr_in addr;                                            //地址结构体
    addr.sin_family = AF_INET;                                   //地址为IPv4协议
    addr.sin_port = htons(9999);                                 //端口为9999
    addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);               //绑定本机的地址
    if (bind(skt_server, (sockaddr *)&addr, sizeof(addr)) == -1) //绑定skt
    {
        cout << "绑定地址端口失败";
        WSACleanup();
        return -1;
    }
    if (listen(skt_server, 5) == -1) // 5为socket队列最大长度
    {
        cout << "监听Socket失败";
        WSACleanup();
        return -1;
    }
    //以上为常规初始化

    thread t_ctrl(server::server_ctrl, this); //起个线程负责服务器的命令
    t_ctrl.detach();

    //下面循环监听连接请求，一层循环对应一个用户专属线程
    while (notend == 1)
    {
        cout << "监听ing....\n";
        sockaddr addrCli;
        int len = sizeof(addrCli);
        SOCKET *skt_conn = new SOCKET; // 一开始想new一个skt，防止被销毁，但是发现socket实际上是一个数
        *skt_conn = accept(skt_server, &addrCli, &len);
        if (*skt_conn == -1)
        {
            cout << "客户端失联";
            continue;
        }
        UserItem newuser = {//新建个用户并加入map
                            .conn = skt_conn};
        usermap[*skt_conn] = newuser;
        printf("新用户加入,SKT:%d\n", *skt_conn);

        thread t_recvmsg(server::recvmsg, this, skt_conn); //起个线程接收客户端
        t_recvmsg.detach();
    }
    return 0;
}

int server::recvmsg(SOCKET *skt)
{
    SOCKET *skt_conn = (SOCKET *)skt; //取出这个skt
    while (notend == 1)
    {
        char buf_rec[512];
        char buf_send[492];
        cout << "继续接收客户端" << *skt << endl;
        cout << "--------------------\n";
        PktStruct msg_recv; //准备接收的结构体
        if (!RecvAll(skt_conn, buf_rec, sizeof(buf_rec)))
        {
            cout << "接收客户端数据失败" << endl;
            return -1;
        }
        cout << "收到" << *skt_conn << "用户消息,解析结果：\n";
        memcpy(&msg_recv, buf_rec, sizeof(buf_rec)); //拷贝buf，转为结构体

        switch (msg_recv.ctrl) //根据指令选择功能
        {
        case LOOKUP_ROOM:
        {
            cout << "用户查询房间，正在查询\n";
            sprintf(buf_send, "在线房间数:%d,房间列表:\n", roommap.size()); //打印格式：房间数：X\n列表1:XXX,2:xxx
            for (auto iter : roommap)
            {
                char roominfo[64];
                sprintf(roominfo, "房间号:%d,房间名：%s\n", iter.second.id, iter.second.name);
                strcat(buf_send, roominfo);
            }
            cout << "正在打包发送房间数据,";
            PktStruct p = packmsg(MSG_to_sigle, 0, *skt_conn, buf_send);
            sendmsg(&p);
            cout << "已发送房间列表,等待回应\n";
            break;
        }
        case LOOKUP_member:
        {
            cout << "用户查询房间用户，正在查询\n";
            sprintf(buf_send, "当前房间在线用户数:%d,用户列表:\n", roomid_usersktlist[msg_recv.des].size()); //打印格式：房间数：X\n列表1:XXX,2:xxx
            for (auto iter : roomid_usersktlist[msg_recv.des])
            {
                char roominfo[64];
                sprintf(roominfo, "用户id号:%d,用户名：%s\n", iter, usermap[iter].name);
                strcat(buf_send, roominfo);
            }
            cout << "正在打包发送房间用户数据,";
            PktStruct p = packmsg(MSG_to_sigle, 0, *skt_conn, buf_send);
            sendmsg(&p);
            cout << "已发送用户列表\n";
            break;
        }
        case JOIN_ROOM:
        {
            cout << "用户已选择房间,";
            user_selected_room(&msg_recv, skt_conn); //交给函数处理
            break;
        }
        case ERROR_msg:
            cout << "收到bugSOCKET\n";
            break;
        default:
        {
            cout << "转发信息\n";
            sendmsg(&msg_recv);
            break;
        }
        }
    }
    return 0;
}

int server::server_ctrl()
{
    cout << "[command]服务器命令---3:help---2:关机---1:群发消息---0:创建房间\n";
    while (notend)
    {
        int command;
        cin >> command;
        switch (command)
        {
        case 0:
            creat_room();
            break;
        case 2:
            notend = 0;
            break;
        case 1:
        {
            char buf_rec[492];
            cout << "服务器输入消息：";
            cin >> buf_rec;
            PktStruct p = packmsg(MSG_to_all, 0, 0, buf_rec);
            sendmsg(&p);
            cout << "\nsuccess!\n";
            break;
        }
        case 3:
            cout << "[command]服务器命令---0:创建房间---1:群发消息---2:关机---3:help\n";
            break;
        default:
            cout << "错误，请重试\n";
        }
        printf("操作完成，当前用户数：%d ，房间数：%d\n", usermap.size(), roommap.size());
    }

    return 0;
}

int server::sendmsg(PktStruct *sendpkt)
{
    char newMsgBuf[492];  //新定义个变量，因为是加在头部
    switch (sendpkt->src) //先按照来源添加消息头
    {
    case -1:
        cout << "转发消息错误\n";
        return -1;
    case 0:
        strcpy(newMsgBuf, "[来自服务器]");
        break;
    default:
        sprintf(newMsgBuf, "[%d][%s]", (int)sendpkt->src, usermap[sendpkt->src].name);
        break;
    }
    strcat(newMsgBuf, sendpkt->MsgBuf); //连接然后拷贝
    strcpy(sendpkt->MsgBuf, newMsgBuf);

    switch (sendpkt->ctrl)
    {
    case MSG_to_sigle:
        SendAll(usermap[sendpkt->des].conn, (char *)sendpkt, 512);
        cout << sendpkt->src << "->" << sendpkt->des << "的消息单发成功\n";
        break;
    case MSG_to_group:
    {
        list<int> ls = roomid_usersktlist[sendpkt->des]; //所发组的id
        for (auto i : ls)
        {
            SendAll(usermap[i].conn, (char *)sendpkt, 512);
        }
        cout << sendpkt->src << "->" << sendpkt->des << "的消息组发成功\n";
        break;
    }
    case MSG_to_all:
    {
        for (auto i : usermap)
        {
            SendAll(i.second.conn, (char *)sendpkt, 512);
        }
        cout << sendpkt->src << "->" << sendpkt->des << "的消息群发成功\n";
    }
    break;
    }
    return 0;
}

int server::user_selected_room(PktStruct *recvpkt, SOCKET *userskt)
{
    SOCKET *skt_conn = userskt;
    int roomid = recvpkt->des;                  //不是转发，ID放目标房间号
    if (roomid_usersktlist[roomid].size() == 0) //房间还没人，先初始化个列表
    {
        roomid_usersktlist[roomid] = list<int>();
    }
    roomid_usersktlist[roomid].push_back(*skt_conn);
    userskt_roomid[skt_conn] = roomid;                //绑定map
    strcpy(usermap[*skt_conn].name, recvpkt->MsgBuf); //记录名字
    printf("ID%d用户'%s'加入房间%d\n", *skt_conn, usermap[*skt_conn].name, roomid);
    char msg[492];
    sprintf(msg, "ID%d用户'%s'加入房间%d\n", *skt_conn, usermap[*skt_conn].name, roomid);
    PktStruct p = packmsg(MSG_to_group, 0, roomid, msg);
    sendmsg(&p);
    cout << "已通知客户端们\n";
    return 0;
}
int server::creat_room()
{
    int maxusernum;
    char name[16];
    int pwd;
    cout << "输入房间名(16字节内)\n";
    cin >> name;
    RoomItem newroom = {
        .id = (int)roommap.size()};
    strcpy(newroom.name, name);
    roommap[newroom.id] = newroom;
    return 0;
}
