#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <EEPROM.h>

#define LED_PIN 2
#define BTN_PIN D5
#define RST_PIN D0

const char* ssid = "my outlet";
const char* password = "12345678";

AsyncWebServer server(80);

String wifi_ssid = "";
String wifi_password = "";
bool mode;
uint32_t tmr;
uint32_t tmr2;
bool flag = false;
bool flag2 = false;
bool long_press = false;

const char button_page[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP Web Server</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    .switch { position: relative; display: inline-block; width: 60px; height: 34px;}
    .switch input { opacity: 0; width: 0; height: 0;}
    .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; -webkit-transition: .4s; transition: .4s;}
    .slider:before { position: absolute; content: ""; height: 26px; width: 26px; left: 4px; bottom: 4px; background-color: white; -webkit-transition: .4s; transition: .4s;}
    input:checked + .slider { background-color: #2196F3;}
    input:checked + .slider:before { -webkit-transform: translateX(26px); -ms-transform: translateX(26px); transform: translateX(26px);}
  </style>
</head>
<body>
  <h2>IoT outlet</h2>
  <label class="switch"><input type="checkbox" onchange="toggleCheckbox(this)" id="output"><span class="slider"></span></label>
  <script>
    function toggleCheckbox(element) {
      var xhr = new XMLHttpRequest();
      if(element.checked){ xhr.open("GET", "/update?state=1"); }
      else { xhr.open("GET", "/update?state=0"); }
      xhr.send();
    }
    setInterval(function ( ) {
      var xhttp = new XMLHttpRequest();
      xhttp.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          var inputChecked;
          if( this.responseText == 1){ 
            inputChecked = true;
          }
          else { 
            inputChecked = false;
          }
          document.getElementById("output").checked = inputChecked;
        }
      };
      xhttp.open("GET", "/state", true);
      xhttp.send();
    }, 500 ) ;
  </script>
</body>
</html>
)rawliteral";

const char wifi_connect_page[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP Web Server</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
  </style>
</head>
<body>
  <h2>Wi-Fi connection</h2>
  <input id="ssid" value="" class="form-control" pattern="[0-9a-zA-Z.]{1,15}" placeholder="Wi-Fi network name">
  <input id="password" value="" class="form-control" pattern=".{8,15}" onfocus="this.type='text'" type="password" placeholder="Password">
  <input type="button" id="submit" value="Save"></input>
  <script>
    button = document.querySelector('#submit');
    button.addEventListener('click', set_ssid);

    function set_ssid(){
      var ssid = document.querySelector('#ssid').value;
      var password = document.querySelector('#password').value;
      var xhr = new XMLHttpRequest();
      request = "/ssid?ssid=" + ssid + "&password=" + password;
      xhr.open("GET", request);
      xhr.send();
    }
  </script>
</body>
</html>
)rawliteral";

void setup(){
  Serial.begin(115200);
  EEPROM.begin(200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(RST_PIN, OUTPUT);
  digitalWrite(RST_PIN, 1);

  read_wifi_data(10);
  mode_load();
  Serial.println(" ");
  Serial.println(wifi_ssid + " " + wifi_password);
  //mode = true;
  
  if (mode) {
    WiFi.softAP(ssid, password);
    Serial.println(WiFi.softAPIP());
  }
  else {
    WiFi.begin(wifi_ssid, wifi_password);
    
    tmr2 = millis();
    while (WiFi.status() != WL_CONNECTED) {
      if (tmr2 + 1000 < millis()) {
        Serial.println("Connecting to WiFi..");
        tmr2 = millis();
        delay(100);
      }
      check_button();
    }
    Serial.println(WiFi.localIP());
    tmr = millis();
  }
  server_init();
}

void loop() { 
  check_button();
}

void check_button() {
  if (!flag && !digitalRead(BTN_PIN)) {
    flag = true;
    tmr = millis();
  }
  if (flag && millis() - tmr >= 8000) {
    change_mode(true);
    digitalWrite(RST_PIN, 0);
  }
  if (flag && digitalRead(BTN_PIN)) {
    flag = false;
    if (millis() - tmr >= 50) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
  }
}

void save_wifi_data(int addr)
{
  byte len = wifi_ssid.length();
  EEPROM.write(addr, len);
  for (int i = 0; i < len; i++)
  {
    EEPROM.write(addr + 1 + i, wifi_ssid[i]);
  }
  byte len2 = wifi_password.length();
  EEPROM.write(addr + 1 + len, len2);
  for (int i = 0; i < len2; i++) {
    EEPROM.write(addr + 2 + len + i, wifi_password[i]);
  }
  EEPROM.commit();
}

void read_wifi_data(int addr) {
  int len = EEPROM.read(addr);
  char data[len + 1];
  for (int i = 0; i < len; i++)
  {
    data[i] = EEPROM.read(addr + 1 + i);
  }
  data[len] = '\0';
  wifi_ssid = String(data);
  int len2 = EEPROM.read(addr + 1 + len);
  char data2[len2 + 1];
  for (int i = 0; i < len2; i++)
  {
    data2[i] = EEPROM.read(addr + 2 + len + i);
  }
  data2[len2] = '\0';
  wifi_password = String(data2);
}

void server_init() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!mode) request->send_P(200, "text/html", button_page);
    else request->send_P(200, "text/html", wifi_connect_page);
  });

  server.on("/update", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
    if (request->hasParam("state")) {
      inputMessage = request->getParam("state")->value();
      digitalWrite(LED_PIN, inputMessage.toInt());
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/ssid", HTTP_GET, [] (AsyncWebServerRequest *request) {
    bool flag = true;
    if (request->hasParam("ssid")) wifi_ssid = request->getParam("ssid")->value();
    else flag = false;
    if (request->hasParam("password")) wifi_password = request->getParam("password")->value();
    else flag = false;
    if (flag && wifi_ssid.length() > 0 && wifi_password.length() > 7 && wifi_password.length() < 65) {
      save_wifi_data(10);
      delay(500);
      change_mode(false);
      delay(500);
      digitalWrite(RST_PIN, 0);
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/state", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(digitalRead(LED_PIN)).c_str());
  });

  server.begin();
}

void change_mode(bool f) {
  mode = f;
  if (mode) EEPROM.write(2, 1);
  else EEPROM.write(2, 0);
  EEPROM.commit();
}

void mode_load() {
  int i = EEPROM.read(2);
  if (i != 0) mode = true;
  else mode = false;
}