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

#include <fstream>
#include <sstream>
#include <cstring>
#include "extension.h"
#include "dialogs.h"
#include "service_clients/service_factory.h"

const char* new_connection_dialog = R"(
object DialogBox: TDialogBox
  Left = 2322
  Height = 249
  Top = 169
  Width = 391
  Caption = 'Create New Connection'
  ClientHeight = 249
  ClientWidth = 391
  OnShow = DialogBoxShow
  Position = poScreenCenter
  BorderIcons = [biSystemMenu]
  BorderStyle = bsDialog
  LCLVersion = '2.0.0.4'
  object Label1: TLabel
    Left = 16
    Height = 17
    Top = 14
    Width = 80
    Caption = 'Select service:'
    ParentColor = False
  end
  object cmbService: TComboBox
    Left = 112
    Height = 27
    Top = 8
    Width = 264
    ItemHeight = 0
    TabOrder = 0
    ReadOnly = True
    Text = ''
  end
  object Label2: TLabel
    Left = 17
    Height = 17
    Top = 50
    Width = 65
    Caption = 'User name:'
    ParentColor = False
  end
  object edtUserName: TEdit
    Left = 16
    Height = 27
    Top = 72
    Width = 360
    TabOrder = 1
  end
  object saveMethodRadio: TRadioGroup
    Left = 16
    Height = 89
    Top = 112
    Width = 360
    AutoFill = True
    Caption = 'Save token method'
    ChildSizing.LeftRightSpacing = 6
    ChildSizing.EnlargeHorizontal = crsHomogenousChildResize
    ChildSizing.EnlargeVertical = crsHomogenousChildResize
    ChildSizing.ShrinkHorizontal = crsScaleChilds
    ChildSizing.ShrinkVertical = crsScaleChilds
    ChildSizing.Layout = cclLeftToRightThenTopToBottom
    ChildSizing.ControlsPerLine = 1
    ClientHeight = 70
    ClientWidth = 356
    Items.Strings = (
      'Do not save token'
      'Save token in config file'
      'Save token in Totalcmd password manager'
    )
    TabOrder = 2
  end
  object btnOK: TBitBtn
    Left = 216
    Height = 30
    Top = 208
    Width = 75
    Default = True
    DefaultCaption = True
    Kind = bkOK
    ModalResult = 1
    TabOrder = 3
    OnClick = ButtonClick
  end
  object btnCancel: TBitBtn
    Left = 301
    Height = 30
    Top = 208
    Width = 75
    Cancel = True
    DefaultCaption = True
    Kind = bkCancel
    ModalResult = 2
    TabOrder = 4
    OnClick = ButtonClick
  end
end
)";

const char* connection_props_dialog = R"(
object DialogBox: TDialogBox
  Left = 2287
  Height = 287
  Top = 280
  Width = 426
  AutoSize = True
  BorderIcons = [biSystemMenu]
  BorderStyle = bsDialog
  Caption = 'Connection settings'
  ChildSizing.LeftRightSpacing = 10
  ChildSizing.TopBottomSpacing = 10
  ClientHeight = 287
  ClientWidth = 426
  OnShow = DialogBoxShow
  Position = poScreenCenter
  LCLVersion = '2.0.0.4'

  object btnOK: TBitBtn
    Left = 256
    Height = 30
    Top = 248
    Width = 75
    Default = True
    DefaultCaption = True
    Kind = bkOK
    ModalResult = 1
    OnClick = ButtonClick
    TabOrder = 2
  end
  object btnCancel: TBitBtn
    Left = 341
    Height = 30
    Top = 248
    Width = 75
    Cancel = True
    DefaultCaption = True
    Kind = bkCancel
    ModalResult = 2
    OnClick = ButtonClick
    TabOrder = 3
  end

  object PageControl1: TPageControl
    Left = 8
    Height = 240
    Top = 0
    Width = 408
    ActivePage = TabSheet1
    TabIndex = 0
    TabOrder = 2
    object TabSheet1: TTabSheet
      Caption = 'Authorization'
      ClientHeight = 205
      ClientWidth = 402
      object getTokenRadio: TRadioGroup
        Left = 5
        Height = 72
        Top = 8
        Width = 393
        AutoFill = True
        Caption = 'OAuth token'
        ChildSizing.LeftRightSpacing = 6
        ChildSizing.EnlargeHorizontal = crsHomogenousChildResize
        ChildSizing.EnlargeVertical = crsHomogenousChildResize
        ChildSizing.ShrinkHorizontal = crsScaleChilds
        ChildSizing.ShrinkVertical = crsScaleChilds
        ChildSizing.Layout = cclLeftToRightThenTopToBottom
        ChildSizing.ControlsPerLine = 1
        ClientHeight = 53
        ClientWidth = 392
        Items.Strings = (
          'Get OAuth token from service'
          'Enter token manually'
        )
        TabOrder = 0
      end
      object saveMethodRadio: TRadioGroup
        Left = 5
        Height = 105
        Top = 88
        Width = 393
        AutoFill = True
        Caption = 'Save method'
        ChildSizing.LeftRightSpacing = 6
        ChildSizing.EnlargeHorizontal = crsHomogenousChildResize
        ChildSizing.EnlargeVertical = crsHomogenousChildResize
        ChildSizing.ShrinkHorizontal = crsScaleChilds
        ChildSizing.ShrinkVertical = crsScaleChilds
        ChildSizing.Layout = cclLeftToRightThenTopToBottom
        ChildSizing.ControlsPerLine = 1
        ClientHeight = 86
        ClientWidth = 392
        Items.Strings = (
          'Do not save token'
          'Save token in config file'
          'Save token in Totalcmd password manager'
        )
        TabOrder = 1
      end
    end
    object TabSheet2: TTabSheet
      Caption = 'OAuth2 settings'
      ClientHeight = 205
      ClientWidth = 402
      object Label1: TLabel
        Left = 8
        Height = 17
        Top = 8
        Width = 48
        Caption = 'Client ID'
        ParentColor = False
      end
      object edtClientID: TEdit
        Left = 8
        Height = 27
        Top = 30
        Width = 288
        TabOrder = 0
      end
      object Label2: TLabel
        Left = 8
        Height = 17
        Top = 70
        Width = 119
        Caption = 'Local http server port'
        ParentColor = False
      end
      object edtServerPort: TEdit
        Left = 8
        Height = 27
        Top = 92
        Width = 96
        NumbersOnly = True
        TabOrder = 1
        Text = '3359'
      end
      object Label3: TLabel
        Left = 8
        Height = 17
        Top = 136
        Width = 119
        Caption = 'Authentication timeout (sec)'
        ParentColor = False
      end
      object edtAuthTimeout: TEdit
        Left = 8
        Height = 27
        Top = 158
        Width = 96
        NumbersOnly = True
        TabOrder = 2
        Text = '20'
      end
    end
  end
