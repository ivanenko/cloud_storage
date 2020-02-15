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

#include "yandex_rest_client.h"
#include "../plugin_utils.h"
#include <future>
#include <string>
#include "service_client.h"

void _listen_server(httplib::Server* svr, std::promise<std::string> *pr, int port){

    svr->Get("/get_token", [pr](const httplib::Request& req, httplib::Response& res) {
        std::string html = R"(
            <html><body>
                <h2 id="message">Success</h2>
                <script type="text/javascript">
                    var hash = window.location.hash.substr(1);
                    var i = hash.search("access_token=");
                    if(i<0){
                        document.getElementById("message").textContent = "Error! Cannot get auth_token";
                    } else {
                        var token = hash.substr(i)
                                        .split("&")[0]
                                        .split("=")[1];

                        if(token){
                            var xhttp = new XMLHttpRequest();
                            xhttp.onreadystatechange = function() {
                                if (this.readyState == 4 && this.status == 200) {
                                 document.getElementById("message").textContent = "Success! You can close browser and use DC plugin";
                                }
                             };
                            xhttp.open("GET", "/receive_token?access_token="+token, true);
                            xhttp.send();
                        }
                    }
                </script>
            </body></html>
        )";

        res.set_content(html, "text/html");
    });

    svr->Get("/receive_token", [pr](const httplib::Request& req, httplib::Response& res) {
        std::string token = req.get_param_value("access_token");
        res.set_content("OAuth token received", "text/plain");
        pr->set_value(token);
    });

    //int port = svr->bind_to_any_port("localhost");
    //std::cout << "Starting server at port: " << port << std::endl;
    //svr->listen_after_bind();
    svr->listen("localhost", port); // 3359
}

int ServiceClient::_get_port()
{
    return m_port;
}

int ServiceClient::_get_auth_timeout()
{
    return m_auth_timeout;
}

std::string ServiceClient::get_oauth_token()
{
    httplib::Server server;
    std::string token;

    std::promise<std::string> promiseToken;
    std::future<std::string> ftr = promiseToken.get_future();

    //create and run server on separate thread
    std::thread server_thread(_listen_server, &server, &promiseToken, _get_port());

    //TODO add win32 browser open
    std::string url("xdg-open ");
    url += get_auth_page_url();
    system(url.c_str());

    std::string error_message;
    std::future_status ftr_status = ftr.wait_for(std::chrono::seconds(_get_auth_timeout()));
    if(ftr_status == std::future_status::ready) {
        token = ftr.get();
    } else if (ftr_status == std::future_status::timeout){
        error_message = "Timeout";
    } else {
        error_message = "Deffered future";
    }

    server.stop();
    server_thread.join();

    if(!error_message.empty())
        throw std::runtime_error(error_message);

    return token;
}

std::__cxx11::string ServiceClient::url_encode(const std::__cxx11::string& s)
{
    std::__cxx11::string result;

    for (auto i = 0; s[i]; i++) {
        switch (s[i]) {
            case ' ':  result += "%20"; break;
            case '+':  result += "%2B"; break;
            case '/':  result += "%2F"; break;
            case '\'': result += "%27"; break;
            case ',':  result += "%2C"; break;
            case ':':  result += "%3A"; break;
            case ';':  result += "%3B"; break;
            case '#':  result += "%23"; break;
            default:
                auto c = static_cast<uint8_t>(s[i]);
                if (c >= 0x80) {
                    result += '%';
                    char hex[4];
                    size_t len = snprintf(hex, sizeof(hex) - 1, "%02X", c);
                    assert(len == 2);
                    result.append(hex, len);
                } else {
                    result += s[i];
                }
                break;
        }
    }

    return result;
}
