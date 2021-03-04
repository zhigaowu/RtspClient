//   Copyright 2015-2016 Ansersion
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

#ifndef __RTSP_CLIENT_H__
#define __RTSP_CLIENT_H__

#include "SDPData.h"

#include <string>

#ifdef _MSC_VER
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <arpa/inet.h>

typedef int SOCKET;
#define INVALID_SOCKET -1

#endif

#include <stdio.h>

enum ErrorType {
    RTSP_NO_ERROR = 0,
    RTSP_INVALID_URI,
    RTSP_SOCKET_INIT,
    RTSP_SOCKET_CONNECT,
    RTSP_SEND_ERROR, 
    RTSP_RECV_ERROR,
    RTSP_RECV_SDP_ERROR,
    RTSP_USER_EMPTY,
    RTSP_NEGOTIATION_AUTH,
    RTSP_PARSE_SDP_ERROR,
    RTSP_PARSE_SDP_LENGTH_ERROR,
    RTSP_INVALID_MEDIA_SESSION,
    RTSP_RESPONSE_BLANK,
    RTSP_RESPONSE_200 = 200,
    RTSP_RESPONSE_400 = 400,
    RTSP_RESPONSE_401 = 401,
    RTSP_RESPONSE_404 = 404,
    RTSP_RESPONSE_40X = 499,
    RTSP_RESPONSE_500 = 500,
    RTSP_RESPONSE_501 = 501,
    RTSP_RESPONSE_50X = 599,
    RTSP_RTP_PORT_ERROR,
    RTSP_RTP_ERROR,
    RTSP_UNKNOWN_ERROR
};

class RtspClient
{
public:
    typedef void(*ServerDisconnectCallback)(void* userdata, const std::string& media_type);

public:
    RtspClient();
    explicit RtspClient(const std::string& uri);
    ~RtspClient();

    ErrorType DoOPTIONS(const std::string& uri = "");

    ErrorType DoDESCRIBE();

    ErrorType DoSETUP(const std::string& media_type, bool rtp_over_tcp = false);

    ErrorType DoPLAY(const std::string& media_type, double start_time = 0.0f, double* end_time = nullptr, double* scale = nullptr);

    /* To pause all of the media sessions in SDP */
    ErrorType DoPAUSE();

    /* Example: DoPAUSE("video");
        * To pause the first video session in SDP
        * http_tunnel_no_response: 
        *    when using http-tunnelling, rtp and rtsp are transmitted in the same socket, if http_tunnel_no_response is set true, DoPAUSE will not wait for the response,
        *  because the response will be handled in callback function when getting rtp packets(refer to: SetRtspCmdClbk) 
        *  YOU MUST SET THE CALLBACK, OTHERWITH IT WILL BLOCKED WHEN GETTING MEDIA DATA
        * */
    ErrorType DoPAUSE(const std::string& media_type, bool http_tunnel_no_response = false);

    /* To get parameters all of the media sessions in SDP 
    * The most general use is to keep the RTSP session alive: 
    * Invoke this function periodly within TIMEOUT(see: GetSessionTimeout()) */
    ErrorType DoGET_PARAMETER();

    /* Example: DoGET_PARAMETER("video");
        * To get parameters of the first video session in SDP
        * http_tunnel_no_response: 
        *    when using http-tunnelling, rtp and rtsp are transmitted in the same socket, if http_tunnel_no_response is set true, DoGET_PARAMETER will not wait for the response,
        *  because the response will be handled in callback function when getting rtp packets(refer to: SetRtspCmdClbk) 
        *  YOU MUST SET THE CALLBACK, OTHERWITH IT WILL BLOCKED WHEN GETTING MEDIA DATA
        * */
    ErrorType DoGET_PARAMETER(const std::string& media_type, bool http_tunnel_no_response = false);

