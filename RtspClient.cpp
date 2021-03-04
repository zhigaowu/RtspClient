//   Copyright 2015-2019 Ansersion
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//

#include "RtspClient.h"

#include "utils.h"
#include "Base64.hh"

#include <sstream>
#include <iostream>
#include <string>

#include <regex>

#include <sys/types.h>
#ifdef _MSC_VER
#include <time.h>
#else
#include <sys/socket.h>
#include <strings.h>
#include <unistd.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>


#define PORT_RTSP                554
#define VERSION_RTSP             "1.0"
#define VERSION_HTTP             "1.1"

#define RECV_BUF_SIZE            (1 << 12)
#define SEARCH_PORT_RTP_FROM     5000 // '5000' is chosen at random(must be a even number)

const char* const HTTP_HEAD_ACCEPT          = "Accept: ";
const char* const HTTP_HEAD_USER_AGENT      = "User-Agent: ";
const char* const HTTP_HEAD_XSESSION_COOKIE = "x-sessioncookie: ";
const char* const HTTP_HEAD_PRAGMA          = "Pragma: ";
const char* const HTTP_HEAD_CACHE_CONTROL   = "Cache-Control: ";
const char* const HTTP_HEAD_CONTENT_TYPE    = "Content-Type: ";
const char* const HTTP_HEAD_CONTENT_LENGTH  = "Content-Length: ";
const char* const HTTP_HEAD_EXPIRES         = "Expires: ";


const char* const HTTP_HEAD_VALUE_USER_AGENT      = "RtspClient/0.1.0";
const char* const HTTP_HEAD_VALUE_XSESSION_COOKIE = "";
const char* const HTTP_HEAD_VALUE_ACCEPT          = "application/x-rtsp-tunnelled";
const char* const HTTP_HEAD_VALUE_PRAGMA          = "no-cache";
const char* const HTTP_HEAD_VALUE_CACHE_CONTROL   = "no-cache";
const char* const HTTP_HEAD_VALUE_CONTENT_TYPE    = "application/x-rtsp-tunnelled";
const char* const HTTP_HEAD_VALUE_CONTENT_LENGTH  = "32767";
const char* const HTTP_HEAD_VALUE_EXPIRES         = "Sun, 9 Jan 1972 00:00:00 GMT";

const char* const HTTP_HEAD_USER_AGENT_TUNNEL = "User-Agent: %s\r\nx-sessioncookie: %s\r\nAccept: application/x-rtsp-tunnelled\r\nPragma: no-cache\r\nCache-Control: no-cache\r\n";

#define Check_Socket_Return_Error(fd) if(fd == INVALID_SOCKET) return RTSP_INVALID_URI
#define Check_Socket_Return(fd) if(fd == INVALID_SOCKET) return RTSP_SOCKET_INIT

#define Close_Socket(fd) if(fd != INVALID_SOCKET) { closesocket(fd); fd = INVALID_SOCKET; }


bool RtspClient::checkRtspUri(const std::string& uri)
{
    static const std::regex rtsp_with_user_password("(rtsp://)(.+):(.*)@(.+)");
    static const std::regex rtsp_without_user_password("(rtsp://)(.+)");

    bool res = true;
    do 
    {
        std::smatch matchs;
        if (std::regex_match(uri, matchs, rtsp_with_user_password))
        {
            _username = matchs[2].str();
            _password = matchs[3].str();
            _uri_without_user_info = matchs[1].str() + matchs[4].str();
        }
        else if (std::regex_match(uri, matchs, rtsp_without_user_password))
        {
            _uri_without_user_info = uri;
        }
        else
        {
            res = false;
        }
    } while (false);
    return res;
}


void RtspClient::parseAddressAndPort(const std::string& uri)
{
    static const std::regex rtsp_domain_with_param("(rtsp://)(.+)/(.*)");
    static const std::regex rtsp_domain_without_param("(rtsp://)(.+)");

    std::string domain;

    std::smatch matchs;
    if (std::regex_match(uri, matchs, rtsp_domain_with_param))
    {
        domain = matchs[2].str();
    }
    else if (std::regex_match(uri, matchs, rtsp_domain_without_param))
    {
        domain = matchs[2].str();
    }
    else
    {
        std::cerr << "parse address and port error: " << uri << std::endl;
    }

    std::string::size_type pos = domain.find(':');
    if (std::string::npos != pos)
    {
        _address = domain.substr(0, pos);
        _port = (uint16_t)atoi(domain.substr(pos + 1).c_str());
    }
    else
    {
        _address = domain;
    }
}

