//DEBUG OPTION
#define DEBUG 0        //Enable serial debug
#define INFO 1         //Only enable serial show data in EEPROM (Need enable this for Web Debug(Serial Begin))
#define STATUS_INFO 1  //Enable Status and system information info in Web and Telegram
#define SYS_STAT 1     //Enable this to show freeheap (JS interval 5s)
#define PIN_DATA 0     //Enable this to show pin (JS interval 2.5s)

//SYSTEM OPTION
#define WAUTH 1  //Enable WebAuth Function
#define W_WEB 0  //Enable WebAuth Changes on Web
#define TELE 0   //Enable Telegram Bot
#define T_WEB 0  //Enable Telegram Bot Changes on Web
#define MQTT 0   //Enable MQTT intergration

//Reduce Wifi, WebAuth and Telegram to C-Style string (not "String") to reduce heap RAM usage

// Telegram Setting
#if TELE == 1 || T_WEB == 1
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>  // Universal Telegram Bot Library written by Brian Lough: https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot
#include <ArduinoJson.h>
#ifdef ESP8266
X509List cert(TELEGRAM_CERTIFICATE_ROOT);
#endif
WiFiClientSecure client;
char CHAT_ID[13];
int botRequestDelay = 2000;
unsigned long lastTimeBotRan;
UniversalTelegramBot* bot;
#endif

#if MQTT == 1
// MQTT Broker settings
#include <AsyncMqttClient.h>
const char* mqtt_broker = "MQTT_BROKER_ADDRESS";
const uint16_t mqtt_port = 1883;
const char* mqtt_user = "YOUR_MQTT_USER";          // Optional
const char* mqtt_password = "YOUR_MQTT_PASSWORD";  // Optional
AsyncMqttClient mqttClient;
#endif

// Core Library and System (WiFi + WebInterface)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <EEPROM.h>
const char* defaultSSID = "WifiTest";      //Default Wifi name in AP mode
const char* defaultPassword = "Test123!";  //Default password wifi in AP mode

AsyncWebServer server(80);

void startAPMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(defaultSSID, defaultPassword);
#if DEBUG == 1
  Serial.println(F("Started Soft AP"));
#endif
}

void saveWifiCredentials(const char* newSSID, const char* newPassword) {
  bool changes = false;
  EEPROM.begin(512);

  char currentSSID[33];  // 32 characters + null terminator
  for (int i = 0; i < 32; ++i) {
    currentSSID[i] = char(EEPROM.read(i));
    if (currentSSID[i] == 0) break;  // Stop reading at the first null character
  }
  currentSSID[32] = '\0';  // Ensure null termination

  char currentPassword[65];  // 64 characters + null terminator
  for (int i = 32; i < 96; ++i) {
    currentPassword[i - 32] = char(EEPROM.read(i));
    if (currentPassword[i - 32] == 0) break;
  }
  currentPassword[64] = '\0';  // Ensure null termination

  // Compare and update SSID
  if (strcmp(newSSID, currentSSID) != 0) {
#if DEBUG == 1
    Serial.println(F("Updating SSID in EEPROM"));
#endif
    // Write new SSID
    for (int i = 0; i < 32; ++i) {
      EEPROM.write(i, newSSID[i]);
      if (newSSID[i] == 0) break;
    }
    changes = true;
  }
  // Compare and update Password
  if (strcmp(newPassword, currentPassword) != 0) {
#if DEBUG == 1
    Serial.println(F("Updating Password in EEPROM"));
#endif
    // Write new password
    for (int i = 32; i < 96; ++i) {
      EEPROM.write(i, newPassword[i - 32]);
      if (newPassword[i - 32] == 0) break;
    }
    changes = true;
  }
  // Commit changes to EEPROM if necessary
  if (changes) {
    EEPROM.commit();
#if DEBUG == 1
    Serial.println(F("Credentials changed, updated to EEPROM"));
#endif
  } else {
#if DEBUG == 1
    Serial.println(F("Credentials unchanged. No update required."));
#endif
  }
  EEPROM.end();
}

void loadWifiCredentials(char* ssid, char* password) {
  EEPROM.begin(512);  // Ensure EEPROM is initialized

#if DEBUG == 1
  Serial.println(F("Reading SSID from EEPROM:"));
#endif
  for (int i = 0; i < 32; ++i) {
    ssid[i] = char(EEPROM.read(i));
    if (ssid[i] == 0) break;  // Stop at null character
#if DEBUG == 1
    Serial.print(ssid[i]);
#endif
  }
  ssid[31] = '\0';  // Ensure null termination

#if DEBUG == 1
  Serial.println(F("Reading Password from EEPROM:"));
#endif
  for (int i = 32; i < 96; ++i) {
    password[i - 32] = char(EEPROM.read(i));
    if (password[i - 32] == 0) break;  // Stop at null character
#if DEBUG == 1
    Serial.print(password[i - 32]);
#endif
  }
  password[63] = '\0';  // Ensure null termination
  EEPROM.end();         // Ending EEPROM access
}

