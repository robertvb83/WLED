#include "info_provider.h"

const char InfoProvider::_name[] PROGMEM = "InfoProvider";
const char InfoProvider::_enabled[] PROGMEM = "Enable";

static InfoProvider infoProvider;
REGISTER_USERMOD(infoProvider);

void InfoProvider::setup()
{
    loadBirthdayDefaults();
    um_data = new um_data_t();
    if (!um_data)
        return;

    um_data->u_size = 1;
    um_data->u_type = new um_types_t[1];
    um_data->u_data = new void *[1];
    if (!um_data->u_type || !um_data->u_data)
    {
        delete um_data;
        um_data = nullptr;
        return;
    }
    um_data->u_type[0] = UMT_BYTE_ARR;
    um_data->u_data[0] = &infoData;
    updateBirthday();
    renderConfigs();
}

void InfoProvider::loop()
{
    if (!enabled || millis() - lastUpdate < 1000)
        return;

    lastUpdate = millis();
    if (weatherFetchRequested || (openWeatherApiKey.length() > 0 && location.length() > 0 && (lastWeatherUpdate == 0 || millis() - lastWeatherUpdate >= (uint32_t)weatherUpdateMinutes * 60000U)))
    {
        weatherFetchRequested = false;
        if (latitude == 0.0f && longitude == 0.0f)
            updateLocation();
        if (latitude != 0.0f || longitude != 0.0f)
            updateWeather();
        lastWeatherUpdate = millis();
    }
    updateBirthday();
    renderConfigs();
}

void InfoProvider::connected()
{
    lastWeatherUpdate = 0;
    weatherFetchRequested = true;
}

void InfoProvider::appendConfigData()
{
    // InfoProvider-specific labels are rendered directly by settings_um.htm.
}

uint32_t InfoProvider::getWeatherColor() const
{
    if (weather.indexOf(F("thunder")) >= 0)
        return RGBW32(255, 0, 0, 0);
    if (weather.indexOf(F("snow")) >= 0)
        return RGBW32(255, 255, 255, 0);
    if (weather.indexOf(F("storm")) >= 0 || weather.indexOf(F("squall")) >= 0 || weather.indexOf(F("tornado")) >= 0)
        return RGBW32(255, 255, 0, 0);
    if (weather.indexOf(F("rain")) >= 0 || weather.indexOf(F("drizzle")) >= 0)
        return weather.indexOf(F("light")) >= 0 ? RGBW32(80, 190, 255, 0) : RGBW32(0, 80, 255, 0);
    if (weather.indexOf(F("clear")) >= 0)
        return RGBW32(0, 255, 0, 0);
    return RGBW32(128, 128, 128, 0);
}

bool InfoProvider::updateLocation()
{
    if (!WLED_CONNECTED || location.length() == 0 || country.length() == 0 || openWeatherApiKey.length() == 0)
        return false;

    String url = F("http://api.openweathermap.org/geo/1.0/direct?q=");
    url += location;
    url += ',';
    url += country;
    url += F("&limit=1&appid=");
    url += openWeatherApiKey;

    WiFiClient client;
    HTTPClient http;
    if (!http.begin(client, url))
    {
        weatherDebug = F("Geocoding HTTP begin failed");
        return false;
    }
    http.setTimeout(5000);
    const int status = http.GET();
    bool success = false;
    String response;
    if (status == HTTP_CODE_OK)
    {
        response = http.getString();
        DynamicJsonDocument document(1024);
        DeserializationError error = deserializeJson(document, response);
        if (!error && document.is<JsonArray>() && document[0])
        {
            latitude = document[0]["lat"] | 0.0f;
            longitude = document[0]["lon"] | 0.0f;
            success = latitude != 0.0f || longitude != 0.0f;
        }
    }
    http.end();
    weatherDebug = success ? F("Geocoded: ") : F("Geocoding HTTP ");
    if (!success)
    {
        weatherDebug += status;
        if (response.length() > 0 && response.length() < 160)
        {
            weatherDebug += ' ';
            weatherDebug += response;
        }
    }
    if (success)
    {
        weatherDebug += String(latitude, 5);
        weatherDebug += ',';
        weatherDebug += String(longitude, 5);
    }
    return success;
}