    /* To teardown all of the media sessions in SDP */
    ErrorType DoTEARDOWN();

public:
    inline SOCKET GetTcpSocket() { return _rtsp_socket; }
    void GetMediaEndpoints(const std::string& media_type, Endpoint& server, Endpoint& client);
    int GetMediaTimeRate(const std::string& media_type);

private:
    bool checkRtspUri(const std::string& uri);
    void parseAddressAndPort(const std::string& uri);

    ErrorType connectToRtspServer();

private:
    ErrorType checkSockWritable(SOCKET sockfd, struct timeval * tval = NULL);
    ErrorType checkSockReadable(SOCKET sockfd, struct timeval * tval = NULL);

    ErrorType recvRTSP(SOCKET fd, std::string& msg);
    ErrorType sendRTSP(SOCKET fd, const std::string& msg);

    ErrorType recvSDP(SOCKET sockfd, char * msg, size_t size);

    ErrorType sendRTSP(const std::string& msg);
    ErrorType recvRTSP(std::string& msg);

    ErrorType checkResponse(const std::string& response);
    ErrorType doAuth(std::string& response, const std::string& cmd, const std::string& uri);

    ErrorType recvSDP(const std::string& response, std::string& msg);
    
    void parseSDP(const std::string& sdp);

    ErrorType setAvailableRTPPort(uint16_t RTP_port, unsigned short& rtp_port, unsigned short& rtcp_port);

    /* To setup the media sessions
    * media_session:
    *     the media session
    * rtp_over_tcp:
    *    if set true, means using the rtsp tcp socket to transmit rtp packets. If 'http_tunnel_no_response' is also set true, it will be ignored.
    * http_tunnel_no_response:
    *    when using http-tunnelling, rtp and rtsp are transmitted in the same socket, if http_tunnel_no_response is set true, DoPLAY will not wait for the response,
    *  because the response will be handled in callback function when getting rtp packets(refer to: SetRtspCmdClbk), and 'rtp_over_tcp' will be ignored.
    *  YOU MUST SET THE CALLBACK, OTHERWITH IT WILL BLOCKED WHEN GETTING MEDIA DATA
    * */
    ErrorType doSETUP(const std::string& media_type, bool rtp_over_tcp);

    /* Example: DoPLAY();
    * To play the first video session in SDP
    * media_type:
    * "audio"/"video"
    * scale:
    *    playing speed, such as 1.5 means 1.5 times of the normal speed, default NULL=normal speed
    * start_time:
    *    start playing point, such as 20.5 means starting to play at 20.5 seconds, default NULL=play from 0 second or the PAUSE point
    * end_time:
    *    end playing point, such as 20.5 means ending play at 20.5 seconds, default NULL=play to the end
    * http_tunnel_no_response:
    *    when using http-tunnelling, rtp and rtsp are transmitted in the same socket, if http_tunnel_no_response is set true, DoPLAY will not wait for the response,
    *  because the response will be handled in callback function when getting rtp packets(refer to: SetRtspCmdClbk).
    *  YOU MUST SET THE CALLBACK, OTHERWITH IT WILL BLOCKED WHEN GETTING MEDIA DATA
    *
    * */
    ErrorType doPLAY(const std::string& media_type, double start_time, double* end_time, double* scale);

    std::string makeMd5DigestResp(const std::string& realm, const std::string& cmd, const std::string& uri, const std::string& nonce, const std::string& username = "", const std::string& password = "");
    std::string makeBasicResp(const std::string& username = "", const std::string& password = "");

    ErrorType DoRtspOverHttpGet();
    ErrorType DoRtspOverHttpPost();

private:
    std::string _uri;
    std::string _uri_without_user_info;

private:
    std::string _address;
    uint16_t _port;

private:
    // Authentication
    std::string _username;
    std::string _password;

    std::string _realm;
    std::string _nonce;

private:
    SOCKET _rtsp_socket;

    ServerDisconnectCallback disconnect_callback;

private:
    uint16_t _over_http_data_port;
    SOCKET  _over_http_data_socket;

private:
    unsigned int _CSeq;

private:
    std::string _sdp;
    SDPData _sdp_info;

private:
    RtspClient& operator=(RtspClient& rhs);
};

#endif
