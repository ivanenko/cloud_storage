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

#include <iostream>
#include <cstring>
#include <algorithm>
#include <iomanip>

#include "library.h"
#include "wfxplugin.h"
#include "extension.h"
#include "json.hpp"
#include "plugin_utils.h"
#include "dialogs.h"
#include "service_clients/service_client.h"
#include "service_clients/service_factory.h"

#define _plugin_version 0
#define _plugin_name "Cloud Storage"
#define _createstr u"/<New connection (F7)>"

using namespace nlohmann;

int gPluginNumber, gCryptoNr;
tProgressProcW gProgressProcW = NULL;
tLogProcW gLogProcW = NULL;
tRequestProcW gRequestProcW = NULL;
tCryptProcW gCryptProcW = NULL;

std::unique_ptr<tExtensionStartupInfo> gExtensionInfoPtr;

std::string oauth_token, gConfig_file_path;
json gJsonConfig;

thread_local BOOL gIsFolderRemoving = false;

std::map<std::string, std::string> gTokenMap;

ServiceFactory gServiceFactory;


void DCPCALL ExtensionInitialize(tExtensionStartupInfo* StartupInfo)
{
    gExtensionInfoPtr = std::make_unique<tExtensionStartupInfo>();
    memcpy(gExtensionInfoPtr.get(), StartupInfo, sizeof(tExtensionStartupInfo));
}

void DCPCALL FsGetDefRootName(char* DefRootName, int maxlen)
{
    strncpy(DefRootName, _plugin_name, maxlen);
}

void DCPCALL FsSetCryptCallbackW(tCryptProcW pCryptProcW, int CryptoNr, int Flags)
{
    gCryptoNr = CryptoNr;
    gCryptProcW = pCryptProcW;
}

void DCPCALL FsSetDefaultParams(FsDefaultParamStruct* dps)
{
    std::string defaultIni(dps->DefaultIniName);
    int p = defaultIni.find_last_of(kPathSeparator);
    gConfig_file_path = defaultIni.substr(0, p+1) + "cloud_storage.ini";

    std::ifstream i(gConfig_file_path);
    if(i){
        i >> gJsonConfig;
    } else {
        gJsonConfig["version"] = _plugin_version;
        gJsonConfig["connections"] = json::array();

        save_config(gConfig_file_path, gJsonConfig);
    }
}

int DCPCALL FsInitW(int PluginNr, tProgressProcW pProgressProc, tLogProcW pLogProc, tRequestProcW pRequestProc)
{
    gProgressProcW = pProgressProc;
    gLogProcW = pLogProc;
    gRequestProcW = pRequestProc;
    gPluginNumber = PluginNr;

    return 0;
}

// get and configure service client
ServiceClient* getServiceClient(json& gJsonConfig, std::string strConnectionName)
{
    json& connection = get_connection_config(gJsonConfig, strConnectionName);
    std::string strService = connection["service"].get<std::string>();

    ServiceClient *client = gServiceFactory.getClient(strService);

    std::string strToken;
    std::map<std::string, std::string>::iterator it;
    it = gTokenMap.find(connection["name"].get<std::string>());
    if(it != gTokenMap.end()){
        strToken = it->second;
    } else {
        strToken = get_oauth_token(connection, client, gPluginNumber, gRequestProcW, gCryptoNr, gCryptProcW);
        save_config(gConfig_file_path, gJsonConfig);
        gTokenMap[connection["name"].get<std::string>()] = strToken;
    }

    client->set_oauth_token(strToken.c_str());

    return client;
}

