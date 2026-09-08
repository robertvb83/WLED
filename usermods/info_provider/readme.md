# Info Provider

This first version is a local test provider for the Scrolling Text effect.

Enable the usermod and set `config01` in Usermod Settings. A segment named `#INFO01` using the Scrolling Text effect displays that text.

Activate it in a custom PlatformIO environment with:

```ini
custom_usermods = info_provider
```

OpenWeather API 2.5 support is available through the Usermod Settings fields `location`, `country`, `openWeatherApiKey`, and `weatherUpdateMinutes`. The usermod updates `[temp]`, `[maxTemp]`, and `[weather]` in the background. `Fetch weather` triggers an immediate request and `WeatherDebug` shows the last result.

The current implementation uses HTTP because this WLED toolchain does not provide a TLS client header. Do not use it for sensitive deployments without adding TLS support.

Compile-time birthday defaults are read from `birthdays.txt`:

```text
# Format: DD.MM|Name
11.12|Robert
26.10|Theano
```

The available birthday templates are `[birthdayName]` and `[birthdayFull]`.