#if (DEBUG == 1 || INFO == 1) && STATUS_INFO == 1
const char* getDeviceInfo() {
#ifdef ESP8266
  static char devInfo[324];  // Buffer of 180 characters
  int rssi = WiFi.RSSI();
  int channel = WiFi.channel();
  snprintf(devInfo, sizeof(devInfo),
           "<p>Chip ID: %u</p>"
           "<p>Flash ID: %u</p>"
           "<p>Flash Size: %u bytes</p>"
           "<p>Flash Speed: %u Hz</p>"
           "<p>MAC Addr: %s</p>"
           "<p>Wifi SSID: %s</p>"
           "<p>Wifi RSSI: %d dBm</p>"
           "<p>Wifi Channel: %d</p>",
           ESP.getChipId(), ESP.getFlashChipId(), ESP.getFlashChipRealSize(), ESP.getFlashChipSpeed(), WiFi.macAddress().c_str(), WiFi.SSID().c_str(), rssi, channel);

#if DEBUG == 1
  Serial.println(devInfo);
#endif
#endif
  return devInfo;
}

#if SYS_STAT == 1
const char* getDebugInfo() {
  static char heapString[24];  // Static buffer
  int heapSize = ESP.getFreeHeap();
  snprintf(heapString, sizeof(heapString), "Heap left: %d b", heapSize);

#if DEBUG == 1
  Serial.print("Debug - Free Heap: ");
  Serial.println(heapSize);
#endif

  return heapString;
}
#endif

#if PIN_DATA == 1
const char* readAllPins() {
  static char pinStates[150];  // Buffer size increased
  int index = 0;

  // List of safe GPIO pins on most ESP8266 boards
  int pinsToRead[] = { 0, 2, 4, 5, 12, 13, 14, 15 };
  int numPins = sizeof(pinsToRead) / sizeof(pinsToRead[0]);

  for (int i = 0; i < numPins; i++) {
    int pin = pinsToRead[i];
    pinMode(pin, INPUT);           // Set pin to input mode
    int state = digitalRead(pin);  // Read the pin state

    // Append the pin state to the char array
    index += snprintf(&pinStates[index], sizeof(pinStates) - index, "<p>Pin %d: %d</p>", pin, state);
    if (index >= sizeof(pinStates)) break;  // Check for buffer overflow
  }

  // Read and append the analog value of A0
  int analogValue = analogRead(A0);
  snprintf(&pinStates[index], sizeof(pinStates) - index, "<p>Analog A0: %d</p>", analogValue);

#if DEBUG == 1
  Serial.println(pinStates);
#endif

  return pinStates;
}
#endif

#endif

#if WAUTH == 1 || W_WEB == 1

char webuser[33];  // 32 characters + null terminator
char webpass[65];  // 64 characters + null terminator
bool webauth = false;
bool authenticate(AsyncWebServerRequest* request) {
  // Check if both webuser and webpass are empty (authentication disabled)
  if (webuser[0] == '\0' && webpass[0] == '\0') {
#if DEBUG == 1
    Serial.println(F("WebAuth Disable"));
#endif
    return true;  // Skip authentication
  }

  // Proceed with normal authentication
  if (!request->authenticate(webuser, webpass)) {
    request->requestAuthentication();
#if DEBUG == 1
    Serial.println(F("WebAuth Wrong Input"));
#endif
    return false;
  }
#if DEBUG == 1
  Serial.println(F("WebAuth OK"));
#endif
  return true;
}

