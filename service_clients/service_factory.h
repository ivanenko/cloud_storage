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

#ifndef CLOUD_STORAGE_SERVICE_FACTORY_H
#define CLOUD_STORAGE_SERVICE_FACTORY_H

#include "dummy_client.h"
#include "yandex_rest_client.h"
#include "dropbox_client.h"
#include "googledrive_client.h"


class ServiceFactory final {
private:
    DummyClient* dummyClient;
    YandexRestClient* yandexClient;
    DropboxClient* dropboxClient;
    GoogleDriveClient* googledriveClient;

    std::vector<std::pair<std::string, std::string> > names = {
            {"yandex", "Yandex Disk"},
            {"dropbox", "Dropbox"},
            {"gdrive", "Google Drive"},
            {"dummy", "Dummy"}
    };

public:
    ServiceFactory() {
        dummyClient = NULL;
        yandexClient = NULL;
        dropboxClient = NULL;
        googledriveClient = NULL;
    }

    ~ServiceFactory(){
        if(dummyClient)
            delete dummyClient;

        if(yandexClient)
            delete yandexClient;

        if(dropboxClient)
            delete dropboxClient;

        if(googledriveClient)
            delete googledriveClient;
    }

    std::vector<std::pair<std::string, std::string> > get_names() { return names; }

    ServiceClient* getClient(std::string& client){
        if(client == "dummy"){
            if(!dummyClient)
                dummyClient = new DummyClient();

            return dummyClient;
        }

        if(client == "yandex"){
            if(!yandexClient)
                yandexClient = new YandexRestClient();

            return yandexClient;
        }

        if(client == "dropbox"){
            if(!dropboxClient)
                dropboxClient = new DropboxClient();

            return dropboxClient;
        }

        if(client == "gdrive"){
            if(!googledriveClient)
                googledriveClient = new GoogleDriveClient();

            return googledriveClient;
        }

        return NULL;
    }

};

#endif //CLOUD_STORAGE_SERVICE_FACTORY_H
