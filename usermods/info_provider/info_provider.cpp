#include "info_provider.h"

const char InfoProvider::_name[] PROGMEM = "InfoProvider";
const char InfoProvider::_enabled[] PROGMEM = "Enable";

static InfoProvider infoProvider;
REGISTER_USERMOD(infoProvider);
static constexpr uint32_t CalendarRetryDelaysSeconds[] = {15, 30, 60, 120, 300};
static constexpr uint8_t CalendarRetryDelayCount = sizeof(CalendarRetryDelaysSeconds) / sizeof(CalendarRetryDelaysSeconds[0]);
// updateCalendar()/updateWeather() are fully synchronous, multi-second HTTP+parse calls; keep them
// out of the first few boot loop ticks so they can't stall handlePresets() while the boot preset
// is still being applied.
static constexpr uint32_t BootFetchGraceMs = 5000;

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
    applyCalendarCache();
    renderConfigs();
}

void InfoProvider::loop()
{
    if (!enabled || millis() - lastUpdate < 1000)
        return;

    lastUpdate = millis();
    const bool weatherInitialFetchReady = !weatherFetchRequested || toki.getTimeSource() != TOKI_TS_NONE;
    const uint32_t weatherDelayMs = weatherRetryCount > 0 && weatherRetryCount <= CalendarRetryDelayCount
                                        ? CalendarRetryDelaysSeconds[weatherRetryCount - 1] * 1000U
                                        : (uint32_t)weatherUpdateMinutes * 60000U;
    if (millis() >= BootFetchGraceMs && weatherInitialFetchReady && (weatherFetchRequested || (openWeatherApiKey.length() > 0 && location.length() > 0 && (lastWeatherUpdate == 0 || millis() - lastWeatherUpdate >= weatherDelayMs))))
    {
        bool weatherSuccess = false;
        weatherFetchRequested = false;
        if (latitude == 0.0f && longitude == 0.0f)
            weatherSuccess = updateLocation();
        if (latitude != 0.0f || longitude != 0.0f)
            weatherSuccess = updateWeather();
        if (weatherSuccess)
            weatherRetryCount = 0;
        else if (weatherRetryCount <= CalendarRetryDelayCount)
            weatherRetryCount++;
        lastWeatherUpdate = millis();
    }
    if (!calendarFetchedOnce && toki.getTimeSource() != TOKI_TS_NONE)
        applyCalendarCache();
    const uint32_t calendarDelayMs = calendarRetryCount > 0 && calendarRetryCount <= CalendarRetryDelayCount
                                         ? CalendarRetryDelaysSeconds[calendarRetryCount - 1] * 1000U
                                         : (uint32_t)calendarUpdateMinutes * 60000U;
    if (millis() >= BootFetchGraceMs && calendarUrl.length() > 0 && toki.getTimeSource() != TOKI_TS_NONE && (lastCalendarUpdate == 0 || millis() - lastCalendarUpdate >= calendarDelayMs))
    {
        if (updateCalendar())
        {
            calendarFetchedOnce = true;
            calendarRetryCount = 0;
        }
        else if (calendarRetryCount <= CalendarRetryDelayCount)
            calendarRetryCount++;
        lastCalendarUpdate = millis();
    }
    updateBirthday();
    renderConfigs();
}

void InfoProvider::connected()
{
    lastWeatherUpdate = 0;
    weatherRetryCount = 0;
    weatherFetchRequested = true;
    lastCalendarUpdate = 0;
    calendarRetryCount = 0;
    calendarFetchedOnce = false;
}

