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

#include <future>
#include "dropbox_client.h"
#include "../plugin_utils.h"


DropboxClient::DropboxClient()
{
    http_client = new httplib::SSLClient("api.dropboxapi.com");
}

DropboxClient::~DropboxClient()
{
    delete http_client;
}

std::string DropboxClient::get_auth_page_url()
{
    std::string url("https://www.dropbox.com/oauth2/authorize?client_id=ovy1encsqm627kl\\&response_type=token\\&redirect_uri=http%3A%2F%2Flocalhost%3A3359%2Fget_token");
    return url;
}

void DropboxClient::set_oauth_token(const char *token)
{
    assert(token != NULL);
    //this->token = token;

    std::string header_token = "Bearer ";
    header_token += token;
    headers = { {"Authorization", header_token} };
}

void DropboxClient::throw_response_error(httplib::Response* resp){
    if(resp){
        std::string message;
        try{
            json js = json::parse(resp->body);
            if(js["error_summary"].is_string())
                message = js["error_summary"].get<std::string>();
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

pResources DropboxClient::get_resources(std::string path, BOOL isTrash)
{
    json jsBody = {
            {"path", (path=="/")? "": path},
            {"recursive", false},
            {"limit", 2000}
    };

    auto r = http_client->Post("/2/files/list_folder", headers, jsBody.dump(), "application/json");

    if(r.get() && r->status==200){
        const auto js = json::parse(r->body);

        //TODO check if has_more=true
        return prepare_folder_result(js, (path == "/") );
    } else {
        throw_response_error(r.get());
    }
}

pResources DropboxClient::prepare_folder_result(json js, BOOL isRoot)
{
    if(!js["entries"].is_array())
        throw std::runtime_error("Wrong Json format");

    int total = js["entries"].size();

    pResources pRes = new tResources;
    pRes->nSize = total;
    pRes->nCount = 0;
    pRes->resource_array = new WIN32_FIND_DATAW[pRes->nSize];

    int i=0;
    for(auto& item: js["entries"]){
        wcharstring wName = UTF8toUTF16(item["name"].get<std::string>());

        size_t str_size = (MAX_PATH > wName.size()+1)? (wName.size()+1): MAX_PATH;
        memcpy(pRes->resource_array[i].cFileName, wName.data(), sizeof(WCHAR) * str_size);

        if(item[".tag"].get<std::string>() == "file") {
            pRes->resource_array[i].dwFileAttributes = 0;
            pRes->resource_array[i].nFileSizeLow = (DWORD) item["size"].get<int>();
            pRes->resource_array[i].nFileSizeHigh = 0;
            pRes->resource_array[i].ftCreationTime = parse_iso_time(item["client_modified"].get<std::string>());
            pRes->resource_array[i].ftLastWriteTime = parse_iso_time(item["server_modified"].get<std::string>());
        } else {
            pRes->resource_array[i].dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
            pRes->resource_array[i].nFileSizeLow = 0;
            pRes->resource_array[i].nFileSizeHigh = 0;
            pRes->resource_array[i].ftCreationTime = get_now_time();
            pRes->resource_array[i].ftLastWriteTime = get_now_time();
        }

        i++;
    }

    return pRes;
}

void DropboxClient::makeFolder(std::string utf8Path)
{
    json jsBody;
    jsBody["path"] = utf8Path;
    jsBody["autorename"] = false;

    auto r = http_client->Post("/2/files/create_folder_v2", headers, jsBody.dump(), "application/json");

    if(!r.get() || r->status!=200)
        throw_response_error(r.get());
}

void DropboxClient::removeResource(std::string utf8Path)
{
    json jsBody = { {"path", utf8Path} };

    auto r = http_client->Post("/2/files/delete_v2", headers, jsBody.dump(), "application/json");

    if(!r.get() || r->status!=200)
        throw_response_error(r.get());
}

void DropboxClient::downloadFile(std::string path, std::ofstream &ofstream)
{
    httplib::SSLClient cli("content.dropboxapi.com");
    json jsParams = { {"path", path} };

    httplib::Headers hd = headers;
    hd.emplace("Dropbox-API-Arg", jsParams.dump());
    std::string strEmpty;

    auto r = cli.Post("/2/files/download", hd, strEmpty, "text/plain");

    if(r.get() && r->status == 200){
        ofstream.write(r->body.data(), r->body.size());
    } else {
        throw_response_error(r.get());
    }
}

void DropboxClient::uploadFile(std::string path, std::ifstream &ifstream, BOOL overwrite)
{
    //TODO use upload_session/start for files more than 150Mb

    httplib::SSLClient cli("content.dropboxapi.com");
    json jsParams = {
            {"path", path},
            {"mode", { {".tag", (overwrite? "overwrite": "add")} } },
            {"autorename", false}
    };

    httplib::Headers hd = headers;
    hd.emplace("Dropbox-API-Arg", jsParams.dump());
    std::string strEmpty;

    std::stringstream body;
    body << ifstream.rdbuf();

    auto r = cli.Post("/2/files/upload", hd, body.str(), "application/octet-stream");

    if(r.get() && r->status == 200){
        return;
    } else {
        throw_response_error(r.get());
    }

    //TODO throw status 409 if file exists
}

void DropboxClient::move(std::string from, std::string to, BOOL overwrite)
{
    json jsBody = {
            {"from_path", from},
            {"to_path", to},
            {"autorename", false}
    };

    auto r = http_client->Post("/2/files/move_v2", headers, jsBody.dump(), "application/json");

    if(!r.get() || r->status!=200)
        throw_response_error(r.get());
}

void DropboxClient::copy(std::string from, std::string to, BOOL overwrite)
{
    json jsBody = {
            {"from_path", from},
            {"to_path", to},
            {"autorename", false}
    };

    auto r = http_client->Post("/2/files/copy_v2", headers, jsBody.dump(), "application/json");

    if(!r.get() || r->status!=200)
        throw_response_error(r.get());
}

void DropboxClient::run_command(std::string remoteName, std::vector<std::string> &arguments)
{

}
