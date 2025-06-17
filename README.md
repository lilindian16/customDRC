# Custom DRC for the Bit10 DSP

## Reverse Engineering

![Bit10 DRC PCB with screen removed](/Images/Boards/audisonDRCBoard.png)

### Schema

![Bit10 DRC schema](/Images/Schema/Bit10DRC_rev_eng.png)

### RS485 Bus Packet Capture

An RS485 decoder was tapped in parallel to the Bit 10 bus. Packets
were capture using a logic analyser and Sigrok with the Puleview frontend probing the TX and RX pins of the decoder

Pulseview UART Decoder Settings:

- Baudrate: 38,400
- Word Length: 8 bits
- Parity: Mark / Space (1 indicates address, 0 indicates data)
- Stop Bits: 1
- Bit Order: LSB first

![Trace view of RS485 packet using Pulseview](/Images/Traces/rs485_bus_capture.png)

### Packet Structure

| Receiver Address | Transmitter Address | Delimeter | Packet Length | Command | Values  |     Checksum     |
| :--------------: | :-----------------: | :-------: | :-----------: | :-----: | :-----: | :--------------: |
|      1 Byte      |       1 Byte        |   0x00    |    1 Byte     | 1 Byte  | n-bytes | 8-bit Modulo 256 |

> [!NOTE]
> Packet length includes the checksum

### Device Addresses

|   Device   | Address |
| :--------: | :-----: |
| Master MCU |  0x00   |
|   DSP IC   |  0x46   |
| PC Via USB |  0x5A   |
|    DRC     |  0x80   |

### Commands

|     Command      |    Value    |                Notes                |
| :--------------: | :---------: | :---------------------------------: |
|   Volume: 0x0A   | 0x00 - 0x78 |      Mute to -58dB (120 steps)      |
|  Balance: 0x0B   | 0x00 - 0x24 | 0x00 = full left, 0x24 = full right |
|   Fader: 0x0C    | 0x00 - 0x24 | 0x00 = full front, 0x24 = full rear |
| Sub Volume: 0x0D | 0x00 - 0x18 |            Mute to -12dB            |

## Hardware

The Custom DRC board interfaces with the Bit10 via the same 6-pin 2.54mm header connector. The Custom DRC
accepts user input from two rotary encoders with push-button input on both

|  Encoder  |   Function    |  Switch Press Function   |
| :-------: | :-----------: | :----------------------: |
| Encoder A | Master Volume | DSP Memory Select Toggle |
| Encoder B |  Sub Volume   |     Not Assigned Yet     |

> [!TIP]
> Functionality of each encoder can be changed with firmware

### Bare PCB

![Rev B PCB Front](/Images/Boards/RevB/revb_front_pcb.png)

![Rev B PCB Rear](/Images/Boards/RevB/revb_rear_pcb.png)

### Assembled Board

![Reb B PCB Assembled](/Images/Boards/RevB/revb_front_pop.png)

Boards can be programmed via TC2030 header or 2.54mm pin header (left unpopulated). Once programmed,
further updates can be handled via the OTA Update interface through the local webapp

### Pinout

|    GPIO     |     Function     |                         Note                         |
| :---------: | :--------------: | :--------------------------------------------------: |
| GPIO_NUM_18 |     RS485 TX     |                                                      |
| GPIO_NUM_27 |     RS485 RX     | Must be in RTC power domain to allow wake from sleep |
| GPIO_NUM_19 |   RS485 TX EN    |                                                      |
| GPIO_NUM_35 |   Encoder 1 A    |   ESP32 has no internal pull-up / down on this pin   |
| GPIO_NUM_32 |   Encoder 1 B    |                                                      |
| GPIO_NUM_33 | Encoder 1 Switch |                                                      |
| GPIO_NUM_36 |   Encoder 2 A    |   ESP32 has no internal pull-up / down on this pin   |
| GPIO_NUM_39 |   Encoder 2 B    |   ESP32 has no internal pull-up / down on this pin   |
| GPIO_NUM_34 | Encoder 2 Switch |   ESP32 has no internal pull-up / down on this pin   |
| GPIO_NUM_5  |   DSP Power EN   |          Active high output to keep DSP on           |
| GPIO_NUM_25 |     LED_PIN      |                                                      |

### Board Bringup - Scope Captures

![Encoder Pin B during rotation](/Images/Traces/enc_b_cw.png)

![Capture of button signal on single press](/Images/Traces/enc_button_press_single.png)

![Capture of button signal with double press](/Images/Traces/enc_button_double_press.png)

## Firmware

https://github.com/lilindian16/customDRC/tree/main/Firmware/AC_Link_Control

Built with PlatformIO VSCode entension

