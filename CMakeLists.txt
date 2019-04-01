cmake_minimum_required(VERSION 3.13)
project(cloud_storage)

set(CMAKE_CXX_STANDARD 14)

list(APPEND CLOUD_SERVICES
        service_clients/service_client.h
        service_clients/service_client.cpp
        service_clients/service_factory.h
        service_clients/dummy_client.h
        service_clients/dummy_client.cpp
        service_clients/yandex_rest_client.h
        service_clients/yandex_rest_client.cpp
        service_clients/dropbox_client.h
        service_clients/dropbox_client.cpp
        service_clients/googledrive_client.h
        service_clients/googledrive_client.cpp
        # add here your clients
)

#add_library(cloud_storage SHARED library.cpp library.h httplib.h json.hpp plugin_utils.h dialogs.cpp dialogs.h service_client.h service_client.cpp service_clients/dummy_client.h service_clients/dummy_client.cpp)
add_library(cloud_storage SHARED library.cpp library.h service_clients/httplib.h json.hpp plugin_utils.h plugin_utils.cpp dialogs.cpp dialogs.h ${CLOUD_SERVICES})
target_link_libraries(cloud_storage pthread ssl crypto)
set_target_properties(cloud_storage PROPERTIES PREFIX "" SUFFIX ".wfx")
