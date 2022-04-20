
#include "esp_camera.h"
#include <HTTPClient.h>
// #include "FS.h"               // SD Card ESP32
// #include "SD_MMC.h"           // SD Card ESP32
#include "soc/soc.h"          // Disable brownout problems
#include "soc/rtc_cntl_reg.h" // Disable brownout problems
#include "driver/rtc_io.h"
#include <EEPROM.h> // read and write from flash memory


#define debug 

#include <WiFiClientSecure.h>
#include <ssl_client.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <WiFiClientSecure.h>
#include <WiFiMulti.h>
#include <ArduinoJson.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <Ticker.h>
Ticker ticker;

#ifndef LED_BUILTIN
#define LED_BUILTIN 33 // ESP32 DOES NOT DEFINE LED_BUILTIN
#endif
#define LED 33
#ifdef debug
  #define debugprint(x) Serial.print(x)
  #define debugprintln(x) Serial.println(x)
  #define debugprintF(x) Serial.print(F(x))
#else
  #define debugprint(x)
  #define debugprintF(x)
#endif

//
// WARNING!!! Make sure that you have either selected ESP32 Wrover Module,
//            or another board which has PSRAM enabled
//

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

#define SERIAL_TX 1
#define SERIAL_RX 3

#define FLASH_BULB 4

#define RESET_BTN 12
#define SHUTTER 13


struct Settings {
  char c8_server[100] = "https://YOUR_SERVER.bru-2.zeebe.camunda.io";
  char c8_auth[50] = "https://login.cloud.camunda.io/oauth/token";
  char c8_client_id[50] = "YOUR_CLIENT_ID";
  char c8_client_secret[80] = "YOUR_CLIENT_SECRET";
  char c8_process_id[50] = "YOUR_PROCESS_ID";
} sett;

#define EEPROM_SIZE sizeof(Settings)
#define REDLED 15
#define GREENLED 14
//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  debugprintln("Entered config mode");
  debugprintln(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  debugprintln(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}

// Not sure if WiFiClientSecure checks the validity date of the certificate.
// Setting clock just to be sure...
void setClock() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  debugprintF("Waiting for NTP time sync: ");
  time_t nowSecs = time(nullptr);
  while (nowSecs < 8 * 3600 * 2) {
    delay(500);
    debugprintF(".");
    yield();
    nowSecs = time(nullptr);
  }

  debugprintln();
  struct tm timeinfo;
  gmtime_r(&nowSecs, &timeinfo);
  debugprintF("Current time: ");
  debugprint(asctime(&timeinfo));
}