HANDLE DCPCALL FsFindFirstW(WCHAR* Path, WIN32_FIND_DATAW *FindData)
{
    //TODO read config file here?

    if(gIsFolderRemoving)
        return (HANDLE)-1;

    wcharstring wPath(Path);
    std::replace(wPath.begin(), wPath.end(), u'\\', u'/');
    pResources pRes = NULL;

    if(wPath.size() == 1){ // root of the plugin = connection list
        memset(FindData, 0, sizeof(WIN32_FIND_DATAW));
        memcpy(FindData->cFileName, _createstr+1, sizeof(WCHAR) * strlen16((WCHAR*)_createstr+1));
        FindData->ftCreationTime = get_now_time();
        FindData->ftLastWriteTime.dwHighDateTime=0xFFFFFFFF;
        FindData->ftLastWriteTime.dwLowDateTime=0xFFFFFFFE;

        //get and show all connections from config
        pRes = prepare_connections(gJsonConfig["connections"]);
        return (HANDLE) pRes;
    }

    std::string strConnection, strServicePath;
    splitPath(wPath, strConnection, strServicePath);

    try{
        ServiceClient *client = getServiceClient(gJsonConfig, strConnection);
        pRes = client->get_resources(strServicePath, false);
    } catch (std::runtime_error & e){
        gRequestProcW(gPluginNumber, RT_MsgOK, (WCHAR*)u"Error", (WCHAR*) UTF8toUTF16(e.what()).c_str(), NULL, 0);
    }

    if(!pRes || pRes->nSize==0)
        return (HANDLE)-1;

    if(pRes->resource_array && pRes->nSize>0){
        memset(FindData, 0, sizeof(WIN32_FIND_DATAW));
        memcpy(FindData, pRes->resource_array, sizeof(WIN32_FIND_DATAW));
        pRes->nCount++;
    }

    return (HANDLE) pRes;
}


BOOL DCPCALL FsFindNextW(HANDLE Hdl, WIN32_FIND_DATAW *FindData)
{
    pResources pRes = (pResources) Hdl;

    if(pRes && (pRes->nCount < pRes->nSize) ){
        memcpy(FindData, &pRes->resource_array[pRes->nCount], sizeof(WIN32_FIND_DATAW));
        pRes->nCount++;
        return true;
    } else {
        return false;
    }
}

int DCPCALL FsFindClose(HANDLE Hdl){
    pResources pRes = (pResources) Hdl;
    if(pRes){
        if(pRes->resource_array)
            delete[] pRes->resource_array;

        delete pRes;
    }

    return 0;
}

BOOL DCPCALL FsDeleteFileW(WCHAR* RemoteName)
{
    wcharstring wPath(RemoteName);
    std::replace(wPath.begin(), wPath.end(), u'\\', u'/');

    std::string strConnection, strServicePath;
    splitPath(wPath, strConnection, strServicePath);

    try{
        ServiceClient *client = getServiceClient(gJsonConfig, strConnection);
        if(strServicePath.find("/.Trash") == 0)
            client->deleteFromTrash(strServicePath.substr(7));
        else
            client->removeResource(strServicePath);

        return true;
    } catch (std::runtime_error & e){
        gRequestProcW(gPluginNumber, RT_MsgOK, (WCHAR*)u"Error", (WCHAR*) UTF8toUTF16(e.what()).c_str(), NULL, 0);
        return false;
    }
}

BOOL DCPCALL FsMkDirW(WCHAR* Path)
{
    wcharstring wPath(Path);
    std::replace(wPath.begin(), wPath.end(), u'\\', u'/');

    if(isConnectionName(Path)){
        std::string strName = UTF16toUTF8(Path + 1);

        if(isConnectionExists(gJsonConfig["connections"], strName)){
            gRequestProcW(gPluginNumber, RT_MsgOK, (WCHAR*)u"Error", (WCHAR*)u"Connection with such name already exists", NULL, 0);
            return false;
        }

        json jsonConnection;
        jsonConnection["name"] = strName;

        BOOL bRes = show_new_connection_dlg(jsonConnection);
        if(bRes){
            gJsonConfig["connections"].push_back(jsonConnection);
            save_config(gConfig_file_path, gJsonConfig);
            return true;
        }

        return false;
    }

    std::string strConnection, strServicePath;
    splitPath(wPath, strConnection, strServicePath);

    try{
        ServiceClient *client = getServiceClient(gJsonConfig, strConnection);
        client->makeFolder(strServicePath);
        return true;
    } catch (std::runtime_error & e){
        gRequestProcW(gPluginNumber, RT_MsgOK, (WCHAR*)u"Error", (WCHAR*) UTF8toUTF16(e.what()).c_str(), NULL, 0);
        return false;
    }
}

