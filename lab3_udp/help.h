#ifndef _TEST_H_
#define _TEST_H_

const int BUF_SIZE = 4068;
const int msg_size = 4096;

#pragma pack(1) //字节对齐
struct message  //报文格式
{
    DWORD srcIP, desIP;       // IP
    u_short srcPort, desPort; // port
    BYTE ACK;                 //三个控制位
    BYTE SYN;
    BYTE FIN;
    BYTE nothing;       //没想好，为了对齐占位
    u_short seq;        //消息序号
    u_short ack;        //=ack+1
    u_short msgnumber;  //消息总数
    u_short filesize;   //文件大小（去头
    u_short checksum;   //校验和
    u_short nothing2;   //没想好，对齐占位
    char msg[BUF_SIZE]; // 4096-32=4064
};
#pragma pack()

u_short checksum(message *m, int len)
{
    len /= sizeof(u_short);
    u_short *temp = (u_short *)m;
    u_int sum = 0;
    u_short result = 0;
    while (len--)
    {
        sum += *temp++; // 以16位为单位累加
    }
    if (sum & 0xffff0000)
    {                       // 如果超出了两个字节，说明有溢出
        result = sum >> 16; // 取前16位
        sum &= 0xffff;
    }
    result += (u_short)sum;
    return ~result;
}

bool packdata(message *m, DWORD srcip, DWORD desip, u_short srcport, u_short desport, BYTE ACK, BYTE SYN, BYTE FIN, u_short seq, u_short ack, u_short msgnum, u_short filesize, char *buf)
{
    memset(m, '\0', msg_size);
    m->srcIP = srcip;
    m->desIP = desip;
    m->srcPort = srcport;
    m->desPort = desport;
    m->ACK = ACK;
    m->SYN = SYN;
    m->FIN = FIN;
    m->seq = seq;
    m->ack = ack;
    m->msgnumber = msgnum;
    m->filesize = filesize;
    if (buf != nullptr)
        memcpy(m->msg, buf, filesize);
    m->checksum = checksum(m, msg_size / sizeof(u_short));
    return true;
}

bool print_msg(message *m)
{
    printf("收到数据包:ACK:%d,SYN:%d,FIN:%d,seq:%d,ack:%d,cksum:%d\n",
           m->ACK, m->SYN, m->FIN, m->seq, m->ack, m->checksum);
    return true;
}

#endif