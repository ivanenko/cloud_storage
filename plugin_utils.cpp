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

#include <string>
#include <ctime>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <cstring>
#include <codecvt>
#include <locale>
#include <iomanip>

#include "library.h"
#include "plugin_utils.h"
#include "service_clients/service_client.h"

FILETIME get_now_time()
{
    time_t t2 = time(0);
    int64_t ft = (int64_t) t2 * 10000000 + 116444736000000000;
    FILETIME file_time;
    file_time.dwLowDateTime = ft & 0xffff;
    file_time.dwHighDateTime = ft >> 32;
    return file_time;
}

FILETIME parse_iso_time(std::string time){
    struct tm t, tz;
    strptime(time.c_str(), "%Y-%m-%dT%H:%M:%S%z", &t);
    long int gmtoff = t.tm_gmtoff;
    time_t t2 = mktime(&t) + gmtoff; //TODO gmtoff doesnt correct here
    int64_t ft = (int64_t)t2 * 10000000 + 116444736000000000;
    FILETIME file_time;
    file_time.dwLowDateTime = ft & 0xffff;
    file_time.dwHighDateTime = ft >> 32;
    return file_time;
}

std::string UTF16toUTF8(const WCHAR *p)
{
    std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> convert2;
    return convert2.to_bytes(std::u16string((char16_t*) p));
}

wcharstring UTF8toUTF16(const std::string &str)
{
    std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> convert2;
    std::u16string utf16 = convert2.from_bytes(str);
    return wcharstring((WCHAR*)utf16.data());
}

BOOL file_exists(const std::string& filename)
{
    struct stat buf;
    return stat(filename.c_str(), &buf) == 0;
}

pResources prepare_connections(const nlohmann::json &connections)
{
    pResources pRes = new tResources;
    pRes->nSize = 0;
    pRes->nCount = 0;
    pRes->resource_array = NULL;

    if(connections.is_array() && connections.size()>0){
        pRes->nSize = connections.size();
        pRes->resource_array = new WIN32_FIND_DATAW[pRes->nSize];

        int i=0;
        for(auto obj: connections){
            wcharstring wName = UTF8toUTF16(obj["name"].get<std::string>());
            size_t str_size = (MAX_PATH > wName.size()+1)? (wName.size()+1): MAX_PATH;
            memcpy(pRes->resource_array[i].cFileName, wName.data(), sizeof(WCHAR) * str_size);
            pRes->resource_array[i].dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
            pRes->resource_array[i].nFileSizeLow = 0;
            pRes->resource_array[i].nFileSizeHigh = 0;
            pRes->resource_array[i].ftCreationTime = get_now_time();
            pRes->resource_array[i].ftLastWriteTime = get_now_time();
            pRes->resource_array[i].ftLastAccessTime = get_now_time();
            i++;
        }
    }

    return pRes;
}

BOOL isConnectionExists(json connections, std::string connection_name)
{
    for(auto obj: connections){
        if(obj["name"].get<std::string>() == connection_name){
            return true;
        }
    }

    return false;
}

void splitPath(wcharstring wPath, std::string& strConnection, std::string& strServicePath)
{
    size_t nPos = wPath.find((WCHAR*)u"/", 1);
    wcharstring wConnection, wServicePath;

    if(nPos == std::string::npos){
        wConnection = wPath.substr(1);
        wServicePath = (WCHAR*)u"/";
    } else {
        wConnection = wPath.substr(1, nPos-1);
        wServicePath = wPath.substr(nPos);
    }

    strConnection = UTF16toUTF8(wConnection.data());
    strServicePath = UTF16toUTF8(wServicePath.data());
}

json& get_connection_config(json& globalConf, std::string& strConnection)
{
    // get connection config
    for(nlohmann::json& o: globalConf["connections"]){
        if(o["name"].get<std::string>() == strConnection){
            return o;
            //return o.get_ref<json&>();
        }
    }

    nlohmann::json j_null(nlohmann::json::value_t::null);
    return j_null;
}

json::object_t* get_connection_ptr(json& globalConf, std::string& strConnection)
{
    for(json& o: globalConf["connections"]){
        if(o["name"].get<std::string>() == strConnection){
            return o.get_ptr<json::object_t*>();
        }
    }

    throw std::runtime_error("Connection not found in config");
}

json::iterator get_connection_iter(json& globalConf, std::string& strConnection)
{
    for(json::iterator it = globalConf.begin(); it!=globalConf.end(); ++it){
        if((*it)["name"].get<std::string>() == strConnection){
            return it;
        }
    }

    throw std::runtime_error("Connection not found in config");
}