bool InfoProvider::updateWeather()
{
    if (!WLED_CONNECTED || (latitude == 0.0f && longitude == 0.0f) || openWeatherApiKey.length() == 0)
        return false;

    WiFiClient client;
    HTTPClient http;
    String locationQuery = location;
    locationQuery += ',';
    locationQuery += country;
    String baseUrl = F("http://api.openweathermap.org/data/2.5/");
    String currentUrl = baseUrl + F("weather?q=") + locationQuery + F("&units=metric&appid=") + openWeatherApiKey;
    String forecastUrl = baseUrl + F("forecast?q=") + locationQuery + F("&units=metric&appid=") + openWeatherApiKey;

    String currentResponse;
    String forecastResponse;
    int currentStatus = -1;
    int forecastStatus = -1;
    if (http.begin(client, currentUrl))
    {
        http.setTimeout(5000);
        currentStatus = http.GET();
        if (currentStatus == HTTP_CODE_OK)
            currentResponse = http.getString();
        http.end();
    }
    if (http.begin(client, forecastUrl))
    {
        http.setTimeout(5000);
        forecastStatus = http.GET();
        if (forecastStatus == HTTP_CODE_OK)
            forecastResponse = http.getString();
        http.end();
    }

    bool success = false;
    bool currentParsed = false;
    bool forecastParsed = false;
    String currentErrorText;
    String forecastErrorText;
    DynamicJsonDocument currentDocument(4096);
    DynamicJsonDocument forecastDocument(32768);

    if (currentStatus == HTTP_CODE_OK && currentResponse.length() <= 8192)
    {
        DeserializationError error = deserializeJson(currentDocument, currentResponse);
        if (!error)
            currentParsed = true;
        else
            currentErrorText = error.c_str();
    }
    else
        currentErrorText = F("HTTP status or response too large");

    if (forecastStatus == HTTP_CODE_OK && forecastResponse.length() <= 32768)
    {
        DeserializationError error = deserializeJson(forecastDocument, forecastResponse);
        if (!error)
            forecastParsed = true;
        else
            forecastErrorText = error.c_str();
    }
    else
        forecastErrorText = F("HTTP status or response too large");

    bool currentTemperatureValid = false;
    float currentTemperatureValue = 0.0f;
    if (currentParsed)
    {
        JsonObject current = currentDocument.as<JsonObject>();
        if (!current["main"]["temp"].isNull())
        {
            currentTemperatureValue = current["main"]["temp"].as<float>();
            currentTemperature = roundTemperature ? String(roundf(currentTemperatureValue), 0) : String(currentTemperatureValue, 1);
            currentTemperatureValid = true;
        }
        const char *description = current["weather"][0]["description"] | "";
        if (description[0] != '\0')
            weather = description;
        dailyHighTemperature = currentTemperatureValid ? currentTemperature : "";
        if (forecastParsed)
        {
            const int64_t timezoneOffset = current["timezone"] | 0;
            const int64_t currentDay = ((current["dt"] | 0) + timezoneOffset) / 86400;
            float maxTemperature = currentTemperatureValue;
            uint8_t matchingForecasts = 0;
            for (JsonObject item : forecastDocument["list"].as<JsonArray>())
            {
                if ((((item["dt"] | 0) + timezoneOffset) / 86400) != currentDay)
                    continue;
                matchingForecasts++;
                const float candidate = item["main"]["temp_max"] | -1000.0f;
                if (candidate > maxTemperature)
                    maxTemperature = candidate;
            }
            if (maxTemperature > -999.0f)
                dailyHighTemperature = roundTemperature ? String(roundf(maxTemperature), 0) : String(maxTemperature, 1);
            weatherDebug = F("Forecast values=");
            weatherDebug += matchingForecasts;
        }
    }
    success = currentTemperatureValid && weather.length() > 0;
    if (!success)
    {
        weatherDebug = F("Weather current=");
        weatherDebug += currentStatus;
        weatherDebug += currentParsed ? F(" OK") : currentErrorText;
        weatherDebug += F(" forecast=");
        weatherDebug += forecastStatus;
        weatherDebug += forecastParsed ? F(" OK") : forecastErrorText;
    }
    if (success)
    {
        weatherDebug = F("OK ");
        weatherDebug += locationQuery;
        weatherDebug += F(" temp=");
        weatherDebug += currentTemperature;
        weatherDebug += F(" max=");
        weatherDebug += dailyHighTemperature;
        weatherDebug += F(" weather=");
        weatherDebug += weather;
        if (forecastParsed)
        {
            weatherDebug += F(" forecast used");
        }
    }
    if (success)
    {
        weatherDebug += String(latitude, 5);
        weatherDebug += F(" lon=");
        weatherDebug += String(longitude, 5);
        weatherDebug += F(" temp=");
        weatherDebug += currentTemperature;
        weatherDebug += F(" max=");
        weatherDebug += dailyHighTemperature;
        weatherDebug += F(" weather=");
        weatherDebug += weather;
    }
    if (localTime > 0)
    {
        char timestamp[24];
        snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d",
                 year(localTime), month(localTime), day(localTime), hour(localTime), minute(localTime), second(localTime));
        lastWeatherFetch = timestamp;
    }
    return success;
}