end
)";


extern std::unique_ptr<tExtensionStartupInfo> gExtensionInfoPtr;
extern ServiceFactory gServiceFactory;

static nlohmann::json::object_t *gpJson, *gpJsonConnection;

//============================= New connection dialog
intptr_t DCPCALL DlgProcNew(uintptr_t pDlg, char *DlgItemName, intptr_t Msg, intptr_t wParam, intptr_t lParam)
{
    char *userName = NULL;
    switch (Msg){
        case DN_INITDIALOG:

            for(std::pair<std::string, std::string>& p: gServiceFactory.get_names())
                gExtensionInfoPtr->SendDlgMsg(pDlg, "cmbService", DM_LISTADD, (intptr_t)p.second.c_str(), (intptr_t)p.first.c_str());

            gExtensionInfoPtr->SendDlgMsg(pDlg, "saveMethodRadio", DM_LISTSETITEMINDEX, 0, 0);
            break;

        case DN_CHANGE:
            break;

        case DN_CLICK:
            if(strcmp(DlgItemName, "btnOK")==0){
                userName = (char*)gExtensionInfoPtr->SendDlgMsg(pDlg, "edtUserName", DM_GETTEXT, 1, 0);
                gpJson->emplace("user_name", userName);

                int res = (int)gExtensionInfoPtr->SendDlgMsg(pDlg, "cmbService", DM_LISTGETITEMINDEX, 0, 0);
                std::string service_name = gServiceFactory.get_names()[res].first;
                gpJson->emplace("service", service_name);

                res = gExtensionInfoPtr->SendDlgMsg(pDlg, "saveMethodRadio", DM_LISTGETITEMINDEX, 0, 0);
                switch(res){
                    case 1:
                        gpJson->emplace("save_type", "config");
                        break;
                    case 2:
                        gpJson->emplace("save_type", "password_manager");
                        gpJson->emplace("oauth_token", "");
                        break;
                    default:
                        gpJson->emplace("save_type", "dont_save");
                        gpJson->emplace("oauth_token", "");
                }

                gpJson->emplace("get_token_method", "oauth");

                gExtensionInfoPtr->SendDlgMsg(pDlg, DlgItemName, DM_CLOSE, 1, 0);
            } else if(strcmp(DlgItemName, "btnCancel")==0){
                gExtensionInfoPtr->SendDlgMsg(pDlg, DlgItemName, DM_CLOSE, 2, 0);
            }

            break;
    }
    return 0;
}

BOOL show_new_connection_dlg(nlohmann::json &json_obj)
{
    gpJson = json_obj.get_ptr<nlohmann::json::object_t*>();

    BOOL res = gExtensionInfoPtr->DialogBoxLFM((intptr_t)new_connection_dialog, strlen(new_connection_dialog), DlgProcNew);
    return res;
}

