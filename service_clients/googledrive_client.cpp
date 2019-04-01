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

#include "googledrive_client.h"
#include "../plugin_utils.h"

const char* exportDlg = R"(
object dialogBox: TDialogBox
  Left = 2200
  Height = 115
  Top = 186
  Width = 318
  Caption = 'Export'
  OnShow = DialogBoxShow
  Position = poScreenCenter
  BorderIcons = [biSystemMenu]
  BorderStyle = bsDialog
  ClientHeight = 115
  ClientWidth = 318
  LCLVersion = '2.0.0.4'
  object cmbOptions: TComboBox
    Left = 8
    Height = 27
    Top = 32
    Width = 296
    ItemHeight = 0
    TabOrder = 0
    Text = ''
  end
  object btnOK: TBitBtn
    Left = 144
    Height = 30
    Top = 72
    Width = 75
    Default = True
    DefaultCaption = True
    Kind = bkOK
    ModalResult = 1
    TabOrder = 1
    OnClick = ButtonClick
  end
  object btnCancel: TBitBtn
    Left = 229
    Height = 30
    Top = 72
    Width = 75
    Cancel = True
    DefaultCaption = True
    Kind = bkCancel
    ModalResult = 2
    TabOrder = 2
    OnClick = ButtonClick
  end
  object lblHead: TLabel
    Left = 8
    Height = 17
    Top = 8
    Width = 157
    Caption = 'Export Google Document to:'
    ParentColor = False
  end
end
)";

struct mimeType {
    std::string mType;
    std::string name;
    std::string extension;

    mimeType(std::string mType, std::string name, std::string ext): mType(mType), name(name), extension(ext) {}
};

std::map<std::string, std::vector<mimeType> > export_types {
        {"application/vnd.google-apps.drawing", {
             mimeType("image/jpeg", "JPEG", "jpeg"), mimeType("image/png", "PNG", "png"),
             mimeType("image/svg+xml", "SVG", "svg"), mimeType("application/pdf", "PDF", "pdf")
           }
        },
        {"application/vnd.google-apps.document", {
             mimeType("text/html", "HTML", "html"), mimeType("application/zip", "HTML (zipped)", "zip"),
             mimeType("text/plain", "Plain text", "txt"), mimeType("application/rtf", "Rich text", "rtf"),
             mimeType("application/vnd.oasis.opendocument.text", "Open Office doc", "odt"), mimeType("application/pdf", "PDF", "pdf"),
             mimeType("application/vnd.openxmlformats-officedocument.wordprocessingml.document", "MS Word document", "doc"),
             mimeType("application/epub+zip", "EPUB", "epub")
           }
        },
        {"application/vnd.google-apps.spreadsheet", {
             mimeType("application/vnd.openxmlformats-officedocument.spreadsheetml.sheet", "MS Excel", "xlsx"),
             mimeType("application/x-vnd.oasis.opendocument.spreadsheet", "Open Office sheet", "ods"),
             mimeType("application/pdf", "PDF", "pdf"), mimeType("text/csv", "CSV (first sheet only)", "csv"),
             mimeType("text/tab-separated-values", "TSV (first sheet only)", "tsv"),
             mimeType("application/zip", "HTML (zipped)", "zip")
          }
        },
        {"application/vnd.google-apps.presentation", {
             mimeType("application/vnd.openxmlformats-officedocument.presentationml.presentation", "MS PowerPoint", "ppt"),
             mimeType("application/vnd.oasis.opendocument.presentation", "Open Office presentation", "odp"),
             mimeType("application/pdf", "PDF", "pdf"),
             mimeType("text/plain", "Plain text", "txt")
          }
        }
};

std::string g_path;

extern std::unique_ptr<tExtensionStartupInfo> gExtensionInfoPtr;

GoogleDriveClient::GoogleDriveClient()
{
    m_http_client = new httplib::SSLClient("www.googleapis.com");
    m_resourceNamesMap["/"] = "root";
    m_client_id = "1019190623375-06j9q3kgqnborccd85fudtf0f7rk7138.apps.googleusercontent.com";
}

