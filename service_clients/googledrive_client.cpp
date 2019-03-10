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

#include "googledrive_client.h"
#include "../plugin_utils.h"


GoogleDriveClient::GoogleDriveClient()
{
    http_client = new httplib::SSLClient("www.googleapis.com");
    resourceNamesMap["/"] = "root";
}

GoogleDriveClient::~GoogleDriveClient()
{
    delete http_client;
}

std::string GoogleDriveClient::get_auth_page_url()
{
    std::string url("https://accounts.google.com/o/oauth2/v2/auth?response_type=token\\&redirect_uri=http://localhost:3359/get_token\\&scope=https://www.googleapis.com/auth/drive\\&client_id=1019190623375-06j9q3kgqnborccd85fudtf0f7rk7138.apps.googleusercontent.com");
    return url;
}

void GoogleDriveClient::throw_response_error(httplib::Response* resp){
    if(resp){
        std::string message;
        try{
            json js = json::parse(resp->body);
            if(js["error"].is_object())
                message = js["error"]["message"].get<std::string>();
            else
                message = resp->body;
        } catch(...){
            message = resp->body;
        }

        throw service_client_exception(resp->status, message);
    } else {
        throw std::runtime_error("Unknown error");
    }
}

void GoogleDriveClient::set_oauth_token(const char *token)
{
    assert(token != NULL);
    //this->token = token;

    std::string header_token = "Bearer ";
    header_token += token;
    headers = { {"Authorization", header_token}, {"Accept", "application/json"} };
}

pResources GoogleDriveClient::get_resources(std::string path, BOOL isTrash)
{
    // mimeType='application/vnd.google-apps.folder'
    // $search = "title='stackoverflow' AND mimeType = 'application/vnd.google-apps.folder' AND trashed != true";

    std::string folderId("root"), folderName;
    if(path != "/") {
        int p = path.find_last_of('/');
        folderName = path.substr(p + 1);
        if (resourceNamesMap.find(folderName) != resourceNamesMap.end())
            folderId = resourceNamesMap[folderName];
    }

    std::string url = "/drive/v3/files?q=trashed=false and '";
    url += folderId;
    url += "' in parents&fields=files(id,name,size,createdTime,modifiedTime,parents,mimeType)";

    auto r = http_client->Get(url.c_str(), headers);

    if(r.get() && r->status==200){
        const auto js = json::parse(r->body);

        return prepare_folder_result(js, (path == "/") );
    } else {
        throw_response_error(r.get());
    }
}

pResources GoogleDriveClient::prepare_folder_result(json js, BOOL isRoot)
{
    if(!js["files"].is_array())
        throw std::runtime_error("Wrong Json format");

    int total = js["files"].size();

    pResources pRes = new tResources;
    pRes->nSize = total;
    pRes->nCount = 0;
    pRes->resource_array = new WIN32_FIND_DATAW[pRes->nSize];

    int i=0;
    for(auto& item: js["files"]){
        wcharstring wName = UTF8toUTF16(item["name"].get<std::string>());

        size_t str_size = (MAX_PATH > wName.size()+1)? (wName.size()+1): MAX_PATH;
        memcpy(pRes->resource_array[i].cFileName, wName.data(), sizeof(WCHAR) * str_size);

        if(item["mimeType"].get<std::string>() == "application/vnd.google-apps.folder") {
            pRes->resource_array[i].dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
            pRes->resource_array[i].nFileSizeLow = 0;
            pRes->resource_array[i].nFileSizeHigh = 0;
            pRes->resource_array[i].ftCreationTime = get_now_time();
            pRes->resource_array[i].ftLastWriteTime = get_now_time();
        } else {
            pRes->resource_array[i].dwFileAttributes = 0;
            if(item["size"].is_string())
                pRes->resource_array[i].nFileSizeLow = (DWORD) std::stoi(item["size"].get<std::string>());
            else
                pRes->resource_array[i].nFileSizeLow = (DWORD) 0;
            pRes->resource_array[i].nFileSizeHigh = 0;
            pRes->resource_array[i].ftCreationTime = parse_iso_time(item["createdTime"].get<std::string>());
            pRes->resource_array[i].ftLastWriteTime = parse_iso_time(item["modifiedTime"].get<std::string>());
        }

        resourceNamesMap[item["name"].get<std::string>()] = item["id"].get<std::string>();

        i++;
    }

    return pRes;
}

void GoogleDriveClient::makeFolder(std::string utf8Path)
{
    std::string folderName, parentFolderId("root");
    int p = utf8Path.find_last_of('/');
    folderName = utf8Path.substr(p + 1);
    if(p>0){
        int p2 = utf8Path.substr(0, p).find_last_of('/');
        std::string parentFolderName = utf8Path.substr(p2+1, p-1);
        if(resourceNamesMap.find(parentFolderName) != resourceNamesMap.end())
            parentFolderId = resourceNamesMap[parentFolderName];
    }

    json jsBody = {
            {"name", folderName},
            {"mimeType", "application/vnd.google-apps.folder"},
            {"parents", {parentFolderId}}
    };

    httplib::Headers hd = headers;
    hd.emplace("Content-Type", "application/json");

    auto r = http_client->Post("/drive/v3/files", headers, jsBody.dump(), "application/json");

    if(r.get() && r->status==200){
        json js = json::parse(r->body);
        resourceNamesMap["folderName"] = js["id"].get<std::string>();
    } else {
        throw_response_error(r.get());
    }
}

