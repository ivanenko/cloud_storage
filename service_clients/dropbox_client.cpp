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

#include "dropbox_client.h"
#include "../plugin_utils.h"

#define CHUNK_SIZE 150000000

DropboxClient::DropboxClient()
{
    m_http_client = new httplib::SSLClient("api.dropboxapi.com");
    m_client_id = "ovy1encsqm627kl";
}

DropboxClient::~DropboxClient()
{
    delete m_http_client;
}

std::string DropboxClient::get_auth_page_url()
{
    std::string url("https://www.dropbox.com/oauth2/authorize?response_type=token\\&redirect_uri=http%3A%2F%2Flocalhost%3A3359%2Fget_token\\&client_id=");
    url += _get_client_id();
    return url;
}

void DropboxClient::set_oauth_token(const char *token)
{
    assert(token != NULL);

    std::string header_token = "Bearer ";
    header_token += token;
    m_headers = { {"Authorization", header_token} };
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
            {"limit", 200}
    };

    auto r = m_http_client->Post("/2/files/list_folder", m_headers, jsBody.dump(), "application/json");

    if(r.get() && r->status==200){
        pResources pRes = new tResources;
        pRes->nCount = 0;

        auto js = json::parse(r->body);
        prepare_folder_result(js, pRes, (path == "/") );

        while(js["has_more"].is_boolean() && js["has_more"].get<bool>() == true){
            jsBody = {
                    {"cursor", js["cursor"].get<std::string>()}
            };
            r = m_http_client->Post("/2/files/list_folder/continue", m_headers, jsBody.dump(), "application/json");
            if(!r.get() || r->status!=200)
                throw_response_error(r.get());

            js = json::parse(r->body);
            prepare_folder_result(js, pRes, (path == "/") );
        }

        return pRes;
    } else {
        throw_response_error(r.get());
    }
}

void DropboxClient::prepare_folder_result(const json &js, pResources pRes, BOOL isRoot)
{
    if(!js["entries"].is_array())
        throw std::runtime_error("Wrong Json format");

    int total = js["entries"].size();
    pRes->resource_array.reserve(pRes->resource_array.size() + total);

    int i=0;
    for(auto& item: js["entries"]){
        WIN32_FIND_DATAW file;
        wcharstring wName = UTF8toUTF16(item["name"].get<std::string>());

        size_t str_size = (MAX_PATH > wName.size()+1)? (wName.size()+1): MAX_PATH;
        memcpy(file.cFileName, wName.data(), sizeof(WCHAR) * str_size);

        if(item[".tag"].get<std::string>() == "file") {
            file.dwFileAttributes = 0;
            file.nFileSizeLow = (DWORD) item["size"].get<int>();
            file.nFileSizeHigh = 0;
            file.ftCreationTime = parse_iso_time(item["client_modified"].get<std::string>());
            file.ftLastWriteTime = parse_iso_time(item["server_modified"].get<std::string>());
        } else {
            file.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
            file.nFileSizeLow = 0;
            file.nFileSizeHigh = 0;
            file.ftCreationTime = get_now_time();
            file.ftLastWriteTime = get_now_time();
        }

        pRes->resource_array.push_back(file);

        i++;
    }
}

void DropboxClient::makeFolder(std::string utf8Path)
{
    json jsBody = {
            {"path", utf8Path},
            {"autorename", true}
    };

    auto r = m_http_client->Post("/2/files/create_folder_v2", m_headers, jsBody.dump(), "application/json");

    if(!r.get() || r->status!=200)
        throw_response_error(r.get());
}

void DropboxClient::removeResource(std::string utf8Path)
{
    json jsBody = { {"path", utf8Path} };

    auto r = m_http_client->Post("/2/files/delete_v2", m_headers, jsBody.dump(), "application/json");

    if(!r.get() || r->status!=200)
        throw_response_error(r.get());
}

void DropboxClient::downloadFile(std::string path, std::ofstream &ofstream, std::string localPath)
{
    httplib::SSLClient cli("content.dropboxapi.com");
    json jsParams = { {"path", path} };

    httplib::Headers hd = m_headers;
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
    ifstream.seekg(0, ifstream.end);
    int fileSize = ifstream.tellg();
    ifstream.seekg(0, ifstream.beg);

    if(fileSize < CHUNK_SIZE){ // use upload_session/start for files more than 150Mb
        _upload_small_file(path, ifstream, overwrite);
    } else {
        _upload_big_file(path, ifstream, overwrite, fileSize);
    }
}

