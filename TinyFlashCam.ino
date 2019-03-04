/**
 * Based on https://github.com/mic159/m5cam
 * 
 * Reads an 2image from the camera and servers it to a connected client.
 * Starts a webserver and starts streaming when a client connects
 * 
 * After the device has started change your computer's WiFi to m5cam 
 * and open the browser at http://192.168.4.1 for a stream
 * or http://192.168.4.1/camera for a still picture
 */
//#define CONFIG_ENABLE_TEST_PATTERN N
#define CONFIG_OV2640_SUPPORT y
//#define CONFIG_OV7725_SUPPORT y
#define CONFIG_ESP32_DEFAULT_CPU_FREQ_240 y
#define CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ 240
#define CONFIG_MEMMAP_SMP y
#define CONFIG_TASK_WDT n
#define CONFIG_LOG_DEFAULT_LEVEL_INFO y
#define CONFIG_LOG_DEFAULT_LEVEL 3
#define CONFIG_D0 17
#define CONFIG_D1 35
#define CONFIG_D2 34
#define CONFIG_D3 5
#define CONFIG_D4 39
#define CONFIG_D5 18
#define CONFIG_D6 36
#define CONFIG_D7 19
#define CONFIG_XCLK 27
#define CONFIG_PCLK 21
#define CONFIG_VSYNC 22
#define CONFIG_HREF 26
#define CONFIG_SDA 25
#define CONFIG_SCL 23
#define CONFIG_RESET 15
#define CONFIG_XCLK_FREQ 20000000
#define CAMERA_PIXEL_FORMAT CAMERA_PF_GRAYSCALE
#define CAMERA_FRAME_SIZE CAMERA_FS_SVGA
#define CAMERA_LED_GPIO 16
#define GROVE3 13
#define GROVE4 4

#include <EEPROM.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <lwip/err.h>
#include <lwip/sockets.h>
#include "camera.h"

WiFiServer server(80);
uint8_t *fb;
size_t frame_size;

//ssid and pw for eeprom. (mode 1)
String essid;
String password;

enum state
{
  CONFIGURED
};

struct credentials
{
  char state;
  char essid[32];
  char pw[32];
  char staticmode;
  uint8_t ip1;
  uint8_t ip2;
  uint8_t ip3;
  uint8_t ip4;
};

credentials creds;

void acquire_frame()
{
  digitalWrite(GROVE4, HIGH);
  digitalWrite(CAMERA_LED_GPIO, HIGH);
  esp_err_t err = camera_run();
  if (err != ESP_OK)
  {
    ESP_LOGW(TAG, "Camera capture failed with error = %d", err);
    return;
  }
  frame_size = camera_get_data_size();
  fb = camera_get_fb();
  digitalWrite(CAMERA_LED_GPIO, LOW);
  digitalWrite(GROVE4, LOW);
}

void serve()
{
  WiFiClient client = server.available();
  if (client)
  {
    //Serial.println("New Client.");
    String currentLine = "";
    while (client.connected())
    {
      if (client.available())
      {
        char c = client.read();
        //Serial.write(c);
        if (c == '\n')
        {
          if (currentLine.length() == 0)
          {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();
            client.print(
                "<style>body{margin: 0}\nimg{height: 100%; width: auto}</style>"
                "<img id='a' src='/camera' onload='this.style.display=\"initial\"; var b = document.getElementById(\"b\"); b.style.display=\"none\"; b.src=\"camera?\"+Date.now(); '>"
                "<img id='b' style='display: none' src='/camera' onload='this.style.display=\"initial\"; var a = document.getElementById(\"a\"); a.style.display=\"none\"; a.src=\"camera?\"+Date.now(); '>");
            client.println();
            break;
          }
          else
          {
            currentLine = "";
          }
        }
        else if (c != '\r')
        {
          currentLine += c;
        }

        if (currentLine.endsWith("GET /expmanual"))
        {
          client.println("HTTP/1.1 200 OK");
          client.println("Content-type:text/html");
          client.println();
          client.print("Setting Manual Exposure");
          client.println();
          break;
        }
        if (currentLine.endsWith("GET /current"))
        {
          client.println("HTTP/1.1 200 OK");
          client.println("Content-type:image/jpeg");
          client.println();
          client.write(fb, frame_size);
          break;
        }
        if (currentLine.endsWith("GET /acquire"))
        {
          acquire_frame();
          client.println("HTTP/1.1 200 OK");
          client.println("Content-type:text/html");
          client.println();
          client.print("Acquiring Frame.");
          break;
        }

        if (currentLine.endsWith("GET /camera"))
        {
          client.println("HTTP/1.1 200 OK");
          client.println("Content-type:image/jpeg");
          client.println();
          acquire_frame();
          client.write(fb, frame_size);
        }
        if (currentLine.endsWith("GET /reset"))
        {
          client.println("HTTP/1.1 200 OK");
          client.println("Content-type:text/html");
          client.println();
          client.print("Resetting mode to SoftAP.");

          EEPROM.begin(512);
          uint8_t modeconfig = 0;
          EEPROM.put(0, modeconfig);
          EEPROM.commit();
          EEPROM.end();

          break;
        }
      }
    }
    // close the connection:
    client.stop();
  }
}

