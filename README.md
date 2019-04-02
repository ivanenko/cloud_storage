# WFX plugin for popular cloud file storages.

This is analog for the Ghisler's "Cloud" plugin, designed to work with popular cloud storages, like Google Drive, Dropbox etc.
This plugin allows you:
* Create several accounts for service
* Navigate and manipulate your files on remote storage
* Upload/download files, export google docs to local file system
* Work with removed files in trash
* Use OAuth2 authorization
* Create and use your own authorization apps
* Use some service specific features in command line

For now you can work with the following services: Google Drive, Dropbox, Yandex Disk.

### Installation
You can get latest release and install it in "Options" window as WFX plugin.
Or you can buld  it from code
```
cmake --build cmake-build-release --target cloud_storage -- -j 2 -DCMAKE_BUILD_TYPE=Release
```
Use cloud_storage.wfx file in cmake-build-release folder

### Security
Account passwords are never used and stored. We use OAuth2 tokens instead. Tokens are not saved by default, and you need to authorize everytime you enter your storage.
You can change settings for your account and save token in config file (not secure), or in DC password manager.