void InfoProvider::appendConfigData()
{
    oappend(F("addInfo('InfoProvider:Enable',1,'<br>Available tags: [temp] [maxTemp] [maxTempPart] [weather] [termin] [birthdayName] [birthdayFull] [birthdayFull0]');"));
    oappend(F("addInfo('InfoProvider:weatherUpdateMinutes',1,'minutes');"));
    oappend(F("addInfo('InfoProvider:calendarUrl',1,'public Google iCal URL');"));
    oappend(F("addInfo('InfoProvider:calendarUpdateMinutes',1,'calendar minutes');"));
    oappend(F("addInfo('InfoProvider:calendarDebug',1,'calendar status');"));
    oappend(F("var f=document.getElementsByName(\"InfoProvider:config01\")[1];if(f&&f.previousSibling&&f.previousSibling.previousSibling)f.previousSibling.previousSibling.nodeValue=' #Info01 ';"));
    oappend(F("var f=document.getElementsByName(\"InfoProvider:config02\")[1];if(f&&f.previousSibling&&f.previousSibling.previousSibling)f.previousSibling.previousSibling.nodeValue=' #Info02 ';"));
    oappend(F("var f=document.getElementsByName(\"InfoProvider:config03\")[1];if(f&&f.previousSibling&&f.previousSibling.previousSibling)f.previousSibling.previousSibling.nodeValue=' #Info03 ';"));
    oappend(F("var f=document.getElementsByName(\"InfoProvider:config04\")[1];if(f&&f.previousSibling&&f.previousSibling.previousSibling)f.previousSibling.previousSibling.nodeValue=' #Info04 ';"));
    oappend(F("var f=document.getElementsByName(\"InfoProvider:config05\")[1];if(f&&f.previousSibling&&f.previousSibling.previousSibling)f.previousSibling.previousSibling.nodeValue=' #Info05 ';"));
    oappend(F("var f=document.getElementsByName(\"InfoProvider:config06\")[1];if(f&&f.previousSibling&&f.previousSibling.previousSibling)f.previousSibling.previousSibling.nodeValue=' #Info06 ';"));
    oappend(F("var f=document.getElementsByName(\"InfoProvider:config07\")[1];if(f&&f.previousSibling&&f.previousSibling.previousSibling)f.previousSibling.previousSibling.nodeValue=' #Info07 ';"));
    oappend(F("var f=document.getElementsByName(\"InfoProvider:config08\")[1];if(f&&f.previousSibling&&f.previousSibling.previousSibling)f.previousSibling.previousSibling.nodeValue=' #Info08 ';"));
    oappend(F("var f=document.getElementsByName(\"InfoProvider:weatherColor01\")[1];if(f&&f.previousSibling&&f.previousSibling.previousSibling)f.previousSibling.previousSibling.nodeValue=' weather color ';"));
    oappend(F("var f=document.getElementsByName(\"InfoProvider:weatherColor02\")[1];if(f&&f.previousSibling&&f.previousSibling.previousSibling)f.previousSibling.previousSibling.nodeValue=' weather color ';"));
    oappend(F("var f=document.getElementsByName(\"InfoProvider:weatherColor03\")[1];if(f&&f.previousSibling&&f.previousSibling.previousSibling)f.previousSibling.previousSibling.nodeValue=' weather color ';"));
    oappend(F("var f=document.getElementsByName(\"InfoProvider:weatherColor04\")[1];if(f&&f.previousSibling&&f.previousSibling.previousSibling)f.previousSibling.previousSibling.nodeValue=' weather color ';"));
    oappend(F("var f=document.getElementsByName(\"InfoProvider:weatherColor05\")[1];if(f&&f.previousSibling&&f.previousSibling.previousSibling)f.previousSibling.previousSibling.nodeValue=' weather color ';"));
    oappend(F("var f=document.getElementsByName(\"InfoProvider:weatherColor06\")[1];if(f&&f.previousSibling&&f.previousSibling.previousSibling)f.previousSibling.previousSibling.nodeValue=' weather color ';"));
    oappend(F("var f=document.getElementsByName(\"InfoProvider:weatherColor07\")[1];if(f&&f.previousSibling&&f.previousSibling.previousSibling)f.previousSibling.previousSibling.nodeValue=' weather color ';"));
    oappend(F("var f=document.getElementsByName(\"InfoProvider:weatherColor08\")[1];if(f&&f.previousSibling&&f.previousSibling.previousSibling)f.previousSibling.previousSibling.nodeValue=' weather color ';"));
    oappend(F("var f=document.getElementsByName(\"InfoProvider:config01\")[1],n=f&&f.previousSibling&&f.previousSibling.previousSibling;if(n){var h=document.createElement('span');h.innerHTML='<div style=\"border-top:1px solid currentColor;margin:8px 0\"></div><b>Configs</b><br>';n.parentNode.insertBefore(h,n);}"));
    oappend(F("var f=document.getElementsByName(\"InfoProvider:BD01\")[1],n=f&&f.previousSibling&&f.previousSibling.previousSibling;if(n){var h=document.createElement('span');h.innerHTML='<div style=\"border-top:1px solid currentColor;margin:8px 0\"></div><b>Birthday list</b><br>';n.parentNode.insertBefore(h,n);}"));
}

