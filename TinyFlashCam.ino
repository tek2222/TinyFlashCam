/**
 * Based on https://github.com/mic159/m5cam
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
// these are the Pinouts of the Grove connector on the board
// using GROVE3 as future Trigger line (INPUT)
#define GROVE3 13
// using GROVE4 as OUTPUT (flash)
#define GROVE4 4

#include <WiFi.h>
//#include <esp_wifi.h>
#include <lwip/err.h>
#include <lwip/sockets.h>
#include "camera.h"

#include <WebServer.h>     // Replace with WebServer.h for ESP32
#include <AutoConnect.h>

#include <Update.h>

const char* updatepage = 
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
 "<div id='prg'>progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/postupdate',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')" 
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script>";

char content[] = "TinyFlashCam M5";
  

WebServer server;
AutoConnect Portal(server);
AutoConnectConfig Config;
   
static camera_pixelformat_t s_pixel_format;
bool video_running=false;


uint8_t *fb;
size_t frame_size;
WiFiClient client;

void rootPage() {

  server.send(200, "text/plain", content);
}

void freerun(){
  client = server.client();
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println();
  client.print("<style>body{margin: 0}\nimg{height: 100%; width: auto}</style>"
                "<img id='a' src='/camera' onload='this.style.display=\"initial\"; var b = document.getElementById(\"b\"); b.style.display=\"none\"; b.src=\"camera?\"+Date.now(); '>"
                "<img id='b' style='display: none' src='/camera' onload='this.style.display=\"initial\"; var a = document.getElementById(\"a\"); a.style.display=\"none\"; a.src=\"camera?\"+Date.now(); '>");
  client.println();
}

void camera() {
  client = server.client();
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:image/jpeg");
  client.println();
  acquire_frame();
  client.write(fb, frame_size);
  client.stop();
  heap_caps_print_heap_info(MALLOC_CAP_8BIT);
}

void current() {
  client = server.client();
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:image/jpeg");
  client.println();
  client.write(fb, frame_size);
  client.stop();
  heap_caps_print_heap_info(MALLOC_CAP_8BIT);
}



void acquire_frame(){
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



void waitForTrigger() {
  client = server.client();
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:image/jpeg");
  client.println();
  while(digitalRead(GROVE3)== 0){
  }
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
  
  client.write(fb, frame_size);
}



void setup() {
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

  if (camera_model == CAMERA_OV2640)
  {
    ESP_LOGI(TAG, "Detected OV2640 camera, using JPEG format");
    s_pixel_format = CAMERA_PF_JPEG;
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
  Serial.println("Camera done setup");
  Serial.println();

  server.on("/", rootPage);
  server.on("/camera", camera);
  server.on("/current", current);
  server.on("/waitForTrigger", waitForTrigger);
  server.on("/freerun", freerun);

  server.on("/update", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", updatepage);
  });

  server.on("/postupdate", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });

  Config.autoReconnect = true;
  Portal.config(Config);

  if (Portal.begin()) {
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
  }
}

/*
void test_wifi() {
  if (WiFi.status() != WL_CONNECTED)
{
  Portal.begin();
}
}
*/
char c=0;




void Menu() {
  delay(1000);
  Serial.setTimeout(10000);
  String essid;   //Declare a String variable to hold your name
  String pw;   //Declare a String variable to hold your name
  int id1,id2,id3,id4;         //Declare an Int variable to hold your age
  //float height;    //Declare a float variable to hold your height

  while (Serial.available()!=0){
  Serial.read();
  }

  Serial.println("ESSid:"); while (Serial.available()==0) {        }     //Wait for user input
  essid=Serial.readStringUntil('\n');                 //Read user input into myName
  Serial.println("pw:"); while (Serial.available()==0) {        }     //Wait for user input
  pw=Serial.readStringUntil('\n');                 //Read user input into myName

  Serial.print("Static IP:"); //Prompt User for input
  while (Serial.available()==0) {          }   //Wait for user inpu}
  id1=Serial.parseInt();                 //Read user input into myName
  Serial.print(id1);Serial.print(".");
 
   while (Serial.available()==0) {          };
   id2=Serial.parseInt();                 //Read user input into myName
  Serial.print(id2);Serial.print(".");
  
   while (Serial.available()==0) {          };
  id3=Serial.parseInt();                 //Read user input into myName
  Serial.print(id3);Serial.print(".");
  
   while (Serial.available()==0) {          };
  id4=Serial.parseInt();                 //Read user input into myName
  Serial.print(id4);Serial.println(" ");
  Config.staip=IPAddress(id1,id2,id3,id4);
  Config.netmask=IPAddress(255,255,255,0);
  Serial.println("Connecting...");
  Portal.config(Config);

  if (Portal.begin(essid.c_str(),pw.c_str())) {
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
  }

}



      
void loop() {
    Portal.handleClient();
    delay(1);

    // Trigger camera with pin 
    if(digitalRead(GROVE3) != 0){
      acquire_frame();
      
    }
 

    if (Serial.available()>0){
      Serial.readBytes(&c,1);
      if (c=='i'){
        Serial.println("TinyFlashCam Serial interface.");
        Menu();
      }
      if (c=='a'){
        Serial.println("aquiring image.");
        acquire_frame();
        }
      }
    
    
}