#if W_WEB == 1
void saveWebCredentials(const char* newUser, const char* newPass) {
  bool changes = false;
  EEPROM.begin(512);
  // Adjusted start addresses for Web User and Web Password
  const int userStartAddr = 96;   // Start address for Web User
  const int passStartAddr = 128;  // Start address for Web Password
  char currentUser[33];           // Length of user + null terminator
  char currentPass[65];           // Length of password + null terminator

  // Read current User from EEPROM
  for (int i = userStartAddr; i < userStartAddr + 32; ++i) {
    currentUser[i - userStartAddr] = char(EEPROM.read(i));
    if (currentUser[i - userStartAddr] == 0) break;
  }
  currentUser[32] = '\0';  // Ensure null termination

  // Read current Password from EEPROM
  for (int i = passStartAddr; i < passStartAddr + 64; ++i) {
    currentPass[i - passStartAddr] = char(EEPROM.read(i));
    if (currentPass[i - passStartAddr] == 0) break;
  }
  currentPass[64] = '\0';  // Ensure null termination
  // Compare and update User
  if (strcmp(newUser, currentUser) != 0) {
#if DEBUG == 1
    Serial.println(F("Updating User in EEPROM"));
#endif
    for (int i = userStartAddr; i < userStartAddr + 32; ++i) {
      EEPROM.write(i, i - userStartAddr < strlen(newUser) ? newUser[i - userStartAddr] : 0);
      if (newUser[i - userStartAddr] == 0) break;
    }
    changes = true;
  }
  // Compare and update Password
  if (strcmp(newPass, currentPass) != 0) {
#if DEBUG == 1
    Serial.println(F("Updating Password in EEPROM"));
#endif
    for (int i = passStartAddr; i < passStartAddr + 64; ++i) {
      EEPROM.write(i, i - passStartAddr < strlen(newPass) ? newPass[i - passStartAddr] : 0);
      if (newPass[i - passStartAddr] == 0) break;
    }
    changes = true;
  }
  // Commit changes to EEPROM if necessary
  if (changes) {
    EEPROM.commit();
#if DEBUG == 1
    Serial.println(F("Web credentials changed, updated to EEPROM"));
#endif
  } else {
#if DEBUG == 1
    Serial.println(F("Web credentials unchanged. No update required."));
#endif
  }
  EEPROM.end();
}
#endif

void loadWebCredentials(char* user, char* pass) {
  EEPROM.begin(512);  // Ensure EEPROM is initialized

  // Adjusted start addresses for Web User and Web Password
  const int userStartAddr = 96;   // Start address for Web User
  const int passStartAddr = 128;  // Start address for Web Password

// Read User from EEPROM
#if DEBUG == 1
  Serial.println(F("Reading User from EEPROM:"));
#endif
  for (int i = userStartAddr; i < userStartAddr + 32; ++i) {
    user[i - userStartAddr] = char(EEPROM.read(i));
    if (user[i - userStartAddr] == 0) break;  // Stop at null character
#if DEBUG == 1
    Serial.print(user[i - userStartAddr]);
#endif
  }
  user[32] = '\0';  // Ensure null termination

// Read Password from EEPROM
#if DEBUG == 1
  Serial.println(F("Reading Password from EEPROM:"));
#endif
  for (int i = passStartAddr; i < passStartAddr + 64; ++i) {
    pass[i - passStartAddr] = char(EEPROM.read(i));
    if (pass[i - passStartAddr] == 0) break;  // Stop at null character
#if DEBUG == 1
    Serial.print(pass[i - passStartAddr]);
#endif
  }
  pass[64] = '\0';  // Ensure null termination

  EEPROM.end();  // Ending EEPROM access
}
#endif

#if TELE == 1 || T_WEB == 1

#if T_WEB == 1
void saveBotCredentials(const char* newToken, const char* newChatID) {
  bool changes = false;
  EEPROM.begin(512);
  // Adjusted start addresses for Telegram Token and Chat ID
  const int tokenStartAddr = 192;   // Start address for Telegram Token
  const int chatIDStartAddr = 238;  // Start address for Telegram Chat ID
  char currentToken[47];            // Length of token + null terminator
  char currentChatID[13];           // Length of chatID + null terminator

  // Read current Token from EEPROM
  for (int i = tokenStartAddr; i < tokenStartAddr + 46; ++i) {
    currentToken[i - tokenStartAddr] = char(EEPROM.read(i));
    if (currentToken[i - tokenStartAddr] == 0) break;
  }
  currentToken[46] = '\0';  // Ensure null termination

  // Read current ChatID from EEPROM
  for (int i = chatIDStartAddr; i < chatIDStartAddr + 12; ++i) {
    currentChatID[i - chatIDStartAddr] = char(EEPROM.read(i));
    if (currentChatID[i - chatIDStartAddr] == 0) break;
  }
  currentChatID[12] = '\0';  // Ensure null termination
  // Compare and update Token
  if (strcmp(newToken, currentToken) != 0) {
#if DEBUG == 1
    Serial.println(F("Updating Token in EEPROM"));
#endif
    for (int i = tokenStartAddr; i < tokenStartAddr + 46; ++i) {
      EEPROM.write(i, i - tokenStartAddr < strlen(newToken) ? newToken[i - tokenStartAddr] : 0);
      if (newToken[i - tokenStartAddr] == 0) break;
    }
    changes = true;
  }
  // Compare and update ChatID
  if (strcmp(newChatID, currentChatID) != 0) {
#if DEBUG == 1
    Serial.println(F("Updating ChatID in EEPROM"));
#endif
    for (int i = chatIDStartAddr; i < chatIDStartAddr + 12; ++i) {
      EEPROM.write(i, i - chatIDStartAddr < strlen(newChatID) ? newChatID[i - chatIDStartAddr] : 0);
      if (newChatID[i - chatIDStartAddr] == 0) break;
    }
    changes = true;
  }
  // Commit changes to EEPROM if necessary
  if (changes) {
    EEPROM.commit();
#if DEBUG == 1
    Serial.println(F("Bot credentials changed, updated to EEPROM"));
#endif
  } else {
#if DEBUG == 1
    Serial.println(F("Bot credentials unchanged. No update required."));
#endif
  }
  EEPROM.end();
}
#endif

