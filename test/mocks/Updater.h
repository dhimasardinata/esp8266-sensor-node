#pragma once
class UpdateClass {
public:
    bool begin(size_t) { return true; }
    size_t write(uint8_t*, size_t) { return 0; }
    bool end() { return true; }
};
extern UpdateClass Update;
