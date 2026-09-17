# PresenceSwitch

Turns WLED off automatically when none of a user-selected list of network devices (e.g. phones)
are present, and turns it back on as soon as any of them reappear on the Wi-Fi network.

**ESP32 only.** Device presence is checked with ICMP ping via the ESP-IDF `ping_sock` API. On
ESP8266 the usermod compiles but always reports "absent" (no ping engine implemented).

## How it works

- Enter one or more IPv4 addresses in `extraIPs` on the **Usermod Settings** page ("Config" ->
  "Usermods"), separated by commas, spaces, or semicolons.
- Every `checkIntervalSec` seconds, only the manually configured devices are pinged.
- If none of the watched devices have answered for `offDelayMin` minutes, WLED is switched off
  (brightness set to 0). As soon as one of them answers again, WLED restores the previous
  brightness.

Devices are identified purely by **IPv4 address**, not MAC address. If your router doesn't hand
out the same IP to a device every time, configure a DHCP reservation for the devices you want to
watch so their address stays stable across reboots/sleep.

## Settings

| Setting            | Description                                                                                        |
| ------------------ | -------------------------------------------------------------------------------------------------- |
| `enabled`          | enable/disable the usermod                                                                         |
| `offDelayMin`      | minutes with no watched device present before switching off (`0` = never switch off automatically) |
| `checkIntervalSec` | how often the watched devices are (re-)pinged                                                      |
| `extraIPs`         | comma-, space-, or semicolon-separated IPv4 addresses to watch                                     |

## JSON API

The current state is reported under the following key:

```json
{ "PresenceSwitch": { "present": true } }
```

## Installation

Add `PresenceSwitch` to `custom_usermods` in your `platformio_override.ini`:

```ini
custom_usermods = PresenceSwitch
```
