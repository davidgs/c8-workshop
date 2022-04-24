
#include <base64.h>
#include "esp_camera.h"
#include <HTTPClient.h>
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
// #include <ArduinoJson.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
// #include <Ticker.h>
// Ticker ticker;

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

struct Settings
{
  char c8_server[100] = "YOUR_SERVER.bru-2.zeebe.camunda.io:443";
  char c8_auth[50] = "https://login.cloud.camunda.io/oauth/token";
  char c8_client_id[50] = "YOUR_CLIENT_ID";
  char c8_client_secret[80] = "YOUR_CLIENT_SECRET";
  char c8_process_id[50] = "YOUR_PROCESS_ID";
} sett;

#define EEPROM_SIZE sizeof(Settings)

// gets called when WiFiManager enters configuration mode
void configModeCallback(WiFiManager *myWiFiManager)
{
  debugprintln("Entered config mode");
  debugprintln(WiFi.softAPIP());
  // if you used auto generated SSID, print it
  debugprintln(myWiFiManager->getConfigPortalSSID());
  // entered config mode, make led toggle faster
  // ticker.attach(0.2, tick);
}

 const char ServerCert[] PROGMEM = R"EOF(
 -----BEGIN CERTIFICATE-----
MIIFgTCCBGmgAwIBAgIQOXJEOvkit1HX02wQ3TE1lTANBgkqhkiG9w0BAQwFADB7
MQswCQYDVQQGEwJHQjEbMBkGA1UECAwSR3JlYXRlciBNYW5jaGVzdGVyMRAwDgYD
VQQHDAdTYWxmb3JkMRowGAYDVQQKDBFDb21vZG8gQ0EgTGltaXRlZDEhMB8GA1UE
AwwYQUFBIENlcnRpZmljYXRlIFNlcnZpY2VzMB4XDTE5MDMxMjAwMDAwMFoXDTI4
MTIzMTIzNTk1OVowgYgxCzAJBgNVBAYTAlVTMRMwEQYDVQQIEwpOZXcgSmVyc2V5
MRQwEgYDVQQHEwtKZXJzZXkgQ2l0eTEeMBwGA1UEChMVVGhlIFVTRVJUUlVTVCBO
ZXR3b3JrMS4wLAYDVQQDEyVVU0VSVHJ1c3QgUlNBIENlcnRpZmljYXRpb24gQXV0
aG9yaXR5MIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEAgBJlFzYOw9sI
s9CsVw127c0n00ytUINh4qogTQktZAnczomfzD2p7PbPwdzx07HWezcoEStH2jnG
vDoZtF+mvX2do2NCtnbyqTsrkfjib9DsFiCQCT7i6HTJGLSR1GJk23+jBvGIGGqQ
Ijy8/hPwhxR79uQfjtTkUcYRZ0YIUcuGFFQ/vDP+fmyc/xadGL1RjjWmp2bIcmfb
IWax1Jt4A8BQOujM8Ny8nkz+rwWWNR9XWrf/zvk9tyy29lTdyOcSOk2uTIq3XJq0
tyA9yn8iNK5+O2hmAUTnAU5GU5szYPeUvlM3kHND8zLDU+/bqv50TmnHa4xgk97E
xwzf4TKuzJM7UXiVZ4vuPVb+DNBpDxsP8yUmazNt925H+nND5X4OpWaxKXwyhGNV
icQNwZNUMBkTrNN9N6frXTpsNVzbQdcS2qlJC9/YgIoJk2KOtWbPJYjNhLixP6Q5
D9kCnusSTJV882sFqV4Wg8y4Z+LoE53MW4LTTLPtW//e5XOsIzstAL81VXQJSdhJ
WBp/kjbmUZIO8yZ9HE0XvMnsQybQv0FfQKlERPSZ51eHnlAfV1SoPv10Yy+xUGUJ
5lhCLkMaTLTwJUdZ+gQek9QmRkpQgbLevni3/GcV4clXhB4PY9bpYrrWX1Uu6lzG
KAgEJTm4Diup8kyXHAc/DVL17e8vgg8CAwEAAaOB8jCB7zAfBgNVHSMEGDAWgBSg
EQojPpbxB+zirynvgqV/0DCktDAdBgNVHQ4EFgQUU3m/WqorSs9UgOHYm8Cd8rID
ZsswDgYDVR0PAQH/BAQDAgGGMA8GA1UdEwEB/wQFMAMBAf8wEQYDVR0gBAowCDAG
BgRVHSAAMEMGA1UdHwQ8MDowOKA2oDSGMmh0dHA6Ly9jcmwuY29tb2RvY2EuY29t
L0FBQUNlcnRpZmljYXRlU2VydmljZXMuY3JsMDQGCCsGAQUFBwEBBCgwJjAkBggr
BgEFBQcwAYYYaHR0cDovL29jc3AuY29tb2RvY2EuY29tMA0GCSqGSIb3DQEBDAUA
A4IBAQAYh1HcdCE9nIrgJ7cz0C7M7PDmy14R3iJvm3WOnnL+5Nb+qh+cli3vA0p+
rvSNb3I8QzvAP+u431yqqcau8vzY7qN7Q/aGNnwU4M309z/+3ri0ivCRlv79Q2R+
/czSAaF9ffgZGclCKxO/WIu6pKJmBHaIkU4MiRTOok3JMrO66BQavHHxW/BBC5gA
CiIDEOUMsfnNkjcZ7Tvx5Dq2+UUTJnWvu6rvP3t3O9LEApE9GQDTF1w52z97GA1F
zZOFli9d31kWTz9RvdVFGD/tSo7oBmF0Ixa1DVBzJ0RHfxBdiSprhTEUxOipakyA
vGp4z7h/jnZymQyd/teRCBaho1+V
-----END CERTIFICATE-----
 )EOF";

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

