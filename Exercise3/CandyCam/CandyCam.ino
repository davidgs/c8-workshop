#include <base64.h>

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
  pinMode(LED, OUTPUT);
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

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("Camera init failed with error 0x%x", err);
    flashError();
  }

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
  WiFiManagerParameter camunda_cloud_server("c8_server", "ZeeBe Address", "YOUR_SERVER.bru-2.zeebe.camunda.io:443", 100, " ");
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
void loop() {
  if(digitalRead(RESET_BTN) == HIGH){
    debugprintln("Resetting!!");
    //reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(1000);
  }
  if (digitalRead(SHUTTER) == HIGH) {
    digitalWrite(FLASH_BULB, HIGH);
    debugprint("Shutter Pressed");
    delay(2000);
    digitalWrite(FLASH_BULB, LOW);
    camera_fb_t *fb = NULL;
    delay(1000);
    fb = esp_camera_fb_get();
    if (!fb)
    {
      Serial.println("Camera capture failed");
      digitalWrite(GREENLED, LOW);
      int x = 10;
      while (x > 0)
      {
        digitalWrite(REDLED, HIGH);
        delay(500);
        digitalWrite(REDLED, LOW);
        x--;
      }
      digitalWrite(GREENLED, HIGH);
      return;
    }
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

  //   char inputString[] = "Base64EncodeExample";
  // int inputStringLength = strlen(inputString);

  // Serial.print("Input string is:\t");
  // Serial.println(inputString);

  // Serial.println();

  // int encodedLength = Base64.encodedLength(inputStringLength);
  // char encodedString[encodedLength];
  // Base64.encode(encodedString, inputString, inputStringLength);
  // Serial.print("Encoded string is:\t");
  // Serial.println(encodedString);

  //     char inputString[] = "Base64EncodeExample";
      
      // int encodedLength = base64_encodedLength(fb->len);
	// String encoded = base64::encode(fb->buf, fb->len);
  // DynamicJsonDocument doc(65536);
  char *upload;
  int len = sprintf(upload, "{\"zeebeClientID\": \"wROQIC_haG_T6932iWZYsFxwuSIbR~UG\", \"zeebeClientSecret\": \"UD5aifgUg9RXJ00.Dy3sIKGG2Qj1lqPJD7-P~Oj5CgysuSjYF.XS1scmEh_Kr2Tg\", \"zeeBeAddress\": \"b5161a28-2fd3-4879-a99e-60f7478ad3d5.bru-2.zeebe.camunda.io:443\", \"processID\": \"TestScriptWorker\", \"variables\": {\"length\": %d,  \"image\": \"", fb->len);
	// doc["zeebeClientID"] = "wROQIC_haG_T6932iWZYsFxwuSIbR~UG";
  // doc["zeebeClientSecret"] = "UD5aifgUg9RXJ00.Dy3sIKGG2Qj1lqPJD7-P~Oj5CgysuSjYF.XS1scmEh_Kr2Tg";
  // doc["zeeBeAddress"] = "b5161a28-2fd3-4879-a99e-60f7478ad3d5.bru-2.zeebe.camunda.io:443";
  // doc["processID"] = "TestScriptWorker";
  // JsonObject variables = doc.createNestedObject("variables");
  // variables["image"] = fb->buf;
  // variables["length"] = fb->len;
      // Serial.print("Length: ");
      // Serial.println(len);
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
    WiFiClientSecure *client = new WiFiClientSecure;
    if(client) {
      client->setCACert(ServerCert);  
    }
    
    // debugprint("Overflowed: ");
    // debugprintln(doc.overflowed());
    // debugprint("Json Doc: ");
    // size_t serialSize = serializeJson(doc, Serial);
    // char *buff;
    // debugprint("\nJson Size: ");
    // debugprintln(serialSize);
    // serializeJson(doc, buff, serialSize);
    // debugprintln();
    debugprintln("Posting ...");
    client->connect("davidgs.com", 5050);
    int tries = 0;
    while (!client->connected()) {
      Serial.printf("*** Can't connect. ***\n-------\n");
      delay(500);
      debugprint(".");
      client->connect("davidgs.com", 5050);
      tries++;
      if(tries > 10){
        return;
      }
    }
    debugprintln("Connected!");
    // char *start;
    // sprintf(start, "POST /CreateInstance HTTP/1.0\r\nHost: login.cloud.camunda.io\r\nUser-Agent: ESP32-CAM\r\nContent-Type: application/json\r\nAccept-encoding: *\r\n");
    // char *final;
    // int l = sizeof(start) + len + 10;
    // int foo = sprintf(final, "%sContent-Length: %d\r\n\r\n", start, l);
    // client->print( final);
    // client->print(upload);
    // client->printf("%s\"}}\r\n", fb->buf);
    // client->print("\"}}\r\n");
    // client->print("Host: login.cloud.camunda.io\r\n");
    // client->print("User-Agent: ESP32-CAM\r\n");
    // client->print("Content-Length: ");
    // client->print(pBuff.length() + encoded.length());
    // client->print("\r\n");
    // client->print("Content-Type: application/json\r\n");
    // client->print("Accept-encoding: *\r\n");
    //client->print(pBuff);    
    // debugprintln(pBuff.length() + encoded.length());
  //   String in_buffer = "";
  //   uint32_t to = millis() + 10000;
  //   if (client->connected()) {
  //     debugprintln("Reading response ...");
  //     do {
  //       int avail = client->available();
  //       if(avail > 0){
  //         debugprintln("Data available!");
  //         break;
  //       }
  //       debugprint(".");
  //       delay(500);
  //     } while (millis() < to);
  //     debugprintln();
  //     to = millis() + 5000;
  //     do {
  //       char tmp[512];
  //       memset(tmp, 0, 512);
  //       int rlen = client->read((uint8_t*)tmp, sizeof(tmp) - 1);
  //       if (rlen < 0) {
  //         break;
  //       }
  //       debugprint(tmp);
  //       in_buffer += tmp;
  //     } while (millis() < to);
  //     debugprintln();
  //     debugprintln("Finished reading");
  //   }
  //   client->stop();
  //   Serial.printf("\nDone!\n-------\n\n");
  // // char input[MAX_INPUT_LENGTH];
  //   debugprintln("Read in: ");
  //   debugprintln(in_buffer);
  
  //   in_buffer = in_buffer.substring(in_buffer.indexOf("{"), in_buffer.length());
  //   debugprintln();
  //   debugprintln("Just JSON: ");
  //   debugprintln(in_buffer); 
        //      // Make a HTTP request:
        //
        String start_request = "";
        String end_request = "";
        start_request = start_request +
                        "\n--AaB03x\n" +
                        "Content-Disposition: form-data; name=\"uploadfile\"; filename=foo.png\"\n" +
                        "Content-Transfer-Encoding: binary\n\n";
        debugprint("First Part: ");
        debugprintln(start_request);
        String midRequest = "";
        midRequest += "\n--AaB03x\nContent-Disposition: form-data; foo=\"bar\"; \nContent-Transfer-Encoding: text\n\n";
        end_request = end_request + "\n--AaB03x--\n";
        uint16_t full_length;
        debugprintln(midRequest);
        debugprintln(end_request);
        full_length = start_request.length() + fb->len + midRequest.length() + end_request.length();
        debugprint("Length: ");
        debugprintln(full_length);
        client->println("POST /CreateInstance HTTP/1.1");
        client->println("Host: example.com");
        client->println("User-Agent: ESP32");
        client->println("Content-Type: multipart/form boundary=--AaB03x");
        client->print("Content-Length: ");
        client->println(full_length);
        client->println("\r\n");
        // client->println(start_string);
        client->print(start_request);
        const int bufSize = 2048;
        uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n=0; n<fbLen; n=n+1024) {
      if (n+1024 < fbLen) {
        client->write(fbBuf, 1024);
        fbBuf += 1024;
      }
      else if (fbLen%1024>0) {
        size_t remainder = fbLen%1024;
        client->write(fbBuf, remainder);
      }
    }   
        // byte clientBuf[bufSize];
        // int clientCount = 0;
        // while (file.available())
        // {
        //   clientBuf[clientCount] = file.read();
        //   clientCount++;
        //   if (clientCount > (bufSize - 1))
        //   {
        //     client.write((const uint8_t *)clientBuf, bufSize);
        //     // Serial.print((char *)clientBuf);
        //     clientCount = 0;
        //   }
        // }
        // if (clientCount > 0)
        // {
        //   client.write((const uint8_t *)clientBuf, clientCount);
        //   // Serial.print((char *)clientBuf);
        // }
        client->print(midRequest);
        client->print(end_request);
        client->stop();
        Serial.println("Done!");

        esp_camera_fb_return(fb);
  //     }
    }
}
