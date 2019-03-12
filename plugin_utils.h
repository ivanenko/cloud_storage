/*
Wfx plugin for working with cloud storage services

Copyright (C) 2019 Ivanenko Danil (ivanenko.danil@gmail.com)

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
        License as published by the Free Software Foundation; either
        version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
        Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
        Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
*/

#ifndef CLOUD_STORAGE_PLUGIN_UTILS_H
#define CLOUD_STORAGE_PLUGIN_UTILS_H

#include "json.hpp"
#include "service_clients/service_client.h"
#include "wfxplugin.h"

const char kPathSeparator =
#ifdef _WIN32
        '\\';
#else
        '/';
#endif

using namespace nlohmann;

FILETIME get_now_time();

FILETIME parse_iso_time(std::string time);

std::string UTF16toUTF8(const WCHAR *p);

wcharstring UTF8toUTF16(const std::string &str);

BOOL file_exists(const std::string &filename);

void save_config(const std::string &path, const json &jsonConfig);

std::string get_oauth_token(json& jsConfig, ServiceClient* client, int pluginNumber, tRequestProcW requestProc,
                            int cryptoNumber, tCryptProcW cryptProc);

void removeOldToken(json &jsConfig, std::string &strConnectionName, int pluginNumber, int cryptoNumber, tCryptProcW cryptProc);

pResources prepare_connections(const json &connections);

BOOL isConnectionExists(json connections, std::string connection_name);

void splitPath(wcharstring wPath, std::string& strConnection, std::string& strServicePath);

json& get_connection_config(json& globalConf, std::string& strConnection);
json::object_t* get_connection_ptr(json& globalConf, std::string& strConnection);
json::iterator get_connection_iter(json& globalConf, std::string& strConnection);

BOOL isConnectionName(WCHAR *Path);

size_t strlen16(register const WCHAR* string);

std::vector<wcharstring> split(const wcharstring s, WCHAR separator);

#endif //CLOUD_STORAGE_PLUGIN_UTILS_H
