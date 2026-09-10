#pragma once

#include "wled.h"
#include "info_provider_birthdays.h"

#include <HTTPClient.h>
#include <WiFiClient.h>

class InfoProvider : public Usermod
{
private:
    static constexpr uint8_t BirthdaySlots = 32;
    static const char _name[];
    static const char _enabled[];

    bool enabled = true;
    String location;
    String country = "DE";
    float latitude = 0.0f;
    float longitude = 0.0f;
    String openWeatherApiKey;
    uint16_t weatherUpdateMinutes = 30;
    String weatherDebug = "Not fetched";
    String configs[8] = {
        "Test text", "", "", "", "", "", "", ""};
    scroll_info_data_t infoData{};
    uint32_t lastUpdate = 0;
    uint32_t lastWeatherUpdate = 0;
    bool weatherFetchRequested = false;
    String currentTemperature;
    String dailyHighTemperature;
    String weather;
    String nextCalendarEvent;
    String birthdayName;
    String birthdayFull;
    String birthdayFullZero;
    String birthdays[BirthdaySlots];
    bool birthdayDefaultsLoaded = false;

    void renderConfigs();
    void loadBirthdayDefaults();
    void updateBirthday();
    bool updateLocation();
    bool updateWeather();
    void fetchWeatherNow();
    String renderWledTokens(const String &source) const;
    String renderTemplate(const String &source) const;
    static void copyToBuffer(char *destination, size_t capacity, const String &value);

public:
    void setup() override;
    void loop() override;
    void connected() override;
    void appendConfigData() override;
    bool getUMData(um_data_t **data) override;
    void readFromJsonState(JsonObject &root) override;
    bool readFromConfig(JsonObject &root) override;
    void addToConfig(JsonObject &root) override;
    uint16_t getId() override { return USERMOD_ID_INFO_PROVIDER; }
};
