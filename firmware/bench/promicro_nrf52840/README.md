# Pro Micro nRF52840 headless bench

A separate USB-powered bench application for a bare Pro Micro/SuperMini-type
nRF52840 board. This reuses the shared BLE configuration protocol, settings,
flash artwork and QR/text/animation renderers. It does not drive external GPIO,
probe buses, capture audio, or claim a physical display. USB is its pairing-code
display. Normal prototype/Rev A firmware contains no USB passkey getter.

## Build

```powershell
./scripts/dev.ps1 build -App firmware/bench/promicro_nrf52840 -Board promicro_nrf52840/nrf52840/uf2 -BuildDir build/promicro-bench -Pristine
```

The SDK target uses the factory Adafruit/nice!nano UF2 bootloader layout with
S140 v6 reservation: code at0x26000, artwork0xa0000..0xe1fff, settings
0xec000..0xf3fff, bootloader0xf4000..0xfffff. There is no MCUboot or secondary
firmware slot in this bench image. The bootloader/SoftDevice are retained;
the application uses the SDK SoftDevice Controller, not the reserved S140 stack.
The complete Rev A image remains the full-feature size/layout reference.

Only deploy after identifying the actual board/bootloader. Do not copy Rev A
merged images onto this board. The compiled UF2 contains only the application
range. A matching serial DFU package can replace the app when no USB drive is
exposed; it must contain no SoftDevice/bootloader image and explicitly require
S140 6.1.1 (0x00B6), matching this candidate's0x26000 application offset.

## USB display and app test

With the test firmware running, identify its Zephyr USB serial port in Device
Manager (COM10 in the initial bench). Use the pinned SDK Python/pyserial:

```powershell
C:/ncs/toolchains/4f5b6ad6dd/opt/bin/python.exe scripts/bench/promicro_console.py --port COM10 --pair
```

1. Open Pixel Badge. Disconnect the DK if connected.
2. Select **Pixel Badge Bench**. If both boards advertise, select this name.
3. Enter the six-digit code shown in the computer terminal.
4. Refresh settings; apply a name, brightness and text/drawing. Check exact app
   readback. `s` in the terminal reports RAM-rendered frame CRC/mode/brightness.
5. Upload an animation, reconnect/refresh, then unplug/replug USB and verify
   settings/artwork plus bonded reconnection. Close the USB display before
   unplugging; restart it on the current COM port afterward.

In the terminal: `p` requests a fresh60-second pairing window, `s` requests
status, `q` quits. It must run interactively; redirected output is refused.
An optional `--status-file build/promicro-public-status.json` writes whitelisted
public runtime counters only, never the pairing code. Pairing still requires
LESC/MITM/security level4 and uses the same one-bond policy. Do not capture the
code terminal in logs/screenshots.

NFC status is **tag not detected**. URL/resync operations cannot pass without a
real tag; a failed URL transaction must restore the old value. Audio/motion
capabilities are absent. Brightness/mode/content changes validate configuration,
rendering and flash behavior only; there are no LEDs to observe.

This board can establish nRF52840 startup, single-core BLE/app behavior, settings/
artwork storage, reconnect and retention. It cannot validate the E73 antenna,
Rev A pins/clocks/power/charger, PDM acoustics, real IMU input, LED timing/current/
optics, or combined scan/audio/radio scheduling. Those remain board bring-up.

References: [pinned SDK board documentation](https://docs.zephyrproject.org/latest/boards/others/promicro_nrf52840/doc/index.html),
[factory bootloader protocol/layout](https://github.com/adafruit/Adafruit_nRF52_Bootloader).