void tick()
{
  //toggle state
  digitalWrite(LED, !digitalRead(LED));     // set pin to the opposite state
}
int pictureNumber = 0;
void setup()
{
  #ifdef debug
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  #endif

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  // Serial.setDebugOutput(true);
  pinMode(FLASH_BULB, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RESET_BTN, INPUT);
  pinMode(SHUTTER, INPUT);
  pinMode(REDLED, OUTPUT);
  pinMode(GREENLED, OUTPUT);
  digitalWrite(REDLED, LOW);
  digitalWrite(GREENLED, LOW);
  // start ticker with 0.5 because we start in AP mode and try to connect
  ticker.attach(0.6, tick);
  int z = 0;
  while (z < 20)
  {
    digitalWrite(GREENLED, HIGH);
    delay(250);
    digitalWrite(GREENLED, LOW);
    digitalWrite(REDLED, HIGH);
    delay(250);
    digitalWrite(REDLED, LOW);
    z++;
  }
  //digitalWrite(lampledPin, HIGH);
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  //init with high specs to pre-allocate larger buffers
  if (psramFound())
  {
    //  Serial.println("Getting XGA Images");
    config.frame_size = FRAMESIZE_XGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  }
  else
  {
    Serial.println("Stuck with SVGA");
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("Camera init failed with error 0x%x", err);
    flashError();
  }
  // if (!SD_MMC.begin())
  // {
  //   Serial.println("SD Card Mount Failed");
  //   flashError();
  // }

  // uint8_t cardType = SD_MMC.cardType();
  // if (cardType == CARD_NONE)
  // {
  //   Serial.println("No SD Card attached");
  //   flashError();
  // }

  sensor_t *s = esp_camera_sensor_get();
  //initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID)
  {
    s->set_vflip(s, 1);       //flip it back
    s->set_brightness(s, 1);  //up the blightness just a bit
    s->set_saturation(s, -2); //lower the saturation
  }
  //drop down frame size for higher initial frame rate
  s->set_framesize(s, FRAMESIZE_XGA);

#if defined(CAMERA_MODEL_M5STACK_WIDE)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wm;
  //reset settings - for testing
  if(digitalRead(RESET_BTN) == HIGH){
    wm.resetSettings();
    ticker.detach();

  }
  EEPROM.begin( 512 );
  EEPROM.get(0, sett);
  Serial.println("Settings loaded");
  Serial.print(sett.c8_server);

  WiFiManagerParameter camunda_auth_server("c8_auth_server", "ZeeBe Auth Server", "https://login.cloud.camunda.io/oauth/token", 50, " ");
  wm.addParameter(&camunda_auth_server);
  WiFiManagerParameter camunda_cloud_server("c8_server", "ZeeBe Address", "https://YOUR_SERVER.bru-2.zeebe.camunda.io", 100, " ");
  wm.addParameter(&camunda_cloud_server);
  WiFiManagerParameter camunda_client_id("c8_client_id", "ZeeBe Client ID", "YOUR_CLIENT_ID", 50, " ");
  wm.addParameter(&camunda_client_id);
  WiFiManagerParameter camunda_client_secret("c8_client_secret", "ZeeBe Client Secret", "YOUR_CLIENT_SECRET", 80, " ");
  wm.addParameter(&camunda_client_secret);
  WiFiManagerParameter camunda_process_id("c8_process_id", "Camunda Process ID", "YOUR_PROCESS_ID", 50, " ");
  wm.addParameter(&camunda_process_id);
  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wm.setAPCallback(configModeCallback);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wm.autoConnect()) {
    debugprintln("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(1000);
  }

  //if you get here you have connected to the WiFi
  debugprintln("connected...yay :)");
  
  setClock();
  // ticker.detach();
  sett.c8_server[99] = '\0';
  strncpy(sett.c8_server, camunda_cloud_server.getValue(), 100);
  sett.c8_client_id[49] = '\0';
  strncpy(sett.c8_client_id, camunda_client_id.getValue(), 50);
  sett.c8_auth[49] = '\0';
  strncpy(sett.c8_auth, camunda_auth_server.getValue(), 50);
  sett.c8_client_secret[79] = '\0';
  strncpy(sett.c8_client_secret, camunda_client_secret.getValue(), 80);
  sett.c8_process_id[49] = '\0';
  strncpy(sett.c8_process_id, camunda_process_id.getValue(), 50);

  debugprint("ZeeBe Address: \t");
  debugprintln(sett.c8_server);
  debugprint("ZeeBe Client ID: \t");
  debugprintln(sett.c8_client_id);
  debugprint("ZeeBe Auth Server: \t");
  debugprintln(sett.c8_auth);
  debugprint("ZeeBe Client Secret: \t");
  debugprintln(sett.c8_client_secret);
  debugprint("Camunda Process ID: ");
  debugprintln(sett.c8_process_id);
  debugprintln();  
  //startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");

}