void tick() {
  // toggle state
  digitalWrite(LED, !digitalRead(LED)); // set pin to the opposite state
}

void setup() {
#ifdef debug
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
#endif

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  pinMode(LED, OUTPUT);
  pinMode(FLASH_BULB, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RESET_BTN, INPUT);
  pinMode(SHUTTER, INPUT);
  // start ticker with 0.5 because we start in AP mode and try to connect
  // ticker.attach(0.6, tick);
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
  // init with high specs to pre-allocate larger buffers
  if (psramFound()) {
    config.frame_size = FRAMESIZE_XGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    Serial.println("Stuck with SVGA");
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    // flashError();
  }

  sensor_t *s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);       // flip it back
    s->set_brightness(s, 1);  // up the blightness just a bit
    s->set_saturation(s, -2); // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  s->set_framesize(s, FRAMESIZE_XGA);

#if defined(CAMERA_MODEL_M5STACK_WIDE)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  // WiFiManager
  // Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wm;
  // reset settings - for testing
  if (digitalRead(RESET_BTN) == HIGH) {
    wm.resetSettings();
    // ticker.detach();
  }
  EEPROM.begin(512);
  EEPROM.get(0, sett);
  Serial.println("Settings loaded");
  Serial.print(sett.c8_server);

  WiFiManagerParameter camunda_auth_server("c8_auth_server", "ZeeBe Auth Server", "https://login.cloud.camunda.io/oauth/token", 50, " ");
  wm.addParameter(&camunda_auth_server);
  WiFiManagerParameter camunda_cloud_server("c8_server", "ZeeBe Address", "YOUR_SERVER.bru-2.zeebe.camunda.io:443", 100, " ");
  wm.addParameter(&camunda_cloud_server);
  WiFiManagerParameter camunda_client_id("c8_client_id", "ZeeBe Client ID", "YOUR_CLIENT_ID", 50, " ");
  wm.addParameter(&camunda_client_id);
  WiFiManagerParameter camunda_client_secret("c8_client_secret", "ZeeBe Client Secret", "YOUR_CLIENT_SECRET", 80, " ");
  wm.addParameter(&camunda_client_secret);
  WiFiManagerParameter camunda_process_id("c8_process_id", "Camunda Process ID", "YOUR_PROCESS_ID", 50, " ");
  wm.addParameter(&camunda_process_id);
  // set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wm.setAPCallback(configModeCallback);
  // fetches ssid and pass and tries to connect
  // if it does not connect it starts an access point with the specified name
  // here  "AutoConnectAP"
  // and goes into a blocking loop awaiting configuration
  if (!wm.autoConnect()) {
    debugprintln("failed to connect and hit timeout");
    // reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(1000);
  }

  // if you get here you have connected to the WiFi
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
  // startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
}