void loadBotCredentials(char* token, char* chatID) {
  EEPROM.begin(512);

  const int tokenStartAddr = 192;
  const int chatIDStartAddr = 238;

  // Read Token from EEPROM
  for (int i = tokenStartAddr; i < tokenStartAddr + 46; ++i) {
    token[i - tokenStartAddr] = char(EEPROM.read(i));
    if (token[i - tokenStartAddr] == 0) break;
  }
  token[46] = '\0';

  // Read ChatID from EEPROM
  for (int i = chatIDStartAddr; i < chatIDStartAddr + 12; ++i) {
    chatID[i - chatIDStartAddr] = char(EEPROM.read(i));
    if (chatID[i - chatIDStartAddr] == 0) break;
  }
  chatID[12] = '\0';

  EEPROM.end();
}

void checkNewMessages() {
  if (millis() > lastTimeBotRan + botRequestDelay) {
    int numNewMessages = bot->getUpdates(bot->last_message_received + 1);
    while (numNewMessages) {
#if DEBUG == 1
      Serial.println(F("Get data from tele"));
#endif
      handleNewMessages(numNewMessages);
      numNewMessages = bot->getUpdates(bot->last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }
}

void handleNewMessages(int numNewMessages) {
#if DEBUG == 1
  Serial.println(F("NewMessages"));
  Serial.println(String(numNewMessages));
#endif

  for (int i = 0; i < numNewMessages; i++) {
    // Chat id of the requester
    String chat_id = String(bot->messages[i].chat_id);
#if DEBUG == 1
    Serial.println(chat_id);
    Serial.println(CHAT_ID);
#endif
    if (chat_id != CHAT_ID) {
      bot->sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }
    // Print the received message
    String text = bot->messages[i].text;
#if DEBUG == 1
    Serial.println(text);
#endif

    String from_name = bot->messages[i].from_name;

    if (text == "/start") {
      String welcome = "Welcome, " + from_name + ".\n";
      welcome += "Use the following example command to .\n\n";
      String keyboardJson =
        "["
        "[{\"text\":\"Info\",\"url\":\"https://danielamani.com\"}],"
        "[{\"text\":\"Heap Balance\",\"callback_data\":\"/heap\"}],"
        "[{\"text\":\"IP Address\",\"callback_data\":\"/ip\"}]"
        "]";
      bot->sendMessageWithInlineKeyboard(chat_id, welcome, "", keyboardJson);
    }
    if (text == "/heap") {
      bot->sendMessage(chat_id, String(ESP.getFreeHeap()), "");
    }
    if (text == "/ip") {
      bot->sendMessage(chat_id, WiFi.localIP().toString(), "");
    }
  }
}
#endif

//Recycle top & CSS of HTML
const char cssStyle[] PROGMEM = R"rawliteral(
    <html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>
    body {font-family: Arial, sans-serif;background-color: #1a1a1a;color: #e0e0e0;margin: 0;padding: 0;text-align: center;}
    h1, h2, h3, label {color: #fff;}
    form {width: 70%;margin: 20px auto 0;text-align: left;}
    input, button {display: block;width: 100%;margin: 10px 0;padding: 15px;border-radius: 4px;box-sizing: border-box;}
    input {border: 1px solid #555;background-color: #333;color: #ddd;font-size: 1.2em;}
    button {display: block;width: 80%;margin: 10px auto;border: none;background-color: #008CBA;color: white;cursor: pointer;font-size: 16px;}
    button:hover {background-color: #007B9A;}</style></head><body><br />)rawliteral";

const char config_html[] PROGMEM = R"rawliteral(%CSS%
    <h1>System Configuration</h1><hr>
    <h2>Wifi</h2><p>Only Wifi 2.4Ghz supported for ESP8266</p>
    <form id="wifiForm">SSID: <input type='text' name='ssid'><br>Password: <input type='password' name='password'><br>
    <button type='button' onclick='submitWiFiSettings()'>Save</button></form><hr> %F_Wauth% %F_Tele% %S_Mqtt% <br />
    <button onclick="window.location.href='%IP%';">Back to main page</button>
    <a href="danielamani.com">Code by: DanielAmani.com</a>
    <script>function submitWiFiSettings(){ var xhr = new XMLHttpRequest();
    var formData = new FormData(document.getElementById('wifiForm'));
    xhr.open("POST", "/setwifi", true);xhr.onreadystatechange = function() {
    if (xhr.readyState === XMLHttpRequest.DONE && xhr.status === 200) {
    lert("Settings Wifi updated!");}};xhr.send(formData);}
    %S_Wauth% %S_Tele% %S_Mqtt% </script></body></html>)rawliteral";

#if WAUTH == 1 && W_WEB == 1
const char FWauth[] PROGMEM = R"rawliteral(
    <h2>Web Auth</h2><p>If User and Password is save as blank will disable webauth</p><form id="webAuthForm">
    User: <input type='text' name='webuser'><br>Password: <input type='password' name='webpass'><br>
    <button type='button' onclick='submitWebAuthSettings()'>Save</button></form><hr>)rawliteral";

const char SWauth[] PROGMEM = R"rawliteral(
    function submitWebAuthSettings() {
    var xhr = new XMLHttpRequest();
    var formData = new FormData(document.getElementById('webAuthForm'));
    xhr.open("POST", "/setWebAuth", true);
    xhr.onreadystatechange = function() {
    if (xhr.readyState === XMLHttpRequest.DONE && xhr.status === 200) {
    alert("Settings WebAuth updated!");}};xhr.send(formData);})rawliteral";
#endif

#if TELE == 1 && T_WEB == 1
const char FTele[] PROGMEM = R"rawliteral(
    <h2>Telegram</h2><p>Use BotFather and RawDataBot to get the info</p><form id="teleForm">
    Token: <input type='text' name='token'><br>ChatID: <input type='text' name='chatID'><br>
    <button type='button' onclick='submitTeleSettings()'>Save</button></form><hr>)rawliteral";

const char STele[] PROGMEM = R"rawliteral(
    function submitTeleSettings() {
    var xhr = new XMLHttpRequest();
    var formData = new FormData(document.getElementById('teleForm'));
    xhr.open("POST", "/setTele", true);
    xhr.onreadystatechange = function() {
    if (xhr.readyState === XMLHttpRequest.DONE && xhr.status === 200) {
    alert("Settings Telegram updated!");}};xhr.send(formData);})rawliteral";
#endif
#if MQTT == 1
const char FMqtt[] PROGMEM = R"rawliteral(
    <h2>MQTT</h2><form id="MQTTForm">
    Address: <input type='text' name='mqttaddr'><br>
    Port: <input type='number' name='mqttport'><br>
    User: <input type='text' name='mqttuser'><br>
    Password: <input type='password' name='mqttpassword'><br>
    <button type='button' onclick='submitMQTTSettings()'>Save</button></form><hr>)rawliteral";
const char SMqtt[] PROGMEM = R"rawliteral(
    function submitMQTTSettings() {
    var xhr = new XMLHttpRequest();
    var formData = new FormData(document.getElementById('MQTTForm'));
    xhr.open("POST", "/setMQTT", true);
    xhr.onreadystatechange = function() {
    if (xhr.readyState === XMLHttpRequest.DONE && xhr.status === 200) {
    alert("Settings MQTT updated!");}};xhr.send(formData);}  )rawliteral";