static int calendarDaySerial(int yearValue, int monthValue, int dayValue)
{
    int days = 0;
    for (int yearIndex = 2000; yearIndex < yearValue; yearIndex++)
        days += ((yearIndex % 4 == 0 && (yearIndex % 100 != 0 || yearIndex % 400 == 0)) ? 366 : 365);
    static const uint16_t monthDays[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    days += monthDays[monthValue - 1] + dayValue - 1;
    if (monthValue > 2 && (yearValue % 4 == 0 && (yearValue % 100 != 0 || yearValue % 400 == 0)))
        days++;
    return days;
}

static String calendarRuleValue(const String &rule, const char *name)
{
    String key = String(name) + '=';
    int start = rule.indexOf(key);
    if (start < 0)
        return "";
    start += key.length();
    int end = rule.indexOf(';', start);
    return rule.substring(start, end < 0 ? rule.length() : end);
}

static bool calendarDateExcluded(const String &exdates, int yearValue, int monthValue, int dayValue)
{
    char date[9];
    snprintf(date, sizeof(date), "%04d%02d%02d", yearValue, monthValue, dayValue);
    return exdates.indexOf(date) >= 0;
}

static void calendarDateAfter(int yearValue, int monthValue, int dayValue, int offset, int &resultYear, int &resultMonth, int &resultDay)
{
    resultYear = yearValue;
    resultMonth = monthValue;
    resultDay = dayValue;
    while (offset-- > 0)
    {
        resultDay++;
        int daysInMonth = 31;
        if (resultMonth == 4 || resultMonth == 6 || resultMonth == 9 || resultMonth == 11)
            daysInMonth = 30;
        else if (resultMonth == 2)
            daysInMonth = (resultYear % 4 == 0 && (resultYear % 100 != 0 || resultYear % 400 == 0)) ? 29 : 28;
        if (resultDay > daysInMonth)
        {
            resultDay = 1;
            resultMonth++;
            if (resultMonth > 12)
            {
                resultMonth = 1;
                resultYear++;
            }
        }
    }
}

static bool calendarWeekdayMatches(const String &byDay, int weekdayValue, int dayOfMonth)
{
    if (byDay.length() == 0)
        return true;
    static const char *names[] = {"SU", "MO", "TU", "WE", "TH", "FR", "SA"};
    for (uint8_t index = 0; index < 7; index++)
    {
        if (byDay.indexOf(names[index]) < 0)
            continue;
        int ordinal = 0;
        int position = byDay.indexOf(names[index]);
        if (position > 0)
            ordinal = byDay.substring(0, position).toInt();
        if (weekdayValue == index + 1 && (ordinal == 0 || ((dayOfMonth - 1) / 7 + 1) == ordinal))
            return true;
    }
    return false;
}

static bool calendarOccurrenceMatches(const String &rule, int baseYear, int baseMonth, int baseDay, int yearValue, int monthValue, int dayValue, int weekdayValue, int occurrenceOffset)
{
    if (rule.length() == 0)
        return yearValue == baseYear && monthValue == baseMonth && dayValue == baseDay;
    const String frequency = calendarRuleValue(rule, "FREQ");
    const int interval = max(1L, calendarRuleValue(rule, "INTERVAL").toInt());
    const int baseSerial = calendarDaySerial(baseYear, baseMonth, baseDay);
    const int candidateSerial = calendarDaySerial(yearValue, monthValue, dayValue);
    if (candidateSerial < baseSerial)
        return false;
    const int dayDifference = candidateSerial - baseSerial;
    const String byMonth = calendarRuleValue(rule, "BYMONTH");
    const String byMonthDay = calendarRuleValue(rule, "BYMONTHDAY");
    const String byDay = calendarRuleValue(rule, "BYDAY");
    if (frequency == "DAILY")
        return dayDifference % interval == 0;
    if (frequency == "WEEKLY")
        return dayDifference % (7 * interval) == 0 && calendarWeekdayMatches(byDay, weekdayValue, dayValue);
    if (frequency == "MONTHLY")
    {
        const int monthDifference = (yearValue - baseYear) * 12 + monthValue - baseMonth;
        if (monthDifference < 0 || monthDifference % interval != 0)
            return false;
        if (byMonthDay.length() > 0)
            return byMonthDay.toInt() == dayValue;
        if (byDay.length() > 0)
            return calendarWeekdayMatches(byDay, weekdayValue, dayValue);
        return dayValue == baseDay;
    }
    if (frequency == "YEARLY")
    {
        if ((yearValue - baseYear) < 0 || (yearValue - baseYear) % interval != 0)
            return false;
        if (byMonth.length() > 0 && byMonth.toInt() != monthValue)
            return false;
        if (byMonthDay.length() > 0)
            return byMonthDay.toInt() == dayValue;
        return monthValue == baseMonth && dayValue == baseDay;
    }
    return false;
}

// weekday() returns 1=Sunday..7=Saturday; index accordingly for German short names.
static const char *calendarGermanWeekday(uint8_t weekdayValue)
{
    static const char *names[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
    return names[(weekdayValue - 1) % 7];
}

// Converts a UTC iCal date/time ("Z" suffix) into local wall-clock date/time components,
// so recurrence and "already passed today" checks compare like-for-like against localTime.
static void calendarUtcToLocal(int utcYear, int utcMonth, int utcDay, int utcHour, int utcMinute, int utcSecond,
                                int &localYear, int &localMonth, int &localDay, int &localMinuteOfDay)
{
    tmElements_t tm;
    tm.Year = CalendarYrToTm(utcYear);
    tm.Month = utcMonth;
    tm.Day = utcDay;
    tm.Hour = utcHour;
    tm.Minute = utcMinute;
    tm.Second = utcSecond;
    const time_t utcEpoch = makeTime(tm);
    const long rawOffsetSeconds = (long)localTime - (long)toki.second();
    const long localOffsetSeconds = rawOffsetSeconds >= 0
                                        ? ((rawOffsetSeconds + 30) / 60) * 60
                                        : ((rawOffsetSeconds - 30) / 60) * 60;
    const time_t localEpoch = (time_t)((long)utcEpoch + localOffsetSeconds);
    localYear = year(localEpoch);
    localMonth = month(localEpoch);
    localDay = day(localEpoch);
    localMinuteOfDay = hour(localEpoch) * 60 + minute(localEpoch);
}

bool InfoProvider::updateCalendar()
{
    WiFiClient client;
    HTTPClient http;
    if (!WLED_CONNECTED || !http.begin(client, calendarUrl))
    {
        calendarDebug = F("Calendar HTTP begin failed");
        return false;
    }
    http.setTimeout(8000);
    const int status = http.GET();
    if (status != HTTP_CODE_OK)
    {
        calendarDebug = F("Calendar HTTP ");
        calendarDebug += status;
        http.end();
        return false;
    }

    WiFiClient *stream = http.getStreamPtr();
    size_t responseBytes = 0;
    String eventStart;
    String eventRule;
    String eventExdates;
    String eventSummary;
    String bestEvent;
    String bestSummaryPlain;
    bool bestIsTimed = false;
    uint16_t eventCount = 0;
    uint16_t matchingEventCount = 0;
    int bestDay = 7;
    int bestMinutes = 1441;
    const int todaySerial = calendarDaySerial(year(localTime), month(localTime), day(localTime));

    bool inEvent = false;
    while (stream && (http.connected() || stream->available()))
    {
        String line = stream->readStringUntil('\n');
        responseBytes += line.length() + 1;
        line.trim();

        if (line == F("BEGIN:VEVENT"))
        {
            eventCount++;
            inEvent = true;
            eventStart = "";
            eventRule = "";
            eventExdates = "";
            eventSummary = "";
        }
        else if (inEvent && line.startsWith(F("DTSTART")))
            eventStart = line.substring(line.indexOf(':') + 1);
        else if (inEvent && line.startsWith(F("RRULE:")))
            eventRule = line.substring(6);
        else if (inEvent && line.startsWith(F("EXDATE")))
            eventExdates += line.substring(line.indexOf(':') + 1);
        else if (inEvent && line.startsWith(F("SUMMARY:")))
            eventSummary = line.substring(8);
        else if (inEvent && line == F("END:VEVENT"))
        {
            inEvent = false;
            if (eventStart.length() < 8 || eventSummary.length() == 0)
                continue;
            int eventYear = eventStart.substring(0, 4).toInt();
            int eventMonth = eventStart.substring(4, 6).toInt();
            int eventDay = eventStart.substring(6, 8).toInt();
            int eventMinutes = 0;
            const bool isTimed = eventStart.length() >= 13 && eventStart[8] == 'T';
            // Google iCal exports timed events in UTC ("...Z"); convert to local wall-clock
            // time before comparing against localTime, otherwise the timezone offset makes
            // "still upcoming today" events look like they already passed (or vice versa).
            const bool isUtc = isTimed && eventStart.endsWith("Z");
            if (isTimed)
            {
                const int utcHour = eventStart.substring(9, 11).toInt();
                const int utcMinute = eventStart.substring(11, 13).toInt();
                if (isUtc)
                {
                    const int utcSecond = eventStart.length() >= 15 ? eventStart.substring(13, 15).toInt() : 0;
                    calendarUtcToLocal(eventYear, eventMonth, eventDay, utcHour, utcMinute, utcSecond,
                                       eventYear, eventMonth, eventDay, eventMinutes);
                }
                else
                    eventMinutes = utcHour * 60 + utcMinute;
            }
            const int currentMinutes = hour(localTime) * 60 + minute(localTime);

            int dayOffset = -1;
            int candidateYear = year(localTime);
            int candidateMonth = month(localTime);
            int candidateDay = day(localTime);
            for (uint8_t offset = 0; offset <= 6; offset++)
            {
                int testYear, testMonth, testDay;
                calendarDateAfter(candidateYear, candidateMonth, candidateDay, offset, testYear, testMonth, testDay);
                int testWeekday = (weekday(localTime) - 1 + offset) % 7 + 1;
                if (calendarDateExcluded(eventExdates, testYear, testMonth, testDay))
                    continue;
                if (!calendarOccurrenceMatches(eventRule, eventYear, eventMonth, eventDay, testYear, testMonth, testDay, testWeekday, offset))
                    continue;
                if (offset == 0 && isTimed && eventMinutes < currentMinutes)
                    continue;
                const String until = calendarRuleValue(eventRule, "UNTIL");
                if (until.length() >= 8 && calendarDaySerial(testYear, testMonth, testDay) > calendarDaySerial(until.substring(0, 4).toInt(), until.substring(4, 6).toInt(), until.substring(6, 8).toInt()))
                    continue;
                dayOffset = offset;
                break;
            }
            if (dayOffset < 0)
                continue;
            matchingEventCount++;
            // TODO: add a separate template tag listing all-day events within the lookahead
            // window (currently only nextCalendarEvent/[termin] is exposed).
            // All-day events carry eventMinutes==0, so without this they'd always outrank a
            // same-day timed event; sort all-day events after any still-upcoming timed one.
            const int sortMinutes = isTimed ? eventMinutes : 1440;
            if (dayOffset < bestDay || (dayOffset == bestDay && sortMinutes < bestMinutes))
            {
                bestDay = dayOffset;
                bestMinutes = sortMinutes;
                bestEvent = eventSummary;
                bestSummaryPlain = eventSummary;
                bestIsTimed = isTimed;
                if (isTimed)
                    bestEvent = String(eventMinutes / 60) + ':' + (eventMinutes % 60 < 10 ? "0" : "") + String(eventMinutes % 60) + ' ' + bestEvent;
            }
        }
    }
    http.end();

    if (bestEvent.length() == 0)
    {
        nextCalendarEvent = "";
        calendarDebug = F("HTTP 200 bytes=");
        calendarDebug += responseBytes;
        calendarDebug += F(" events=");
        calendarDebug += eventCount;
        calendarDebug += F(" matches=");
        calendarDebug += matchingEventCount;
        calendarDebug += F(" no event within 6 days");
        return true;
    }
    String prefix = bestDay == 0 ? F("Heute") : (bestDay == 1 ? F("Morgen") : String(calendarGermanWeekday((weekday(localTime) + bestDay - 1) % 7 + 1)));
    nextCalendarEvent = prefix + ' ' + bestEvent;
    // Cache the absolute occurrence so it can be shown immediately on next boot, before the
    // first fetch/parse of the (potentially large) ics file has completed.
    calendarCacheDaySerial = todaySerial + bestDay;
    calendarCacheMinutes = bestIsTimed ? (int16_t)bestMinutes : (int16_t)-1;
    calendarCacheSummary = bestSummaryPlain;
    calendarDebug = F("HTTP 200 bytes=");
    calendarDebug += responseBytes;
    calendarDebug += F(" events=");
    calendarDebug += eventCount;
    calendarDebug += F(" matches=");
    calendarDebug += matchingEventCount;
    calendarDebug += F(" result=");
    calendarDebug += nextCalendarEvent;
    return true;
}

// Reconstructs [termin] from the last persisted cache so an event is shown right after boot,
// before WiFi/NTP are ready and the first ics fetch+parse (which can take a while) completes.
void InfoProvider::applyCalendarCache()
{
    if (toki.getTimeSource() == TOKI_TS_NONE || calendarCacheDaySerial < 0 || calendarCacheSummary.length() == 0)
        return;
    const int todaySerial = calendarDaySerial(year(localTime), month(localTime), day(localTime));
    const int dayOffset = calendarCacheDaySerial - todaySerial;
    if (dayOffset < 0 || dayOffset > 6)
    {
        nextCalendarEvent = "";
        return;
    }
    const bool isTimed = calendarCacheMinutes >= 0;
    if (dayOffset == 0 && isTimed && calendarCacheMinutes < hour(localTime) * 60 + minute(localTime))
    {
        nextCalendarEvent = "";
        return;
    }
    String prefix = dayOffset == 0 ? F("Heute") : (dayOffset == 1 ? F("Morgen") : String(calendarGermanWeekday((weekday(localTime) + dayOffset - 1) % 7 + 1)));
    String event = calendarCacheSummary;
    if (isTimed)
        event = String(calendarCacheMinutes / 60) + ':' + (calendarCacheMinutes % 60 < 10 ? "0" : "") + String(calendarCacheMinutes % 60) + ' ' + event;
    nextCalendarEvent = prefix + ' ' + event;
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
    if (toki.getTimeSource() != TOKI_TS_NONE)
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
    {
        if (updateWeather())
            weatherRetryCount = 0;
        else if (weatherRetryCount <= CalendarRetryDelayCount)
            weatherRetryCount++;
    }
    else if (weatherRetryCount <= CalendarRetryDelayCount)
        weatherRetryCount++;
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
    calendarUrl = top["calendarUrl"] | calendarUrl;
    calendarUpdateMinutes = top["calendarUpdateMinutes"] | calendarUpdateMinutes;
    calendarDebug = top["calendarDebug"] | calendarDebug;
    calendarCacheSummary = top["calendarCacheSummary"] | calendarCacheSummary;
    calendarCacheDaySerial = top["calendarCacheDay"] | calendarCacheDaySerial;
    calendarCacheMinutes = top["calendarCacheMinutes"] | calendarCacheMinutes;
    weatherUpdateMinutes = top["weatherUpdateMinutes"] | weatherUpdateMinutes;
    roundTemperature = top["roundTemperature"] | roundTemperature;
    lastWeatherFetch = top["lastWeatherFetch"] | lastWeatherFetch;
    weatherUpdateMinutes = constrain(weatherUpdateMinutes, (uint16_t)1, (uint16_t)1440);
    calendarUpdateMinutes = constrain(calendarUpdateMinutes, (uint16_t)1, (uint16_t)1440);
    lastWeatherUpdate = 0;
    weatherRetryCount = 0;
    lastCalendarUpdate = 0;
    calendarRetryCount = 0;
    calendarFetchedOnce = false;
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
    top["calendarUrl"] = calendarUrl;
    top["calendarUpdateMinutes"] = calendarUpdateMinutes;
    top["calendarDebug"] = calendarDebug;
    top["calendarCacheSummary"] = calendarCacheSummary;
    top["calendarCacheDay"] = calendarCacheDaySerial;
    top["calendarCacheMinutes"] = calendarCacheMinutes;
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