BOOL DCPCALL FsRemoveDirW(WCHAR* RemoteName)
{
    wcharstring wPath(RemoteName);
    std::replace(wPath.begin(), wPath.end(), u'\\', u'/');

    if(isConnectionName(RemoteName)){
        std::string strName = UTF16toUTF8(RemoteName + 1);
        json::iterator it = get_connection_iter(gJsonConfig["connections"], strName);
        gJsonConfig["connections"].erase(it);
        save_config(gConfig_file_path, gJsonConfig);

        return true;
    }

    std::string strConnection, strServicePath;
    splitPath(wPath, strConnection, strServicePath);

    try{
        ServiceClient* client = getServiceClient(gJsonConfig, strConnection);
        client->removeResource(strServicePath);
        return true;
    } catch (std::runtime_error & e){
        gRequestProcW(gPluginNumber, RT_MsgOK, (WCHAR*)u"Error", (WCHAR*) UTF8toUTF16(e.what()).c_str(), NULL, 0);
        return false;
    }
}

int DCPCALL FsRenMovFileW(WCHAR* OldName, WCHAR* NewName, BOOL Move, BOOL OverWrite, RemoteInfoStruct* ri)
{
    wcharstring wOldName(OldName), wNewName(NewName);
    std::replace(wOldName.begin(), wOldName.end(), u'\\', u'/');
    std::replace(wNewName.begin(), wNewName.end(), u'\\', u'/');

    if(isConnectionName(OldName)){
        if(!isConnectionName(NewName))
            return FS_FILE_WRITEERROR;

        if(!Move)
            return FS_FILE_WRITEERROR;

        std::string newName = UTF16toUTF8(NewName + 1);
        
        if(isConnectionExists(gJsonConfig["connections"], newName)){
            gRequestProcW(gPluginNumber, RT_MsgOK, (WCHAR*)u"Error", (WCHAR*)u"Connection with such name already exists", NULL, 0);
            return FS_FILE_WRITEERROR;
        }

        std::string strOldName = UTF16toUTF8(OldName + 1);
        json::iterator it = get_connection_iter(gJsonConfig["connections"], strOldName);
        (*it)["name"] = newName;
        save_config(gConfig_file_path, gJsonConfig);

        return FS_FILE_OK;
    }

    try{
        int err = gProgressProcW(gPluginNumber, OldName, NewName, 0);
        if (err)
            return FS_FILE_USERABORT;

        std::string strConnection, strServiceOldPath, strServiceNewPath;
        splitPath(wOldName, strConnection, strServiceOldPath);
        splitPath(wNewName, strConnection, strServiceNewPath);

        ServiceClient* client = getServiceClient(gJsonConfig, strConnection);

        if(Move){
            client->move(strServiceOldPath, strServiceNewPath, OverWrite);
        } else {
            client->copy(strServiceOldPath, strServiceNewPath, OverWrite);
        }
        gProgressProcW(gPluginNumber, OldName, NewName, 100);
    } catch(service_client_exception & e){
        if(e.get_status() == 409){
            return FS_FILE_EXISTS;
        } else {
            gRequestProcW(gPluginNumber, RT_MsgOK, (WCHAR*)u"Error", (WCHAR*) UTF8toUTF16(e.what()).c_str(), NULL, 0);
            return FS_FILE_WRITEERROR;
        }
    } catch (std::runtime_error & e){
        gRequestProcW(gPluginNumber, RT_MsgOK, (WCHAR*)u"Error", (WCHAR*) UTF8toUTF16(e.what()).c_str(), NULL, 0);
        return FS_FILE_WRITEERROR;
    }

    return FS_FILE_OK;
}