void InfoProvider::fetchWeatherNow()
{
    latitude = 0.0f;
    longitude = 0.0f;
    if (updateLocation())
        updateWeather();
    lastWeatherUpdate = millis();
}

void InfoProvider::readFromJsonState(JsonObject &root)
{
    JsonObject top = root[FPSTR(_name)];
    if (!top.isNull() && (top["fetch"] | false))
        weatherFetchRequested = true;
}

String InfoProvider::renderTemplate(const String &source) const
{
    String rendered = source;
    rendered.replace("[temp]", currentTemperature);
    String maxTemperaturePart;
    if (dailyHighTemperature.length() > 0 && currentTemperature.toFloat() < dailyHighTemperature.toFloat())
    {
        maxTemperaturePart = ">";
        maxTemperaturePart += dailyHighTemperature;
    }
    rendered.replace("[maxTempPart]", maxTemperaturePart);
    if (dailyHighTemperature.length() == 0)
    {
        rendered.replace(" > [maxTemp]", "");
        rendered.replace(">[maxTemp]", "");
    }
    rendered.replace("[maxTemp]", dailyHighTemperature);
    rendered.replace("[weather]", weather);
    rendered.replace("[max]", dailyHighTemperature);
    rendered.replace("[wetter]", weather);
    rendered.replace("[termin]", nextCalendarEvent);
    if (birthdayName.length() == 0)
        rendered.replace(" [birthdayName]", "");
    rendered.replace("[birthdayName]", birthdayName);
    rendered.replace("[birthdayFull0]", birthdayFullZero);
    rendered.replace("[birthdayFull]", birthdayFull);
    rendered.replace("[current temperature]", currentTemperature);
    rendered.replace("[daily high temperature]", dailyHighTemperature);
    rendered.replace("[weather]", weather);
    rendered.replace("[next calendar event]", nextCalendarEvent);
    return renderWledTokens(rendered);
}

