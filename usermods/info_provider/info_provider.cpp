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
    testCounter++;
    updateBirthday();
    renderConfigs();
}

void InfoProvider::appendConfigData()
{
    char script[192];
    oappend(F("addInfo('InfoProvider:Enable',1,'<br>Available templates: [temp] [maxTemp] [weather] [termin] [counter] [birthdayName] [birthdayFull]');"));
    for (uint8_t index = 0; index < 8; index++)
    {
        snprintf(script, sizeof(script),
                 "addInfo('InfoProvider:config%02u',1,'','Use #INFO%02u');",
                 index + 1, index + 1);
        oappend(script);
    }
    oappend(F("addInfo('InfoProvider:config08',1,'<br>Birthday list','');"));
    for (uint8_t index = 0; index < BirthdaySlots; index++)
    {
        snprintf(script, sizeof(script),
                 "addInfo('InfoProvider:birthday%02u',1,'','DD.MM|Name');",
                 index + 1);
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
    snprintf(date, sizeof(date), "%02u.%02u.", day(localTime), month(localTime));
    birthdayFull = date;
    if (birthdayName.length() > 0)
    {
        birthdayFull += ' ';
        birthdayFull += birthdayName;
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
    loadBirthdayDefaults();
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
    for (uint8_t index = 0; index < BirthdaySlots; index++)
    {
        char key[13];
        snprintf(key, sizeof(key), "birthday%02u", index + 1);
        if (top[key].isNull())
            complete = false;
        birthdays[index] = top[key] | birthdays[index];
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
    for (uint8_t index = 0; index < 8; index++)
    {
        char key[9];
        snprintf(key, sizeof(key), "config%02u", index + 1);
        top[key] = configs[index];
    }
    for (uint8_t index = 0; index < BirthdaySlots; index++)
    {
        char key[13];
        snprintf(key, sizeof(key), "birthday%02u", index + 1);
        top[key] = birthdays[index];
    }
}
