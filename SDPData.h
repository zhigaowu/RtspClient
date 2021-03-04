//   Copyright 2018 Ansersion
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

#ifndef __SDP_DATA_H__
#define __SDP_DATA_H__

#include <Common.h>

#include <string>
#include <vector>

class SDPData
{
public:
    typedef struct _Network
    {
        std::string net_type;
        std::string addr_type;
        std::string address;
    } Network;

    typedef struct _Media
    {
        std::string type;
        unsigned short port = 0;

        std::string control;
        std::string transport;
        std::string codec;
        int time_rate;
        int format = 0; // media format: DynamicRTP-Type-XX

        std::string session;
        int timeout = 0;

        Endpoint client;
        Endpoint server;

        Network connection;
    } Media;

    typedef std::vector<Media> MediaArray;

    typedef struct _Owner
    {
        std::string owner;
        std::string id;
        std::string ver;

        std::string email;

        Network network;
    } Owner;

    typedef struct _ActiveTime
    {
        double start = 0.0f;
        double stop = 0.0f;
    } ActiveTime;

    typedef struct _Session
    {
        std::string name;
        std::string information;
        std::string tool;
        std::string type;
        std::string control;

        std::string action;

        ActiveTime time;
        
        MediaArray media_array;
    } Session;

public:
    SDPData();
    SDPData(const std::string& sdp);

    ~SDPData();

    void Parse(const std::string &sdp);

    void ParseMediaRtpPort(const std::string& media_type, unsigned short rtp_port, unsigned short rtcp_port);
    void ParseMediaSessionInfomation(const std::string& media_type, const std::string &setup_response);

    inline int GetSdpVersion() {   return _sdp_version;    }

    inline const std::string& GetSessionName() { return _session.name; }
    std::string GetSessionControlUri(const std::string& base);

    inline const SDPData::MediaArray& GetMedia() { return _session.media_array; }

    std::string GetMediaControlUri(const std::string& media_type, const std::string& base);
    std::string GetMediaTransport(const std::string& media_type);
    std::string GetMediaSessionID(const std::string& media_type);

private:
    /* RFC2327.6 */
    int _sdp_version;

private:
    Owner _owner;

private:
    Session _session;
};

#endif
