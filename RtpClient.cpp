
#include "RtpClient.h"

#include "ErrorCode.h"

#ifdef _MSC_VER
#ifdef RTP_SUPPORT_THREAD
#pragma comment(lib, "jthread.lib")
#endif
#pragma comment(lib, "jrtplib.lib")
#endif

RtpClient::RtpClient()
    : _session_param()
    , _udp_v4(), _udp_session()
    , _tcp_v4(nullptr), _tcp_session()
    , _running(false), _thread(), _locker(), _condition(), _payloads()
{
}

RtpClient::~RtpClient()
{
    if (_thread.joinable())
    {
        _thread.join();
    }
}

int RtpClient::Create(SOCKET fd, int time_rate)
{
    std::lock_guard<std::mutex> lg(_locker);
    if (_running)
    {
        return 0;
    }

    if (!_tcp_v4)
    {
        _tcp_v4 = new RTSPTCPTransmitter();
    }

    bool threadsafe = false;
#ifdef RTP_SUPPORT_THREAD
    threadsafe = true;
#endif // RTP_SUPPORT_THREAD

    _session_param.SetOwnTimestampUnit(1.0 / (double)time_rate);
    _session_param.SetAcceptOwnPackets(true);
    _session_param.SetProbationType(RTPSources::NoProbation);
    _session_param.SetMaximumPacketSize(1500);

    int res = 0;
    if ((res = _tcp_v4->Init(threadsafe)) >= 0 &&
        (res = _tcp_v4->Create(65535, nullptr)) >= 0 &&
        (res = _udp_session.Create(_session_param, _tcp_v4)) >= 0)
    {
        _udp_session.AddDestination(RTPTCPAddress(fd));
    }

    if (res < 0)
    {
        std::cerr << RTPGetErrorString(res) << std::endl;
    }
    else
    {
        _running = true;
        _thread = std::thread(&RtpClient::Run, this);
    }

    return res;
}

int RtpClient::Create(const Endpoint& server, const Endpoint& client, int time_rate)
{
    std::lock_guard<std::mutex> lg(_locker);
    if (_running)
    {
        return 0;
    }

    _session_param.SetOwnTimestampUnit(1.0 / (double)time_rate);
    _session_param.SetAcceptOwnPackets(true);
    _session_param.SetProbationType(RTPSources::NoProbation);

    _udp_v4.SetPortbase(client.rtp_port);
    _udp_v4.SetForcedRTCPPort(client.rtcp_port);
    int res = _udp_session.Create(_session_param, &_udp_v4, RTPTransmitter::IPv4UDPProto);
    if (res < 0)
    {
        std::cerr << RTPGetErrorString(res) << std::endl;
    }
    else
    {
        RTPIPv4Address addr(ntohl(inet_addr(server.address.c_str())), server.rtp_port, server.rtcp_port);
        res = _udp_session.AddDestination(addr);
        if (res < 0)
        {
            std::cerr << RTPGetErrorString(res) << std::endl;
        }
        else
        {
            _running = true;
            _thread = std::thread(&RtpClient::Run, this);
        }
    }
    return res;
}

void RtpClient::Destroy()
{
    std::lock_guard<std::mutex> lg(_locker);
    if (_running)
    {
        _udp_session.BYEDestroy(RTPTime(10, 0), 0, 0);

        _running = false;
        _condition.notify_all();
    }
    if (_tcp_v4)
    {
        _tcp_v4->Destroy();
        delete _tcp_v4;
        _tcp_v4 = nullptr;
    }
}

int RtpClient::FetchData(unsigned char* data, int needed)
{
    int fetched = 0;
#ifdef WAIT_TILL_DATA
    while (needed > 0)
    {
        std::unique_lock<std::mutex> ul(_locker);
        _condition.wait_for(ul, std::chrono::milliseconds(10), [this]() { return !_payloads.empty(); });
        if (!_payloads.empty())
        {
            Payload& payload = _payloads.front();
            if (payload.len > needed)
            {
                memcpy(data + fetched, payload.curr, needed);
                payload.curr += needed;
                payload.len -= needed;

                fetched += needed;
                needed = 0;
            }
            else if (payload.len == needed)
            {
                memcpy(data + fetched, payload.curr, needed);
                fetched += needed;
                needed = 0;

                _udp_session.DeletePacket(payload.packet);
                _payloads.pop();
            }
            else
            {
                memcpy(data + fetched, payload.curr, payload.len);
                fetched += payload.len;
                needed -= payload.len;

                _udp_session.DeletePacket(payload.packet);
                _payloads.pop();
            }
        }
    }
#else
    std::unique_lock<std::mutex> ul(_locker);
    _condition.wait_for(ul, std::chrono::milliseconds(10), [this]() { return !_payloads.empty(); });
    while (!_payloads.empty() && needed > 0)
    {
        Payload& payload = _payloads.front();
        if (payload.len > needed)
        {
            memcpy(data + fetched, payload.curr, needed);
            payload.curr += needed;
            payload.len -= needed;

            fetched += needed;
            needed = 0;
        } 
        else if (payload.len == needed)
        {
            memcpy(data + fetched, payload.curr, needed);
            fetched += needed;
            needed = 0;

            _udp_session.DeletePacket(payload.packet);
            _payloads.pop();
        }
        else
        {
            memcpy(data + fetched, payload.curr, payload.len);
            fetched += payload.len;
            needed -= payload.len;

            _udp_session.DeletePacket(payload.packet);
            _payloads.pop();
        }
    }
#endif
    return fetched;
}

void RtpClient::ClearData()
{
    std::lock_guard<std::mutex> lg(_locker);
    while (!_payloads.empty())
    {
        Payload& payload = _payloads.front();
        _udp_session.DeletePacket(payload.packet);
        _payloads.pop();
    }
}

void RtpClient::Run()
{
    while (_running)
    {
        std::list<RTPPacket*> packets;

        _udp_session.BeginDataAccess();
        // check incoming packets
        if (_udp_session.GotoFirstSourceWithData())
        {
            do
            {
                RTPPacket *pack;
                while ((pack = _udp_session.GetNextPacket()) != NULL)
                {
                    packets.push_back(pack);
                }
            } while (_udp_session.GotoNextSourceWithData());
        }
        _udp_session.EndDataAccess();

        if (!packets.empty())
        {
            FeedData(packets);
        }

#ifndef RTP_SUPPORT_THREAD
        int res = _udp_session.Poll();
        if (res < 0)
        {
            std::cerr << RTPGetErrorString(res) << std::endl;
        }
#endif

        RTPTime::Wait(RTPTime(0, 5000));
    }
}

void RtpClient::FeedData(const std::list<RTPPacket*>& packets)
{
    std::lock_guard<std::mutex> lg(_locker);
    for (RTPPacket* packet : packets)
    {
        std::cout << "recv: " << (int)packet->GetSSRC() << ", " << (int)packet->GetPayloadType() << ", " << packet->GetSequenceNumber() << ", " << packet->GetPacketLength() << std::endl;

        _payloads.emplace(Payload(packet));
    }
    _condition.notify_one();
}


