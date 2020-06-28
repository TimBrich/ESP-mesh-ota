# ESP-mesh-ota
esp connect to mesh network and can autoupdate from enternet.
* board - esp32
* ide - Platformio
* framework - esp-idf
* led module - ws2812B
## Tasks
1. Realise connection and work with mesh network;
2. Automating updates using a remote server;
## Description
* For first task i use esp-wifi-mesh.h because i whant have maximum control and use all feature. So all device start autoconfiguration mesh and after connect root device send all child special numbers for set led state.
* For autoupdate i use ota technology, espetialy esp-https-ota.h. Using [Server] i can ask about relevant firmware and download, install. 
