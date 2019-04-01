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

#ifndef CLOUD_STORAGE_SERVICE_CLIENT_H
#define CLOUD_STORAGE_SERVICE_CLIENT_H

#include <string>
#include <sstream>
#include "../common.h"
#include "../json.hpp"
#include "../library.h"
#include "../extension.h"

using namespace nlohmann;

class service_client_exception: public std::runtime_error
{
public:
    service_client_exception(int status_code, const std::string& message):
            status_code(status_code),
            std::runtime_error(message)
    {
        std::stringstream s;
        s << "Error code: " << std::to_string(status_code);
        if(!message.empty())
            s << ";\n" << message;

        msg_ = s.str();
    }

    virtual const char* what() const throw () {
        msg_.c_str();
    }

    int get_status(){ return status_code; }

    std::string get_message(){ return std::runtime_error::what(); }

protected:
    int status_code;
    std::string msg_;
};


class ServiceClient {
protected:
    int m_port;
    std::string m_client_id;

    std::__cxx11::string url_encode(const std::__cxx11::string& s);
    int _get_port();
    virtual std::string _get_client_id() { return m_client_id; };

public:

    ServiceClient(){ m_port = 3359; };

    virtual ~ServiceClient() {};

    virtual std::string get_auth_page_url() = 0;

    virtual std::string get_oauth_token();

    virtual void set_oauth_token(const char *token) = 0;

    virtual json get_disk_info() = 0;

    virtual pResources get_resources(std::string path, BOOL isTrash) = 0;

    virtual void makeFolder(std::string utf8Path) = 0;

    virtual void removeResource(std::string utf8Path) = 0;

    virtual void downloadFile(std::string path, std::ofstream &ofstream) = 0;

    virtual void uploadFile(std::string path, std::ifstream &ifstream, BOOL overwrite) = 0;

    virtual void move(std::string from, std::string to, BOOL overwrite) = 0;

    virtual void copy(std::string from, std::string to, BOOL overwrite) = 0;

    virtual void saveFromUrl(std::string urlFrom, std::string pathTo) = 0;

    // remove all files from trash
    virtual void cleanTrash() = 0;

    // remove one file or folder from trash
    virtual void deleteFromTrash(std::string utf8Path) = 0;

    virtual void run_command(std::string remoteName, std::vector<std::string> &arguments) = 0;

    virtual void set_port(int port) { m_port = port; };

    virtual void set_client_id(std::string client_id) { m_client_id = client_id; };

};

#endif //CLOUD_STORAGE_SERVICE_CLIENT_H
