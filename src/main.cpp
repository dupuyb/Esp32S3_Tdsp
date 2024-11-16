#include "Arduino.h"
#include "driver/rtc_io.h" // brownout detector
// Other
#include "OneButton.h"
#include "WiFi.h"
#include "gui.h"
#include "lvgl.h"
#include "pin_config.h"
#include "sntp.h"
#include "time.h"
// PureThermal
#include "LeptonFLiR.h"
#include <SPI.h>
#include <Wire.h>
// test touch screen version
// #include "TouchLib.h"

// WebFrame
#define DEBUG_FRAMEWEB // Debug frame before header
#undef DEBUG_ESP_HTTP_SERVER
#include "FrameWeb.h"
FrameWeb frame;

// Purethermal FLIR LEPTON IR module 160x120 pixels.
// LeptonFLiR ESP32XLib uses  pins 19=MISO, 23=MOSI, 16=SCK  and 5=SS.
// LILYGO T-Display ESP32-53  pins 13=MISO, 11=MOSI, 12=SCLK and 10=CS0,
#ifdef LEPTON
const byte flirCSPin = 3;
LeptonFLiR flirController(Wire, flirCSPin); // Library using Wire and chip select pin D22 -> D3
#endif

// Frame & Wifi task
QueueHandle_t qDsp = xQueueCreate(5, sizeof(uint32_t));
const char *HOSTNAME = "Esp32-S3";
#define WIFI_TASK_PRIORITY 12 // Low numbers denote low priority tasks
TaskHandle_t wifiCxHandle = NULL;

OneButton button1(PIN_BUTTON_1, true);
OneButton button2(PIN_BUTTON_2, true);

// Time facilities
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;
struct tm timeinfo; // time struct
const char *ntpServer = "pool.ntp.org";
DataMsg dmsg;

// Frame option
void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {}
void configModeCallback(WiFiManager *myWiFiManager) {}
void saveConfigCallback() {}

void my_log_cb(const char *dsc)
{
  Serial.print(dsc);
}

