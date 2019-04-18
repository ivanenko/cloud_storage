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
#include "../common.h"
#include "dummy_client.h"
#include "../library.h"
#include "../wfxplugin.h"
#include "../plugin_utils.h"

//DummyClient::DummyClient(){
//
//}
//
//DummyClient::~DummyClient(){
//
//}

pResources DummyClient::get_resources(std::string path, BOOL isTrash) {
    json files;
    if(path == "/")
        files = json::array({
                         json::object({{"name", "dummy1"}}),
                         json::object({{"name", "dummy2"}}),
                         json::object({{"name", "dummy3"}}),
                      });

    pResources res = prepare_folder_result(files);
    return res;
}

pResources DummyClient::prepare_folder_result(json& result)
{
    pResources pRes = new tResources;
    pRes->nCount = 0;

    if(result.is_array() && result.size()>0){
        pRes->resource_array.resize(result.size());

        int i=0;
        for(auto& o: result){
            wcharstring wName = UTF8toUTF16(o["name"].get<std::string>());
            size_t str_size = (MAX_PATH > wName.size()+1)? (wName.size()+1): MAX_PATH;
            memcpy(pRes->resource_array[i].cFileName, wName.data(), sizeof(WCHAR) * str_size);
            pRes->resource_array[i].dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
            pRes->resource_array[i].nFileSizeLow = 200;
            pRes->resource_array[i].nFileSizeHigh = 0;
            pRes->resource_array[i].ftCreationTime = get_now_time();
            pRes->resource_array[i].ftLastWriteTime = get_now_time();
            pRes->resource_array[i].ftLastAccessTime = get_now_time();
            i++;
        }
    }

    return pRes;
}



