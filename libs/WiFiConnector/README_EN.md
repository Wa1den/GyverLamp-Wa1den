This is an automatic translation and may be incorrect in some places. See the source README and examples for authoritative information.

[![latest](https://img.shields.io/github/v/release/GyverLibs/WiFiConnector.svg?color=brightgreen)](https://github.com/GyverLibs/WiFiConnector/releases/latest/download/WiFiConnector.zip)
[![PIO](https://badges.registry.platformio.org/packages/gyverlibs/library/WiFiConnector.svg)](https://registry.platformio.org/libraries/gyverlibs/WiFiConnector)
[![Foo](https://img.shields.io/badge/Website-AlexGyver.ru-blue.svg?style=flat-square)](https://alexgyver.ru/)
[![Foo](https://img.shields.io/badge/%E2%82%BD%24%E2%82%AC%20%D0%9F%D0%BE%D0%B4%D0%B4%D0%B5%D1%80%D0%B6%D0%B0%D1%82%D1%8C-%D0%B0%D0%B2%D1%82%D0%BE%D1%80%D0%B0-orange.svg?style=flat-square)](https://alexgyver.ru/support_alex/)
[![Foo](https://img.shields.io/badge/README-ENGLISH-blueviolet.svg?style=flat-square)](https://github-com.translate.goog/GyverLibs/WiFiConnector?_x_tr_sl=ru&_x_tr_tl=en)  

[![Foo](https://img.shields.io/badge/ПОДПИСАТЬСЯ-НА%20ОБНОВЛЕНИЯ-brightgreen.svg?style=social&logo=telegram&color=blue)](https://t.me/GyverLibs)

# WiFiConnector
Asynchronous WiFi connection with access point creation during connection timeout

### Compatibility
ESP8266, ESP32

## Contents
- [Use of use](#usage)
- [Versions](#versions)
- [Installation](#install)
- [Bugs and feedback](#feedback)

<a id="usage"></a>

## Use of use
Use it when your WiFi password is stored somewhere. When transmitting an empty line, an access point will be launched. If you fail to connect, the access point for the web interface and other things will also unfold.

### Description of classes
```cpp
// AP name, AP password, timeout in seconds, disable AP when successfully connecting to STA (otherwise AP STA)
WiFiConnectorClass(const String& APname = "ESP AP",
                    const String& APpass = "",
                    uint16_t timeout = 60 * 3,
                    bool closeAP = true);

// name
void setName(const String& name);

// password
void setPass(const String& pass);

// set the connection timeout in seconds. 0 - turn off timeout
void setTimeout(uint16_t timeout);

// Automatically disable AP when connecting to STA (silent on)
void closeAP(bool close);

// connect the handler of a successful connection
void onConnect(ConnectorCallback cb);

// connect the connection error handler, will be called after the AP starts
void onError(ConnectorCallback cb);

// connect. If the ssid is not set, the AP will be launched.
bool connect(const String& ssid, const String& pass = emptyString);

// call in the loop. It will return true when the state changes.
bool tick();

// Connection status. True - connected, false - launched AR
bool connected();

// connection
bool connecting();
```

### Examples
```cpp
#include <Arduino.h>
#include <WiFiConnector.h>

void setup() {
    WiFiConnector.connect("SSID", "PASS");

    WiFiConnector.onConnect([]() {
        Serial.print("Connected. Local IP: ");
        Serial.println(WiFi.localIP());
    });
    WiFiConnector.onError([]() {
        Serial.print("WiFi error. AP IP: ");
        Serial.println(WiFi.softAPIP());
    });
}

void loop() {
    WiFiConnector.tick();
}
```

<a id="versions"></a>

## Versions
- v1.0

<a id="install"></a>
## Installation
- The library can be found under the name **WiFiConnector** and installed through the library manager in:
    - Arduino IDE
    - Arduino IDE v2
    - PlatformIO
- [Download the library](https://github.com/GyverLibs/WiFiConnector/archive/refs/heads/main.zip).zip archive for manual installation:
    - Unpack and put in *C:\Program Files (x86)\Arduino\libraries* (Windows x64)
    - Unpack and put in *C:\Program Files\Arduino\libraries* (Windows x32)
    - Unpack and put in *Documents/Arduino/libraries/ *
    - (Arduino IDE) Automatic installation from .zip: *Sketch/Connect library/Add .ZIP library...* and specify downloaded archive
- Read more detailed instructions for installing libraries[here](https://alexgyver.ru/arduino-first/#%D0%A3%D1%81%D1%82%D0%B0%D0%BD%D0%BE%D0%B2%D0%BA%D0%B0_%D0%B1%D0%B8%D0%B1%D0%BB%D0%B8%D0%BE%D1%82%D0%B5%D0%BA)
### Update
- I recommend always updating the library: new versions fix errors and bugs, as well as optimize and add new features.
- Through the library manager IDE: find the library as when installing and click "Update"
- Manually: **Delete the folder with the old version** and then put the new one in its place. “Replacement” can not be done: sometimes new versions delete files that will remain when replaced and can lead to errors!

<a id="feedback"></a>

## Bugs and feedback
If you find bugs, create **Issue**, or better write to the mail immediately.[alex@alexgyver.ru](mailto:alex@alexgyver.ru)  
The library is open for revision and your **Pull Requests*!

When reporting bugs or incorrect work of the library, it is necessary to specify:
- Library version
- What is used by the IC
- SDK version (for ESP)
- Arduino IDE version
- Are embedded examples that use features and designs that cause bugs in your code working correctly?
- What code was downloaded, what work was expected from it and how it works in reality
- Ideally, attach the minimum code in which the bug is observed. Not a canvas of a thousand lines, but a minimum code.