std::string get_oauth_token(json& jsConfig, ServiceClient* client, int pluginNumber, tRequestProcW requestProc, int cryptoNumber,
                            tCryptProcW cryptProc)
{
    std::string oauth_token;

    if(jsConfig["get_token_method"].get<std::string>() == "oauth"){
        if(jsConfig["save_type"].get<std::string>() == "dont_save"){
            oauth_token = client->get_oauth_token();
            return oauth_token;
        }

        if(jsConfig["save_type"].get<std::string>() == "config"){
            if(jsConfig["oauth_token"].is_string() && !jsConfig["oauth_token"].get<std::string>().empty()) {
                oauth_token = jsConfig["oauth_token"].get<std::string>();
            } else {
                oauth_token = client->get_oauth_token();
                jsConfig["oauth_token"] = oauth_token;
                // TODO save config to file
            }

            return oauth_token;
        }

        if(jsConfig["save_type"].get<std::string>() == "password_manager"){
            WCHAR psw[MAX_PATH];
            wcharstring wName = UTF8toUTF16(jsConfig["name"].get<std::string>());
            int res = cryptProc(pluginNumber, cryptoNumber, FS_CRYPT_LOAD_PASSWORD, (WCHAR*)wName.data(), psw, MAX_PATH);
            if(res != FS_FILE_OK){
                oauth_token = client->get_oauth_token();
                wcharstring psw2 = UTF8toUTF16(oauth_token);
                cryptProc(pluginNumber, cryptoNumber, FS_CRYPT_SAVE_PASSWORD, (WCHAR*)wName.data(), (WCHAR*)psw2.c_str(), psw2.size()+1);
            } else {
                oauth_token = UTF16toUTF8(psw);
            }

            return oauth_token;
        }
    }


    if(jsConfig["get_token_method"].get<std::string>() == "manual"){
        if(jsConfig["save_type"].get<std::string>() == "dont_save"){
            WCHAR psw[MAX_PATH];
            psw[0] = 0;
            BOOL res = requestProc(pluginNumber, RT_Other, (WCHAR*)u"Enter oauth token", (WCHAR*)u"Enter oauth token", psw, MAX_PATH);
            if(res > 0)
                oauth_token = UTF16toUTF8(psw);

            return oauth_token;
        }

        if(jsConfig["save_type"].get<std::string>() == "config"){
            if(jsConfig["oauth_token"].is_string() && !jsConfig["oauth_token"].get<std::string>().empty()) {
                oauth_token = jsConfig["oauth_token"].get<std::string>();
            } else {
                WCHAR psw[MAX_PATH];
                psw[0] = 0;
                BOOL res = requestProc(pluginNumber, RT_Other, (WCHAR*)u"Enter oauth token", (WCHAR*)u"Enter oauth token", psw, MAX_PATH);
                if(res != 0){
                    oauth_token = UTF16toUTF8(psw);
                    jsConfig["oauth_token"] = oauth_token;
                    // TODO save config to file
                }
            }
            return oauth_token;
        }

        if(jsConfig["save_type"].get<std::string>() == "password_manager"){
            wcharstring wName = UTF8toUTF16(jsConfig["name"].get<std::string>());
            WCHAR psw[MAX_PATH];
            int res = cryptProc(pluginNumber, cryptoNumber, FS_CRYPT_LOAD_PASSWORD, (WCHAR*)wName.data(), psw, MAX_PATH);
            if(res != FS_FILE_OK){
                WCHAR psw2[MAX_PATH];
                psw2[0] = 0;
                BOOL res2 = requestProc(pluginNumber, RT_Other, (WCHAR*)u"Enter oauth token", (WCHAR*)u"Enter oauth token", psw2, MAX_PATH);
                if(res2 != 0){
                    oauth_token = UTF16toUTF8(psw2);
                    cryptProc(pluginNumber, cryptoNumber, FS_CRYPT_SAVE_PASSWORD, (WCHAR*)wName.data(), psw2, MAX_PATH);
                }
            } else {
                oauth_token = UTF16toUTF8(psw);
            }

            return oauth_token;
        }
    }

    return oauth_token;
}

void save_config(std::string &path, json jsonConfig)
{
    std::ofstream o(path);
    o << std::setw(4) << jsonConfig << std::endl;
}

size_t strlen16(register const WCHAR* string) {
    if (!string) return 0;
    register size_t len = 0;
    while(string[len++]);
    return len;
}

std::string narrow(const std::wstring & wstr)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> convert;
    return convert.to_bytes(wstr);
}

std::wstring widen(const std::string & str)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> convert;
    return convert.from_bytes(str);
}

std::wstring widen(const WCHAR* p)
{
#ifdef _WIN32
    //sizeof(wchar_t) == sizeof(WCHAR) == 2 byte
    return std::wstring((wchar_t*)p);
#else
    //sizeof(wchar_t) > sizeof(WCHAR) in Linux
    int nCount = 0, i=0;
    WCHAR* tmp = (WCHAR*) p;
    while(*tmp++) nCount++;

    std::wstring result(nCount+1, '\0');
    while(*p){
        result[i++] = (wchar_t) *(p++);
    }
    result[i] = '\0';

    return result;
#endif //_WIN32
}

// Check if we are in the plugin root
// if we have more than one '/' (ex. /connection1/folder1) - we are not in the root
BOOL isConnectionName(WCHAR *Path)
{
    int nCount = 0;
    WCHAR *p = Path;
    while(*p != 0){
        if(*p == u'/')
            nCount++;

        if(nCount>1)
            return false;

        p++;
    }

    return true;
}

std::vector<wcharstring> split(const wcharstring s, WCHAR separator)
{
    std::vector<std::basic_string<WCHAR> > result;

    int prev_pos = 0, pos = 0;
    while(pos <= s.size()){
        if(s[pos] == separator){
            result.push_back(s.substr(prev_pos, pos-prev_pos));
            prev_pos = ++pos;
        }
        pos++;
    }

    result.push_back(s.substr(prev_pos, pos-prev_pos));

    return result;
}