void loop() {
  if (digitalRead(RESET_BTN) == HIGH) {
    debugprintln("Resetting!!");
    // reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(1000);
  }
  if (digitalRead(SHUTTER) == HIGH) {
    digitalWrite(FLASH_BULB, HIGH);
    debugprint("Shutter Pressed");
    delay(500);
    camera_fb_t *fb = NULL;
    fb = esp_camera_fb_get();
    delay(1000);
    digitalWrite(FLASH_BULB, LOW);
    if (!fb) {
      Serial.println("Camera capture failed");
      return;
    }
    debugprintln("picture taken");
    debugprintln("setting up credentials");
    char upload[512];
    int len = sprintf(upload, "{\"zeebeClientID\": \"%s\", \"zeebeClientSecret\": \"%s\", \"zeeBeAddress\": \"%s\", \"processID\": \"%s\", \"variables\": {\"length\": %d,  \"image\": \"foo.jpg\"}}", sett.c8_client_id, sett.c8_client_secret, sett.c8_server, sett.c8_process_id, fb->len);
    debugprintln("Credentials set: ");
    debugprintln(upload);
      debugprintln("setting CA Cert");
     WiFiClientSecure *client = new WiFiClientSecure;
     if (client){
       client->setCACert(ServerCert);
     }
    debugprintln("cert set");
    debugprintln("Posting ...");
     client->connect("davidgs.com", 5050);
     int tries = 0;
     while (!client->connected()) {
       Serial.printf("*** Can't connect. ***\n-------\n");
       delay(500);
       debugprint(".");
       client->connect("davidgs.com", 5050);
       tries++;
       if (tries > 10) {
         return;
       }
     }
    debugprintln("Connected!");
    size_t size = fb->len;
//    unsigned char[size] base_enc_buff;
//    int enc_len = mbedtls_base64_encode(&base_enc_buff, size,
    String buffer = base64::encode((uint8_t *)fb->buf, fb->len);
    Serial.print("Length of encoded message: ");
    Serial.println(buffer.length());
    esp_camera_fb_return(fb);
    // char start_request[] = "\n--AaB03x\nContent-Disposition: form-data; name=\"credentials\"; \nContent-Transfer-Encoding: text\r\n";
    // char mid_request[] = "\n--AaB03x\nContent-Disposition: form-data; name=\"image_file\"; filename=\"foo.jpg\"\r\nContent-Type: image/jpeg\r\n\r\nContent-Transfer-Encoding: base64\n\n";
    // char end_request[] = "\n--AaB03x--\n";
    // debugprint("First Part: ");
    // debugprintln(start_request);
    // debugprintln(upload);
    // debugprintln(mid_request);
    // debugprintln(buffer);
    // debugprintln(end_request);
    size_t full_length;
    full_length = 280 + buffer.length() +128;
    debugprint("Length: ");
    debugprintln(full_length);
     client->println("POST /CreateInstance HTTP/1.1");
     debugprintln("POST /CreateInstance HTTP/1.1");
     client->println("Host: example.com");
     debugprintln("Host: example.com");
     client->println("User-Agent: ESP32");
     debugprintln("User-Agent: ESP32");
     client->println("Content-Type: multipart/form-data; boundary=--AaB03x");
     debugprintln("Content-Type: multipart/form-data; boundary=--AaB03x");
     client->print("Content-Length: ");
     debugprint("Content-Length: ");
     client->println(full_length);
     debugprintln(full_length);
     client->println("\r\n");
     debugprintln("\r\n");
     client->println("\n--AaB03x\nContent-Disposition: form-data; name=\"credentials\"; \nContent-Transfer-Encoding: text\r\n");
     debugprintln("\n--AaB03x\nContent-Disposition: form-data; name=\"credentials\"; \nContent-Transfer-Encoding: text\r\n");
     client->println(upload);
     debugprintln(upload);
     client->println("\n--AaB03x\nContent-Disposition: form-data; name=\"image_file\"; filename=\"foo.jpg\"\r\nContent-Type: image/jpeg\r\n\r\nContent-Transfer-Encoding: base64\n\n");
     debugprintln("\n--AaB03x\nContent-Disposition: form-data; name=\"image_file\"; filename=\"foo.jpg\"\r\nContent-Type: image/jpeg\r\n\r\nContent-Transfer-Encoding: base64\n\n");
      client->println(buffer);
      debugprintln(buffer);
     client->println("\n--AaB03x--\n");
     debugprintln("\n--AaB03x--\n");
//    const int bufSize = 2048;
//    char payload[full_length + 10];
//    debugprintln("assembling payload ...");
//    sprintf(payload, "\n--AaB03x\nContent-Disposition: form-data; name=\"credentials\"; \nContent-Transfer-Encoding: text\r\n%s\r\n\n--AaB03x\nContent-Disposition: form-data; name=\"image_file\"; filename=\"foo.jpg\"\r\nContent-Type: image/jpeg\r\n\r\nContent-Transfer-Encoding: base64\n\n\r\n%s\r\n\n--AaB03x--\n\n", upload, buffer);
//    // size_t fbLen = buffer.length();
//    HTTPClient http;
//    http.begin("https://davidgs.com:5050/CreateInstance");
//    http.addHeader("Content-Type",  "multipart/form-data; boundary=--AaB03x");     
//    int httpResponseCode = http.POST(payload);
//
//    if(httpResponseCode>0){
//      Serial.print(httpResponseCode);
//      Serial.print(" Returned String: ");
//      Serial.println(http.getString());
//    } else {      
//      Serial.print("POST Error: ");
//      Serial.print(httpResponseCode);
//    }      
    // for (size_t n = 0; n < fbLen; n = n + 1024) {
    //   if (n + 1024 < fbLen) {
    //     client->write(encoded, 1024);
    //     encoded += 1024;
    //   }
    //   else if (fbLen % 1024 > 0) {
    //     size_t remainder = fbLen % 1024;
    //     client->write(encoded, remainder);
    //   }
    // }
    // client->print(end_request);
     client->stop();
    Serial.println("Done!");
    
    //     }
  }
}