int DCPCALL FsGetFileW(WCHAR* RemoteName, WCHAR* LocalName, int CopyFlags, RemoteInfoStruct* ri)
{
    if(CopyFlags & FS_COPYFLAGS_RESUME)
        return FS_FILE_NOTSUPPORTED;

    try{
        std::ofstream ofs;
        wcharstring wRemoteName(RemoteName), wLocalName(LocalName);
        std::replace(wRemoteName.begin(), wRemoteName.end(), u'\\', u'/');

        BOOL isFileExists = file_exists(UTF16toUTF8(wLocalName.data()));
        if(isFileExists && !(CopyFlags & FS_COPYFLAGS_OVERWRITE) )
            return FS_FILE_EXISTS;

        ofs.open(UTF16toUTF8(wLocalName.data()), std::ios::binary | std::ofstream::out | std::ios::trunc);
        if(!ofs || ofs.bad())
            return FS_FILE_WRITEERROR;

        int err = gProgressProcW(gPluginNumber, RemoteName, LocalName, 0);
        if(err)
            return FS_FILE_USERABORT;

        std::string strConnection, strServicePath;
        splitPath(wRemoteName, strConnection, strServicePath);

        ServiceClient* client = getServiceClient(gJsonConfig, strConnection);
        client->downloadFile(strServicePath, ofs);
        gProgressProcW(gPluginNumber, RemoteName, LocalName, 100);

        if(CopyFlags & FS_COPYFLAGS_MOVE)
            FsDeleteFileW(RemoteName);

    } catch (std::runtime_error & e){
        gRequestProcW(gPluginNumber, RT_MsgOK, (WCHAR*)u"Error", (WCHAR*) UTF8toUTF16(e.what()).c_str(), NULL, 0);
        return FS_FILE_READERROR;
    }

    //ofstream closes file in destructor
    return FS_FILE_OK;
}

int DCPCALL FsPutFileW(WCHAR* LocalName,WCHAR* RemoteName,int CopyFlags) {
    if (CopyFlags & FS_COPYFLAGS_RESUME)
        return FS_FILE_NOTSUPPORTED;

    try {
        std::ifstream ifs;
        wcharstring wRemoteName(RemoteName), wLocalName(LocalName);
        std::replace(wRemoteName.begin(), wRemoteName.end(), u'\\', u'/');
        ifs.open(UTF16toUTF8(wLocalName.data()), std::ios::binary | std::ofstream::in);
        if (!ifs || ifs.bad())
            return FS_FILE_READERROR;

        int err = gProgressProcW(gPluginNumber, LocalName, RemoteName, 0);
        if (err)
            return FS_FILE_USERABORT;

        std::string strConnection, strServicePath;
        splitPath(wRemoteName, strConnection, strServicePath);

        ServiceClient* client = getServiceClient(gJsonConfig, strConnection);
        client->uploadFile(strServicePath, ifs, (CopyFlags & FS_COPYFLAGS_OVERWRITE));
        gProgressProcW(gPluginNumber, LocalName, RemoteName, 100);

        ifs.close();

        if(CopyFlags & FS_COPYFLAGS_MOVE)
            std::remove(UTF16toUTF8(LocalName).c_str());

    } catch(service_client_exception & e){
        if(e.get_status() == 409){
            return FS_FILE_EXISTS;
        } else {
            gRequestProcW(gPluginNumber, RT_MsgOK, (WCHAR*)u"Error", (WCHAR*) UTF8toUTF16(e.what()).c_str(), NULL, 0);
            return FS_FILE_WRITEERROR;
        }
    } catch (std::runtime_error & e){
        gRequestProcW(gPluginNumber, RT_MsgOK, (WCHAR*)u"Error", (WCHAR*) UTF8toUTF16(e.what()).c_str(), NULL, 0);
        return FS_FILE_WRITEERROR;
    }

    return FS_FILE_OK;
}


