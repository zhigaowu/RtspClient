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

#include "SDPData.h"

#include <regex>
#include <sstream>

SDPData::SDPData()
    : _sdp_version(0)
    , _owner()
    , _session()
{
}

SDPData::SDPData(const std::string& sdp)
    : _sdp_version(0)
    , _owner()
    , _session()
{
    Parse(sdp);
}

SDPData::~SDPData() 
{
}

void SDPData::Parse(const std::string &sdp)
{
    static const std::regex key_value_pattern("([a-zA-Z])=(.*)");

    int media_index = -1;
    for (std::string::size_type off = 0, pos = sdp.find("\r\n"); pos != std::string::npos; pos = sdp.find("\r\n", off))
    {
        std::smatch matchs;
        const std::string& line = sdp.substr(off, pos - off);
        if (std::regex_match(line, matchs, key_value_pattern))
        {
            if ("v" == matchs[1].str())
            {
                _sdp_version = atoi(matchs[2].str().c_str());
            } 
            else if ("e" == matchs[1].str())
            {
                _owner.email = matchs[2].str();
            }
            else if ("s" == matchs[1].str())
            {
                _session.name = matchs[2].str();
            }
            else if ("i" == matchs[1].str())
            {
                _session.information = matchs[2].str();
            }
            else if ("t" == matchs[1].str())
            {
                std::istringstream spliter(matchs[2].str());
                std::vector<std::string> objs((std::istream_iterator<std::string>(spliter)), std::istream_iterator<std::string>());

                _session.time.start = atof(objs[0].c_str());
                _session.time.stop = atof(objs[1].c_str());
            }
            else if("o" == matchs[1].str())
            {
                std::istringstream spliter(matchs[2].str());
                std::vector<std::string> objs((std::istream_iterator<std::string>(spliter)), std::istream_iterator<std::string>());

                switch (objs.size())
                {
                case 6:
                    _owner.network.address = objs[5];
                case 5:
                    _owner.network.addr_type = objs[4];
                case 4:
                    _owner.network.net_type = objs[3];
                case 3:
                    _owner.ver = objs[2];
                case 2:
                    _owner.id = objs[1];
                case 1:
                    _owner.owner = objs[0];
                default:
                    break;
                }
            }
            else if ("m" == matchs[1].str())
            {
                media_index++;

                std::istringstream spliter(matchs[2].str());
                std::vector<std::string> objs((std::istream_iterator<std::string>(spliter)), std::istream_iterator<std::string>());

                if (_session.media_array.size() <= media_index)
                {
                    _session.media_array.push_back(Media{});
                }

                Media& media = _session.media_array[media_index];
                media.type = objs[0];
                switch (objs.size())
                {
                case 4:
                    media.format = atoi(objs[3].c_str());
                case 3:
                    media.transport = objs[2];
                case 2:
                    media.port = (unsigned short)atoi(objs[1].c_str());
                default:
                    break;
                }
            }
            else if ("a" == matchs[1].str())
            {
                std::string value = matchs[2].str();
                if (_session.media_array.empty())
                {
                    if (value.find("control") == (std::string::size_type)0)
                    {
                        _session.control = value.substr(8);
                    }
                    else if (value.find("tool") == (std::string::size_type)0)
                    {
                        _session.tool = value.substr(5);
                    }
                    else if (value.find("type") == (std::string::size_type)0)
                    {
                        _session.type = value.substr(5);
                    }
                }
                else
                {
                    Media& media = _session.media_array[media_index];

                    if (value.find("rtpmap") == (std::string::size_type)0)
                    {
                        std::istringstream spliter(value.substr(6));
                        std::vector<std::string> objs((std::istream_iterator<std::string>(spliter)), std::istream_iterator<std::string>());
                        std::string::size_type p = objs[1].find('/');
                        if (std::string::npos == p)
                        {
                            media.codec = objs[1];
                        }
                        else
                        {
                            media.codec = objs[1].substr(0, p);
                            media.time_rate = atoi(objs[1].substr(p + 1).c_str());
                        }
                    }
                    else if (value.find("control") == (std::string::size_type)0)
                    {
                        media.control = value.substr(8);
                    }
                }
            }
            else if ("c" == matchs[1].str() && media_index < _session.media_array.size())
            {
                std::istringstream spliter(matchs[2].str());
                std::vector<std::string> objs((std::istream_iterator<std::string>(spliter)), std::istream_iterator<std::string>());

                Media& media = _session.media_array[media_index];
                switch (objs.size())
                {
                case 3:
                    media.connection.address = objs[2];
                case 2:
                    media.connection.addr_type = objs[1];
                case 1:
                    media.connection.net_type = objs[0];
                default:
                    break;
                }
            }
            else
            {
            }
        }
        off = pos + 2;
    }
}


