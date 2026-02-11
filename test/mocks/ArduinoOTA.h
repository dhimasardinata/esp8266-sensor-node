#pragma once

class ArduinoOTAClass {
public:
    void setHostname(const char*) {}
    void begin() {}
    void end() {}
    void handle() {}
};
extern ArduinoOTAClass ArduinoOTA;
