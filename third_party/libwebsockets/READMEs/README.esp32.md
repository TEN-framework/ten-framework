ESP32 Support
=============

See \ref esp32 for details on how to build lws as a component in an ESP-IDF project.

Lws provides a "factory" application

https://github.com/warmcat/lws-esp32-factory

and a test application which implements the generic lws server test apps

https://github.com/warmcat/lws-esp32-test-server-demos

The behaviours of the generic factory are are quite rich, and cover uploading SSL certs through factory and user configuration, AP selection and passphrase entry, and managing a switch to allow the user to force entry to user setup mode at boot subsequently.

The factory app comes with partitioning for a 1MB factory partition containing that app and data, and a single 2.9MB OTA partition containing the main app.

The factory app is able to do OTA updates for both the factory and OTA partition slots; updating the factory slot first writes the new image to the OTA slot and copies it into place at the next boot, after which the user can reload the OTA slot.

State|Image|AP SSID|Port|URL|Mode
---|---|---|---|---|---
Factory Reset or Uninitialized|Factory|AP: ESP_012345|80|http://192.168.4.1|factory.html - to set certificates and serial
User configuration|Factory|AP: config-model-serial|443|https://192.168.4.1|index.html - user set up his AP information
Operation|OTA|Station only|443|https://model-serial.local|OTA application

## Basic Auth

The lws-esp32-test-server-demos app also demos basic auth.

On a normal platform this is done by binding a mount to a text file somewhere in the filesystem, which
contains user:password information one per line.

On ESP32 there is not necessarily any generic VFS in use.  So instead, the basic auth lookup is bound to
a given nvs domain, where the username is the key and the password the value.  main/main.c in the test
demos app shows how to both make the mount use basic auth, and how to set a user:password combination
using nvs.