void flashError()
{
  while (1)
  {
    digitalWrite(REDLED, HIGH);
    delay(500);
    digitalWrite(REDLED, LOW);
  }
}
void loop()
{
  if(digitalRead(RESET_BTN) == HIGH){
    debugprintln("Resetting!!");
    //reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(1000);
  }
  if (digitalRead(SHUTTER) == HIGH)
  {

    digitalWrite(FLASH_BULB, HIGH);
    debugprint("Shutter Pressed");
    delay(2000);
    digitalWrite(FLASH_BULB, LOW);
  //   camera_fb_t *fb = NULL;
  //   delay(1000);
  //   fb = esp_camera_fb_get();
  //   if (!fb)
  //   {
  //     Serial.println("Camera capture failed");
  //     digitalWrite(GREENLED, LOW);
  //     int x = 10;
  //     while (x > 0)
  //     {
  //       digitalWrite(REDLED, HIGH);
  //       delay(500);
  //       digitalWrite(REDLED, LOW);
  //       x--;
  //     }
  //     digitalWrite(GREENLED, HIGH);
  //     return;
  //   }
  //   String path = "/picture" + String(pictureNumber) + ".jpg";
  //   fs::FS &fs = SD_MMC;
  //   // Serial.printf("Picture file name: %s\n", path.c_str());
  //   while (fs.exists(path.c_str()))
  //   {
  //     Serial.printf("File %s exists, incrementing...", path.c_str());
  //     pictureNumber++;
  //     path = "/picture" + String(pictureNumber) + ".jpg";
  //   }
  //   File file = fs.open(path.c_str(), FILE_WRITE);
  //   if (!file)
  //   {
  //     Serial.println("Failed to open file in writing mode");
  //   }
  //   else
  //   {
  //     file.write(fb->buf, fb->len); // payload (image), payload length
  //     Serial.printf("Saved file to path: %s\n", path.c_str());
  //     EEPROM.write(0, pictureNumber);
  //     EEPROM.commit();
  //   }
  //   file.close();
  //   digitalWrite(4, LOW);

  //   file = fs.open(path.c_str(), FILE_READ);
  //   String fileName = file.name();
  //   String fileSize = String(file.size());

  //   if (file)
  //   {
  //     byte server[] = {192, 168, 1, 213};
  //     if (client.connect(server, 9090))
  //     {
  //       Serial.println(F("connected to server"));

  //       //      // Make a HTTP request:
  //       //
  //       String start_request = "";
  //       String end_request = "";
  //       start_request = start_request +
  //                       "\n--AaB03x\n" +
  //                       "Content-Disposition: form-data; name=\"uploadfile\"; filename=\"" + file.name() + "\"\n" +
  //                       "Content-Transfer-Encoding: binary\n\n";
  //       String midRequest = "";
  //       midRequest += "\n--AaB03x\nContent-Disposition: form-data; foo=\"bar\"; \nContent-Transfer-Encoding: text\n\n";
  //       end_request = end_request + "\n--AaB03x--\n";
  //       uint16_t full_length;
  //       full_length = start_request.length() + file.size() + midRequest.length() + end_request.length();
  //       Serial.println("Connected ok!");
  //       client.println("POST /photo HTTP/1.1");
  //       client.println("Host: example.com");
  //       client.println("User-Agent: ESP32");
  //       client.println("Content-Type: multipart/form-data; boundary=AaB03x");
  //       client.print("Content-Length: ");
  //       client.println(full_length);
  //       client.print(start_request);
  //       const int bufSize = 2048;
  //       byte clientBuf[bufSize];
  //       int clientCount = 0;
  //       while (file.available())
  //       {
  //         clientBuf[clientCount] = file.read();
  //         clientCount++;
  //         if (clientCount > (bufSize - 1))
  //         {
  //           client.write((const uint8_t *)clientBuf, bufSize);
  //           // Serial.print((char *)clientBuf);
  //           clientCount = 0;
  //         }
  //       }
  //       if (clientCount > 0)
  //       {
  //         client.write((const uint8_t *)clientBuf, clientCount);
  //         // Serial.print((char *)clientBuf);
  //       }
  //       client.print(midRequest);
  //       client.print(end_request);
  //       client.stop();
  //       Serial.println("Done!");

  //       esp_camera_fb_return(fb);
  //     }
  //   }
  }
}
