# PresenceSwitch

Turns WLED off automatically when none of a user-selected list of network devices (e.g. phones)
are present, and turns it back on as soon as any of them reappear on the Wi-Fi network.

**ESP32 only.** Device presence is checked with ICMP ping via the ESP-IDF `ping_sock` API. On
ESP8266 the usermod compiles but always reports "absent" (no ping engine implemented).

## How it works

- Shortly after WLED connects to Wi-Fi, and whenever triggered via the JSON API, the usermod
  scans the local `/24` subnet (`x.x.x.1` - `x.x.x.254`, based on the device's own IP) by pinging
  every host address once.
- Devices that answer are listed as checkboxes on the **Usermod Settings** page ("Config" ->
  "Usermods"). Tick the ones you want WLED to watch, then hit **Save**.
- Every `checkIntervalSec` seconds, only the watched devices are pinged (not the whole subnet).
- If none of the watched devices have answered for `offDelayMin` minutes, WLED is switched off
  (brightness set to 0). As soon as one of them answers again, WLED restores the previous
  brightness.

Devices are identified purely by **IPv4 address**, not MAC address. If your router doesn't hand
out the same IP to a device every time, configure a DHCP reservation for the devices you want to
watch so their address stays stable across reboots/sleep.

## Settings

| Setting                                          | Description                                                                                        |
| ------------------------------------------------ | -------------------------------------------------------------------------------------------------- |
| `enabled`                                        | enable/disable the usermod                                                                         |
| `offDelayMin`                                    | minutes with no watched device present before switching off (`0` = never switch off automatically) |
| `checkIntervalSec`                               | how often the watched devices are (re-)pinged                                                      |
| `ip_a_b_c_d` (one per discovered/watched device) | check to watch this device's presence                                                              |

## JSON API

Trigger a new subnet scan (e.g. after connecting a new device to your Wi-Fi) by sending:

```json
{ "PresenceSwitch": { "scan": true } }
```

to `/json/state`. The current state is reported back under the same key:

```json
{ "PresenceSwitch": { "present": true, "scanning": false } }
```

## Installation

Add `PresenceSwitch` to `custom_usermods` in your `platformio_override.ini`:

```ini
custom_usermods = PresenceSwitch
```
