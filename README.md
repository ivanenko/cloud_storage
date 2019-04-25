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
You can get [latest release](https://github.com/ivanenko/cloud_storage/releases) and install it in "Options" window as WFX plugin.
Or you can buld  it from code
```
sudo apt-get install g++ libssl-dev
git clone https://github.com/ivanenko/cloud_storage.git
cd cloud_storage
mkdir cmake-build-release
cmake -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Release . -Bcmake-build-release
cmake --build cmake-build-release --target cloud_storage -- -j 2
```
Use cloud_storage.wfx file in cmake-build-release folder

### Security
Account passwords are never used and stored. We use OAuth2 tokens instead. Tokens are not saved by default, and you need to authorize everytime you enter your storage.
You can change settings for your account and save token in config file (not secure), or in DC password manager.

You can also create your own authorization apps, get its client_id and use it for granting access to your files.
Read more about creating app for dropbox [here](https://www.dropbox.com/developers), or on [this wiki page](https://github.com/ivanenko/cloud_storage/wiki/Create-and-use-your-own-authorization-app) for all used services.
Save your applications client_id in 'Connection settings' window, 'OAuth setting' tab.

### Service specific features
Every service provide specific and unique commands. It could be download files from internet directly to your storage, or export google document in different formats.
You can read more about this on [wiki page](https://github.com/ivanenko/cloud_storage/wiki/Service-specific-features)

### Contribute
If you want to add new service - [check this wiki page](https://github.com/ivanenko/cloud_storage/wiki)