#endif

const char index_html[] PROGMEM = R"rawliteral(
  %CSS%
  <h1>Webpage Homepage</h1><hr><p>This is home page </p><br /><hr><footer>
  <button onclick="window.location.href='%IP%/configsystem';">Go to Config System</button>
  <button onclick="window.location.href='%IP%/debugsystem';">Go to System Info</button>
  <a href="danielamani.com">Code by: DanielAmani.com</a></footer></body></html>)rawliteral";

#if (DEBUG == 1 || INFO == 1) && STATUS_INFO == 1
const char debugsystem_html[] PROGMEM = R"rawliteral(
  %CSS% <h1>System Info</h1><hr>
  <p>This page will show hardware, heap and pin status (limited to template)</p><hr>%CHIP_INFO%<span id="sys"></span><span id="pin"></span><hr>
  <br /><footer><button onclick="window.location.href='%IP%/';">Back to main page</button><a href="danielamani.com">Code by: DanielAmani.com</a></footer>
  </body><script>%INV_HEAP% %INV_PINS%</script></html>)rawliteral";

#if SYS_STAT == 1
const char inv_heap[] PROGMEM = R"rawliteral(setInterval(function ( ) {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200){
        document.getElementById("sys").innerHTML = this.responseText;}};
    xhttp.open("GET", "/sysinfo", true);xhttp.send();},5000);)rawliteral";
