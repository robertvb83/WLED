#include "info_provider.h"

const char InfoProvider::_name[] PROGMEM = "InfoProvider";
const char InfoProvider::_enabled[] PROGMEM = "Enable";

static InfoProvider infoProvider;
REGISTER_USERMOD(infoProvider);

void InfoProvider::setup()
{
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
    testCounter++;
    updateBirthday();
    renderConfigs();
}

void InfoProvider::appendConfigData()
{
    char script[192];
    oappend(F("addInfo('InfoProvider:config01',0,'','Available templates: [temp] [maxTemp] [weather] [termin] [counter] [birthdayName] [birthdayFull]');"));
    for (uint8_t index = 0; index < 8; index++)
    {
        snprintf(script, sizeof(script),
                 "addInfo('InfoProvider:config%02u',1,'','Use #INFO%02u');",
                 index + 1, index + 1);
        oappend(script);
    }
}

String InfoProvider::renderTemplate(const String &source) const
{
    String rendered = source;
    rendered.replace("[counter]", String(testCounter));
    rendered.replace("[temp]", currentTemperature);
    rendered.replace("[maxTemp]", dailyHighTemperature);
    rendered.replace("[weather]", weather);
    rendered.replace("[max]", dailyHighTemperature);
    rendered.replace("[wetter]", weather);
    rendered.replace("[termin]", nextCalendarEvent);
    if (birthdayName.length() == 0)
        rendered.replace(" [birthdayName]", "");
    rendered.replace("[birthdayName]", birthdayName);
    rendered.replace("[birthdayFull]", birthdayFull);
    rendered.replace("[test counter]", String(testCounter));
    rendered.replace("[current temperature]", currentTemperature);
    rendered.replace("[daily high temperature]", dailyHighTemperature);
    rendered.replace("[weather]", weather);
    rendered.replace("[next calendar event]", nextCalendarEvent);
    return rendered;
}

void InfoProvider::updateBirthday()
{
    birthdayName = "";
    for (uint8_t index = 0; index < INFO_PROVIDER_BIRTHDAY_DEFAULT_COUNT; index++)
    {
        const InfoProviderBirthdayDefault &entry = INFO_PROVIDER_BIRTHDAY_DEFAULTS[index];
        if (entry.day == day(localTime) && entry.month == month(localTime))
        {
            if (birthdayName.length() > 0)
                birthdayName += F(" & ");
            birthdayName += entry.name;
        }
    }

    char date[8];
    snprintf(date, sizeof(date), "%02u.%02u.", day(localTime), month(localTime));
    birthdayFull = date;
    if (birthdayName.length() > 0)
    {
        birthdayFull += ' ';
        birthdayFull += birthdayName;
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
    for (uint8_t index = 0; index < 8; index++)
    {
        String rendered = renderTemplate(configs[index]);
        copyToBuffer(infoData.text[index], sizeof(infoData.text[index]), rendered);
        infoData.valid[index] = enabled && rendered.length() > 0;
        infoData.color[index] = 0;
    }
}

bool InfoProvider::readFromConfig(JsonObject &root)
{
    JsonObject top = root[FPSTR(_name)];
    bool complete = !top.isNull();
    enabled = top[FPSTR(_enabled)] | enabled;
    for (uint8_t index = 0; index < 8; index++)
    {
        char key[9];
        snprintf(key, sizeof(key), "config%02u", index + 1);
        if (top[key].isNull())
            complete = false;
        configs[index] = top[key] | configs[index];
        if (configs[index].length() > WLED_MAX_SEGNAME_LEN)
            configs[index].remove(WLED_MAX_SEGNAME_LEN);
    }
    renderConfigs();
    return complete;
}

void InfoProvider::addToConfig(JsonObject &root)
{
    JsonObject top = root.createNestedObject(FPSTR(_name));
    top[FPSTR(_enabled)] = enabled;
    for (uint8_t index = 0; index < 8; index++)
    {
        char key[9];
        snprintf(key, sizeof(key), "config%02u", index + 1);
        top[key] = configs[index];
    }
}