void DropboxClient::_upload_small_file(std::string& path, std::ifstream &ifstream, BOOL overwrite)
{
    httplib::SSLClient cli("content.dropboxapi.com");
    json jsParams = {
            {"path", path},
            {"mode", { {".tag", (overwrite? "overwrite": "add")} } },
            {"autorename", false}
    };

    httplib::Headers hd = m_headers;
    hd.emplace("Dropbox-API-Arg", jsParams.dump());

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

void DropboxClient::_upload_big_file(std::string& path, std::ifstream &ifstream, BOOL overwrite, int fileSize)
{
    int count = fileSize / CHUNK_SIZE + ((fileSize % CHUNK_SIZE) > 0 ? 1 : 0);

    httplib::SSLClient cli("content.dropboxapi.com");
    httplib::Headers hd = m_headers;
    json jsParams = { {"close", false} };
    hd.emplace("Dropbox-API-Arg", jsParams.dump());

    int offset = 0;
    std::string body;
    body.reserve(CHUNK_SIZE);
    body.resize(CHUNK_SIZE);
    ifstream.read(&body[0], CHUNK_SIZE);

    auto r = cli.Post("/2/files/upload_session/start", hd, body, "application/octet-stream");

    if(!r.get() || r->status!=200)
        throw_response_error(r.get());

    const auto js = json::parse(r->body);
    std::string session_id = js["session_id"].get<std::string>();

    int c = count - 2;
    while(c > 0){
        offset += CHUNK_SIZE;
        c--;

        jsParams = {
                {"cursor",
                          { {"session_id", session_id}, {"offset", offset} }
                },
                {"close", false}
        };
        hd.erase("Dropbox-API-Arg");
        hd.emplace("Dropbox-API-Arg", jsParams.dump());
        ifstream.read(&body[0], CHUNK_SIZE);
        r = cli.Post("/2/files/upload_session/append_v2", hd, body, "application/octet-stream");

        if(!r.get() || r->status!=200){
            // 409 is reserved for file exists error
            if(r->status == 409)
                r->status = 500;
            throw_response_error(r.get());
        }

        //TODO check incorrect_offset error - to restart from correct one, status = 409
    }

    offset += CHUNK_SIZE;
    jsParams = {
            {"cursor",
                      { {"session_id", session_id}, {"offset", offset} }
            },
            {"commit",
                    {
                            {"path", path},
                            {"mode", { {".tag", (overwrite? "overwrite": "add")} } },
                            {"autorename", false}
                    }
            }
    };
    hd.erase("Dropbox-API-Arg");
    hd.emplace("Dropbox-API-Arg", jsParams.dump());
    body.resize(fileSize - offset);
    ifstream.read(&body[0], fileSize - offset);
    r = cli.Post("/2/files/upload_session/finish", hd, body, "application/octet-stream");

    if(r.get() && r->status == 200){
        return;
    } else {
        throw_response_error(r.get());
    }

}

void DropboxClient::move(std::string from, std::string to, BOOL overwrite)
{
    json jsBody = {
            {"from_path", from},
            {"to_path", to},
            {"autorename", false}
    };

    auto r = m_http_client->Post("/2/files/move_v2", m_headers, jsBody.dump(), "application/json");

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

    auto r = m_http_client->Post("/2/files/copy_v2", m_headers, jsBody.dump(), "application/json");

    if(!r.get() || r->status!=200)
        throw_response_error(r.get());
}

void DropboxClient::saveFromUrl(std::string urlFrom, std::string pathTo)
{
    json jsBody = {
            {"path", pathTo},
            {"url", urlFrom}
    };

    auto r = m_http_client->Post("/2/files/save_url", m_headers, jsBody.dump(), "application/json");
    if(!r.get() || r->status!=200)
        throw_response_error(r.get());

    json js = json::parse(r->body);
    if(js["async_job_id"].is_string()){
        json js_async = { {"async_job_id", js["async_job_id"].get<std::string>()} };
        BOOL success = false;
        int waitCount = 0;
        do{
            std::this_thread::sleep_for(std::chrono::seconds(2));
            auto r2 = m_http_client->Post("/2/files/save_url/check_job_status", m_headers, js_async.dump(), "application/json");
            if(!r2.get() || r2->status!=200)
                throw std::runtime_error("Error getting operation status");

            json js2 = json::parse(r2->body);
            if(js2[".tag"].is_string() && js2[".tag"].get<std::string>()=="complete")
                success = true;

            if(js2["error"].is_string())
                throw service_client_exception(500, "Operation error");

            // if(json2[".tag"].string_value() == "in_progress") - Do nothing, just wait
            waitCount++;

        } while(success==false && waitCount<30*5); //wait at least 5 min

        if(success==false && waitCount>=30*5)
            throw std::runtime_error("Operation is too long");
    }
}

void DropboxClient::downloadZip(std::string pathFrom, std::string pathTo)
{
    httplib::SSLClient cli("content.dropboxapi.com");
    pathFrom.erase(--pathFrom.end()); // remove last slash
    json jsParams = { {"path", pathFrom} };

    if(file_exists(pathTo))
        throw std::runtime_error("File exists");

    std::ofstream ofs;
    ofs.open(pathTo, std::ios::binary | std::ofstream::out | std::ios::trunc);
    if(!ofs || ofs.bad())
        throw std::runtime_error("File create error");

    httplib::Headers hd = m_headers;
    hd.emplace("Dropbox-API-Arg", jsParams.dump());
    std::string strEmpty;

    auto r = cli.Post("/2/files/download_zip", hd, strEmpty, "text/plain");

    if(r.get() && r->status == 200){
        ofs.write(r->body.data(), r->body.size());
        ofs.close();
    } else {
        throw_response_error(r.get());
    }
}

void DropboxClient::run_command(std::string remoteName, std::vector<std::string> &arguments)
{
    if(arguments[0] == "download"){
        std::string fname;
        if(arguments.size()==2){
            // extract filename from url
            std::string::size_type p = arguments[1].find_last_of('/');
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

    if(arguments[0] == "zip"){
        //TODO check dest file name

        if(arguments.size()==2){ // no folder name in command, use remoteName as source
            downloadZip(remoteName, arguments[1]);
        } else if(arguments.size()==3){
            downloadZip(arguments[1], arguments[2]);
        } else {
            throw std::runtime_error("Command format error");
        }

        return;
    }

    throw std::runtime_error("Command is not supported");
}