void DCPCALL FsStatusInfoW(WCHAR* RemoteDir, int InfoStartEnd, int InfoOperation)
{
    if(InfoOperation == FS_STATUS_OP_DELETE || InfoOperation == FS_STATUS_OP_RENMOV_MULTI){
        if(InfoStartEnd == FS_STATUS_START)
            gIsFolderRemoving = true;
        else
            gIsFolderRemoving = false;
    }
}

int DCPCALL FsExecuteFileW(HWND MainWin, WCHAR* RemoteName, WCHAR* Verb)
{
    wcharstring wVerb(Verb), wRemoteName(RemoteName);

    if(wVerb.find((WCHAR*)u"open") == 0){
        wcharstring wCreate((WCHAR*)_createstr);
        if (wRemoteName == wCreate){
            WCHAR connection_name[MAX_PATH];
            connection_name[0] = 0;
            BOOL res = gRequestProcW(gPluginNumber, RT_Other, (WCHAR*)u"New connection", (WCHAR*)u"Enter connection name", connection_name, MAX_PATH);
            if(res != 0){
                std::string strConnection = UTF16toUTF8(connection_name);
                
                if(isConnectionExists(gJsonConfig["connections"], strConnection)){
                    gRequestProcW(gPluginNumber, RT_MsgOK, (WCHAR*)u"Error", (WCHAR*)u"Connection with such name already exists", NULL, 0);
                    return FS_EXEC_ERROR;
                }
                
                json json_obj;
                json_obj["name"] = strConnection;
                BOOL bRes = show_new_connection_dlg(json_obj);
                if(bRes){
                    gJsonConfig["connections"].push_back(json_obj);
                    save_config(gConfig_file_path, gJsonConfig);
                    return FS_EXEC_OK;
                }
            }
        } else {
            std::string var = UTF16toUTF8(RemoteName + 1);
        }
    }

    if(wVerb.find((WCHAR*)u"properties") == 0){
        if(wRemoteName == (WCHAR*) u"/"){ // show global plugin properties
            // TODO show global plugin config dialog
        }

        if(isConnectionName(RemoteName)){
            std::string strName = UTF16toUTF8(RemoteName + 1);
            json::object_t *obj = get_connection_ptr(gJsonConfig, strName);
            int res = show_connection_properties_dlg(obj);
            if(res)
                save_config(gConfig_file_path, gJsonConfig);
        }
    }

    if(wVerb.find((WCHAR*)u"quote") == 0){
        std::vector<wcharstring> wStrings = split(wVerb, (WCHAR)u' ');
        wStrings.erase(wStrings.begin());
        if(wStrings.size() == 0)
            return FS_EXEC_ERROR;

        if( !isConnectionName(RemoteName) ){
            std::string strConnection, strServicePath;
            splitPath(RemoteName, strConnection, strServicePath);

            try{
                ServiceClient *client = getServiceClient(gJsonConfig, strConnection);
                std::vector<std::string> strings;
                for(wcharstring s: wStrings)
                    strings.push_back(UTF16toUTF8(s.c_str()));

                client->run_command(strServicePath, strings);
            } catch (std::runtime_error & e){
                gRequestProcW(gPluginNumber, RT_MsgOK, (WCHAR*)u"Error", (WCHAR*) UTF8toUTF16(e.what()).c_str(), NULL, 0);
                return FS_EXEC_ERROR;
            }

            if(wStrings[1] == (WCHAR*)u"trash" && wStrings.size()==2){
                WCHAR* p = (WCHAR*)u"/.Trash";
                memcpy(RemoteName, p, sizeof(WCHAR) * 8);
                return FS_EXEC_SYMLINK;
            }
        }
    }

    return FS_EXEC_OK;
}

