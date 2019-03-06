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
#include <future>
#include "../common.h"
#include "../library.h"
#include "../wfxplugin.h"
#include "../plugin_utils.h"
#include "yandex_rest_client.h"
#include "httplib.h"

YandexRestClient::YandexRestClient(){
    http_client = new httplib::SSLClient("cloud-api.yandex.net");
}

YandexRestClient::~YandexRestClient(){
    delete http_client;
}

std::string YandexRestClient::get_auth_page_url()
{
    std::string url("https://oauth.yandex.ru/authorize?client_id=bc2f272cc37349b7a1320b9ac7826ebf\\&response_type=token");
    return url;
}

void YandexRestClient::throw_response_error(httplib::Response* resp){
    if(resp){
        std::string message;
        auto js = json::parse(resp->body); //throws exception if error

        if(!js.is_null() && !js["description"].is_null())
            message = js["description"].get<std::string>();
        else
            message = resp->body;

        throw service_client_exception(resp->status, message);
    } else {
        throw std::runtime_error("Unknown error");
    }
}

void YandexRestClient::set_oauth_token(const char *token)
{
    assert(token != NULL);
    //this->token = token;

    std::string header_token = "OAuth ";
    header_token += token;
    headers = { {"Authorization", header_token} };
}

pResources YandexRestClient::get_resources(std::string path, BOOL isTrash) {
    //TODO turn off unnesesary fields
    std::string url("/v1/disk/");
    if(isTrash)
        url += "trash/";
    url += "resources?limit=1000&path=";
    url += url_encode(path);
    auto r = http_client->Get(url.c_str(), headers);

    if(r.get() && r->status==200){
        const auto json = json::parse(r->body);

        //TODO check if total > limit
        return prepare_folder_result(json, (path == "/") );
    } else {
        throw_response_error(r.get());
    }
}

pResources YandexRestClient::prepare_folder_result(json json, BOOL isRoot)
{
    if(json["_embedded"]["total"].is_null() || json["_embedded"]["items"].is_null())
        throw std::runtime_error("Wrong Json format");

    int total = json["_embedded"]["total"].get<int>();

    pResources pRes = new tResources;
    pRes->nSize = isRoot ? total+1: total;
    pRes->nCount = 0;
    pRes->resource_array = new WIN32_FIND_DATAW[pRes->nSize];

    int i=0;
    for(auto& item: json["_embedded"]["items"]){
        wcharstring wName = UTF8toUTF16(item["name"].get<std::string>());

        size_t str_size = (MAX_PATH > wName.size()+1)? (wName.size()+1): MAX_PATH;
        memcpy(pRes->resource_array[i].cFileName, wName.data(), sizeof(WCHAR) * str_size);

        if(item["type"].get<std::string>() == "file") {
            pRes->resource_array[i].dwFileAttributes = 0;
            pRes->resource_array[i].nFileSizeLow = (DWORD) item["size"].get<int>();
            pRes->resource_array[i].nFileSizeHigh = 0;
        } else {
            pRes->resource_array[i].dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
            pRes->resource_array[i].nFileSizeLow = 0;
            pRes->resource_array[i].nFileSizeHigh = 0;
        }
        pRes->resource_array[i].ftCreationTime = parse_iso_time(item["created"].get<std::string>());
        pRes->resource_array[i].ftLastWriteTime = parse_iso_time(item["modified"].get<std::string>());

        i++;
    }

    if(isRoot){
        memcpy(pRes->resource_array[total].cFileName, u".Trash", sizeof(WCHAR) * 7);
        pRes->resource_array[total].dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        pRes->resource_array[total].nFileSizeLow = 0;
        pRes->resource_array[total].nFileSizeHigh = 0;
    }

    return pRes;
}

void YandexRestClient::makeFolder(std::string utf8Path)
{
    std::string url("/v1/disk/resources?path=");
    url += url_encode(utf8Path);
    std::string empty_body;
    auto r = http_client->Put(url.c_str(), headers, empty_body, "text/plain");

    if(!r.get() || r->status!=201)
        throw_response_error(r.get());

}