static camera_pixelformat_t s_pixel_format;
bool video_running = false;

int udp_server = -1;
struct sockaddr_in destination;

uint8_t get_state()
{
  EEPROM.begin(512);
  uint8_t configmode;
  EEPROM.get(0, configmode);
  EEPROM.end();
  Serial.print("Recovered Mode:");
  Serial.println(configmode);
  return configmode;
}

void handleSerialInput()
{
  if (Serial.available() > 0)
  {

    char input = Serial.read();

    if (input == 'l')
      loadCredentials();

    if (input == 'r')
      resetCredentials();

    if (input == 'i')
    {

      String str;
      Serial.setTimeout(100000);
      while (Serial.available() > 0)
        Serial.read();

      Serial.println(" Configuring Device interactively");
      Serial.print("ESSID:");
      str = Serial.readStringUntil('\n');
      str.toCharArray(creds.essid, 32);
      Serial.println(creds.essid);
      Serial.print("Password:");

      str = Serial.readStringUntil('\n');
      str.toCharArray(creds.pw, 32);
      Serial.println(creds.pw);
      Serial.println("Enter 'static' to configure static IP:");
      String s = Serial.readStringUntil('\n');
      if (s == "static")
      {
        creds.staticmode = 1;
        Serial.print("1st ip number:");
        creds.ip1 = (char)Serial.parseInt();
        Serial.println(creds.ip1);
        Serial.print("2nd ip number:");
        creds.ip2 = (char)Serial.parseInt();
        Serial.println(creds.ip2);
        Serial.print("3rd ip number:");
        creds.ip3 = (char)Serial.parseInt();
        Serial.println(creds.ip3);
        Serial.print("4th ip number:");
        creds.ip4 = (char)Serial.parseInt();
        Serial.print(creds.ip4);
        Serial.println('.');
      }
      else
      {
        creds.staticmode = 0;
        Serial.println("Configuring DHCP");
      }

      Serial.print("Type 'yes' to save:");
      while (Serial.available() > 0)
        Serial.read();

      String yes = Serial.readStringUntil('\n');
      if (yes == "yes")
      {
        creds.state = 1; // set this creds state to configured.
        saveCredentials();
      }
      else
        Serial.println("Not saving.");
    }
  }
}