String InfoProvider::renderWledTokens(const String &source) const
{
    String result;
    result.reserve(source.length() + 16);
    char sec[5];
    int amPmHour = hour(localTime);
    bool isAm = true;
    if (useAMPM)
    {
        if (amPmHour > 11)
        {
            amPmHour -= 12;
            isAm = false;
        }
        if (amPmHour == 0)
            amPmHour = 12;
        sprintf_P(sec, PSTR(" %2s"), (isAm ? "AM" : "PM"));
    }
    else
    {
        sprintf_P(sec, PSTR(":%02d"), second(localTime));
    }

    for (size_t position = 0; position < source.length();)
    {
        if (source[position] != '#')
        {
            result += source[position++];
            continue;
        }

        char token[7];
        size_t tokenLength = 0;
        while (tokenLength < 6 && position + tokenLength < source.length())
        {
            token[tokenLength] = std::toupper(source[position + tokenLength]);
            tokenLength++;
        }
        token[tokenLength] = '\0';

        bool zero = false;
        size_t advance = 1;
        char value[32] = {0};
        if (!strncmp_P(token, PSTR("#DATE"), 5))
        {
            sprintf_P(value, zero ? PSTR("%02d.%02d.%04d") : PSTR("%d.%d.%d"), day(localTime), month(localTime), year(localTime));
            advance = 5;
        }
        else if (!strncmp_P(token, PSTR("#DDMM"), 5))
        {
            zero = token[5] == '0';
            sprintf_P(value, zero ? PSTR("%02d.%02d") : PSTR("%d.%d"), day(localTime), month(localTime));
            advance = zero ? 6 : 5;
        }
        else if (!strncmp_P(token, PSTR("#MMDD"), 5))
        {
            zero = token[5] == '0';
            sprintf_P(value, zero ? PSTR("%02d/%02d") : PSTR("%d/%d"), month(localTime), day(localTime));
            advance = zero ? 6 : 5;
        }
        else if (!strncmp_P(token, PSTR("#TIME"), 5))
        {
            sprintf_P(value, PSTR("%2d:%02d%s"), amPmHour, minute(localTime), sec);
            advance = 5;
        }
        else if (!strncmp_P(token, PSTR("#HHMM"), 5))
        {
            sprintf_P(value, PSTR("%d:%02d"), amPmHour, minute(localTime));
            advance = 5;
        }
        else if (!strncmp_P(token, PSTR("#YYYY"), 5))
        {
            sprintf_P(value, PSTR("%04d"), year(localTime));
            advance = 5;
        }
        else if (!strncmp_P(token, PSTR("#MONL"), 5))
        {
            snprintf(value, sizeof(value), "%s", monthStr(month(localTime)));
            advance = 5;
        }
        else if (!strncmp_P(token, PSTR("#DDDD"), 5))
        {
            snprintf(value, sizeof(value), "%s", dayStr(weekday(localTime)));
            advance = 5;
        }
        else if (!strncmp_P(token, PSTR("#MON"), 4))
        {
            snprintf(value, sizeof(value), "%s", monthShortStr(month(localTime)));
            advance = 4;
        }
        else if (!strncmp_P(token, PSTR("#DAY"), 4))
        {
            snprintf(value, sizeof(value), "%s", dayShortStr(weekday(localTime)));
            advance = 4;
        }
        else if (!strncmp_P(token, PSTR("#YY"), 3))
        {
            sprintf_P(value, PSTR("%02d"), year(localTime) % 100);
            advance = 3;
        }
        else if (!strncmp_P(token, PSTR("#HH"), 3))
        {
            zero = token[3] == '0';
            sprintf_P(value, zero ? PSTR("%02d") : PSTR("%d"), amPmHour);
            advance = zero ? 4 : 3;
        }
        else if (!strncmp_P(token, PSTR("#MM"), 3))
        {
            zero = token[3] == '0';
            sprintf_P(value, zero ? PSTR("%02d") : PSTR("%d"), month(localTime));
            advance = zero ? 4 : 3;
        }
        else if (!strncmp_P(token, PSTR("#SS"), 3))
        {
            zero = token[3] == '0';
            sprintf_P(value, zero ? PSTR("%02d") : PSTR("%d"), second(localTime));
            advance = zero ? 4 : 3;
        }
        else if (!strncmp_P(token, PSTR("#MO"), 3))
        {
            zero = token[3] == '0';
            sprintf_P(value, zero ? PSTR("%02d") : PSTR("%d"), month(localTime));
            advance = zero ? 4 : 3;
        }
        else if (!strncmp_P(token, PSTR("#DD"), 3))
        {
            zero = token[3] == '0';
            sprintf_P(value, zero ? PSTR("%02d") : PSTR("%d"), day(localTime));
            advance = zero ? 4 : 3;
        }

        if (value[0] != '\0')
            result += value;
        else
        {
            result += source[position];
            advance = 1;
        }
        position += advance;
    }
    return result;
}

void InfoProvider::updateBirthday()
{
    birthdayName = "";
    for (uint8_t index = 0; index < BirthdaySlots; index++)
    {
        const int separator = birthdays[index].indexOf('|');
        if (separator < 0)
            continue;
        const int dateSeparator = birthdays[index].indexOf('.');
        if (dateSeparator <= 0 || dateSeparator >= separator)
            continue;
        const uint8_t entryDay = birthdays[index].substring(0, dateSeparator).toInt();
        const uint8_t entryMonth = birthdays[index].substring(dateSeparator + 1, separator).toInt();
        if (entryDay == day(localTime) && entryMonth == month(localTime))
        {
            if (birthdayName.length() > 0)
                birthdayName += F(" & ");
            birthdayName += birthdays[index].substring(separator + 1);
        }
    }

    char date[8];
    char dateZero[8];
    snprintf(date, sizeof(date), "%u.%u.", day(localTime), month(localTime));
    snprintf(dateZero, sizeof(dateZero), "%02u.%02u.", day(localTime), month(localTime));
    birthdayFull = date;
    birthdayFullZero = dateZero;
    if (birthdayName.length() > 0)
    {
        birthdayFull += ' ';
        birthdayFull += birthdayName;
        birthdayFullZero += ' ';
        birthdayFullZero += birthdayName;
    }
}

void InfoProvider::loadBirthdayDefaults()
{
    if (birthdayDefaultsLoaded)
        return;
    birthdayDefaultsLoaded = true;
    for (uint8_t index = 0; index < INFO_PROVIDER_BIRTHDAY_DEFAULT_COUNT && index < BirthdaySlots; index++)
    {
        const InfoProviderBirthdayDefault &entry = INFO_PROVIDER_BIRTHDAY_DEFAULTS[index];
        char value[WLED_MAX_SEGNAME_LEN + 1];
        snprintf(value, sizeof(value), "%02u.%02u|%s", entry.day, entry.month, entry.name);
        birthdays[index] = value;
    }
}

