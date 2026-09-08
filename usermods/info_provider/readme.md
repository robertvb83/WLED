# Info Provider

This first version is a local test provider for the Scrolling Text effect.

Enable the usermod and set `config01` in Usermod Settings. A segment named `#INFO01` using the Scrolling Text effect displays that text.

Activate it in a custom PlatformIO environment with:

```ini
custom_usermods = info_provider
```

OpenWeather, calendar input, template expansion, and weather colors are intentionally not included yet.

Compile-time birthday defaults are read from `birthdays.txt`:

```text
# Format: DD.MM|Name
11.12|Robert
26.10|Theano
```

The available birthday templates are `[birthdayName]` and `[birthdayFull]`.