#endif
#if PIN_DATA == 1
const char inv_pins[] PROGMEM = R"rawliteral(setInterval(function ( ) {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200){
        document.getElementById("pin").innerHTML = this.responseText;}};
    xhttp.open("GET", "/pininfo", true);xhttp.send();},1500);)rawliteral";
#endif
#endif

String processor(const String& var) {
  if (var == "CSS") {
    return cssStyle;
  }
  if (var == "IP") {
    IPAddress ip = WiFi.localIP();
    char ipString[25];
    if (ip == IPAddress(0, 0, 0, 0)) {
      // IP is 0.0.0.0, so set to default 192.168.4.1
      strcpy(ipString, "http://192.168.4.1");
    } else {
      // Format the actual IP address
      sprintf(ipString, "http://%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    }
    return ipString;
  }
#if WAUTH == 1
#if W_WEB == 1
  if (var == "F_Wauth") {
    return FWauth;
  }
  if (var == "S_Wauth") {
    return SWauth;
  }
#elif W_WEB == 0
  if (var == "F_Wauth") {
    const char myString[] = "<p>WebAuth is ACTIVE but changes using Web are DISABLE</p><hr>";
    return myString;
  }
#endif
#endif

#if TELE == 1
#if T_WEB == 1
  if (var == "F_Tele") {
    return FTele;
  }
  if (var == "S_Tele") {
    return STele;
  }
#elif T_WEB == 0
  if (var == "F_Tele") {
    const char myString[] = "<p>Telegram is ACTIVE but changes using Web are DISABLE</p><hr>";
    return myString;
  }
#endif
#endif

#if MQTT == 1
  if (var == "F_Mqtt") {
    return FMqtt;
  }
  if (var == "S_Mqtt") {
    return SMqtt;
  }
#endif

#if (DEBUG == 1 || INFO == 1) && STATUS_INFO == 1

  if (var == "CHIP_INFO") {
    return getDeviceInfo();
  }

#if SYS_STAT == 1
  if (var == "SYS_INFO") {
    return getDebugInfo();
  }
  if (var == "INV_HEAP") {
    return inv_heap;
  }
#endif
#if PIN_DATA == 1
  if (var == "PIN_INFO") {
    return readAllPins();
  }
  if (var == "INV_PINS") {
    return inv_pins;
  }

#endif
#endif
  return String();  // Default return for any other case
}

void startServer() {
#if DEBUG == 1
  Serial.println(F("Starting configuration server"));
#endif

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
#if WAUTH == 1
    if (!authenticate(request)) return;  // Check authentication
#endif
    String index = FPSTR(index_html);
    index.replace("%CSS%", processor("CSS"));
    delay(150);  //Very big string, need some delay
    index.replace("%IP%", processor("IP"));
    delay(150);  //Very big string, need some delay
    response->addHeader("Content-Encoding", "gzip");
    request->send(200, "text/html", index);
  });
#if STATUS_INFO == 0
  server.on("/debugsystem", HTTP_GET, [](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response = request->beginResponse(200, "text/plain", "WebDebug Disable");
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
  });
#endif
#if (DEBUG == 1 || INFO == 1) && STATUS_INFO == 1
  server.on("/debugsystem", HTTP_GET, [](AsyncWebServerRequest* request) {
#if WAUTH == 1
    if (!authenticate(request)) return;  // Check authentication
#endif
    String index = FPSTR(debugsystem_html);
    index.replace("%CSS%", processor("CSS"));
    delay(150);  //Very big string, need some delay
    index.replace("%CHIP_INFO%", processor("CHIP_INFO"));
    index.replace("%INV_HEAP%", processor("INV_HEAP"));
    index.replace("%INV_PINS%", processor("INV_PINS"));
    index.replace("%IP%", processor("IP"));
    delay(150);  //Very big string, need some delay
    request->send(200, "text/html", index);
  });

#if SYS_STAT == 1
  server.on("/sysinfo", HTTP_GET, [](AsyncWebServerRequest* request) {
#if WAUTH == 1
    if (!authenticate(request)) return;  // Check authentication
#endif
    AsyncWebServerResponse* response = request->beginResponse(200, "text/plain", getDebugInfo());
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
  });
#endif
#if PIN_DATA == 1
  server.on("/pininfo", HTTP_GET, [](AsyncWebServerRequest* request) {
#if WAUTH == 1
    if (!authenticate(request)) return;  // Check authentication
#endif
    AsyncWebServerResponse* response = request->beginResponse(200, "text/plain", readAllPins());
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
  });
#endif

#endif
  // Serve the WiFi settings page at "/configsystem" instead of "/"
  server.on("/configsystem", HTTP_GET, [](AsyncWebServerRequest* request) {
#if WAUTH == 1
    if (!authenticate(request)) return;  // Check authentication
#endif
    String index = FPSTR(config_html);
    delay(100);  //Very big string, need some delay
    index.replace("%CSS%", processor("CSS"));
    index.replace("%F_Wauth%", processor("F_Wauth"));
    index.replace("%S_Wauth%", processor("S_Wauth"));
    index.replace("%F_Tele%", processor("F_Tele"));
    index.replace("%S_Tele%", processor("S_Tele"));
    index.replace("%F_Mqtt%", processor("F_Mqtt"));
    index.replace("%S_Mqtt%", processor("S_Mqtt"));
    index.replace("%IP%", processor("IP"));
    delay(100);  //Very big string, need some delay
    request->send(200, "text/html", index);
  });
  // Handle the form submission at "/setwifi"
  server.on("/setwifi", HTTP_POST, [](AsyncWebServerRequest* request) {
#if WAUTH == 1
    if (!authenticate(request)) return;  // Check authentication
#endif
    if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
#if DEBUG == 1
      Serial.println(F("Received WiFi settings data"));
#endif
      // Create buffers for ssid and password
      char ssid[33];      // Extra space for null terminator
      char password[65];  // Extra space for null terminator
      // Read ssid
      AsyncWebParameter* p = request->getParam("ssid", true);
      strncpy(ssid, p->value().c_str(), sizeof(ssid));
      ssid[sizeof(ssid) - 1] = '\0';  // Ensure null termination
      // Read password
      p = request->getParam("password", true);
      strncpy(password, p->value().c_str(), sizeof(password));
      password[sizeof(password) - 1] = '\0';  // Ensure null termination
      saveWifiCredentials(ssid, password);
      request->send(200, "text/plain", "WiFi settings saved. Rebooting...");
      delay(1000);
      ESP.restart();
    } else {
      request->send(400, "text/plain", "Invalid request");
    }
  });