### PlatformIO ini file

```
    ; PlatformIO Project Configuration File
    ;
    ;   Build options: build flags, source filter
    ;   Upload options: custom upload port, speed and extra flags
    ;   Library options: dependencies, extra library storages
    ;   Advanced options: extra scripting
    ;
    ; Please visit documentation for the other options and examples
    ; https://docs.platformio.org/page/projectconf.html

    [env:ESP32-WROOM-32UE]
    platform = espressif32
    board = esp32dev
    framework = arduino
    board_build.f_cpu = 240000000L; Force MCU frequency 24MHz
    board_build.partitions = partitions_custom.csv; Use custom partition table
    upload_speed = 921600 ; Change upload speed
    monitor_speed = 115200 ; Change serial terminal speed
    build_flags = ; Extra build flags
        -DCORE_DEBUG_LEVEL=ARDUHAL_LOG_LEVEL_INFO
    extra_scripts =
        pre:buildscript_versioning_header.py    ; Auto versioning script
```

### Custom Partition Table

|   Name   | Type | Subtype  | Offset | Size  | Flags |
| :------: | :--: | :------: | :----: | :---: | :---: |
|   nvs    | data |   nvs    |  36K   |  20K  |       |
| otadata  | data |   ota    |  56K   |  8K   |       |
|   app0   | app  |  ota_0   |  64K   | 1900K |       |
|   app1   | app  |  ota_1   |        | 1900K |       |
| coredump | data | coredump |        |  64K  |       |

### Dependencies

- [ArduinoJson](https://arduinojson.org/)
- [ArduinoNvs](https://github.com/rpolitex/ArduinoNvs)
- [AsyncTCP](https://github.com/me-no-dev/AsyncTCP)
- [ESP32Encoder](https://github.com/madhephaestus/ESP32Encoder)
- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)
- [espsoftwareserial](https://github.com/plerup/espsoftwareserial)

### Custom DRC Source

https://github.com/lilindian16/customDRC/tree/main/Firmware/AC_Link_Control/lib/CustomDRC

---

CustomDRC

The _kernel_ that manages the controller. Loads previous DSP settings from NVS on bootup. Also handles
encoder button press callbacks and the LED output (event based)

---

AudisonLinkBus

The backend that drives comms between controller and DSP on RS485 bus. The RS485 protocol requires 9-bit UART (8 bit data with
1 bit to show if packet is the receiver address or data). The ESP32 Hardware UART module is unable to handle 9-bit packets.
Therefore, RX is handled with software serial. TX is handled with the ESP32 RMT peripheral (and a little bit of abuse). It has been re-purposed to
be used as a UART driver. This reduces the software overhead of using softwareserial for both TX and RX especially since we also have
to drive a webapp frontend with realtime updates

---

CustomDRCWebServer

Handles socket connection to webserver for front-end user interaction

---

DRCEncoder

Handles user input via two encoders. Encoder rotations trigger an interrupt which gives the required
sempahore to enable the encoder task to trigger volume / bass value updates

## Software

The GUI frontend software for the Custom DRC can be accessed via the local ESP32 webserver. Connect to the ESP32's AP (_SSID: Custom-DRC_) with the required password (_12345678_) - very secure, I know :) We will eventually add support for updating the WiFi credentials via the webapp. Head to 192.168.1.1 and you will be met with the webapp frontend

![Custom DRC Frontend (hosted locally on ESP32)](/Images/Software/CDRC_mainpage.png)

### Features

- Remote control of Bit10 audio settings via webapp
- Realtime updates of encoder inputs and button inputs
- Simple and quick OTA update via webapp

### Source

Custom DRC software source is based on vanilla HTML / CSS / JS. PicoCSS was used to create a
responsive interface that easily scales across all devices.

### Source Translation

The webapp source files are converted to C header files using a [python script](https://github.com/lilindian16/customDRC/blob/main/WebDesign/Mockup/convert_to_headers.py). This allows for easy inclusion of the software into the firmware build process for the ESP32. This means that further software updates are easily managed via OTA updates through the webapp interface

## Final Result

### Master Volume Adjustment - 0x00 (mute)

![Master Volume Adjust - 0x00 (mute)](/Images/Traces/volume_adjust_0x00_20240821_082051.png)

### Master Volume Adjustment - 0x3C (50% Attenuation)

![Master Volume Adjust - 0x3C (50% Attenuation)](/Images/Traces/volume_adjust_0x3C_20240821_081120.png)

### Master Volume Adjustment - 0x78 (Max Volume)

![Master Volume Adjust - 0x78 (Max Volume)](/Images/Traces/volume_adjust_0x78_20240821_080611.png)