bool InfoProvider::getUMData(um_data_t **data)
{
    if (!enabled || !um_data || !data)
        return false;
    *data = um_data;
    return true;
}

void InfoProvider::copyToBuffer(char *destination, size_t capacity, const String &value)
{
    if (capacity == 0)
        return;
    size_t length = value.length();
    if (length >= capacity)
        length = capacity - 1;
    memcpy(destination, value.c_str(), length);
    destination[length] = '\0';
}

void InfoProvider::renderConfigs()
{
    const uint32_t weatherColor = getWeatherColor();
    for (uint8_t index = 0; index < 8; index++)
    {
        String rendered = renderTemplate(configs[index]);
        copyToBuffer(infoData.text[index], sizeof(infoData.text[index]), rendered);
        infoData.valid[index] = enabled && rendered.length() > 0;
        infoData.color[index] = weatherColors[index] ? weatherColor : 0;
    }
}

bool InfoProvider::readFromConfig(JsonObject &root)
{
    loadBirthdayDefaults();
    JsonObject top = root[FPSTR(_name)];
    bool complete = !top.isNull();
    enabled = top[FPSTR(_enabled)] | enabled;
    location = top["location"] | location;
    country = top["country"] | country;
    latitude = 0.0f;
    longitude = 0.0f;
    openWeatherApiKey = top["openWeatherApiKey"] | openWeatherApiKey;
    weatherUpdateMinutes = top["weatherUpdateMinutes"] | weatherUpdateMinutes;
    roundTemperature = top["roundTemperature"] | roundTemperature;
    lastWeatherFetch = top["lastWeatherFetch"] | lastWeatherFetch;
    weatherUpdateMinutes = constrain(weatherUpdateMinutes, (uint16_t)1, (uint16_t)1440);
    lastWeatherUpdate = 0;
    weatherFetchRequested = true;
    if (top["location"].isNull() || top["country"].isNull() || top["openWeatherApiKey"].isNull() || top["weatherUpdateMinutes"].isNull() || top["roundTemperature"].isNull())
        complete = false;
    for (uint8_t index = 0; index < 8; index++)
    {
        char key[9];
        snprintf(key, sizeof(key), "config%02u", index + 1);
        if (top[key].isNull())
            complete = false;
        configs[index] = top[key] | configs[index];
        if (configs[index].length() > WLED_MAX_SEGNAME_LEN)
            configs[index].remove(WLED_MAX_SEGNAME_LEN);
        char colorKey[15];
        snprintf(colorKey, sizeof(colorKey), "weatherColor%02u", index + 1);
        weatherColors[index] = top[colorKey] | weatherColors[index];
        if (top[colorKey].isNull())
            complete = false;
    }
    for (uint8_t index = 0; index < BirthdaySlots; index++)
    {
        char key[13];
        char legacyKey[13];
        snprintf(key, sizeof(key), "BD%02u", index + 1);
        snprintf(legacyKey, sizeof(legacyKey), "birthday%02u", index + 1);
        if (top[key].isNull() && top[legacyKey].isNull())
            complete = false;
        birthdays[index] = top[key] | (top[legacyKey] | birthdays[index]);
        if (birthdays[index].length() > WLED_MAX_SEGNAME_LEN)
            birthdays[index].remove(WLED_MAX_SEGNAME_LEN);
    }
    updateBirthday();
    renderConfigs();
    return complete;
}

void InfoProvider::addToConfig(JsonObject &root)
{
    JsonObject top = root.createNestedObject(FPSTR(_name));
    top[FPSTR(_enabled)] = enabled;
    top["location"] = location;
    top["country"] = country;
    top["openWeatherApiKey"] = openWeatherApiKey;
    top["weatherUpdateMinutes"] = weatherUpdateMinutes;
    top["roundTemperature"] = roundTemperature;
    top["lastWeatherFetch"] = lastWeatherFetch;
    for (uint8_t index = 0; index < 8; index++)
    {
        char key[9];
        snprintf(key, sizeof(key), "config%02u", index + 1);
        top[key] = configs[index];
        char colorKey[15];
        snprintf(colorKey, sizeof(colorKey), "weatherColor%02u", index + 1);
        top[colorKey] = weatherColors[index];
    }
    for (uint8_t index = 0; index < BirthdaySlots; index++)
    {
        char key[13];
        snprintf(key, sizeof(key), "BD%02u", index + 1);
        top[key] = birthdays[index];
    }
}
