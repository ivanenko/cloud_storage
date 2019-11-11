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

#include <regex>
#include "onedrive_client.h"
#include "../plugin_utils.h"

#include <iostream>

#define CHUNK_SIZE 4000000


OneDriveClient::OneDriveClient()
{
    m_http_client = new httplib::SSLClient("graph.microsoft.com");
    m_resourceNamesMap["/"] = "root";
    m_client_id = "123";
}

OneDriveClient::~OneDriveClient()
{
    delete m_http_client;
}

std::string OneDriveClient::get_auth_page_url()
{
    std::string url("https://login.microsoftonline.com/common/oauth2/v2.0/authorize?scope=onedrive.readwrite\\&response_type=token\\&redirect_uri=http://localhost:3359/get_token\\&client_id=");
    url += _get_client_id();
    return url;
}

void OneDriveClient::throw_response_error(httplib::Response* resp){
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

void OneDriveClient::set_oauth_token(const char *token)
{
    assert(token != NULL);

    std::string header_token = "Bearer ";
    header_token += token;
    //std::cout << token;
    m_headers = { {"Authorization", header_token}, {"Accept", "application/json"} };
}

pResources OneDriveClient::get_resources(std::string path, BOOL isTrash)
{
    std::string folderId("root"), folderName;
    if (m_resourceNamesMap.find(path) != m_resourceNamesMap.end())
        folderId = m_resourceNamesMap[path];

    std::string url;
    if(folderId == "root"){
        url = "/v1.0/me/drive/root/children";
    } else {
        url = "/v1.0/me/drive/items/" + folderId + "/children";
    }

    auto r = m_http_client->Get(url.c_str(), m_headers);

    if(r.get() && r->status==200){
        const auto js = json::parse(r->body);

        return prepare_folder_result(js, path);
    } else {
        throw_response_error(r.get());
    }
}

pResources OneDriveClient::prepare_folder_result(json js, std::string &path)
{
    if(!js["value"].is_array())
        throw std::runtime_error("Wrong Json format");

    BOOL isRoot = (path == "/");
    int total = js["value"].size();

    pResources pRes = new tResources;
    pRes->nCount = 0;
    pRes->resource_array.resize(isRoot ? total+1: total);

    int i=0;
    for(auto& item: js["value"]){
        wcharstring wName = UTF8toUTF16(item["name"].get<std::string>());

        size_t str_size = (MAX_PATH > wName.size()+1)? (wName.size()+1): MAX_PATH;
        memcpy(pRes->resource_array[i].cFileName, wName.data(), sizeof(WCHAR) * str_size);

        if(item.find("folder") != item.end()) {
            pRes->resource_array[i].dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
            pRes->resource_array[i].nFileSizeLow = 0;
            pRes->resource_array[i].nFileSizeHigh = 0;
            pRes->resource_array[i].ftCreationTime = get_now_time();
            pRes->resource_array[i].ftLastWriteTime = get_now_time();
        } else {
            pRes->resource_array[i].dwFileAttributes = 0;
            if(item["size"].is_number())
                pRes->resource_array[i].nFileSizeLow = (DWORD) item["size"].get<int>();
            else
                pRes->resource_array[i].nFileSizeLow = (DWORD) 0;
            pRes->resource_array[i].nFileSizeHigh = 0;
            pRes->resource_array[i].ftCreationTime = parse_iso_time(item["createdDateTime"].get<std::string>());
            pRes->resource_array[i].ftLastWriteTime = parse_iso_time(item["lastModifiedDateTime"].get<std::string>());
        }

        std::string p = path;
        if(path != "/")
            p += "/";
        p += item["name"].get<std::string>();
        m_resourceNamesMap[p] = item["id"].get<std::string>();
        //m_mimetypesMap[p] = item["mimeType"].get<std::string>();

        i++;
    }

    if(isRoot){
        memcpy(pRes->resource_array[total].cFileName, u".Trash", sizeof(WCHAR) * 7);
        pRes->resource_array[total].dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        pRes->resource_array[total].nFileSizeLow = 0;
        pRes->resource_array[total].nFileSizeHigh = 0;
        pRes->resource_array[total].ftCreationTime = get_now_time();
        pRes->resource_array[total].ftLastWriteTime = get_now_time();
    }

    return pRes;
}

void OneDriveClient::makeFolder(std::string utf8Path)
{
    std::string folderName, parentFolderId("root");
    int p = utf8Path.find_last_of('/');
    folderName = utf8Path.substr(p + 1);
    if(p>0){
        std::string parentFolderPath = utf8Path.substr(0, p);
        if(m_resourceNamesMap.find(parentFolderPath) != m_resourceNamesMap.end())
            parentFolderId = m_resourceNamesMap[parentFolderPath];
    }

    std::string url;
    if(parentFolderId == "root"){
        url = "/v1.0/me/drive/root/children";
    } else {
        url = "/v1.0/me/drive/items/" + parentFolderId + "/children";
    }

    json jsBody = {
            {"name", folderName},
            {"folder", json::object()},
            {"@microsoft.graph.conflictBehavior", "rename"}
    };

    std::cout << jsBody.dump();

    httplib::Headers hd = m_headers;
    hd.emplace("Content-Type", "application/json");

    auto r = m_http_client->Post(url.c_str(), hd, jsBody.dump(), "application/json");

    if(r.get() && r->status==201){
        json js = json::parse(r->body);
        m_resourceNamesMap[utf8Path] = js["id"].get<std::string>();
    } else {
        throw_response_error(r.get());
    }
}

void OneDriveClient::removeResource(std::string utf8Path)
{
    if(m_resourceNamesMap.find(utf8Path) == m_resourceNamesMap.end())
        throw std::runtime_error("resource ID not found");

    std::string url("/v1.0/me/drive/items/");
    url += m_resourceNamesMap[utf8Path];
    auto r = m_http_client->Delete(url.c_str(), m_headers);

    if(!r.get() || r->status!=204){
        throw_response_error(r.get());
    }
}

void OneDriveClient::downloadFile(std::string path, std::ofstream &ofstream, std::string localPath)
{
    if(m_resourceNamesMap.find(path) == m_resourceNamesMap.end())
        throw std::runtime_error("resource ID not found");

    std::string url("/v1.0/me/drive/items/");
    url += m_resourceNamesMap[path];
    url += "/content";

    auto r = m_http_client->Get(url.c_str(), m_headers);
    if(r.get() && r->status==302){
        url = r->get_header_value("Location");

        std::string server_url, request_url;

        std::smatch m;
        auto pattern = std::regex("https://(.+?)(/.+)");
        if (std::regex_search(url, m, pattern)) {
            server_url = m[1].str();
            request_url = m[2].str();
        } else {
            throw std::runtime_error("Error parsing file href for download");
        }

        // TODO download big file by parts with range header
        httplib::SSLClient cli2(server_url.c_str());
        auto r2 = cli2.Get(request_url.c_str());

        if(r2.get() && r2->status == 200){
            ofstream.write(r2->body.data(), r2->body.size());
        } else {
            throw_response_error(r2.get());
        }
    }
}

void OneDriveClient::uploadFile(std::string path, std::ifstream &ifstream, BOOL overwrite)
{
    ifstream.seekg(0, ifstream.end);
    int fileSize = ifstream.tellg();
    ifstream.seekg(0, ifstream.beg);

    if(fileSize < CHUNK_SIZE){ // use upload_session/start for files more than 150Mb
        _upload_small_file(path, ifstream, overwrite);
    } else {
        _upload_big_file(path, ifstream, overwrite, fileSize);
    }
}

void OneDriveClient::_upload_small_file(std::string& path, std::ifstream &ifstream, BOOL overwrite)
{
    std::stringstream body;
    body << ifstream.rdbuf();

    std::string fileName, parentFolderId("root");
    int p = path.find_last_of('/');
    fileName = path.substr(p + 1);
    if(p>0){
        std::string parentFolderPath = path.substr(0, p);
        if(m_resourceNamesMap.find(parentFolderPath) != m_resourceNamesMap.end())
            parentFolderId = m_resourceNamesMap[parentFolderPath];
    }

    std::string url("/v1.0/me/drive/items/");
    url += parentFolderId;
    url += ":/";
    url += url_encode(fileName);
    url += ":/content";

    auto r = m_http_client->Put(url.c_str(), m_headers, body.str(), "application/octet-stream");

    if(r.get() && r->status == 201){
        return;
    } else {
        throw_response_error(r.get());
    }

}

void OneDriveClient::_upload_big_file(std::string& path, std::ifstream &ifstream, BOOL overwrite, int fileSize)
{
    std::string fileName, parentFolderId("root");
    int p = path.find_last_of('/');
    fileName = path.substr(p + 1);
    if(p>0){
        std::string parentFolderPath = path.substr(0, p);
        if(m_resourceNamesMap.find(parentFolderPath) != m_resourceNamesMap.end())
            parentFolderId = m_resourceNamesMap[parentFolderPath];
    }

    std::string url("/v1.0/me/drive/items/");
    url += parentFolderId;
    url += ":/";
    url += fileName;
    url += ":/createUploadSession";

    json jsParams = { {"item", {"@microsoft.graph.conflictBehavior", "rename"} } };
    auto r = m_http_client->Post("/2/files/upload_session/start", m_headers, jsParams.dump(), "application/json");

    std::string uploadUrl;
    if(r.get() && r->status == 200) {
        json js = json::parse(r->body);
        uploadUrl = js["uploadUrl"].get<std::string>();
    } else {
        throw_response_error(r.get());
    }

    std::string server_url, request_url;
    std::smatch m;
    auto pattern = std::regex("https://(.+?)(/.+)");
    if (std::regex_search(url, m, pattern)) {
        server_url = m[1].str();
        request_url = m[2].str();
    } else {
        throw std::runtime_error("Error parsing file href for upload");
    }

    httplib::SSLClient cli(server_url.c_str());
    httplib::Headers hd; // = m_headers;
    int count = fileSize / CHUNK_SIZE + ((fileSize % CHUNK_SIZE) > 0 ? 1 : 0);
    int offset = 0, c = 0;

    std::string body;
    body.reserve(CHUNK_SIZE);
    body.resize(CHUNK_SIZE);

    while(count >= 0){

        hd.emplace("Content-Length", std::to_string((count == 0 ? fileSize - offset : CHUNK_SIZE)) );
        int rest = (count == 0 ? fileSize - 1 : c * offset + CHUNK_SIZE - 1);

        std::stringstream range;
        range << "bytes" << c * offset << "-" << rest << "/" << fileSize;

        hd.erase("Content-Range");
        hd.emplace("Content-Range", range.str());

        ifstream.read(&body[0], (count == 0 ? fileSize - offset : CHUNK_SIZE));

        r = cli.Put(request_url.c_str(), hd, body, "application/octet-stream");

        offset += CHUNK_SIZE;
        count --;
        c++;
    }

}

void OneDriveClient::move(std::string from, std::string to, BOOL overwrite)
{
    std::string fileId, newParentId("root");
    if(m_resourceNamesMap.find(from) == m_resourceNamesMap.end())
        throw std::runtime_error("Cannot find file ID");

    fileId = m_resourceNamesMap[from];

    int p = to.find_last_of('/');
    std::string toFileName = to.substr(p + 1);
    if(p>0){
        std::string newParentPath = to.substr(0, p);
        if(m_resourceNamesMap.find(newParentPath) != m_resourceNamesMap.end())
            newParentId = m_resourceNamesMap[newParentPath];
    }

    std::string url("/v1.0/me/drive/items/");
    url += fileId;

    json jsParams = {
            {"parentReference", {"id", newParentId} },
            "name", toFileName
    };

    auto r = m_http_client->Patch(url.c_str(), m_headers, jsParams.dump(), "application/json");

    if(!r.get() || r->status!=200)
        throw_response_error(r.get());
}

void OneDriveClient::copy(std::string from, std::string to, BOOL overwrite)
{
    std::string fileId, toFileName, toFolderId("root");
    if(m_resourceNamesMap.find(from) == m_resourceNamesMap.end())
        throw std::runtime_error("Cannot find file ID");

    fileId = m_resourceNamesMap[from];

    int p = to.find_last_of('/');
    toFileName = to.substr(p + 1);
    if(p>0){
        std::string newFolderPath = to.substr(0, p);
        if(m_resourceNamesMap.find(newFolderPath) != m_resourceNamesMap.end())
            toFolderId = m_resourceNamesMap[newFolderPath];
    }

    std::string url("/v1.0/me/drive/items/");
    url += fileId;
    url += "/copy";

    json jsParams = {
            {"parentReference", {"id", toFolderId} },
            "name", toFileName
    };

    auto r = m_http_client->Post(url.c_str(), m_headers, jsParams.dump(), "application/json");

    if(!r.get() || r->status!=202)
        throw_response_error(r.get());
}

void OneDriveClient::cleanTrash()
{
    auto r = m_http_client->Delete("/drive/v3/files/trash", m_headers);

    if(!r.get() || r->status<=200 || r->status>=300)
        throw_response_error(r.get());
}