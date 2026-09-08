#pragma once

#include "wled.h"
#include "info_provider_birthdays.h"

class InfoProvider : public Usermod
{
private:
    static const char _name[];
    static const char _enabled[];

    bool enabled = true;
    String configs[8] = {
        "Test text", "", "", "", "", "", "", ""};
    scroll_info_data_t infoData{};
    uint32_t lastUpdate = 0;
    uint32_t testCounter = 0;
    String currentTemperature;
    String dailyHighTemperature;
    String weather;
    String nextCalendarEvent;
    String birthdayName;
    String birthdayFull;

    void renderConfigs();
    void updateBirthday();
    String renderTemplate(const String &source) const;
    static void copyToBuffer(char *destination, size_t capacity, const String &value);

public:
    void setup() override;
    void loop() override;
    void appendConfigData() override;
    bool getUMData(um_data_t **data) override;
    bool readFromConfig(JsonObject &root) override;
    void addToConfig(JsonObject &root) override;
    uint16_t getId() override { return USERMOD_ID_INFO_PROVIDER; }
};