void GoogleDriveClient::removeResource(std::string utf8Path)
{
    int p = utf8Path.find_last_of('/');
    std::string resourceName = utf8Path.substr(p + 1);
    if(resourceNamesMap.find(resourceName) == resourceNamesMap.end())
        throw std::runtime_error("resource ID not found");

    std::string url("/drive/v3/files/");
    url += resourceNamesMap[resourceName];
    auto r = http_client->Delete(url.c_str(), headers);

    if(!r.get() || r->status!=204){
        throw_response_error(r.get());
    }
}

void GoogleDriveClient::downloadFile(std::string path, std::ofstream &ofstream)
{
    int p = path.find_last_of('/');
    std::string resourceName = path.substr(p + 1);
    if(resourceNamesMap.find(resourceName) == resourceNamesMap.end())
        throw std::runtime_error("resource ID not found");

    std::string url("/drive/v3/files/");
    url += resourceNamesMap[resourceName];
    url += "?alt=media";

    auto r = http_client->Get(url.c_str(), headers);

    if(r.get() && r->status == 200){
        ofstream.write(r->body.data(), r->body.size());
    } else {
        throw_response_error(r.get());
    }
}

void GoogleDriveClient::uploadFile(std::string path, std::ifstream &ifstream, BOOL overwrite)
{
    std::string resourceName, parentFolderId("root");
    int p = path.find_last_of('/');
    resourceName = path.substr(p + 1);
    if(p>0){
        int p2 = path.substr(0, p).find_last_of('/');
        std::string parentFolderName = path.substr(p2+1, p-1);
        if(resourceNamesMap.find(parentFolderName) != resourceNamesMap.end())
            parentFolderId = resourceNamesMap[parentFolderName];
    }

    json jsParams = {
            {"name", resourceName},
            {"parents", {parentFolderId}},
            //TODO add create_datetime
    };

    auto r = http_client->Post("/upload/drive/v3/files?uploadType=resumable", headers, jsParams.dump(), "application/json");

    if(!r.get() || r->status != 200)
        throw_response_error(r.get());

    if(!r->has_header("Location"))
        throw std::runtime_error("Cannot find Location header");

    std::string downloadUrl = r->get_header_value("Location").substr(26); // remove 'https://www.googleapis.com'

    std::stringstream body;
    body << ifstream.rdbuf();

    r = http_client->Put(downloadUrl.c_str(), headers, body.str(), "application/octet-stream");

    if(r.get() && (r->status==200 || r->status==201)){
        return;
    } else {
        // TODO resume interrupted upload
        // https://developers.google.com/drive/api/v3/resumable-upload
        throw_response_error(r.get());
    }
}

void GoogleDriveClient::move(std::string from, std::string to, BOOL overwrite)
{
    std::string fileId, oldParentId("root"), newParentId("root");
    int p = from.find_last_of('/');
    std::string fileName = from.substr(p + 1);
    if(resourceNamesMap.find(fileName) == resourceNamesMap.end())
        throw std::runtime_error("Cannot find file ID");

    fileId = resourceNamesMap[fileName];

    if(p>0){
        int p2 = from.substr(0, p).find_last_of('/');
        std::string oldParentName = from.substr(p2+1, p-1);
        if(resourceNamesMap.find(oldParentName) != resourceNamesMap.end())
            oldParentId = resourceNamesMap[oldParentName];
    }

    p = to.find_last_of('/');
    if(p>0){
        int p2 = to.substr(0, p).find_last_of('/');
        std::string newParentName = to.substr(p2+1, p-1);
        if(resourceNamesMap.find(newParentName) != resourceNamesMap.end())
            newParentId = resourceNamesMap[newParentName];
    }

    std::string url("/drive/v3/files/");
    url += fileId;
    url += "?removeParents=";
    url += oldParentId;
    url += "&addParents=";
    url += newParentId;

    auto r = http_client->Patch(url.c_str(), headers, "", "application/json");

    if(!r.get() || r->status!=200)
        throw_response_error(r.get());
}

void GoogleDriveClient::copy(std::string from, std::string to, BOOL overwrite)
{
    std::string fileId, fromFileName, toFileName, toFolderId("root");
    int p = from.find_last_of('/');
    fromFileName = from.substr(p + 1);
    if(resourceNamesMap.find(fromFileName) == resourceNamesMap.end())
        throw std::runtime_error("Cannot find file ID");

    fileId = resourceNamesMap[fromFileName];

    p = to.find_last_of('/');
    toFileName = to.substr(p + 1);
    if(p>0){
        int p2 = to.substr(0, p).find_last_of('/');
        std::string newFolderName = to.substr(p2+1, p-1);
        if(resourceNamesMap.find(newFolderName) != resourceNamesMap.end())
            toFolderId = resourceNamesMap[newFolderName];
    }

    json jsBody = {
            {"name", toFileName},
            {"parents", {toFolderId}},
    };

    std::string url("/drive/v3/files/");
    url += fileId;
    url += "/copy";

    auto r = http_client->Post(url.c_str(), headers, jsBody.dump(), "application/json");

    if(!r.get() || r->status!=200)
        throw_response_error(r.get());
}