//============================== Connection properties dialog
intptr_t DCPCALL DlgProcProps(uintptr_t pDlg, char *DlgItemName, intptr_t Msg, intptr_t wParam, intptr_t lParam)
{
    char *clientId, *portNumber, *timeout;
    switch (Msg){
        case DN_INITDIALOG:
            if(gpJsonConnection->at("get_token_method").is_string()
                && gpJsonConnection->at("get_token_method").get<std::string>() == "manual")
                gExtensionInfoPtr->SendDlgMsg(pDlg, "getTokenRadio", DM_LISTSETITEMINDEX, 1, 0);
            else
                gExtensionInfoPtr->SendDlgMsg(pDlg, "getTokenRadio", DM_LISTSETITEMINDEX, 0, 0);

            if(gpJsonConnection->at("save_type").is_string()
                && gpJsonConnection->at("save_type").get<std::string>() == "config")
                gExtensionInfoPtr->SendDlgMsg(pDlg, "saveMethodRadio", DM_LISTSETITEMINDEX, 1, 0);
            else if(gpJsonConnection->at("save_type").is_string()
                    && gpJsonConnection->at("save_type").get<std::string>() == "password_manager")
                gExtensionInfoPtr->SendDlgMsg(pDlg, "saveMethodRadio", DM_LISTSETITEMINDEX, 2, 0);
            else
                gExtensionInfoPtr->SendDlgMsg(pDlg, "saveMethodRadio", DM_LISTSETITEMINDEX, 0, 0); // dont_save by default

            if(gpJsonConnection->at("client_id").is_string())
                gExtensionInfoPtr->SendDlgMsg(pDlg, "edtClientID", DM_SETTEXT, (intptr_t)gpJsonConnection->at("client_id").get<std::string>().c_str(), 0);

            if(gpJsonConnection->at("port").is_number()){
                std::string port = std::to_string(gpJsonConnection->at("port").get<int>());
                gExtensionInfoPtr->SendDlgMsg(pDlg, "edtServerPort", DM_SETTEXT, (intptr_t)port.c_str(), 0);
            }

            if(gpJsonConnection->at("auth_timeout").is_number()){
                std::string timeout = std::to_string(gpJsonConnection->at("auth_timeout").get<int>());
                gExtensionInfoPtr->SendDlgMsg(pDlg, "edtAuthTimeout", DM_SETTEXT, (intptr_t)timeout.c_str(), 0);
            }

            break;

        case DN_CLICK:
            if(strcmp(DlgItemName, "btnOK")==0){
                int res = gExtensionInfoPtr->SendDlgMsg(pDlg, "getTokenRadio", DM_LISTGETITEMINDEX, 0, 0);
                gpJsonConnection->at("get_token_method") = (res==0)? "oauth" : "manual";

                res = gExtensionInfoPtr->SendDlgMsg(pDlg, "saveMethodRadio", DM_LISTGETITEMINDEX, 0, 0);
                switch(res){
                    case 1:
                        gpJsonConnection->at("save_type") = "config";
                        break;
                    case 2:
                        gpJsonConnection->at("save_type") = "password_manager";
                        gpJsonConnection->at("oauth_token") = "";
                        break;
                    default:
                        gpJsonConnection->at("save_type") = "dont_save";
                        gpJsonConnection->at("oauth_token") = "";
                }

                clientId = (char*)gExtensionInfoPtr->SendDlgMsg(pDlg, "edtClientID", DM_GETTEXT, 1, 0);
                if(strlen(clientId) > 0)
                    gpJsonConnection->at("client_id") = clientId;
                else
                    gpJsonConnection->at("client_id") = nullptr;
                //gpJsonConnection->at("client_id") = (strlen(clientId) > 0 ? clientId: nullptr);

                portNumber = (char*)gExtensionInfoPtr->SendDlgMsg(pDlg, "edtServerPort", DM_GETTEXT, 1, 0);
                if(strlen(portNumber) > 0)
                    gpJsonConnection->at("port") = std::atoi(portNumber);
                else
                    gpJsonConnection->at("port") = nullptr;

                timeout = (char*)gExtensionInfoPtr->SendDlgMsg(pDlg, "edtAuthTimeout", DM_GETTEXT, 1, 0);
                if(strlen(timeout) > 0)
                    gpJsonConnection->at("auth_timeout") = std::atoi(timeout);
                else
                    gpJsonConnection->at("auth_timeout") = nullptr;

                gExtensionInfoPtr->SendDlgMsg(pDlg, DlgItemName, DM_CLOSE, 1, 0);
            } else if(strcmp(DlgItemName, "btnCancel")==0){
                gExtensionInfoPtr->SendDlgMsg(pDlg, DlgItemName, DM_CLOSE, 2, 0);
            }

            break;
    }
    return 0;
}

BOOL show_connection_properties_dlg(nlohmann::json::object_t* json_obj)
{
    gpJsonConnection = json_obj;

    BOOL res = gExtensionInfoPtr->DialogBoxLFM((intptr_t)connection_props_dialog, strlen(connection_props_dialog), DlgProcProps);
    return res;
}

