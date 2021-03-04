
/*****************************************************************************
*                                                                            *
*  @file     RTParserImpl.h                                                  *
*  @brief    RTParser implement class declaration                            *
*                                                                            *
*  Details.                                                                  *
*                                                                            *
*  @author   ZhiGao.Wu                                                       *
*  @email    wuzhigaoem@gmail.com                                            *
*  @date     2019/05/08                                                      *
*                                                                            *
*----------------------------------------------------------------------------*
*  Remark   :                                                                *
*                                                                            *
*****************************************************************************/

#ifndef __RTP_CLIENT_HEADER_H__
#define __RTP_CLIENT_HEADER_H__

#include "Common.h"

#include "jrtplib3/rtpsession.h"
#include "jrtplib3/rtpudpv4transmitter.h"
#include "jrtplib3/rtpudpv6transmitter.h"
#include "jrtplib3/rtptcptransmitter.h"
#include "jrtplib3/rtpipv4address.h"
#include "jrtplib3/rtptcpaddress.h"
#include "jrtplib3/rtpsessionparams.h"
#include "jrtplib3/rtperrors.h"
#include "jrtplib3/rtplibraryversion.h"
#include "jrtplib3/rtpsourcedata.h"

#include <iostream>

#include <thread>
#include <mutex>
#include <condition_variable>

#include <queue>

using namespace jrtplib;

class RtpClient
{
private:
    class RTPTCPSession : public RTPSession
    {
    public:
        RTPTCPSession() : RTPSession() { }
        ~RTPTCPSession() { }
    protected:
        void OnValidatedRTPPacket(RTPSourceData *srcdat, RTPPacket *rtppack, bool isonprobation, bool *ispackethandled)
        {
            printf("SSRC %x Got packet (%d bytes) in OnValidatedRTPPacket from source 0x%04x!\n", GetLocalSSRC(),
                (int)rtppack->GetPayloadLength(), srcdat->GetSSRC());
            DeletePacket(rtppack);
            *ispackethandled = true;
        }

        void OnRTCPSDESItem(RTPSourceData *srcdat, RTCPSDESPacket::ItemType t, const void *itemdata, size_t itemlength)
        {
            char msg[1024];

            memset(msg, 0, sizeof(msg));
            if (itemlength >= sizeof(msg))
                itemlength = sizeof(msg) - 1;

            memcpy(msg, itemdata, itemlength);
            printf("SSRC %x Received SDES item (%d): %s from SSRC %x\n", GetLocalSSRC(), (int)t, msg, srcdat->GetSSRC());
        }
    };

    class RTSPTCPTransmitter : public RTPTCPTransmitter
    {
    public:
        RTSPTCPTransmitter()
            : RTPTCPTransmitter(NULL)
        { }

        void OnSendError(SocketType sock)
        {
            std::cerr << "Error sending over socket " << sock << ", removing destination" << std::endl;
            DeleteDestination(RTPTCPAddress(sock));
        }

        void OnReceiveError(SocketType sock)
        {
            std::cerr << "Error receiving from socket " << sock << ", removing destination" << std::endl;
            DeleteDestination(RTPTCPAddress(sock));
        }
    };

    typedef RTPSession RTPUDPSession;

public:
    RtpClient();
    ~RtpClient();

    int Create(SOCKET fd, int time_rate);
    int Create(const Endpoint& server, const Endpoint& client, int time_rate);
    void Destroy();

    int FetchData(unsigned char* data, int needed);
    void ClearData();

private:
    void Run();

private:
    void FeedData(const std::list<RTPPacket*>& packets);

private:
    RTPSessionParams _session_param;

private:
    RTPUDPv4TransmissionParams _udp_v4;
    RTPUDPSession _udp_session;

private:
    RTSPTCPTransmitter* _tcp_v4;
    RTPTCPSession _tcp_session;

private:
    bool _running;
    std::thread _thread;
    std::mutex _locker;
    std::condition_variable _condition;

    struct Payload
    {
        RTPPacket* packet = nullptr;

        unsigned char* head = nullptr;
        int size = 0;
        unsigned char* curr = nullptr;
        int len = 0;

        Payload(RTPPacket* packet)
        {
            this->packet = packet;
            //this->head = packet->GetPayloadData();
            //this->size = (int)(packet->GetPayloadLength());
            this->head = packet->GetPacketData();
            this->size = (int)(packet->GetPacketLength());
            this->curr = head;
            this->len = size;
        }
    };
    std::queue<Payload> _payloads;
    
private:
    RtpClient& operator=(RtpClient& rhs);
};

#endif

