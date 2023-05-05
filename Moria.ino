#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

const char* host = "HOST";
const char* ssid = "SSID";
const char* password = "PASSWORD";

WebServer server(80);

// Set username and password for authentication
const char* http_username = "admin";
const char* http_password = "admin";

const char* updatePage =
"<!DOCTYPE html>"
"<html>"
"<head>"
"<title>Page with Side Menu</title>"
"<style>"
".menu {position: fixed;top: 0;left: 0;height: 100%;width: 100px;background-color: #333;color: #fff;padding: 20px;}"
".menu ul {list-style: none;padding: 0;margin: 0;}"
".menu li {margin-bottom: 10px;}"
".menu a {color: #fff;text-decoration: none;}"
".content {margin-left: 200px;padding: 20px;}"
".custom-file-upload {display: inline-block;padding: 10px 20px;font-size: 16px;font-weight: bold;color: #fff;background-color: #66b0ff;border-radius: 5px;cursor: pointer;}"
".custom-file-upload:hover {background-color: #3396ff;}"
".custom-file-upload:active {background-color: #3396ff;}"
".custom-update-button {display: inline-block;padding: 10px 20px;font-size: 16px;font-weight: bold;color: #fff;background-color: #f44336;border-radius: 5px;cursor: pointer;margin-left: 10px;}"
".custom-update-button:hover {background-color: #d32f2f;}"
".custom-update-button:active {background-color: #f44336;}"
"</style>"
"</head>"
"<body>"
"<div class='menu'>"
"<ul>"
"<li><a href='#' onclick='changeContent(1)'>Information</a></li>"
"<li><a href='#' onclick='changeContent(2)'>Settings</a></li>"
"<li><a href='#' onclick='changeContent(3)'>Debug</a></li>"
"<li><a href='#' onclick='changeContent(4)'>Firmware</a></li>"
"</ul>"
"</div>"
"<div class='content' id='main-content'>"
"<h1>Welcome to my page!</h1>"
"<p>This is the main content area.</p>"
"<p>Here you can write whatever you want.</p>"
"<p>Enjoy!</p>"
"</div>"
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<script>"
"function changeContent(page) {"
"var content = document.getElementById('main-content');"
"switch (page) {"
"case 1:"
"content.innerHTML = '<h1>Information</h1><p>Moria v0.2</p>';"
"break;"
"case 2:"
"content.innerHTML = '<h1>Settings</h1><p>Here you can write about your company or yourself.</p>';"
"break;"
"case 3:"
"content.innerHTML = '<h1>Debug</h1><p>Here you can list the services that you offer.</p>';"
"break;"
"case 4:"
"content.innerHTML = \"<h1>Firmware Update</h1><form method='POST' action='#' enctype='multipart/form-data' id='upload_form'><input class='custom-file-upload' type='file' name='update'><input type='submit' class='custom-update-button' value='Update'></form><progress id='progress-bar' max='100' value='0'></progress>\";"
"break;"
"}"
 "       }"

"$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
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
    "$('#progress-bar').val(Math.round(per*100));"
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
 "</script>"
"</body>"
"</html>";


const char* mainPage =
 "<h1>Debug ESP32 v0.1</h1>"
 "<h1>Moria v0.1</h1>"
 "<form action='/login' method='post'><input type='submit' value='Login'></form>";

String hostname = "Debug ESP32";

void setup() {
  Serial.begin(9600);
  ConnectWifi();
  ServerInit();
  
  pinMode(27, OUTPUT);
}

void ConnectWifi()
{
  WiFi.begin(ssid, password);
  WiFi.setHostname(hostname.c_str()); //define hostname
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("Connected to WiFi");
  Serial.println(WiFi.localIP());
}

void ServerInit()
{
  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  server.on("/", []() {
    server.send(200, "text/html", mainPage);
  });
  
  Serial.println("mDNS responder started");
  server.on("/login", []() {
    if (isAuthorized()) {
      server.send(200, "text/html", updatePage);
    }
  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
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
  server.begin();
}

bool isAuthorized() {
  if (!server.authenticate(http_username, http_password)) {
    server.requestAuthentication();
    return false;
  }
  return true;
}


boolean ledFlag = false;
uint32_t ledTimer;

void loop() {  
  server.handleClient();
  
  // Your programm 
  if (millis() - ledTimer > 1000)
  {
      ledTimer = millis();
      digitalWrite(27, ledFlag);
      ledFlag = !ledFlag;
  }
  
}
