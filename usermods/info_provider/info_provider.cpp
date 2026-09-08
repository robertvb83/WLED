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
    renderConfigs();
}

void InfoProvider::loop()
{
    // Reserved for future live data updates.
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
        copyToBuffer(infoData.text[index], sizeof(infoData.text[index]), configs[index]);
        infoData.valid[index] = enabled && configs[index].length() > 0;
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