ErrorType RtspClient::connectToRtspServer()
{
    _rtsp_socket = socket(AF_INET, SOCK_STREAM, 0);
    Check_Socket_Return(_rtsp_socket);

    struct sockaddr_in serv_addr;
    // Connect to server
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(_port);
    serv_addr.sin_addr.s_addr = inet_addr(_address.c_str());

    if (connect(_rtsp_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0 && errno != EINPROGRESS)
    {
        Close_Socket(_rtsp_socket);
        return RTSP_SOCKET_CONNECT;
    }
    return RTSP_NO_ERROR;
}

ErrorType RtspClient::checkSockWritable(SOCKET sockfd, struct timeval * tval)
{
    // fd_set Wset;
    // struct timeval Tval;
    // FD_ZERO(&Wset);
    // FD_SET(sockfd, &Wset);
    // if(!tval) {
    //     Tval.tv_sec = SELECT_TIMEOUT_SEC;
    //     Tval.tv_usec = SELECT_TIMEOUT_USEC;
    // } else {
    //     Tval = *tval;
    // }

    // while(select(sockfd + 1, NULL, &Wset, NULL, &Tval) != 0) {
    //     if(FD_ISSET(sockfd, &Wset)) {
    //         return CHECK_OK;
    //     }
    //     return CHECK_ERROR;
    // }   
    // printf("Select Timeout\n");
    // return CHECK_ERROR;
    return RTSP_NO_ERROR;
}

ErrorType RtspClient::checkSockReadable(SOCKET sockfd, struct timeval * tval)
{
    // fd_set Rset;
    // struct timeval Tval;
    // FD_ZERO(&Rset);
    // FD_SET(sockfd, &Rset);
    // if(!tval) {
    //     Tval.tv_sec = SELECT_TIMEOUT_SEC;
    //     Tval.tv_usec = SELECT_TIMEOUT_USEC;
    // } else {
    //     Tval = *tval;
    // }

    // while(select(sockfd + 1, &Rset, NULL, NULL, &Tval) != 0) {
    //     if(FD_ISSET(sockfd, &Rset)) {
    //         return CHECK_OK;
    //     }
    //     return CHECK_ERROR;
    // }   
    // printf("Select Timeout\n");
    // return CHECK_ERROR;
    return RTSP_NO_ERROR;
}

ErrorType RtspClient::sendRTSP(SOCKET fd, const std::string& msg)
{
    int SendResult = 0;
    int Index = 0;
    ErrorType Err = RTSP_NO_ERROR;

    int size = (int)msg.size();
    char* data = (char*)msg.data();
    while (Index < size) {
        if (RTSP_NO_ERROR != checkSockWritable(fd))
        {
            Err = RTSP_SEND_ERROR;
            break;
        }
        SendResult = Writen(fd, data + Index, size);
        if (SendResult < 0)
        {
            if (errno == EINTR) continue;
            else if (errno == EWOULDBLOCK || errno == EAGAIN) continue;
            else {
                Err = RTSP_SEND_ERROR;
                break;
            }
        }
        else if (SendResult == 0)
        {
            Err = RTSP_SEND_ERROR;
            break;
        }
        Index += SendResult;
    }

    return Err;
}

ErrorType RtspClient::sendRTSP(const std::string& msg)
{
    if (_over_http_data_port != 0) {
        char * encodedBytes = base64Encode(msg.c_str(), (unsigned int)msg.length());
        if (NULL == encodedBytes)
        {
            return RTSP_SEND_ERROR;
        }
        if (RTSP_NO_ERROR != sendRTSP(_over_http_data_socket, std::string(encodedBytes)))
        {
            delete[] encodedBytes;
            return RTSP_SEND_ERROR;
        }
        delete[] encodedBytes;
    }
    else {
        if (RTSP_NO_ERROR != sendRTSP(_rtsp_socket, msg))
        {
            return RTSP_SEND_ERROR;
        }
    }
    return RTSP_NO_ERROR;
}

ErrorType RtspClient::recvRTSP(SOCKET fd, std::string& msg)
{
    int RecvResult = 0;
    int Index = 0;
    ErrorType Err = RTSP_NO_ERROR;

    msg.resize(RECV_BUF_SIZE);
    char* data = (char*)msg.data();
    while (Index < RECV_BUF_SIZE)
    {
        if (RTSP_NO_ERROR != checkSockReadable(fd))
        {
            Err = RTSP_RECV_ERROR;
            break;
        }
        RecvResult = ReadLine(fd, data + Index, RECV_BUF_SIZE);
        if (RecvResult < 0)
        {
            if (errno == EINTR) continue;
            else if (errno == EWOULDBLOCK || errno == EAGAIN)
            {
                Err = RTSP_NO_ERROR;
                break;
            }
            else
            {
                Err = RTSP_RECV_ERROR;
                break;
            }
        }
        else if (RecvResult == 0)
        {
            Err = RTSP_RECV_ERROR;
            break;
        }

        /*
        * Single with "\r\n" or "\n" is the termination tag of RTSP.
        * */
        if (strcmp(data + Index, "\r\n") == 0 || strcmp(data + Index, "\n") == 0)
        {
            Err = RTSP_NO_ERROR;
            break;
        }
        Index += RecvResult;
    }
    msg.resize(Index);
    return Err;
}

ErrorType RtspClient::recvRTSP(std::string& msg)
{
    if (_over_http_data_port != 0)
    {
        // to do http tunnel
    }
    else
    {
        if (RTSP_NO_ERROR != recvRTSP(_rtsp_socket, msg))
        {
            return RTSP_RECV_ERROR;
        }
    }
    return RTSP_NO_ERROR;
}

ErrorType RtspClient::checkResponse(const std::string& response)
{
    static const std::regex response_pattern("RTSP/([0-9]+\\.[0-9]+)(\\s+)([0-9]+)(\\s+)([.|\\S|\\s]*)");

    int code = 501;
    std::smatch matchs;
    if (std::regex_match(response, matchs, response_pattern))
    {
        code = atoi(matchs[3].str().c_str());
    }

    return (ErrorType)code;
}

ErrorType RtspClient::doAuth(std::string& response, const std::string& cmd, const std::string& uri)
{
    /* RFC2617 */
    ErrorType res = RTSP_NO_ERROR;
    do
    {
        if (_username.empty())
        {
            res = RTSP_USER_EMPTY;
            break;
        }

        static const std::regex digest("WWW-Authenticate: *Digest +realm=\"(.+)\", *nonce=\"([a-zA-Z0-9]+)\"");
        static const std::regex basic("WWW-Authenticate: *Basic +realm=\"(.+)\"");

        std::stringstream Msg("");
        Msg << cmd << " " << _uri << " " << "RTSP/" << VERSION_RTSP << "\r\n";
        Msg << "CSeq: " << ++_CSeq << "\r\n";

        for (std::string::size_type off = 0, pos = response.find("\r\n"); pos != std::string::npos; pos = response.find("\r\n", off))
        {
            std::smatch matchs;
            const std::string& line = response.substr(off, pos - off);
            if (std::regex_match(line, matchs, digest))
            {
                _realm = matchs[1].str();
                _nonce = matchs[2].str();

                Msg << "Authorization: Digest username=\"" << _username << "\", realm=\""
                    << _realm << "\", nonce=\"" << _nonce << "\", uri=\"" << uri
                    << "\", response=\"" << makeMd5DigestResp(_realm, cmd, uri, _nonce) << "\"\r\n";
                break;
            }
            else if (std::regex_match(response, matchs, basic))
            {
                _realm = matchs[1].str();
                Msg << "Authorization: Basic " << makeBasicResp();
                break;
            }
            off = pos + 2;
        }
        Msg << "\r\n";

        res = sendRTSP(Msg.str());
        if (RTSP_NO_ERROR != res)
        {
            break;
        }

        response.resize(0);
        res = recvRTSP(response);
        if (RTSP_NO_ERROR != res)
        {
            break;
        }

        res = checkResponse(response);
    } while (false);

    return res;
}

ErrorType RtspClient::recvSDP(SOCKET sockfd, char* msg, size_t size)
{
    int RecvResult = 0;
    int Index = 0;

    ErrorType Err = RTSP_NO_ERROR;

    memset(msg, 0, size);
    while (size > 0) {
        if (RTSP_NO_ERROR != (Err = checkSockReadable(sockfd)))
        {
            break;
        }
        RecvResult = Readn(sockfd, msg + Index, (int)size);
        if (RecvResult < 0) 
        {
            if (errno == EINTR) continue;
            else if (errno == EWOULDBLOCK || errno == EAGAIN) 
            {
                Err = RTSP_RECV_SDP_ERROR;
                break;
            }
            else 
            {
                Err = RTSP_RECV_SDP_ERROR;
                break;
            }
        }
        else if (RecvResult == 0) 
        {
            Err = RTSP_RECV_SDP_ERROR;
            break;
        }
        Index += RecvResult;
        size -= RecvResult;
    }

    return Err;
}

ErrorType RtspClient::recvSDP(const std::string& response, std::string& msg)
{
    static const std::regex length_pattern("Content-Length: +([0-9]+)");

    for (std::string::size_type off = 0, pos = response.find("\r\n"); pos != std::string::npos; pos = response.find("\r\n", off))
    {
        std::smatch matchs;
        const std::string& line = response.substr(off, pos - off);

        if (std::regex_match(line, matchs, length_pattern))
        {
            msg.resize(atoi(matchs[1].str().c_str()));
        }
        off = pos + 2;
    }

    if (msg.empty())
    {
        std::cerr << "unrecognized response: " << response << std::endl;
        return RTSP_PARSE_SDP_LENGTH_ERROR;
    }

    if (_over_http_data_port != 0)
    {
        return recvSDP(_over_http_data_socket, (char*)msg.data(), msg.size());
    } 
    else
    {
        return recvSDP(_rtsp_socket, (char*)msg.data(), msg.size());
    }
}

void RtspClient::parseSDP(const std::string& sdp)
{
    _sdp_info.Parse(sdp);
}

ErrorType RtspClient::setAvailableRTPPort(uint16_t RTP_port, unsigned short& rtp_port, unsigned short& rtcp_port)
{
    ErrorType res = RTSP_NO_ERROR;

    SOCKET RTPSockfd = INVALID_SOCKET, RTCPSockfd = INVALID_SOCKET;

    struct sockaddr_in servaddr;
    // Create RTP and RTCP UDP socket 
    for (rtp_port = RTP_port; rtp_port < 65535; rtp_port = rtp_port + 2)
    {
        // Bind RTP Port
        if ((RTPSockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        {
            res = RTSP_RTP_PORT_ERROR;
            break;
        }

        memset(&servaddr, 0, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        servaddr.sin_port = htons(rtp_port);
        if (bind(RTPSockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
        {
            Close_Socket(RTPSockfd);
            continue;
        }

        // Bind RTCP Port
        rtcp_port = rtp_port + 1;
        if ((RTCPSockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        {
            Close_Socket(RTPSockfd);
            res = RTSP_RTP_PORT_ERROR;
            break;
        }

        memset(&servaddr, 0, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        servaddr.sin_port = htons(rtcp_port);
        if (bind(RTCPSockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
        {
            Close_Socket(RTPSockfd);
            Close_Socket(RTCPSockfd);
            continue;
        }

        Close_Socket(RTPSockfd);
        Close_Socket(RTCPSockfd);
        break;
    }
    return res;
}

ErrorType RtspClient::doSETUP(const std::string& media_type, bool rtp_over_tcp)
{
    static const std::string Cmd("SETUP");

    ErrorType res = RTSP_NO_ERROR;
    do 
    {
        std::stringstream Msg("");

        std::string control_uri = _sdp_info.GetMediaControlUri(media_type, _uri_without_user_info);
        std::string transport = _sdp_info.GetMediaTransport(media_type);

        Msg << Cmd << " " << control_uri << " " << "RTSP/" << VERSION_RTSP << "\r\n";
        if (_over_http_data_port > 0)
        {
            Msg << "Transport:" << " " << transport << "/TCP;";
            Msg << "interleaved=0-1\r\n";
        }
        else if (rtp_over_tcp)
        {
            Msg << "Transport:" << " " << transport << "/TCP;";
            Msg << "interleaved=0-1\r\n";
        }
        else
        {
            unsigned short rtp_port = 0, rtcp_port = 0;
            res = setAvailableRTPPort(SEARCH_PORT_RTP_FROM, rtp_port, rtcp_port);
            if (RTSP_NO_ERROR != res)
            {
                break;
            }

            _sdp_info.ParseMediaRtpPort(media_type, rtp_port, rtcp_port);

            Msg << "Transport:" << " " << transport << ";";
            Msg << "unicast;" << "client_port=" << rtp_port << "-" << rtcp_port << "\r\n";
        }

        Msg << "CSeq: " << ++_CSeq << "\r\n";
        Msg << HTTP_HEAD_USER_AGENT << HTTP_HEAD_VALUE_USER_AGENT << "\r\n";
        if (_realm.length() > 0 && _nonce.length() > 0) 
        {
            /* digest auth */
            std::string Md5Response = makeMd5DigestResp(_realm, Cmd, control_uri, _nonce);
            if (Md5Response.length() != MD5_SIZE) 
            {
                std::cerr << "Make MD5 digest response error" << std::endl;
                return RTSP_RESPONSE_401;
            }
            Msg << "Authorization: Digest username=\"" << _username << "\", realm=\""
                << _realm << "\", nonce=\"" << _nonce << "\", uri=\"" << control_uri
                << "\", response=\"" << Md5Response << "\"\r\n";
        }
        else if (_realm.length() > 0) 
        {
            /* basic auth */
            Msg << "Authorization: Basic " << makeBasicResp();
        }
        Msg << "\r\n";

        res = sendRTSP(Msg.str());
        if (RTSP_NO_ERROR != res) 
        {
            break;
        }

        std::string response;
        res = recvRTSP(response);
        if (RTSP_NO_ERROR != res) 
        {
            break;
        }

        // check username and password, if any
        res = checkResponse(response);
        if (res != RTSP_RESPONSE_200)
        {
            res = RTSP_NEGOTIATION_AUTH;
            break;
        }
        else
        {
            res = RTSP_NO_ERROR;
        }

        _sdp_info.ParseMediaSessionInfomation(media_type, response);
    } while (false);
    return res;
}

ErrorType RtspClient::doPLAY(const std::string& media_type, double start_time, double* end_time, double* scale)
{
    static const std::string Cmd("PLAY");

    ErrorType res = RTSP_NO_ERROR;
    do
    {
        std::stringstream Msg("");
        Msg << Cmd << " " << _uri << " " << "RTSP/" << VERSION_RTSP << "\r\n";
        if (scale)
        {
            char floatChar[32];
            sprintf(floatChar, "%.1f", *scale);
            Msg << "Scale: " << floatChar << "\r\n";
        }

        char floatChar[32] = { 0 };
        Msg << "Range: npt=";
        sprintf(floatChar, "%.1f", start_time);
        Msg << floatChar;

        Msg << "-";
        if (end_time)
        {
            sprintf(floatChar, "%.1f", *end_time);
            Msg << floatChar;
        }
        Msg << "\r\n";

        Msg << "CSeq: " << ++_CSeq << "\r\n";
        Msg << HTTP_HEAD_USER_AGENT << HTTP_HEAD_VALUE_USER_AGENT << "\r\n";
        Msg << "Session: " << _sdp_info.GetMediaSessionID(media_type) << "\r\n";
        if (_realm.length() > 0 && _nonce.length() > 0)
        {
            /* digest auth */
            std::string Md5Response = makeMd5DigestResp(_realm, Cmd, _uri, _nonce);
            if (Md5Response.length() != MD5_SIZE)
            {
                std::cerr << "Make MD5 digest response error" << std::endl;
                res = RTSP_RESPONSE_401;
                break;
            }
            Msg << "Authorization: Digest username=\"" << _username << "\", realm=\""
                << _realm << "\", nonce=\"" << _nonce << "\", uri=\"" << _uri
                << "\", response=\"" << Md5Response << "\"\r\n";
        }
        else if (_realm.length() > 0)
        {
            /* basic auth */
            std::string BasicResponse = makeBasicResp();
            Msg << "Authorization: Basic " << BasicResponse;
        }
        Msg << "\r\n";

        res = sendRTSP(Msg.str());
        if (RTSP_NO_ERROR != res)
        {
            break;
        }

        std::string response;
        res = recvRTSP(response);
        if (RTSP_NO_ERROR != res)
        {
            break;
        }

        // check username and password, if any
        res = checkResponse(response);
        if (res != RTSP_RESPONSE_200)
        {
            break;
        }
        else
        {
            res = RTSP_NO_ERROR;
        }
    } while (false);

    return res;
}

std::string RtspClient::makeMd5DigestResp(const std::string& realm, const std::string& cmd, const std::string& uri, const std::string& nonce, const std::string& username, const std::string& password)
{
    std::string tmp("");
    char ha1buf[MD5_BUF_SIZE] = { 0 };
    char ha2buf[MD5_BUF_SIZE] = { 0 };
    char habuf[MD5_BUF_SIZE] = { 0 };

    tmp.assign("");
    if (username.empty()) {
        tmp = _username + ":" + realm + ":" + _password;
    }
    else
    {
        tmp = username + ":" + realm + ":" + password;
    }
    Md5sum32((void *)tmp.c_str(), (unsigned char *)ha1buf, tmp.length(), MD5_BUF_SIZE);
    ha1buf[MD5_SIZE] = '\0';

    tmp.assign("");
    tmp = cmd + ":" + uri;
    Md5sum32((void *)tmp.c_str(), (unsigned char *)ha2buf, tmp.length(), MD5_BUF_SIZE);
    ha2buf[MD5_SIZE] = '\0';

    tmp.assign(ha1buf);
    tmp = tmp + ":" + nonce + ":" + ha2buf;
    Md5sum32((void *)tmp.c_str(), (unsigned char *)habuf, tmp.length(), MD5_BUF_SIZE);
    habuf[MD5_SIZE] = '\0';

    tmp.assign("");
    tmp.assign(habuf);

    return tmp;

}

std::string RtspClient::makeBasicResp(const std::string& username, const std::string& password)
{
    std::string tmp("");
    if (username.empty()) {
        tmp = _username + ":" + _password;
    }
    else
    {
        tmp = username + ":" + password;
    }
    char * encodedBytes = base64Encode(tmp.c_str(), (unsigned int)tmp.length());
    if (NULL == encodedBytes) {
        return "";
    }
    tmp.assign(encodedBytes);
    delete[] encodedBytes;

    return tmp;
}

RtspClient::RtspClient()
    : _uri(""), _uri_without_user_info()
    , _address(""), _port(PORT_RTSP)
    , _username(""), _password(""), _realm(""), _nonce("")
    , _rtsp_socket(INVALID_SOCKET)
    , _over_http_data_port(0)
    , _over_http_data_socket(INVALID_SOCKET)
    , _CSeq(0)
    , _sdp(), _sdp_info()
{
    disconnect_callback = NULL;
}

RtspClient::RtspClient(const std::string& uri)
    : _uri(uri), _uri_without_user_info()
    , _address(""), _port(PORT_RTSP)
    , _username(""), _password(""), _realm(""), _nonce("")
    , _rtsp_socket(INVALID_SOCKET)
    , _over_http_data_port(0)
    , _over_http_data_socket(INVALID_SOCKET)
    , _CSeq(0)
    , _sdp(), _sdp_info()
{
    disconnect_callback = NULL;
}

RtspClient::~RtspClient()
{
    _over_http_data_port = 0;
    Close_Socket(_rtsp_socket);
    Close_Socket(_over_http_data_socket);
}

ErrorType RtspClient::DoOPTIONS(const std::string& uri)
{
    static const std::string Cmd = "OPTIONS";

    if (!uri.empty())
    {
        _uri = uri;
    }

    ErrorType res = RTSP_NO_ERROR;
    do 
    {
        if (_uri.empty() || !checkRtspUri(_uri))
        {
            res = RTSP_INVALID_URI;
            break;
        }

        parseAddressAndPort(_uri_without_user_info);

        res = connectToRtspServer();
        if (RTSP_NO_ERROR != res)
        {
            break;
        }

        std::stringstream Msg;
        Msg << Cmd << " " << _uri << " " << "RTSP/" << VERSION_RTSP << "\r\n";
        Msg << "CSeq: " << ++_CSeq << "\r\n";
        Msg << HTTP_HEAD_USER_AGENT << HTTP_HEAD_VALUE_USER_AGENT << "\r\n";
        Msg << "\r\n";

        res = sendRTSP(Msg.str());
        if (RTSP_NO_ERROR != res)
        {
            break;
        }

        std::string response;
        res = recvRTSP(response);
        if (RTSP_NO_ERROR != res)
        {
            break;
        }

        // check username and password, if any
        res = checkResponse(response);
        if (res != RTSP_RESPONSE_200)
        {
            break;
        }
        
        res = RTSP_NO_ERROR;
    } while (false);

    return res;
}

ErrorType RtspClient::DoDESCRIBE()
{
    static const std::string Cmd("DESCRIBE");

    std::stringstream Msg;
    Msg << Cmd << " " << _uri << " " << "RTSP/" << VERSION_RTSP << "\r\n";
    Msg << "CSeq: " << ++_CSeq << "\r\n";
    Msg << HTTP_HEAD_USER_AGENT << HTTP_HEAD_VALUE_USER_AGENT << "\r\n";
    Msg << HTTP_HEAD_ACCEPT << "application/sdp" << "\r\n";
    Msg << "\r\n";

    ErrorType res = sendRTSP(Msg.str());
    do 
    {
        if (RTSP_NO_ERROR != res) 
        {
            break;
        }

        std::string response;
        res = recvRTSP(response);
        if (RTSP_NO_ERROR != res) 
        {
            break;
        }

        res = checkResponse(response);
        if (res != RTSP_RESPONSE_401)
        {
            break;
        }

        res = doAuth(response, Cmd, _uri);
        if (res != RTSP_RESPONSE_200)
        {
            break;
        }

        res = recvSDP(response, _sdp);
        if (RTSP_NO_ERROR != res)
        {
            break;
        }

        parseSDP(_sdp);
    } while (false);
    return res;
}

ErrorType RtspClient::DoSETUP(const std::string& media_type, bool rtp_over_tcp)
{
    ErrorType res = RTSP_NO_ERROR;
    if ("all" == media_type)
    {
        const SDPData::MediaArray& media_array = _sdp_info.GetMedia();
        for (const SDPData::Media& media : media_array)
        {
            res = doSETUP(media.type, rtp_over_tcp);
            if (RTSP_NO_ERROR != res)
            {
                break;
            }
        }
    } 
    else
    {
        res = doSETUP(media_type, rtp_over_tcp);
    }
    return res;
}

ErrorType RtspClient::DoPLAY(const std::string& media_type, double start_time, double* end_time, double* scale)
{
    ErrorType res = RTSP_NO_ERROR;
    if ("all" == media_type)
    {
        const SDPData::MediaArray& media_array = _sdp_info.GetMedia();
        for (const SDPData::Media& media : media_array)
        {
            res = doPLAY(media.type, start_time, end_time, scale);
            if (RTSP_NO_ERROR != res)
            {
                break;
            }
        }
    }
    else
    {
        res = doPLAY(media_type, start_time, end_time, scale);
    }
    return res;
}

ErrorType RtspClient::DoPAUSE()
{
    ErrorType Err = RTSP_NO_ERROR;
    ErrorType ErrAll = RTSP_NO_ERROR;

    /*for(map<string, MediaSession *>::iterator it = MediaSessionMap->begin(); it != MediaSessionMap->end(); it++) {
        Err = DoPAUSE(it->second, false);
        if(RTSP_NO_ERROR == ErrAll) ErrAll = Err; // Remeber the first error
        printf("PAUSE Session %s: %s\n", it->first.c_str(), ParseError(Err).c_str());
    }*/

    return ErrAll;
}

/*
ErrorType RtspClient::DoPAUSE(MediaSession * media_session, bool http_tunnel_no_response)
{
    if(!media_session) {
        return RTSP_INVALID_MEDIA_SESSION;
    }
#ifdef _MSC_VER
    SOCKET Sockfd = INVALID_SOCKET;
#else
    int Sockfd = -1;
#endif

    Sockfd = connectToRtspServer();
    Check_Socket_Return_Error(Sockfd);
    
    string Cmd("PAUSE");
    stringstream Msg("");
    Msg << Cmd << " " << _uri << " " << "RTSP/" << VERSION_RTSP << "\r\n";
    Msg << "CSeq: " << ++_CSeq << "\r\n";
    Msg << "Session: " << media_session->SessionID << "\r\n";
    Msg << "\r\n";

    ErrorType ret;// = SendRTSP(Sockfd, _over_http_data_port, Msg.str());
    if(RTSP_NO_ERROR != ret) {
        return ret;
    }
    if(http_tunnel_no_response) {
        return RTSP_NO_ERROR;
    }
    //ret = RecvRTSP(Sockfd, _over_http_data_port, RtspResponse);
    if(RTSP_NO_ERROR != ret) {
        return ret;
    }
    // check username and password, if any
    //if(CheckAuth(Sockfd, Cmd, _uri) != CHECK_OK) 
    {
        cout << "CheckAuth: error" << endl;
        Close(Sockfd);
        return RTSP_RESPONSE_401;
    }

    // if(RTSP_NO_ERROR == Err && RTSP_NO_ERROR != SendRTSP(Sockfd, Msg.str())) {
    //     Close(Sockfd);
    //     // close(Sockfd);
    //     Sockfd = -1;
    //     Err = RTSP_SEND_ERROR;
    // }
    //if(RTSP_NO_ERROR == Err && RTSP_NO_ERROR != RecvRTSP(Sockfd, &RtspResponse)) {
    //    Close(Sockfd);
    //    // close(Sockfd);
    //    Sockfd = -1;
    //    Err = RTSP_RECV_ERROR;
    //}
    // close(Sockfd);
    return RTSP_NO_ERROR;
}*/

ErrorType RtspClient::DoPAUSE(const std::string& media_type, bool http_tunnel_no_response)
{
    ErrorType Err = RTSP_NO_ERROR;
    bool IgnoreCase = true;

    /*for(it = MediaSessionMap->begin(); it != MediaSessionMap->end(); it++) {
        if(Regex.Regex(it->first.c_str(), media_type.c_str(), IgnoreCase)) break;
    }

    if(it != MediaSessionMap->end()) {
        Err = DoPAUSE(it->second, http_tunnel_no_response);
        return Err;
    }*/
    Err = RTSP_INVALID_MEDIA_SESSION;
    return Err;
}

ErrorType RtspClient::DoGET_PARAMETER()
{
    ErrorType Err = RTSP_NO_ERROR;
    ErrorType ErrAll = RTSP_NO_ERROR;

    /*for(map<string, MediaSession *>::iterator it = MediaSessionMap->begin(); it != MediaSessionMap->end(); it++) {
        Err = DoGET_PARAMETER(it->second, false);
        if(RTSP_NO_ERROR == ErrAll) ErrAll = Err; // Remeber the first error
        printf("GET_PARAMETER Session %s: %s\n", it->first.c_str(), ParseError(Err).c_str());
    }*/

    return ErrAll;
}

/*
ErrorType RtspClient::DoGET_PARAMETER(MediaSession * media_session, bool http_tunnel_no_response)
{
    if(!media_session) {
        return RTSP_INVALID_MEDIA_SESSION;
    }
    // ErrorType Err = RTSP_NO_ERROR;
#ifdef _MSC_VER
    SOCKET Sockfd = INVALID_SOCKET;
#else
    int Sockfd = -1;
#endif

    Sockfd = connectToRtspServer();
    Check_Socket_Return_Error(Sockfd);
    
    string Cmd("GET_PARAMETER");
    stringstream Msg("");
    Msg << Cmd << " " << _uri << " " << "RTSP/" << VERSION_RTSP << "\r\n";
    Msg << "CSeq: " << ++_CSeq << "\r\n";
    Msg << "Session: " << media_session->SessionID << "\r\n";
    Msg << "\r\n";

    ErrorType ret;// = SendRTSP(Sockfd, _over_http_data_port, Msg.str());
    if(RTSP_NO_ERROR != ret) {
        return ret;
    }
    if(http_tunnel_no_response) {
        return RTSP_NO_ERROR;
    }
    //ret = RecvRTSP(Sockfd, _over_http_data_port, RtspResponse);
    if(RTSP_NO_ERROR != ret) {
        return ret;
    }
    // check username and password, if any
    //if(CheckAuth(Sockfd, Cmd, _uri) != CHECK_OK) 
    {
        cout << "CheckAuth: error" << endl;
        Close(Sockfd);
        return RTSP_RESPONSE_401;
    }

    return RTSP_NO_ERROR;
}*/

ErrorType RtspClient::DoGET_PARAMETER(const std::string& media_type, bool http_tunnel_no_response)
{
    ErrorType Err = RTSP_NO_ERROR;
    bool IgnoreCase = true;

    /*for(it = MediaSessionMap->begin(); it != MediaSessionMap->end(); it++) {
        if(Regex.Regex(it->first.c_str(), media_type.c_str(), IgnoreCase)) break;
    }

    if(it != MediaSessionMap->end()) {
        Err = DoGET_PARAMETER(it->second, http_tunnel_no_response);
        return Err;
    }*/
    Err = RTSP_INVALID_MEDIA_SESSION;
    return Err;
}

ErrorType RtspClient::DoTEARDOWN()
{
    static const std::string Cmd("TEARDOWN");
    
    ErrorType res = RTSP_NO_ERROR;

    const SDPData::MediaArray& media_array = _sdp_info.GetMedia();
    for (const SDPData::Media& media : media_array)
    {
        if (!media.session.empty())
        {
            std::stringstream Msg("");
            Msg << Cmd << " " << _uri << " " << "RTSP/" << VERSION_RTSP << "\r\n";
            Msg << "CSeq: " << ++_CSeq << "\r\n";
            Msg << "Session: " << _sdp_info.GetMediaSessionID(media.type) << "\r\n";
            if (_realm.length() > 0 && _nonce.length() > 0)
            {
                /* digest auth */
                std::string Md5Response = makeMd5DigestResp(_realm, Cmd, _uri, _nonce);
                if (Md5Response.length() != MD5_SIZE) {
                    std::cerr << "Make MD5 digest response error" << std::endl;
                    res = RTSP_RESPONSE_401;
                    break;
                }
                Msg << "Authorization: Digest username=\"" << _username << "\", realm=\""
                    << _realm << "\", nonce=\"" << _nonce << "\", uri=\"" << _uri
                    << "\", response=\"" << Md5Response << "\"\r\n";
            }
            else if (_realm.length() > 0)
            {
                /* basic auth */
                std::string BasicResponse = makeBasicResp();
                Msg << "Authorization: Basic " << BasicResponse;
            }
            Msg << "\r\n";

            res = sendRTSP(Msg.str());
            if (RTSP_NO_ERROR != res)
            {
                break;
            }
        }
    }

    if (RTSP_NO_ERROR == res)
    {
        _over_http_data_port = 0;
        Close_Socket(_over_http_data_socket);
        Close_Socket(_rtsp_socket);
    }

    return res;
}


void RtspClient::GetMediaEndpoints(const std::string& media_type, Endpoint& server, Endpoint& client)
{
    const SDPData::MediaArray& media_array = _sdp_info.GetMedia();
    for (const SDPData::Media& media : media_array)
    {
        if (media_type == media.type)
        {
            server = media.server;
            client = media.client;
            break;
        }
    }
}

int RtspClient::GetMediaTimeRate(const std::string& media_type)
{
    const SDPData::MediaArray& media_array = _sdp_info.GetMedia();
    for (const SDPData::Media& media : media_array)
    {
        if (media_type == media.type)
        {
            return media.time_rate;
        }
    }
    return 10;
}

ErrorType RtspClient::DoRtspOverHttpGet()
{
#ifdef _MSC_VER
    SOCKET Sockfd = INVALID_SOCKET;
#else
    int Sockfd = -1;
#endif

    /*Sockfd = connectToRtspServer(_over_http_data_port);
    Check_Socket_Return_Error(Sockfd);

    std::string Cmd("GET");
    std::stringstream Msg("");
    Msg << Cmd << " " << GetResource() << " " << "HTTP/" << VERSION_HTTP << "\r\n";
    Msg << HTTP_HEAD_USER_AGENT << HTTP_HEAD_VALUE_USER_AGENT << "\r\n";
    Msg << HTTP_HEAD_XSESSION_COOKIE << UpdateXSessionCookie() << "\r\n";
    Msg << HTTP_HEAD_ACCEPT<< HTTP_HEAD_VALUE_ACCEPT << "\r\n";
    Msg << HTTP_HEAD_PRAGMA<< HTTP_HEAD_VALUE_PRAGMA << "\r\n";
    Msg << HTTP_HEAD_CACHE_CONTROL << HTTP_HEAD_VALUE_CACHE_CONTROL << "\r\n";
    Msg << "\r\n";
    // cout << "DEBUG: " << Msg.str();

    if(RTSP_NO_ERROR != sendRTSP(Sockfd, Msg.str())) {
        Close(Sockfd);
        return RTSP_SEND_ERROR;
    }*/
    /*if(RTSP_NO_ERROR != RecvRTSP(Sockfd, RtspResponse)) {
        Close(Sockfd);
        return RTSP_RECV_ERROR;
    }*/

    return RTSP_NO_ERROR;
}

ErrorType RtspClient::DoRtspOverHttpPost()
{
#ifdef _MSC_VER
    SOCKET Sockfd = INVALID_SOCKET;
#else
    int Sockfd = -1;
#endif

    /*Sockfd = connectToRtspServer();
    Check_Socket_Return_Error(Sockfd);

    string Cmd("POST");
    stringstream Msg("");
    Msg << Cmd << " " << GetResource() << " " << "HTTP/" << VERSION_HTTP << "\r\n";
    Msg << HTTP_HEAD_USER_AGENT << HTTP_HEAD_VALUE_USER_AGENT << "\r\n";
    Msg << HTTP_HEAD_XSESSION_COOKIE << HTTP_HEAD_VALUE_XSESSION_COOKIE << "\r\n";
    Msg << HTTP_HEAD_CONTENT_TYPE << HTTP_HEAD_VALUE_CONTENT_TYPE << "\r\n";
    Msg << HTTP_HEAD_PRAGMA<< HTTP_HEAD_VALUE_PRAGMA << "\r\n";
    Msg << HTTP_HEAD_CACHE_CONTROL << HTTP_HEAD_VALUE_CACHE_CONTROL << "\r\n";
    Msg << HTTP_HEAD_CONTENT_LENGTH << HTTP_HEAD_VALUE_CONTENT_LENGTH << "\r\n";
    Msg << HTTP_HEAD_EXPIRES << HTTP_HEAD_VALUE_EXPIRES << "\r\n";
    Msg << "\r\n";
    // cout << "DEBUG: " << Msg.str();

    if(RTSP_NO_ERROR != sendRTSP(Sockfd, Msg.str())) {
        Close(Sockfd);
        return RTSP_SEND_ERROR;
    }
    // if(RTSP_NO_ERROR != RecvRTSP(Sockfd, &RtspResponse)) {
    //     Close(Sockfd);
    //     return RTSP_RECV_ERROR;
    // }*/

    return RTSP_NO_ERROR;
}

/*****************/
/* Tools Methods */

/*
#ifdef _MSC_VER
SOCKET RtspClient::connectToRtspServer(uint16_t rtsp_over_http_data_port)
#else
int RtspClient::connectToRtspServer(uint16_t rtsp_over_http_data_port)
#endif
{
#ifdef _MSC_VER
    SOCKET Sockfd = INVALID_SOCKET;
#else
    int Sockfd = -1;
#endif
    struct sockaddr_in Servaddr;
    string RtspUri("");
    // int SockStatus = -1;

    if(_over_http_data_socket > 0) {
        return _over_http_data_socket;
    }

    if(_uri.length() != 0) RtspUri.assign(_uri);
    else {
        _over_http_data_socket = Sockfd;
        return Sockfd;
    }

    if((Sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket created error");
        _over_http_data_socket = Sockfd;
        return Sockfd;
    }

    // Set Sockfd NONBLOCK
    // SockStatus = fcntl(Sockfd, F_GETFL, 0);
    // fcntl(Sockfd, F_SETFL, SockStatus | O_NONBLOCK);

    // Connect to server
    memset(&Servaddr, 0, sizeof(Servaddr));
    Servaddr.sin_family = AF_INET;
    Servaddr.sin_port = htons(rtsp_over_http_data_port);
    Servaddr.sin_addr.s_addr = GetIP(RtspUri);

    if(connect(Sockfd, (struct sockaddr *)&Servaddr, sizeof(Servaddr)) < 0 && errno != EINPROGRESS) {
        perror("connect error");
#ifdef _MSC_VER
        closesocket(Sockfd);
#else
        close(Sockfd);
#endif
        Sockfd = -1;
        _over_http_data_socket = Sockfd;
        return Sockfd;
    }
    if(!checkSockWritable(Sockfd)) {
        Sockfd = -1;
        _over_http_data_socket = Sockfd;
        return Sockfd;
    }

    _over_http_data_socket = Sockfd;
    return Sockfd;
}*/

/*
std::string RtspClient::GetResource(const std::string& uri)
{
    //### example uri: rtsp://192.168.15.100/test ###//
    MyRegex Regex;
    std::string RtspUri("");
    // string Pattern("rtsp://([0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3})");
    std::string Pattern("rtsp://.+(/.+)");
    // string Pattern("rtsp://192.168.1.143(/ansersion)");
    std::list<std::string> Groups;
    bool IgnoreCase = true;

    if(uri.length() != 0) {
        RtspUri.assign(uri);
        _uri.assign(uri);
    }
    else if(_uri.length() != 0) RtspUri.assign(_uri);
    else return "";

    if(!Regex.Regex(RtspUri.c_str(), Pattern.c_str(), &Groups, IgnoreCase)) {
        return "";
    }
    Groups.pop_front();
    return Groups.front();
}
*/

/*********************/
/* Protected Methods */

/*
std::string RtspClient::UpdateXSessionCookie()
{
    time_t timep;
    char habuf[MD5_BUF_SIZE] = {0};
    time(&timep);
    Md5sum32((void *)&timep, (unsigned char *)habuf, sizeof(timep), MD5_BUF_SIZE);
    habuf[23] = '\0';
    return std::string(habuf);
}*/

// uint8_t * RtspClient::GetMediaData(MediaSession * media_session, uint8_t * buf, size_t * size, size_t max_size) 
// {
//     if(!media_session) return NULL;
//     return media_session->GetMediaData(buf, size, max_size);
// }

