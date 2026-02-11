@echo off
echo Building Node 5 (GH 1)...
echo [settings] > node_id.ini
echo gh_id = 1 >> node_id.ini
echo node_id = 5 >> node_id.ini
echo ota_ip = 192.168.19.86 >> node_id.ini
pio run -e wemosd1mini_usb
copy .pio\build\wemosd1mini_usb\firmware.bin firmware_node_5.bin

echo Building Node 7 (GH 2)...
echo [settings] > node_id.ini
echo gh_id = 2 >> node_id.ini
echo node_id = 7 >> node_id.ini
echo ota_ip = 192.168.19.86 >> node_id.ini
pio run -e wemosd1mini_usb
copy .pio\build\wemosd1mini_usb\firmware.bin firmware_node_7.bin

echo Building Node 9 (GH 2)...
echo [settings] > node_id.ini
echo gh_id = 2 >> node_id.ini
echo node_id = 9 >> node_id.ini
echo ota_ip = 192.168.19.86 >> node_id.ini
pio run -e wemosd1mini_usb
copy .pio\build\wemosd1mini_usb\firmware.bin firmware_node_9.bin

echo All builds complete!