void SDPData::ParseMediaRtpPort(const std::string& media_type, unsigned short rtp_port, unsigned short rtcp_port)
{
    for (Media& media : _session.media_array)
    {
        if (media_type == media.type)
        {
            media.client.rtp_port = rtp_port;
            media.client.rtcp_port = rtcp_port;
            break;
        }
    }
}

void SDPData::ParseMediaSessionInfomation(const std::string& media_type, const std::string &setup_response)
{
    static const std::regex session_id_timeout_pattern("Session:([ ]+)([0-9a-fA-F]+);timeout=([0-9]+)");
    static const std::regex session_id_pattern("Session:([ ]+)([0-9a-fA-F]+);*");

    static const std::regex trasnport_info_pattern("Transport:([ ]+)(.+)");

    size_t media_index = 0;
    while (media_index < _session.media_array.size())
    {
        if (media_type == _session.media_array[media_index].type)
        {
            break;
        }
        ++media_index;
    }

    if (media_index < _session.media_array.size())
    {
        for (std::string::size_type off = 0, pos = setup_response.find("\r\n"); pos != std::string::npos; pos = setup_response.find("\r\n", off))
        {
            std::smatch matchs;
            const std::string& line = setup_response.substr(off, pos - off);
            if (std::regex_match(line, matchs, session_id_timeout_pattern))
            {
                _session.media_array[media_index].session = matchs[2].str();
                _session.media_array[media_index].timeout = atoi(matchs[3].str().c_str());
                break;
            }
            else if (std::regex_match(line, matchs, session_id_pattern))
            {
                _session.media_array[media_index].session = matchs[2].str();
                _session.media_array[media_index].timeout = 30;
                break;
            }
            else if (std::regex_match(line, matchs, trasnport_info_pattern))
            {
                std::istringstream stream_spliter(matchs[2].str());
                std::string token;
                while (std::getline(stream_spliter, token, ';')) 
                {
                    std::string::size_type pos = token.find('=');
                    if (std::string::npos != pos)
                    {
                        std::string tkey = token.substr(0, pos);
                        if ("source" == tkey)
                        {
                            _session.media_array[media_index].server.address = token.substr(pos + 1);
                        }
                        else if ("server_port" == tkey)
                        {
                            std::string port_range = token.substr(pos + 1);

                            std::string::size_type p = port_range.find('-');
                            if (std::string::npos != p)
                            {
                                _session.media_array[media_index].server.rtp_port = (unsigned short)atoi(port_range.substr(0, p).c_str());
                                _session.media_array[media_index].server.rtcp_port = (unsigned short)atoi(port_range.substr(p + 1).c_str());
                            }
                            else
                            {
                                _session.media_array[media_index].server.rtp_port = (unsigned short)atoi(port_range.substr(0, p).c_str());
                                _session.media_array[media_index].server.rtcp_port = _session.media_array[media_index].server.rtp_port + 1;
                            }
                        }
                    }
                }
                break;
            }
            off = pos + 2;
        }
    }
}


std::string SDPData::GetSessionControlUri(const std::string& base)
{
    std::string control_uri;
    if (base[base.size() - 1] == '/')
    {
        control_uri = base + _session.control;
    }
    else
    {
        control_uri = base + "/" + _session.control;
    }
    return control_uri;
}

std::string SDPData::GetMediaControlUri(const std::string& media_type, const std::string& base)
{
    std::string control_uri;
    for (const Media& media : _session.media_array)
    {
        if (media_type == media.type)
        {
            if (media.control.find("rtsp://") != std::string::npos)
            {
                control_uri = media.control;
            }
            else
            {
                if (base[base.size() - 1] == '/')
                {
                    control_uri = base + media.control;
                }
                else
                {
                    control_uri = base + "/" + media.control;
                }
            }
            break;
        }
    }
    return control_uri;
}

std::string SDPData::GetMediaTransport(const std::string& media_type)
{
    std::string transport;
    for (const Media& media : _session.media_array)
    {
        if (media_type == media.type)
        {
            transport = media.transport;
            break;
        }
    }
    return transport;
}

std::string SDPData::GetMediaSessionID(const std::string& media_type)
{
    std::string session;
    for (const Media& media : _session.media_array)
    {
        if (media_type == media.type)
        {
            session = media.session;
            break;
        }
    }
    return session;
}