// Time HH:MM.ss
String getTime()
{
  char temp[10];
  snprintf(temp, 10, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  return String(temp);
}

// Date as europeen format
String getDate(bool sh = false)
{
  char temp[20];
  if (sh)
    snprintf(temp, 20, "%02d/%02d/%04d", timeinfo.tm_mday, (timeinfo.tm_mon + 1), (1900 + timeinfo.tm_year));
  else
    snprintf(temp, 20, "%02d/%02d/%04d %02d:%02d:%02d", timeinfo.tm_mday, (timeinfo.tm_mon + 1), (1900 + timeinfo.tm_year), timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  return String(temp);
}

void IRAM_ATTR startingTask(void *pvParameter)
{
  char dVal[6] = "empty";
  LV_LOG_USER("start");
  // while (1) { lv_obj_pos
  xQueueReceive(qDsp, dVal, portMAX_DELAY);
  if (dVal[0] == 'w')
  {
    LV_LOG_USER("cmd=wifi");
    dmsg.msg = "Start AP";
    // Start framework lock in case off wifi is in AP
    if (WiFi.status() != WL_CONNECTED)
    {
      dmsg.msg = "A.P ?";
      frame.externalHtmlTools = "Specific home page is visible at :<a class='button' href='/index.html'>Main-Page</a>";
      frame.setup(HOSTNAME); // SPIFS, WIFI, OTA, Server stating...
      dmsg.msg = "Wait NTP";
      int lp = 0;
      while (timeinfo.tm_year < 100 && lp < 100)
      {
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); // init and get the time
        getLocalTime(&timeinfo, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
        dmsg.msg = "NTP...";
        lp++;
      }
      frame.saveConfiguration(frame.filename, frame.config);
    }
    dmsg.msg = "Time ok";
  }
  //   vTaskDelay( pdMS_TO_TICKS( 135 ) );
  //   taskYIELD();
  // }
  vTaskDelete(NULL); // auto destroy
  LV_LOG_USER("deleted");
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Start Esp32s3 test application");

  // TEST
  //  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
#ifdef LEPTON
  // Purethermal TEST 160x120
  Wire.begin();          // Wire must be started first
  Wire.setClock(1000000); // Supported baud rates are 100kHz, 400kHz, and 1000kHz
  SPI.begin();
  // Using memory allocation mode 80x60 8bpp and fahrenheit temperature mode
  flirController.init(LeptonFLiR_ImageStorageMode_80x60_8bpp, LeptonFLiR_TemperatureMode_Fahrenheit);
  // Setting use of AGC for histogram equalization (since we only have 8-bit per pixel data anyways)
  flirController.agc_setAGCEnabled(ENABLED);
  flirController.sys_setTelemetryEnabled(ENABLED); // Ensure telemetry is enabled

 // flirController.printModuleInfo();
#endif
  Serial.print("MISO=");
  Serial.println(MISO);
  Serial.print("MOSI=");
  Serial.println(MOSI);
  Serial.print("SCK=");
  Serial.println(SCK);
  Serial.print("SS=");
  Serial.println(SS);

  // Init LGVL
  Serial.println("Start LOGGER");
  lv_log_register_print_cb(my_log_cb);

  // LV_LOG_USER("Start LCD setup");
  lcd_setup();

  // Buttons functions
  button1.attachClick([]()
                      {
        pinMode(PIN_POWER_ON, OUTPUT);
        pinMode(PIN_LCD_BL, OUTPUT);
        digitalWrite(PIN_POWER_ON, LOW);
        digitalWrite(PIN_LCD_BL, LOW);
        esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BUTTON_2, 0); // 1 = High, 0 = Low
        esp_deep_sleep_start(); });
  button2.attachClick([]()
                      { gui_switch_page(); });

  LV_LOG_USER("Thread startingTask starting...");
  // Get Time
  xTaskCreate(&startingTask, "startingTask", 4096, NULL, WIFI_TASK_PRIORITY, &wifiCxHandle);

  // start Wifi
  xQueueSend(qDsp, "wifi", 0);

  // Start after SPIFS is opened
  delay(800);

  gui_pages();
}

// --------------------------------------------------------------
void updateTime(String tt)
{
  tt += "\n";
  tt += getDate(false);
  lv_msg_send(MSG_PAGE2, tt.c_str());
  gui_switch_clock(timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}

uint32_t last_tick = 0;
float fval;
int fpsec = 0;
int wifiLost = 0;
static volatile int wifiSt = -1;

void loop()
{
  // Call frame loop
  if (timeinfo.tm_year > 100)
    frame.loop();
  lv_timer_handler();
  button1.tick();
  button2.tick();

  // PIN_BAT_VOLT ADC_6db
  if (millis() - last_tick > 1000)
  {
    last_tick = millis();
    dmsg.count = fpsec;
    fpsec = 1;
    // New implementation
    getLocalTime(&timeinfo, 0);

    // Wifi status
    wifiSt = WiFi.status();
    if ((timeinfo.tm_min % 2 == 0) && timeinfo.tm_sec == 30)
    {
      if ((wifiSt != WL_CONNECTED))
      {
        wifiLost++;
        if (wifiLost == 2)
        {
          LV_LOG_USER("WiFi connection is lost. cnt:%d minutes", wifiLost);
          WiFi.disconnect();
        }
        if (wifiLost == 4)
        {
          if (WiFi.reconnect())
          {
            LV_LOG_USER("WiFi reconnect OK (%d).", wifiLost);
            wifiLost = 0;
          }
        }
        if (wifiLost > 10)
          wifiLost = 0;
      }
    } // Every 2'30

    updateTime(WiFi.localIP().toString());

    esp_chip_info_t t;
    esp_chip_info(&t);
    // Send table as TSV
    String text2 = ESP.getChipModel();
    text2 += '\t';
    text2 += ESP.getPsramSize() / 1024;
    text2 += " kB\t";
    text2 += ESP.getFlashChipSize() / 1024;
    text2 += " kB\t";
    text2 += WiFi.macAddress().c_str();
    text2 += '\t';
    text2 += WiFi.localIP().toString().c_str();
    text2 += '\t';
    text2 += getDate(false);
    text2 += '\t';
    text2 += "#0000ff Bruno #";
    text2 += '\t';
    lv_msg_send(MSG_TABLE, &text2);
  } // End second

  // more faster
  dmsg.raw = analogRead(PIN_BAT_VOLT);
  dmsg.volt = map(dmsg.raw, 0, 4095, 0, 3300);
  dmsg.value = (float)dmsg.volt / 1000.0;
  dmsg.fsin = 1650 + 1650 * sin(fval);
  lv_msg_send(MSG_PAGE1, &dmsg);
  fpsec++;
  fval = fval + 0.1;
}