void setup()
{
  esp_log_level_set("camera", ESP_LOG_DEBUG);
  Serial.begin(115200);
  esp_err_t err;

  ESP_ERROR_CHECK(gpio_install_isr_service(0));
  pinMode(CAMERA_LED_GPIO, OUTPUT);
  pinMode(GROVE3, INPUT);
  pinMode(GROVE4, OUTPUT);

  digitalWrite(GROVE4, HIGH);
  delay(20);
  digitalWrite(GROVE4, LOW);

  WiFi.disconnect(true);

  camera_config_t camera_config;
  camera_config.ledc_channel = LEDC_CHANNEL_0;
  camera_config.ledc_timer = LEDC_TIMER_0;
  camera_config.pin_d0 = CONFIG_D0;
  camera_config.pin_d1 = CONFIG_D1;
  camera_config.pin_d2 = CONFIG_D2;
  camera_config.pin_d3 = CONFIG_D3;
  camera_config.pin_d4 = CONFIG_D4;
  camera_config.pin_d5 = CONFIG_D5;
  camera_config.pin_d6 = CONFIG_D6;
  camera_config.pin_d7 = CONFIG_D7;
  camera_config.pin_xclk = CONFIG_XCLK;
  camera_config.pin_pclk = CONFIG_PCLK;
  camera_config.pin_vsync = CONFIG_VSYNC;
  camera_config.pin_href = CONFIG_HREF;
  camera_config.pin_sscb_sda = CONFIG_SDA;
  camera_config.pin_sscb_scl = CONFIG_SCL;
  camera_config.pin_reset = CONFIG_RESET;
  camera_config.xclk_freq_hz = CONFIG_XCLK_FREQ;

  camera_model_t camera_model;
  err = camera_probe(&camera_config, &camera_model);
  if (err != ESP_OK)
  {
    return;
  }

  if (camera_model == CAMERA_OV7725)
  {
    s_pixel_format = CAMERA_PIXEL_FORMAT;
    camera_config.frame_size = CAMERA_FRAME_SIZE;

    ESP_LOGI(TAG, "Detected OV7725 camera, using %s bitmap format",
             CAMERA_PIXEL_FORMAT == CAMERA_PF_GRAYSCALE ? "grayscale" : "RGB565");
  }
  else if (camera_model == CAMERA_OV2640)
  {
    ESP_LOGI(TAG, "Detected OV2640 camera, using JPEG format");
    s_pixel_format = CAMERA_PF_JPEG;
    //s_pixel_format =  CAMERA_PF_GRAYSCALE;
    camera_config.frame_size = CAMERA_FRAME_SIZE;
    camera_config.jpeg_quality = 10;
  }
  else
  {
    ESP_LOGE(TAG, "Camera not supported");
    return;
  }

  camera_config.pixel_format = s_pixel_format;
  err = camera_init(&camera_config);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
    return;
  }
  Serial.println("done camera setup");

  if (get_state() == 1)
  {

    loadCredentials();

    if (creds.staticmode == 1)
    {
      Serial.println("Using Static IP");
      // Set your Static IP address
      IPAddress local_IP(creds.ip1, creds.ip2, creds.ip3, creds.ip4);
      // Set your Gateway IP address
      IPAddress gateway(10, 1, 10, 1);

      IPAddress subnet(255, 255, 255, 0);
      IPAddress primaryDNS(8, 8, 8, 8);   //optional
      IPAddress secondaryDNS(8, 8, 4, 4); //optional
      if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS))
      {
        Serial.println("STA Failed to configure");
      }
    }
    else
    {
      Serial.println("Using DHCP");
      if (!WiFi.begin())
      {
        Serial.println("STA Failed to configure");
      }
    }
    Serial.print("Connecting to ");
    Serial.println(creds.essid);

    WiFi.begin(creds.essid, creds.pw);
    // Configures static IP address

    while (WiFi.status() != WL_CONNECTED)
    {
      delay(100);
      handleSerialInput();
      Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
  else
  {

    Serial.println("Starting WiFi AP m5cam");
    WiFi.softAP("m5cam");
    Serial.print("IP address: ");
    IPAddress IP = WiFi.softAPIP();
    Serial.println(IP);
  }

  server.begin();
}

/** Load WLAN credentials from EEPROM */
void loadCredentials()
{
  EEPROM.begin(512);
  EEPROM.get(0, creds);
  EEPROM.end();
  Serial.println("Recovered credentials:");
  Serial.println(creds.essid);
  Serial.println(creds.pw);
  // Serial.println(strlen(password)>0?"********":"<no password>");
}

/** Store WLAN credentials to EEPROM */
void saveCredentials()
{
  Serial.println("Saving credentials");
  EEPROM.begin(512);
  EEPROM.put(0, creds);
  EEPROM.commit();
  EEPROM.end();
}

void resetCredentials()
{
  Serial.println("Resetting Credentials");
  EEPROM.begin(512);
  EEPROM.put(0, 0);
  EEPROM.commit();
  EEPROM.end();
}

void loop()
{
  serve();
  delay(1);
  if (digitalRead(GROVE3) > 0)
  {
    acquire_frame();
    Serial.println("Trigger.");
  }

  handleSerialInput();
}
