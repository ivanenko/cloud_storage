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