#if WAUTH == 1 && W_WEB == 1
  server.on("/setWebAuth", HTTP_POST, [](AsyncWebServerRequest* request) {
#if WAUTH == 1
    if (!authenticate(request)) return;  // Check authentication
#endif
    if (request->hasParam("webuser", true) && request->hasParam("webpass", true)) {
#if DEBUG == 1
      Serial.println(F("Received WebAuth settings data"));
#endif
      // Create buffers for user and password
      char user[33];  // Extra space for null terminator
      char pass[65];  // Extra space for null terminator
      // Read user
      AsyncWebParameter* p = request->getParam("webuser", true);
      strncpy(user, p->value().c_str(), sizeof(user));
      webuser[sizeof(user) - 1] = '\0';  // Ensure null termination
      // Read password
      p = request->getParam("webpass", true);
      strncpy(pass, p->value().c_str(), sizeof(pass));
      webpass[sizeof(pass) - 1] = '\0';  // Ensure null termination
      saveWebCredentials(user, pass);
      request->send(200, "text/plain", "WebAuth settings saved. Rebooting...");
      delay(1000);
      ESP.restart();
    } else {
      request->send(400, "text/plain", "Invalid request");
    }
  });
#endif
#if TELE == 1 && T_WEB == 1
  server.on("/setTele", HTTP_POST, [](AsyncWebServerRequest* request) {
#if WAUTH == 1
    if (!authenticate(request)) return;  // Check authentication
#endif
    if (request->hasParam("token", true) && request->hasParam("chatID", true)) {
#if DEBUG == 1
      Serial.println(F("Received Telegram settings data"));
#endif
      // Create buffers for token and chatID
      char token[47];   // Length of token + null terminator
      char chatID[13];  // Length of chatID + null terminator
      // Read token
      AsyncWebParameter* p = request->getParam("token", true);
      strncpy(token, p->value().c_str(), sizeof(token) - 1);
      token[sizeof(token) - 1] = '\0';  // Ensure null termination
      // Read chatID
      p = request->getParam("chatID", true);
      strncpy(chatID, p->value().c_str(), sizeof(chatID) - 1);
      chatID[sizeof(chatID) - 1] = '\0';  // Ensure null termination
      saveBotCredentials(token, chatID);
      request->send(200, "text/plain", "Telegram settings saved. Rebooting...");
      delay(1000);
      ESP.restart();
    } else {
      request->send(400, "text/plain", "Invalid request");
    }
  });