void YandexRestClient::removeResource(std::string utf8Path)
{
    std::string url("/v1/disk/resources?path=");
    url += url_encode(utf8Path);
    auto r = http_client->Delete(url.c_str(), headers);

    if(r.get() && r->status==204){
        //empty folder or file was removed
        return;
    } else if(r.get() && r->status==202){
        //not empty folder is removing, get operation status
        wait_success_operation(r->body);
        return;
    } else {
        throw_response_error(r.get());
    }
}

void YandexRestClient::wait_success_operation(std::string &body)
{
    const auto json = json::parse(body);
    if(!json["href"].is_string())
        throw std::runtime_error("Getting operation status error: unknown json format");

    int pos = json["href"].get<std::string>().find("/v1/disk");
    std::string url_status = json["href"].get<std::string>().substr(pos);
    BOOL success = false;
    int waitCount = 0;
    do{
        std::this_thread::sleep_for(std::chrono::seconds(2));
        auto r2 = http_client->Get(url_status.c_str(), headers);
        if(r2.get() && r2->status==200){
            const auto json2 = json::parse(r2->body);

            if(!json2["status"].is_string())
                throw std::runtime_error("Getting operation status error: unknown json format");

            if(json2["status"].get<std::string>() == "success")
                success = true;

            if(json2["status"].get<std::string>() == "failure")
                throw service_client_exception(500, "Operation error");

            // if(json2["status"].string_value() == "in-progress") - Do nothing, just wait
            waitCount++;
        } else {
            throw std::runtime_error("Error getting operation status");
        }

    } while(success==false && waitCount<30); //wait at least 1 min

    if(success==false && waitCount>=30)
        throw std::runtime_error("Operation is too long");

}

void YandexRestClient::downloadFile(std::string path, std::ofstream &ofstream)
{
    std::string url("/v1/disk/resources/download?path=");
    url += url_encode(path);

    // Get download link
    auto r = http_client->Get(url.c_str(), headers);
    if(r.get() && r->status==200){
        const auto js = json::parse(r->body);

        if(!js["href"].is_string())
            throw std::runtime_error("Wrong json format");

        url = js["href"].get<std::string>();
    } else {
        throw_response_error(r.get());
    }

    _do_download(url, ofstream);

}

void YandexRestClient::_do_download(std::string url, std::ofstream &ofstream){
    std::string server_url, request_url;

    std::smatch m;
    auto pattern = std::regex("https://(.+?)(/.+)");
    if (std::regex_search(url, m, pattern)) {
        server_url = m[1].str();
        request_url = m[2].str();
    } else {
        throw std::runtime_error("Error parsing file href for download");
    }

    httplib::SSLClient cli2(server_url.c_str());
    auto r2 = cli2.Get(request_url.c_str());
    if(r2.get() && r2->status == 200){
        ofstream.write(r2->body.data(), r2->body.size());
    } else if(r2.get() && r2->status == 302){ //redirect
        if(r2->has_header("Location"))
            _do_download(r2->get_header_value("Location"), ofstream);
        else
            throw std::runtime_error("Cannot find redirect link");
    } else {
        throw_response_error(r2.get());
    }
}

void YandexRestClient::uploadFile(std::string path, std::ifstream& ifstream, BOOL overwrite)
{
    std::string url("/v1/disk/resources/upload?path=");
    url += url_encode(path);
    url += "&overwrite=";
    url += (overwrite ? "true": "false");

    auto r = http_client->Get(url.c_str(), headers);
    if(r.get() && r->status==200){
        const auto js = json::parse(r->body);
        url = js["href"].get<std::string>();
    } else {
        throw_response_error(r.get());
    }

    std::stringstream body;
    body << ifstream.rdbuf();

    std::string server_url, request_url;
    std::smatch m;
    auto pattern = std::regex("https://(.+?):443(/.+)");
    if (std::regex_search(url, m, pattern)) {
        server_url = m[1].str();
        request_url = m[2].str();
    } else {
        throw std::runtime_error("Error parsing file href for upload");
    }

    httplib::SSLClient cli2(server_url.c_str(), 443);
    auto r2 = cli2.Put(request_url.c_str(), headers, body.str(), "application/octet-stream");

    if(r2.get() && (r2->status==201 || r2->status==202)){
        return;
    } else {
        throw_response_error(r2.get());
    }
}

