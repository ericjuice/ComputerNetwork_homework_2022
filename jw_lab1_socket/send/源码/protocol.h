// ctrl字段定义
#ifndef _TEST_H_
#define _TEST_H_
const int LOOKUP_member =-3;
const int LOOKUP_ROOM = -2;
const int JOIN_ROOM = -1;
const int ERROR_msg =0;
const int MSG_to_all = 1;
const int MSG_to_group = 2;
const int MSG_to_sigle = 3;
const int MSG_ctrl = 4;
struct PktStruct // 4+4+4+4+4+492=512字节，为了和buf[512]对齐
{
    int ctrl = ERROR_msg;
    //客户端输入的控制字段
    int src = -1; //源和目标地址,-1代表未初始化,0代表服务器
    int des = -1;
    int timestmp = 0;              // time
    int MsgLen = 0; // msgbuf
    char MsgBuf[492];
};

#endif