#endif
#if MQTT == 1
  server.on("/setMQTT", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (request->hasParam("mqttaddr", true) && request->hasParam("mqttport", true) && request->hasParam("mqttuser", true) && request->hasParam("mqttpassword", true)) {
      Serial.println("Received MQTT settings data");
      String addr = request->getParam("mqttaddr", true)->value();
      int port = request->getParam("mqttport", true)->value().toInt();
      String user = request->getParam("mqttuser", true)->value();
      String pass = request->getParam("mqttpassword", true)->value();
      if (port == 0) {
        port = 1883;
      }
      //saveMQTTCredentials(addr, port, user, pass);
      request->send(200, "text/plain", "MQTT settings saved. Rebooting...");
      delay(1000);
      //ESP.restart();
    } else {
      request->send(400, "text/plain", "Invalid request");
    }
  });
#endif

  server.begin();
#if DEBUG == 1
  Serial.println(F("Configuration server is running"));
#endif
}

void setup() {
#if DEBUG == 1 || INFO == 1
  Serial.begin(115200);
  delay(1000);
#endif

  // Define character arrays for ssid and password
  char ssid[33];      // 32 characters + null terminator
  char password[65];  // 64 characters + null terminator
  // Call the function with the character arrays
  loadWifiCredentials(ssid, password);

// New - Load BOTtoken and CHAT_ID from EEPROM
#if TELE == 1 || T_WEB == 1
  char token[47];
  loadBotCredentials(token, CHAT_ID);
#endif
// Call the function with the character arrays
#if WAUTH == 1 || W_WEB == 1
  loadWebCredentials(webuser, webpass);
#endif

#if DEBUG == 1 || INFO == 1
  Serial.println(F("\nTry to load Data from EEPROM"));
  Serial.println(F("----------------------------------"));
  Serial.print(F("Wifi SSID: "));
  Serial.println(ssid);
  Serial.print(F("Wifi Pass: "));
  Serial.println(password);
#if WAUTH == 1 || W_WEB == 1
  Serial.print(F("Web User: "));
  Serial.println(webuser);
  Serial.print(F("Web Pass: "));
  Serial.println(webpass);
#endif
#if TELE == 1 || T_WEB == 1
  Serial.print(F("Tele Token: "));
  Serial.println(token);
  Serial.print(F("Tele ChatID: "));
  Serial.println(CHAT_ID);
#endif
  Serial.println(F("\n    Powerd by: DanielAmani.com"));
  Serial.println(F("----------------------------------"));
#endif
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
#if DEBUG == 1
    Serial.println(F("\nFailed to connect. Starting an access point..."));
#endif
    startAPMode();
#if DEBUG == 1 || INFO == 1
    Serial.println(F("\nSystem in AP Mode. IP Address: 192.168.4.1"));
    Serial.print(F("MAC Address: "));
    Serial.println(WiFi.macAddress());
#endif
    startServer();
  } else {
#if DEBUG == 1 || INFO == 1
    Serial.print(F("Connected to WiFi. IP Address: "));
    Serial.println(WiFi.localIP());
    Serial.print(F("MAC Address: "));
    Serial.println(WiFi.macAddress());
#endif
    startServer();
  }
#if TELE == 1 || T_WEB == 1
#ifdef ESP8266
  configTime(0, 0, "pool.ntp.org");  // get UTC time via NTP
  client.setTrustAnchors(&cert);     // Add root certificate for api.telegram.org
#endif
  // Initialize UniversalTelegramBot with loaded values
  bot = new UniversalTelegramBot(token, client);
#endif
}

void loop() {
#if TELE == 1 || T_WEB == 1
  checkNewMessages();
#endif
}
