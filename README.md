# esp32s3_T-Dispay_test

Basic application synced with Internet via NTP to test T-Display LilyGo.

## Build and Upload

Please install first [PlatformIO](http://platformio.org/) open source ecosystem for IoT development compatible with **Arduino** IDE and its command line tools (Windows, MacOs and Linux). Also, you may need to install [git](http://git-scm.com/) in your system. After install that you should have the command line tools of PlatformIO. Please test it with `pio --help`. Then please run the next command for build and upload the firmware:

```bash
pio run --target upload
```

## Configuration

Using your USB cable, connect it and run:

```bash
pio device monitor
```


