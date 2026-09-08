#pragma once

#include "wled.h"

class InfoProvider : public Usermod
{
private:
    static const char _name[];
    static const char _enabled[];

    bool enabled = true;
    String config01 = "Test text";
    scroll_info_data_t infoData{};

    void renderConfig01();
    static void copyToBuffer(char *destination, size_t capacity, const String &value);

public:
    void setup() override;
    void loop() override;
    bool getUMData(um_data_t **data) override;
    bool readFromConfig(JsonObject &root) override;
    void addToConfig(JsonObject &root) override;
    uint16_t getId() override { return USERMOD_ID_INFO_PROVIDER; }
};
