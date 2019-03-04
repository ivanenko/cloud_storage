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

#ifndef YDISK_COMMANDER_GTK_DIALOGS_H
#define YDISK_COMMANDER_GTK_DIALOGS_H

#include <string>
#include "json.hpp"

BOOL show_new_connection_dlg(nlohmann::json &json_obj);
BOOL show_connection_properties_dlg(nlohmann::json::object_t *json_obj);

#endif //YDISK_COMMANDER_GTK_DIALOGS_H