GoogleDriveClient::~GoogleDriveClient()
{
    delete m_http_client;
}

std::string GoogleDriveClient::get_auth_page_url()
{
    std::string url("https://accounts.google.com/o/oauth2/v2/auth?response_type=token\\&redirect_uri=http://localhost:3359/get_token\\&scope=https://www.googleapis.com/auth/drive\\&client_id=");
    url += _get_client_id();
    return url;
}

void GoogleDriveClient::throw_response_error(httplib::Response* resp){
    if(resp){
        std::string message;
        try{
            json js = json::parse(resp->body);
            if(js["error"].is_object())
                message = js["error"]["message"].get<std::string>();
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

void GoogleDriveClient::set_oauth_token(const char *token)
{
    assert(token != NULL);

    std::string header_token = "Bearer ";
    header_token += token;
    m_headers = { {"Authorization", header_token}, {"Accept", "application/json"} };
}

pResources GoogleDriveClient::get_resources(std::string path, BOOL isTrash)
{
    std::string folderId("root"), folderName;
    if (m_resourceNamesMap.find(path) != m_resourceNamesMap.end())
        folderId = m_resourceNamesMap[path];

    std::string url = "/drive/v3/files?q=trashed=";
    url += (isTrash? "true": "false");
    url += " and '";
    url += folderId;
    url += "' in parents&fields=files(id,name,size,createdTime,modifiedTime,parents,mimeType)";

    auto r = m_http_client->Get(url.c_str(), m_headers);

    if(r.get() && r->status==200){
        const auto js = json::parse(r->body);

        return prepare_folder_result(js, path);
    } else {
        throw_response_error(r.get());
    }
}

pResources GoogleDriveClient::prepare_folder_result(json js, std::string &path)
{
    if(!js["files"].is_array())
        throw std::runtime_error("Wrong Json format");

    BOOL isRoot = (path == "/");
    int total = js["files"].size();

    pResources pRes = new tResources;
    pRes->nSize = isRoot ? total+1: total;
    pRes->nCount = 0;
    pRes->resource_array = new WIN32_FIND_DATAW[pRes->nSize];

    int i=0;
    for(auto& item: js["files"]){
        wcharstring wName = UTF8toUTF16(item["name"].get<std::string>());

        size_t str_size = (MAX_PATH > wName.size()+1)? (wName.size()+1): MAX_PATH;
        memcpy(pRes->resource_array[i].cFileName, wName.data(), sizeof(WCHAR) * str_size);

        if(item["mimeType"].get<std::string>() == "application/vnd.google-apps.folder") {
            pRes->resource_array[i].dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
            pRes->resource_array[i].nFileSizeLow = 0;
            pRes->resource_array[i].nFileSizeHigh = 0;
            pRes->resource_array[i].ftCreationTime = get_now_time();
            pRes->resource_array[i].ftLastWriteTime = get_now_time();
        } else {
            pRes->resource_array[i].dwFileAttributes = 0;
            if(item["size"].is_string())
                pRes->resource_array[i].nFileSizeLow = (DWORD) std::stoi(item["size"].get<std::string>());
            else
                pRes->resource_array[i].nFileSizeLow = (DWORD) 0;
            pRes->resource_array[i].nFileSizeHigh = 0;
            pRes->resource_array[i].ftCreationTime = parse_iso_time(item["createdTime"].get<std::string>());
            pRes->resource_array[i].ftLastWriteTime = parse_iso_time(item["modifiedTime"].get<std::string>());
        }

        std::string p = path;
        if(path != "/")
            p += "/";
        p += item["name"].get<std::string>();
        m_resourceNamesMap[p] = item["id"].get<std::string>();
        m_mimetypesMap[p] = item["mimeType"].get<std::string>();

        i++;
    }

    if(isRoot){
        memcpy(pRes->resource_array[total].cFileName, u".Trash", sizeof(WCHAR) * 7);
        pRes->resource_array[total].dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        pRes->resource_array[total].nFileSizeLow = 0;
        pRes->resource_array[total].nFileSizeHigh = 0;
        pRes->resource_array[total].ftCreationTime = get_now_time();
        pRes->resource_array[total].ftLastWriteTime = get_now_time();
    }

    return pRes;
}

void GoogleDriveClient::makeFolder(std::string utf8Path)
{
    std::string folderName, parentFolderId("root");
    int p = utf8Path.find_last_of('/');
    folderName = utf8Path.substr(p + 1);
    if(p>0){
        std::string parentFolderPath = utf8Path.substr(0, p);
        if(m_resourceNamesMap.find(parentFolderPath) != m_resourceNamesMap.end())
            parentFolderId = m_resourceNamesMap[parentFolderPath];
    }

    json jsBody = {
            {"name", folderName},
            {"mimeType", "application/vnd.google-apps.folder"},
            {"parents", {parentFolderId}}
    };

    httplib::Headers hd = m_headers;
    hd.emplace("Content-Type", "application/json");

    auto r = m_http_client->Post("/drive/v3/files", m_headers, jsBody.dump(), "application/json");

    if(r.get() && r->status==200){
        json js = json::parse(r->body);
        m_resourceNamesMap[utf8Path] = js["id"].get<std::string>();
    } else {
        throw_response_error(r.get());
    }
}

void GoogleDriveClient::removeResource(std::string utf8Path)
{
    if(m_resourceNamesMap.find(utf8Path) == m_resourceNamesMap.end())
        throw std::runtime_error("resource ID not found");

    std::string url("/drive/v3/files/");
    url += m_resourceNamesMap[utf8Path];
    auto r = m_http_client->Delete(url.c_str(), m_headers);

    if(!r.get() || r->status!=204){
        throw_response_error(r.get());
    }
}

intptr_t DlgProcExport(uintptr_t pDlg, char *DlgItemName, intptr_t Msg, intptr_t wParam, intptr_t lParam)
{
    std::map<std::string, std::vector<mimeType> >::iterator it;
    switch (Msg){

        case DN_INITDIALOG:
            it = export_types.find(g_path);

            for(mimeType& mt: it->second)
                gExtensionInfoPtr->SendDlgMsg(pDlg, "cmbOptions", DM_LISTADD, (intptr_t)mt.name.c_str(), (intptr_t)mt.mType.c_str());

            break;

        case DN_CLICK:
            if(strcmp(DlgItemName, "btnOK")==0){
                int current = (int)gExtensionInfoPtr->SendDlgMsg(pDlg, "cmbOptions", DM_LISTGETITEMINDEX, 0, 0);
                char *mime = (char*)gExtensionInfoPtr->SendDlgMsg(pDlg, "cmbOptions", DM_LISTGETDATA, current, 0);
                g_path = mime;

                gExtensionInfoPtr->SendDlgMsg(pDlg, DlgItemName, DM_CLOSE, 1, 0);
            } else if(strcmp(DlgItemName, "btnCancel")==0){
                gExtensionInfoPtr->SendDlgMsg(pDlg, DlgItemName, DM_CLOSE, 2, 0);
            }

            break;
    }

    return 0;
}

void GoogleDriveClient::downloadFile(std::string path, std::ofstream &ofstream)
{
    if(m_resourceNamesMap.find(path) == m_resourceNamesMap.end())
        throw std::runtime_error("resource ID not found");

    bool exportedType = (export_types.find(m_mimetypesMap[path]) != export_types.end() );

    std::string url("/drive/v3/files/");
    url += m_resourceNamesMap[path];

    if(exportedType){
        url += "/export";
        if(gExtensionInfoPtr){
            g_path = m_mimetypesMap[path];
            BOOL res = gExtensionInfoPtr->DialogBoxLFM((intptr_t)exportDlg, strlen(exportDlg), DlgProcExport);
            if(!res)
                throw std::runtime_error("Cancel export");

            url += "?mimeType=" + g_path;
        }
    } else {
        url += "?alt=media";
    }

    auto r = m_http_client->Get(url.c_str(), m_headers);

    if(r.get() && r->status == 200){
        ofstream.write(r->body.data(), r->body.size());
    } else {
        throw_response_error(r.get());
    }
}

void GoogleDriveClient::uploadFile(std::string path, std::ifstream &ifstream, BOOL overwrite)
{
    std::string resourceName, parentFolderId("root");
    int p = path.find_last_of('/');
    if(p>0){
        std::string parentFolderPath = path.substr(0, p);
        if(m_resourceNamesMap.find(parentFolderPath) != m_resourceNamesMap.end())
            parentFolderId = m_resourceNamesMap[parentFolderPath];
    }

    json jsParams = {
            {"name", resourceName},
            {"parents", {parentFolderId}},
            //TODO add create_datetime
    };

    auto r = m_http_client->Post("/upload/drive/v3/files?uploadType=resumable", m_headers, jsParams.dump(), "application/json");

    if(!r.get() || r->status != 200)
        throw_response_error(r.get());

    if(!r->has_header("Location"))
        throw std::runtime_error("Cannot find Location header");

    std::string downloadUrl = r->get_header_value("Location").substr(26); // remove 'https://www.googleapis.com'

    std::stringstream body;
    body << ifstream.rdbuf();

    r = m_http_client->Put(downloadUrl.c_str(), m_headers, body.str(), "application/octet-stream");

    if(r.get() && (r->status==200 || r->status==201)){
        return;
    } else {
        // TODO resume interrupted upload
        // https://developers.google.com/drive/api/v3/resumable-upload
        throw_response_error(r.get());
    }
}

void GoogleDriveClient::move(std::string from, std::string to, BOOL overwrite)
{
    std::string fileId, oldParentId("root"), newParentId("root");
    int p = from.find_last_of('/');
    if(m_resourceNamesMap.find(from) == m_resourceNamesMap.end())
        throw std::runtime_error("Cannot find file ID");

    fileId = m_resourceNamesMap[from];

    if(p>0){
        std::string oldParentPath = from.substr(0, p);
        if(m_resourceNamesMap.find(oldParentPath) != m_resourceNamesMap.end())
            oldParentId = m_resourceNamesMap[oldParentPath];
    }

    p = to.find_last_of('/');
    if(p>0){
        std::string newParentPath = to.substr(0, p);
        if(m_resourceNamesMap.find(newParentPath) != m_resourceNamesMap.end())
            newParentId = m_resourceNamesMap[newParentPath];
    }

    std::string url("/drive/v3/files/");
    url += fileId;
    url += "?removeParents=";
    url += oldParentId;
    url += "&addParents=";
    url += newParentId;

    auto r = m_http_client->Patch(url.c_str(), m_headers, "", "application/json");

    if(!r.get() || r->status!=200)
        throw_response_error(r.get());
}

void GoogleDriveClient::copy(std::string from, std::string to, BOOL overwrite)
{
    std::string fileId, toFileName, toFolderId("root");
    if(m_resourceNamesMap.find(from) == m_resourceNamesMap.end())
        throw std::runtime_error("Cannot find file ID");

    fileId = m_resourceNamesMap[from];

    int p = to.find_last_of('/');
    toFileName = to.substr(p + 1);
    if(p>0){
        std::string newFolderPath = to.substr(0, p);
        if(m_resourceNamesMap.find(newFolderPath) != m_resourceNamesMap.end())
            toFolderId = m_resourceNamesMap[newFolderPath];
    }

    json jsBody = {
            {"name", toFileName},
            {"parents", {toFolderId}},
    };

    std::string url("/drive/v3/files/");
    url += fileId;
    url += "/copy";

    auto r = m_http_client->Post(url.c_str(), m_headers, jsBody.dump(), "application/json");

    if(!r.get() || r->status!=200)
        throw_response_error(r.get());
}

void GoogleDriveClient::cleanTrash()
{
    auto r = m_http_client->Delete("/drive/v3/files/trash", m_headers);

    if(!r.get() || r->status<=200 || r->status>=300)
        throw_response_error(r.get());
}