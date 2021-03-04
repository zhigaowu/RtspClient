
#ifndef __RTSP_RTP_COMMON_HEADER_H__
#define __RTSP_RTP_COMMON_HEADER_H__

#include <string>

typedef struct _Endpoint
{
    unsigned short rtp_port = 0;
    unsigned short rtcp_port = 0;
    std::string address;
} Endpoint;

#endif