void YandexRestClient::move(std::string from, std::string to, BOOL overwrite)
{
    std::string url("/v1/disk/resources/move?from=");
    url += url_encode(from) + "&path=" + url_encode(to);
    url += "&overwrite=";
    url += (overwrite == true ? "true": "false");
    std::string empty_body;
    auto r = http_client->Post(url.c_str(), headers, empty_body, "text/plain");

    if(r.get() && r->status==201){
        //success
        return;
    } else if(r.get() && r->status==202){
        //not empty folder is moving, get operation status
        wait_success_operation(r->body);
        return;
    } else {
        throw_response_error(r.get());
    }
}

void YandexRestClient::copy(std::string from, std::string to, BOOL overwrite)
{
    //TODO - max 'to' path is the 32760 symbols
    std::string url("/v1/disk/resources/copy?from=");
    url += url_encode(from) + "&path=" + url_encode(to);
    url += "&overwrite=";
    url += (overwrite == true ? "true": "false");
    std::string empty_body;
    auto r = http_client->Post(url.c_str(), headers, empty_body, "text/plain");

    if(r.get() && r->status==201){
        //success
        return;
    } else if(r.get() && r->status==202){
        //not empty folder is copying, get operation status
        wait_success_operation(r->body);
        return;
    } else {
        throw_response_error(r.get());
    }
}

void YandexRestClient::cleanTrash()
{
    std::string url("/v1/disk/trash/resources");
    auto r = http_client->Delete(url.c_str(), headers);

    if(r.get() && r->status==204){
        return;
    } else if(r.get() && r->status==202){
        //not empty folder is removing, get operation status
        wait_success_operation(r->body);
        return;
    } else {
        throw_response_error(r.get());
    }
}

void YandexRestClient::deleteFromTrash(std::string utf8Path)
{
    std::string url("/v1/disk/trash/resources?path=");
    url += url_encode(utf8Path);
    auto r = http_client->Delete(url.c_str(), headers);

    if(r.get() && r->status==204){
        return;
    } else if(r.get() && r->status==202){
        //not empty folder is removing, get operation status
        wait_success_operation(r->body);
        return;
    } else {
        throw_response_error(r.get());
    }
}

void YandexRestClient::saveFromUrl(std::string urlFrom, std::string pathTo)
{
    std::string url("/v1/disk/resources/upload?url=");
    url += url_encode(urlFrom) + "&path=" + url_encode(pathTo);
    std::string empty_body;
    auto r = http_client->Post(url.c_str(), headers, empty_body, "text/plain");

    if(r.get() && r->status==202){
        wait_success_operation(r->body);
        return;
    } else {
        throw_response_error(r.get());
    }
}

void YandexRestClient::run_command(std::string remoteName, std::vector<std::string> &arguments)
{
    if(arguments[0] == "download"){
        std::string fname;
        if(arguments.size()==2){
            // extract filename from url
            std::string::size_type p = arguments[1].find_last_of((WCHAR)u'/');
            if(p == std::string::npos)
                throw std::runtime_error("Command error: cannot get filename from url");

            fname = remoteName + arguments[1].substr(p+1);
        } else if(arguments.size()==3){
            fname = remoteName + arguments[2];
        } else {
            throw std::runtime_error("Command format error");
        }

        saveFromUrl(arguments[1], fname);
        return;
    }

    if(arguments[0] == "trash"){
        if(arguments.size()==2 && arguments[1]=="clean"){
            cleanTrash();
            //int res = gRequestProcW(gPluginNumber, RT_MsgOKCancel, (WCHAR*)u"Warning", (WCHAR*)u"All files from trash will be removed!", NULL, 0);
            //if(res != 0){}
        }

//        if(arguments.size()==2){
//            WCHAR* p = (WCHAR*)u"/.Trash";
//            memcpy(RemoteName, p, sizeof(WCHAR) * 7);
//            return FS_EXEC_SYMLINK;
//        }
    }

    throw std::runtime_error("Command is not supported");
}
