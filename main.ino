#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <Ticker.h>
/*
   This example serves a "hello world" on a WLAN and a SoftAP at the same time.
   The SoftAP allow you to configure WLAN parameters at run time. They are not setup in the sketch but saved on EEPROM.
   Connect your computer or cell phone to wifi network ESP_ap with password 12345678. A popup may appear and it allow you to go to WLAN config. If it does not then navigate to http://192.168.4.1/wifi and config it there.
   Then wait for the module to connect to your wifi and take note of the WLAN IP it got. Then you can disconnect from ESP_ap and return to your regular WLAN.
   Now the ESP8266 is in your network. You can reach it through http://192.168.x.x/ (the IP you took note of) or maybe at http://esp8266.local too.
   This is a captive portal because through the softAP it will redirect any http request to http://192.168.4.1/
*/

/* Set these to your desired softAP credentials. They are not configurable at runtime */
#ifndef APSSID
#define APSSID "OpenLabTaipei"
#define APPSK  "12345678"
#endif

const char *softAP_ssid = APSSID;
const char *softAP_password = APPSK;

/* hostname for mDNS. Should work at least on windows. Try http://esp8266.local */
const char *myHostname = "thegeekman";

/* Don't set this wifi credentials. They are configurated at runtime and stored on EEPROM */
char ssid[32] = "";
char password[32] = "";

// DNS server
const byte DNS_PORT = 53;
DNSServer dnsServer;

// Web server
ESP8266WebServer server(80);

/* Soft AP network parameters */
//IPAddress apIP(192, 168, 4, 1);
IPAddress apIP(8, 8, 8, 8);
IPAddress netMsk(255, 255, 255, 0);


/** Should I connect to WLAN asap? */
boolean connect;

/** Last time I tried to connect to WLAN */
unsigned long lastConnectTry = 0;
unsigned long timeCount = 0;
/** Current WLAN status */
unsigned int status = WL_IDLE_STATUS;

Ticker flipper;

int count = 0;
int ring_start = 0;
void flip() {

  if(ring_start == 0) {
    flipper.attach(0.5, flip);
    return;
  }

  ++count;

  if(count%2 == 0 ){
    digitalWrite(2, HIGH); 
    flipper.attach(0.5, flip);
    ring_start = 0;
    count = 0;
  }
  else{
    digitalWrite(2, LOW); 
    flipper.attach(0.5, flip);
 
  }
  // when the counter reaches a certain value, start blinking like crazy
  /*
  if (count < 10) {
    if(count%2 == 0) digitalWrite(2, LOW); ; 
    else analogWrite(2, 128); 
    flipper.attach(1, flip);
  }
  // when the counter reaches yet another value, stop blinking
  else if (count == 10) {
    flipper.attach(1, flip);
    //flipper.detach();
    analogWrite(2, 0); 
    ring_start = 0;
    count = 0;
  }
  */
}


/** Is this an IP? */
boolean isIp(String str) {
  for (size_t i = 0; i < str.length(); i++) {
    int c = str.charAt(i);
    if (c != '.' && (c < '0' || c > '9')) {
      return false;
    }
  }
  return true;
}

/** IP to String? */
String toStringIp(IPAddress ip) {
  String res = "";
  for (int i = 0; i < 3; i++) {
    res += String((ip >> (8 * i)) & 0xFF) + ".";
  }
  res += String(((ip >> 8 * 3)) & 0xFF);
  return res;
}

/** Load WLAN credentials from EEPROM */
void loadCredentials() {
  EEPROM.begin(512);
  EEPROM.get(0, ssid);
  EEPROM.get(0 + sizeof(ssid), password);
  char ok[2 + 1];
  EEPROM.get(0 + sizeof(ssid) + sizeof(password), ok);
  EEPROM.end();
  if (String(ok) != String("OK")) {
    ssid[0] = 0;
    password[0] = 0;
  }
  Serial.println("Recovered credentials:");
  Serial.println(ssid);
  Serial.println(strlen(password) > 0 ? "********" : "<no password>");
}

/** Store WLAN credentials to EEPROM */
void saveCredentials() {
  EEPROM.begin(512);
  EEPROM.put(0, ssid);
  EEPROM.put(0 + sizeof(ssid), password);
  char ok[2 + 1] = "OK";
  EEPROM.put(0 + sizeof(ssid) + sizeof(password), ok);
  EEPROM.commit();
  EEPROM.end();
}

/** Handle root or redirect to captive portal */
void handleRoot() {
  if (captivePortal()) { // If caprive portal redirect instead of displaying the page.
    return;
  }
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");

  String Page;
  Page += F("<!DOCTYPE html><html><head> <style>body{display: flex; flex-direction: column; justify-content: center; align-items: center;}p{font-weight: bolder; font-size: 4em;}.btn{padding-top: 3em; display: flex; flex-direction: column; justify-content: center; align-items: flex-end;}</style></head><body style=\"background-color: rgba(139, 205, 195, 1);\"> <img src=\"data:image/svg+xml;base64,PD94bWwgdmVyc2lvbj0iMS4wIiBlbmNvZGluZz0iVVRGLTgiIHN0YW5kYWxvbmU9Im5vIj8+CjxzdmcKICAgeG1sbnM6ZGM9Imh0dHA6Ly9wdXJsLm9yZy9kYy9lbGVtZW50cy8xLjEvIgogICB4bWxuczpjYz0iaHR0cDovL2NyZWF0aXZlY29tbW9ucy5vcmcvbnMjIgogICB4bWxuczpyZGY9Imh0dHA6Ly93d3cudzMub3JnLzE5OTkvMDIvMjItcmRmLXN5bnRheC1ucyMiCiAgIHhtbG5zOnN2Zz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciCiAgIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyIKICAgd2lkdGg9IjEzMS43MDI4OG1tIgogICBoZWlnaHQ9IjUyLjE5NjI4OW1tIgogICB2aWV3Qm94PSIwIDAgMTMxLjcwMjg4IDUyLjE5NjI4OSIKICAgdmVyc2lvbj0iMS4xIgogICBpZD0ic3ZnOCI+CiAgPGRlZnMKICAgICBpZD0iZGVmczIiIC8+CiAgPG1ldGFkYXRhCiAgICAgaWQ9Im1ldGFkYXRhNSI+CiAgICA8cmRmOlJERj4KICAgICAgPGNjOldvcmsKICAgICAgICAgcmRmOmFib3V0PSIiPgogICAgICAgIDxkYzpmb3JtYXQ+aW1hZ2Uvc3ZnK3htbDwvZGM6Zm9ybWF0PgogICAgICAgIDxkYzp0eXBlCiAgICAgICAgICAgcmRmOnJlc291cmNlPSJodHRwOi8vcHVybC5vcmcvZGMvZGNtaXR5cGUvU3RpbGxJbWFnZSIgLz4KICAgICAgICA8ZGM6dGl0bGU+PC9kYzp0aXRsZT4KICAgICAgPC9jYzpXb3JrPgogICAgPC9yZGY6UkRGPgogIDwvbWV0YWRhdGE+CiAgPGcKICAgICB0cmFuc2Zvcm09InRyYW5zbGF0ZSgtNDEuMzA3NzA4LC00OS4yMjg2MzcpIgogICAgIGlkPSJsYXllcjEiPgogICAgPGcKICAgICAgIGlkPSJnNzY2MiIKICAgICAgIHRyYW5zZm9ybT0ibWF0cml4KDAuMjY0NTgzMzMsMCwwLDAuMjY0NTgzMzMsLTczLjg4NDgxNywtMjA4LjA0ODQ5KSI+CiAgICAgIDxnCiAgICAgICAgIHN0eWxlPSJmaWxsOiMwMDAwMDAiCiAgICAgICAgIHRyYW5zZm9ybT0idHJhbnNsYXRlKDExMS42NjYyMyw5NzIuNDIxMjIpIgogICAgICAgICBpZD0iZzc2MzAiPgogICAgICAgIDxwYXRoCiAgICAgICAgICAgaWQ9InBhdGg3NjMyIgogICAgICAgICAgIGQ9Im0gNjQwLjIyMzMzLDAuNDYxNjI0OTggMC4yODc0OCwxLjEyMTE3NDQyIC0xLjI5MzY2LC0wLjY4OTk1MzUxIC0xNS44NDAxOSwzOS4xODM2MDcxMSAtMjYuMDc0NDksLTAuODA0OTQ2IDAuMjI5OTksMS4wMzQ5MyAtMS4xNDk5MywtMC42MDM3MDkgLTYuMjY3MDcsMTUuNDA4OTYxIC00LjE2ODQ3LC0yLjM4NjA4OSAyLjkwMzU1LC0wLjAyODc1IC0yLjE1NjEsLTEzLjEzNzg2NCAtMzMuMTc1MjYsLTAuNDU5OTY5IDAuMTcyNDgsMC45Nzc0MzQgLTEuMDM0OTMsLTAuNTQ2MjEzIC01LjQ5MDg4LDEzLjU2OTA4NCAtMi4wNDExMSwtMTQuMTQ0MDQ2IC00NC43MzE5OCwtMS4zNzk5MDYgMC4wODYyLDAuODMzNjkzIC0wLjk3NzQzLC0wLjQ1OTk2OSAtNi4xMjMzNCwxMS4wMzkyNTYgLTUuMzQ3MTQsLTQ1LjQ1MDY4NDUgLTg0LjY5MTc4LC0xLjg2ODYyNCAwLjA4NjIsMi40NDM1ODUyIEwgNDAyLjM2MTc5LDIuODE4OTY0IDMyMy43MDcxLDE5Ny4yNDIxIGggNDYwLjkxNzY2IGwgMzYuNTM4NzgsLTk0LjQzNzM4IC0wLjQzMTIyLC0xLjMyMjQxIGggMC4wNTc1IEwgODExLjU2MTY5LDcyLjg3Nzk4OCA3ODcuNDk5NTYsNzIuNjE5MjU2IDc3Ni45NDkwMiw0NS42MjQ4MjcgaCAtMTQuNTQ2NTEgbCAwLjQ4ODcxLDEuMzc5OTA3IC0xLjY2NzM4LC0wLjg2MjQ0MiAtNC4zNjk3MSwxMC44MzgwMTkgLTQuMDUzNDgsLTExLjU1NjcyMSAtMzcuOTQ3NDQsLTAuNjAzNzA5IDAuMzczNzMsMS4yNjQ5MTUgLTEuNDM3NCwtMC43NDc0NSAtMTEuODcyOTUsMjkuMzUxNzcgTCA2ODEuNTYzMDQsNzQuNTE2NjMgNjYxLjg0MTg3LDAuNzc3ODUzNjUgWiIKICAgICAgICAgICBzdHlsZT0iZmlsbDojMDAwMDAwO2ZpbGwtb3BhY2l0eToxO3N0cm9rZTpub25lIiAvPgogICAgICAgIDxwYXRoCiAgICAgICAgICAgaWQ9InBhdGg3NjM0IgogICAgICAgICAgIGQ9Im0gNDAzLjMzOTMxLDEuNjY5MDQzNSAzLjM2MzUyLDkwLjQ3MDE0NzUgOTEuOTY1MDUsMS44MTExMjggLTEwLjYzNjc5LC05MC40MTI2NTE1IHogbSAyMS4yMTYwNiwxOC4xNDAwMjY1IDQ3LjY2NDI5LDEuMDA2MTgyIDUuMDU5NjYsNTQuNzY1MDU2IC00OS40MTc5MiwtMS40OTQ4OTkgeiIKICAgICAgICAgICBzdHlsZT0iZmlsbDojZmZmZmZmO2ZpbGwtb3BhY2l0eToxO3N0cm9rZTojZmZmZmZmO3N0cm9rZS1vcGFjaXR5OjEiIC8+CiAgICAgICAgPHBhdGgKICAgICAgICAgICBpZD0icGF0aDc2MzYiCiAgICAgICAgICAgZD0ibSA1MDAuMzkyNzYsMzcuNTc1MzcyIDYuODEzMjksNTcuMjM3Mzg5IDE3LjkxMDA0LDAuNDAyNDczIC0zLjIxOTc4LC0yNS42MTQ1MjMgMjcuNzk5MzcsMS4xNzg2NzEgLTQuNTcwOTQsLTMxLjgyNDEwNCB6IG0gMTguMjI2MjcsOC4zNjU2ODUgMTguMTY4NzcsMS4xNDk5MjMgMS41ODExNSwxNC4yMDE1NDIgLTE3LjczNzU2LC0wLjU3NDk2MSB6IgogICAgICAgICAgIHN0eWxlPSJmaWxsOiNmZmZmZmY7ZmlsbC1vcGFjaXR5OjE7c3Ryb2tlOiNmZmZmZmY7c3Ryb2tlLW9wYWNpdHk6MSIgLz4KICAgICAgICA8cGF0aAogICAgICAgICAgIGlkPSJwYXRoNzYzOCIKICAgICAgICAgICBkPSJtIDU1My41MjQ3OCwzOS4wOTcxMTMgOS43NjU0Miw1Ni41NDg2NjYgMzUuMDc0NjUsMC42NjUwNjQgLTEuOTQ5ODksLTEwLjQwNzc0NCAtMjIuMTg0ODMsLTAuNTcxMTMgLTEuOTE3NTksLTguODQ3NTUxIDE5LjAwNTQxLC0wLjM3NjM3IEwgNTg4LjY0NzgzLDYzLjU3ODM5NiA1NjkuMjUsNjMuMDg1MjUxIDU2Ny40NzIxMyw1Mi44ODgzOTQgNTg4Ljg0MjY1LDUyLjcwMDE3MSA1ODYuNjgxODMsMzkuNTUxMDEgNTUzLjUyNDc4LDM5LjA5NjgwNyBaIgogICAgICAgICAgIHN0eWxlPSJmaWxsOiNmZmZmZmY7ZmlsbC1vcGFjaXR5OjE7c3Ryb2tlOiNmZmZmZmY7c3Ryb2tlLW9wYWNpdHk6MSIgLz4KICAgICAgICA8cGF0aAogICAgICAgICAgIGlkPSJwYXRoNzY0MCIKICAgICAgICAgICBkPSJtIDU5Ny4zMTI3OSwzOS4yNjI0MTggMTIuNzE3NzcsNTcuNjg0MTg2IDE2LjAxMDc5LDAuMjI3MTQgLTEwLjY5MzM0LC00Mi4zNTEzNTggMTYuODEyMzIsMC41NDgyODggMTAuNTQ0MTMsNDIuMzE1NzQ4IDEzLjc5NDg2LDAuMjQ2NDcxIEwgNjQxLjY1OTY5LDQwLjYyNSA1OTcuMzEyNzksMzkuMjYyMzY3IHYgLTUuMWUtNSAyLjZlLTUgeiIKICAgICAgICAgICBzdHlsZT0iZmlsbDojZmZmZmZmO2ZpbGwtb3BhY2l0eToxO3N0cm9rZTojZmZmZmZmO3N0cm9rZS1vcGFjaXR5OjEiIC8+CiAgICAgICAgPHBhdGgKICAgICAgICAgICBpZD0icGF0aDc2NDIiCiAgICAgICAgICAgZD0iTSA2NDAuMjMwODcsMC40NTczMDk1NSA2NjUuNzUzODIsOTcuOTE0MDQ1IDcyMS4wNTY3OCw5OC44MjI0NzYgNzE0Ljg3OTM1LDc0LjgwNzQwMSA2ODEuNTczMSw3NC41MDI0NTQgNjYxLjg1NDExLDAuNzc4NTg1OTIgNjQwLjIzMDg3LDAuNDU3NDExNjYgWiIKICAgICAgICAgICBzdHlsZT0iZmlsbDojZmZmZmZmO2ZpbGwtb3BhY2l0eToxO3N0cm9rZTojZmZmZmZmO3N0cm9rZS1vcGFjaXR5OjEiIC8+CiAgICAgICAgPHBhdGgKICAgICAgICAgICBpZD0icGF0aDc2NDQiCiAgICAgICAgICAgZD0ibSA3MTQuODUzMjksNDQuODE5ODgzIDE2LjA0MTQyLDU0Ljk2NjI5MiAxMS41ODU0NywwLjI1ODczNSAtNi44OTk1MywtMjIuODI1OTYzIDE3LjE2MjU5LDAuMTE0OTkzIDguMjc5NDQsMjIuODgzNDYgMTEuMDY4LDAuMjAxMjMgLTE5LjI4OTk1LC01NC45OTUwMzggeiBtIDE0LjA4NjU1LDguMzk0NDM0IDE2LjkzMjYxLDAuNDU5OTY5IDQuOTQ0NjcsMTUuMTc4OTc2IC0xNy43NjYzLC0wLjQ1OTk2OSB6IgogICAgICAgICAgIHN0eWxlPSJmaWxsOiNmZmZmZmY7ZmlsbC1vcGFjaXR5OjE7c3Ryb2tlOiNmZmZmZmY7c3Ryb2tlLW9wYWNpdHk6MSIgLz4KICAgICAgICA8cGF0aAogICAgICAgICAgIGlkPSJwYXRoNzY0NiIKICAgICAgICAgICBkPSJtIDc2Mi40MDI1OSw0NS42MjQ4MjkgMTkuNTQ4NjgsNTQuOTY2MjkxIDM4LjgzODYzLDAuODkxMTkgLTkuMjI4MTMsLTI4LjYwNDMyIC0yNC4wNjIxMywtMC4yNTg3MzIgLTEwLjU1MDU0LC0yNi45OTQ0MjkgeiBtIDI3LjUxMTg5LDM1LjE4NzYyNiAxNi4zMjg5LDAuMjU4NzMzIDMuMDQ3MjksMTAuNTUwNTM4IC0xNi4wNDE0MSwtMC40ODg3MTcgeiIKICAgICAgICAgICBzdHlsZT0iZmlsbDojZmZmZmZmO2ZpbGwtb3BhY2l0eToxO3N0cm9rZTojZmZmZmZmO3N0cm9rZS1vcGFjaXR5OjEiIC8+CiAgICAgICAgPHBhdGgKICAgICAgICAgICBpZD0icGF0aDc2NDgiCiAgICAgICAgICAgZD0ibSA1MjkuMjI3MDYsMTA1LjEzMzMxIDkuMzE0MzgsNzguMDc5NzQgMTguMjgzNzYsLTAuMjg3NDggLTMuNzY1OTksLTMyLjM3MDMyIDI1LjgxNTc2LDAuNjYxMjEgNi40OTcwNiwyOS42OTY3NCAxNy4yMjAwOSwwLjcxODcgLTE4LjgwMTI0LC03NS42MzYxNCB6IG0gMTkuMzE4NywxMS44NDQyMSAyMy4xOTk2OSwwLjg5MTE5IDQuODI5NjcsMjEuNTg5NzkgLTI0Ljg2NzA3LC0wLjY2MTIxIHoiCiAgICAgICAgICAgc3R5bGU9ImZpbGw6I2ZmZmZmZjtmaWxsLW9wYWNpdHk6MTtzdHJva2U6I2ZmZmZmZjtzdHJva2Utb3BhY2l0eToxIiAvPgogICAgICAgIDxwYXRoCiAgICAgICAgICAgaWQ9InBhdGg3NjUwIgogICAgICAgICAgIGQ9Im0gNjAwLjU1NjA0LDEwNy40OTMzMSAxMi44Mjc2LDcxLjY5Mzc5IDE5LjczNTE0LDAuNDIxNjEgLTEzLjcxMjAyLC03MS4zMTAxMSB6IgogICAgICAgICAgIHN0eWxlPSJmaWxsOiNmZmZmZmY7ZmlsbC1vcGFjaXR5OjE7c3Ryb2tlOiNmZmZmZmY7c3Ryb2tlLW9wYWNpdHk6MSIgLz4KICAgICAgICA8cGF0aAogICAgICAgICAgIGlkPSJwYXRoNzY1MiIKICAgICAgICAgICBkPSJtIDYzMS43NzE0LDEwNy40MDQ0MSAxMS43MjkyMSw3MS45Mjc2NSAyMi4xNjQ3NSwwLjg5MTE5IC0yLjk4OTgsLTI2Ljc5MzE5IDM1LjEzMDEzLC0wLjU3NDk2IC0xLjc1MzYzLC00My40NjcwNyB6IG0gMjUuOTAyLDEyLjU2MjkgMjIuODgzNDYsMC44NjI0NSAxLjYwOTg5LDE2LjE1NjQxIC0yMi4yNzk3NSwtMS4xNzg2NyB6IgogICAgICAgICAgIHN0eWxlPSJmaWxsOiNmZmZmZmY7ZmlsbC1vcGFjaXR5OjE7c3Ryb2tlOiNmZmZmZmY7c3Ryb2tlLW9wYWNpdHk6MSIgLz4KICAgICAgICA8cGF0aAogICAgICAgICAgIGlkPSJwYXRoNzY1NCIKICAgICAgICAgICBkPSJtIDcxMC4zNDc5LDE4NS44NTM2MiA1MS41NDQxNCwxLjUxNzAxIC0zLjkyNDkxLC0xNi45MjUxIC0yOS42NDg3NCwtMC4yNjAyMyAtMi43NTY2MywtMTMuNTYwNCAyMS43MTcxMywwLjMwMDU0IC0yLjcxNjY3LC0xNy43ODc0OCAtMjEuMDQwMTYsLTAuOTkwNTEgLTIuOTE1ODMsLTE1LjI5MjAxIDI1Ljk2MjM1LDEuMDMzOTggLTIuMjc5MDYsLTE0LjY5NjEgLTQzLjA5MDQ2LC0xLjI5NjAxIHoiCiAgICAgICAgICAgc3R5bGU9ImZpbGw6I2ZmZmZmZjtmaWxsLW9wYWNpdHk6MTtzdHJva2U6I2ZmZmZmZjtzdHJva2Utb3BhY2l0eToxIiAvPgogICAgICAgIDxwYXRoCiAgICAgICAgICAgaWQ9InBhdGg3NjU2IgogICAgICAgICAgIGQ9Im0gNzUwLjM3MjU5LDEwOS41NDE4NyAxMy44MzI5Myw3Ni42MjUxIDIwLjMxNjI4LDAuMjQ5NTEgLTEzLjcxMjAyLC03NS4zMjkzMyAtMjAuNDM3MTksLTEuNTQ1NjcgdiAxLjZlLTQgeiIKICAgICAgICAgICBzdHlsZT0iZmlsbDojZmZmZmZmO2ZpbGwtb3BhY2l0eToxO3N0cm9rZTojZmZmZmZmO3N0cm9rZS1vcGFjaXR5OjEiIC8+CiAgICAgICAgPHBhdGgKICAgICAgICAgICBpZD0icGF0aDc2NTgiCiAgICAgICAgICAgZD0ibSA0MzQuMzg2OTYsMTA1LjU2MzY3IDEuNTk5NzgsMjEuMDI1MTMgMzcuNzA4MSwxLjE0MjY2IDMuNDI4MDIsNjEuNDc1NjQgMjMuNTM4OTcsLTAuNjg1NiAtNS4wMjc3NCwtNjEuMDE4NTggMjguNzk1MjUsMC45MTQxNCAtMy4xOTk0MywtMjIuMzk2MzEgeiIKICAgICAgICAgICBzdHlsZT0iZmlsbDojZmZmZmZmO2ZpbGwtb3BhY2l0eToxO3N0cm9rZTojZmZmZmZmO3N0cm9rZS1vcGFjaXR5OjEiIC8+CiAgICAgIDwvZz4KICAgIDwvZz4KICA8L2c+Cjwvc3ZnPgo=\"> <p>&#20320;, &#28212;&#26395;&#21147;&#37327;&#21966;?</p><p style=\"color:white\">&#25353;&#19979;&#38283;&#38364;!</p><svg width=\"100%\" height=\"100px\" viewBox=\"0 0 400 400\" xmlns=\"http://www.w3.org/2000/svg\"> <path d=\"M 100 100 L 300 100 L 200 300 z\" fill=\"white\" stroke=\"Blue\" stroke-width=\"3\"/> </svg> <svg width=\"100%\" height=\"100px\" viewBox=\"0 0 400 400\" xmlns=\"http://www.w3.org/2000/svg\"> <path d=\"M 100 100 L 300 100 L 200 300 z\" fill=\"white\" stroke=\"Blue\" stroke-width=\"3\"/> </svg> <svg width=\"100%\" height=\"100px\" viewBox=\"0 0 400 400\" xmlns=\"http://www.w3.org/2000/svg\"> <path d=\"M 100 100 L 300 100 L 200 300 z\" fill=\"white\" stroke=\"Blue\" stroke-width=\"3\"/> </svg> <img class=\"btn\" src=\"data:image/svg+xml;base64,PD94bWwgdmVyc2lvbj0iMS4wIiBlbmNvZGluZz0iVVRGLTgiIHN0YW5kYWxvbmU9Im5vIj8+CjxzdmcKICAgeG1sbnM6ZGM9Imh0dHA6Ly9wdXJsLm9yZy9kYy9lbGVtZW50cy8xLjEvIgogICB4bWxuczpjYz0iaHR0cDovL2NyZWF0aXZlY29tbW9ucy5vcmcvbnMjIgogICB4bWxuczpyZGY9Imh0dHA6Ly93d3cudzMub3JnLzE5OTkvMDIvMjItcmRmLXN5bnRheC1ucyMiCiAgIHhtbG5zOnN2Zz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciCiAgIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyIKICAgeG1sbnM6eGxpbms9Imh0dHA6Ly93d3cudzMub3JnLzE5OTkveGxpbmsiCiAgIHdpZHRoPSI1Mi43Mjk5MTZtbSIKICAgaGVpZ2h0PSI1Mi43Mjk5MTZtbSIKICAgdmlld0JveD0iMCAwIDUyLjcyOTkxNiA1Mi43Mjk5MTYiCiAgIHZlcnNpb249IjEuMSIKICAgaWQ9InN2ZzgiPgogIDxkZWZzCiAgICAgaWQ9ImRlZnMyIj4KICAgIDxsaW5lYXJHcmFkaWVudAogICAgICAgaWQ9ImxpbmVhckdyYWRpZW50MjA5MiI+CiAgICAgIDxzdG9wCiAgICAgICAgIHN0eWxlPSJzdG9wLWNvbG9yOiMwMDdjODE7c3RvcC1vcGFjaXR5OjE7IgogICAgICAgICBvZmZzZXQ9IjAiCiAgICAgICAgIGlkPSJzdG9wMjA4OCIgLz4KICAgICAgPHN0b3AKICAgICAgICAgc3R5bGU9InN0b3AtY29sb3I6IzAwN2M4MTtzdG9wLW9wYWNpdHk6MDsiCiAgICAgICAgIG9mZnNldD0iMSIKICAgICAgICAgaWQ9InN0b3AyMDkwIiAvPgogICAgPC9saW5lYXJHcmFkaWVudD4KICAgIDxsaW5lYXJHcmFkaWVudAogICAgICAgaWQ9ImxpbmVhckdyYWRpZW50MjA4NCI+CiAgICAgIDxzdG9wCiAgICAgICAgIHN0eWxlPSJzdG9wLWNvbG9yOiNmZmZmZmY7c3RvcC1vcGFjaXR5OjE7IgogICAgICAgICBvZmZzZXQ9IjAiCiAgICAgICAgIGlkPSJzdG9wMjA4MCIgLz4KICAgICAgPHN0b3AKICAgICAgICAgc3R5bGU9InN0b3AtY29sb3I6I2ZmZmZmZjtzdG9wLW9wYWNpdHk6MDsiCiAgICAgICAgIG9mZnNldD0iMSIKICAgICAgICAgaWQ9InN0b3AyMDgyIiAvPgogICAgPC9saW5lYXJHcmFkaWVudD4KICAgIDxsaW5lYXJHcmFkaWVudAogICAgICAgZ3JhZGllbnRUcmFuc2Zvcm09InRyYW5zbGF0ZSgyMi41MTM1ODUsLTMyMS45NDQzNykiCiAgICAgICB4bGluazpocmVmPSIjbGluZWFyR3JhZGllbnQyMDg0IgogICAgICAgaWQ9ImxpbmVhckdyYWRpZW50MjA4NiIKICAgICAgIHgxPSI3MDYuMjUyNzUiCiAgICAgICB5MT0iMTYxNC45OTE4IgogICAgICAgeDI9Ijc1OC4wNDA3MSIKICAgICAgIHkyPSIxNjcyLjA5MTEiCiAgICAgICBncmFkaWVudFVuaXRzPSJ1c2VyU3BhY2VPblVzZSIgLz4KICAgIDxsaW5lYXJHcmFkaWVudAogICAgICAgZ3JhZGllbnRUcmFuc2Zvcm09InRyYW5zbGF0ZSgyMi41MTM1ODUsLTMyMS45NDQzNykiCiAgICAgICB4bGluazpocmVmPSIjbGluZWFyR3JhZGllbnQyMDkyIgogICAgICAgaWQ9ImxpbmVhckdyYWRpZW50MjA5NCIKICAgICAgIHgxPSI2NTEuMTMzMTIiCiAgICAgICB5MT0iMTY5OC41ODM3IgogICAgICAgeDI9IjgwMy4wMzk0MyIKICAgICAgIHkyPSIxNDc3LjQ0MDQiCiAgICAgICBncmFkaWVudFVuaXRzPSJ1c2VyU3BhY2VPblVzZSIgLz4KICA8L2RlZnM+CiAgPG1ldGFkYXRhCiAgICAgaWQ9Im1ldGFkYXRhNSI+CiAgICA8cmRmOlJERj4KICAgICAgPGNjOldvcmsKICAgICAgICAgcmRmOmFib3V0PSIiPgogICAgICAgIDxkYzpmb3JtYXQ+aW1hZ2Uvc3ZnK3htbDwvZGM6Zm9ybWF0PgogICAgICAgIDxkYzp0eXBlCiAgICAgICAgICAgcmRmOnJlc291cmNlPSJodHRwOi8vcHVybC5vcmcvZGMvZGNtaXR5cGUvU3RpbGxJbWFnZSIgLz4KICAgICAgICA8ZGM6dGl0bGU+PC9kYzp0aXRsZT4KICAgICAgPC9jYzpXb3JrPgogICAgPC9yZGY6UkRGPgogIDwvbWV0YWRhdGE+CiAgPGcKICAgICB0cmFuc2Zvcm09InRyYW5zbGF0ZSgtODcuMzA0NDc2LC0xMDguNDU3MjMpIgogICAgIGlkPSJsYXllcjEiPgogICAgPGcKICAgICAgIGlkPSJnNzY2MiIKICAgICAgIHRyYW5zZm9ybT0ibWF0cml4KDAuMjY0NTgzMzMsMCwwLDAuMjY0NTgzMzMsLTgyLjIyNDIyNywtMTk5LjExMzQpIj4KICAgICAgPGNpcmNsZQogICAgICAgICBzdHlsZT0iZmlsbDp1cmwoI2xpbmVhckdyYWRpZW50MjA4Nik7ZmlsbC1vcGFjaXR5OjE7c3Ryb2tlOnVybCgjbGluZWFyR3JhZGllbnQyMDk0KTtzdHJva2Utd2lkdGg6MzAuMjM2MjAwMzM7c3Ryb2tlLWxpbmVjYXA6cm91bmQ7c3Ryb2tlLWxpbmVqb2luOnJvdW5kO3N0cm9rZS1taXRlcmxpbWl0OjQ7c3Ryb2tlLWRhc2hhcnJheTpub25lO3N0cm9rZS1vcGFjaXR5OjEiCiAgICAgICAgIGlkPSJwYXRoMjA2NyIKICAgICAgICAgY3k9IjEyNjIuMTE4OCIKICAgICAgICAgY3g9Ijc0MC4zODU1IgogICAgICAgICByPSI4NC41Mjg5ODQiIC8+CiAgICA8L2c+CiAgPC9nPgo8L3N2Zz4K\" onclick=\"(function(){var xmlhttp=new XMLHttpRequest();xmlhttp.onreadystatechange=function(){console.log('Rining');};xmlhttp.open('POST','/rining','');xmlhttp.send();alert('Ringing');})();return false;\"></img></body></html>");
  
  //Page += F("<!DOCTYPE html><html><head> <style>body{display: flex; flex-direction: column; justify-content: center; align-items: center;}p{font-weight: bolder; font-size: 4em;}.btn{padding-top: 3em; display: flex; flex-direction: column; justify-content: center; align-items: flex-end;}</style></head><body style=\"background-color: rgba(139, 205, 195, 1);\"> <img src=\"data:image/svg+xml;base64,PD94bWwgdmVyc2lvbj0iMS4wIiBlbmNvZGluZz0iVVRGLTgiIHN0YW5kYWxvbmU9Im5vIj8+CjxzdmcKICAgeG1sbnM6ZGM9Imh0dHA6Ly9wdXJsLm9yZy9kYy9lbGVtZW50cy8xLjEvIgogICB4bWxuczpjYz0iaHR0cDovL2NyZWF0aXZlY29tbW9ucy5vcmcvbnMjIgogICB4bWxuczpyZGY9Imh0dHA6Ly93d3cudzMub3JnLzE5OTkvMDIvMjItcmRmLXN5bnRheC1ucyMiCiAgIHhtbG5zOnN2Zz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciCiAgIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyIKICAgd2lkdGg9IjEzMS43MDI4OG1tIgogICBoZWlnaHQ9IjUyLjE5NjI4OW1tIgogICB2aWV3Qm94PSIwIDAgMTMxLjcwMjg4IDUyLjE5NjI4OSIKICAgdmVyc2lvbj0iMS4xIgogICBpZD0ic3ZnOCI+CiAgPGRlZnMKICAgICBpZD0iZGVmczIiIC8+CiAgPG1ldGFkYXRhCiAgICAgaWQ9Im1ldGFkYXRhNSI+CiAgICA8cmRmOlJERj4KICAgICAgPGNjOldvcmsKICAgICAgICAgcmRmOmFib3V0PSIiPgogICAgICAgIDxkYzpmb3JtYXQ+aW1hZ2Uvc3ZnK3htbDwvZGM6Zm9ybWF0PgogICAgICAgIDxkYzp0eXBlCiAgICAgICAgICAgcmRmOnJlc291cmNlPSJodHRwOi8vcHVybC5vcmcvZGMvZGNtaXR5cGUvU3RpbGxJbWFnZSIgLz4KICAgICAgICA8ZGM6dGl0bGU+PC9kYzp0aXRsZT4KICAgICAgPC9jYzpXb3JrPgogICAgPC9yZGY6UkRGPgogIDwvbWV0YWRhdGE+CiAgPGcKICAgICB0cmFuc2Zvcm09InRyYW5zbGF0ZSgtNDEuMzA3NzA4LC00OS4yMjg2MzcpIgogICAgIGlkPSJsYXllcjEiPgogICAgPGcKICAgICAgIGlkPSJnNzY2MiIKICAgICAgIHRyYW5zZm9ybT0ibWF0cml4KDAuMjY0NTgzMzMsMCwwLDAuMjY0NTgzMzMsLTczLjg4NDgxNywtMjA4LjA0ODQ5KSI+CiAgICAgIDxnCiAgICAgICAgIHN0eWxlPSJmaWxsOiMwMDAwMDAiCiAgICAgICAgIHRyYW5zZm9ybT0idHJhbnNsYXRlKDExMS42NjYyMyw5NzIuNDIxMjIpIgogICAgICAgICBpZD0iZzc2MzAiPgogICAgICAgIDxwYXRoCiAgICAgICAgICAgaWQ9InBhdGg3NjMyIgogICAgICAgICAgIGQ9Im0gNjQwLjIyMzMzLDAuNDYxNjI0OTggMC4yODc0OCwxLjEyMTE3NDQyIC0xLjI5MzY2LC0wLjY4OTk1MzUxIC0xNS44NDAxOSwzOS4xODM2MDcxMSAtMjYuMDc0NDksLTAuODA0OTQ2IDAuMjI5OTksMS4wMzQ5MyAtMS4xNDk5MywtMC42MDM3MDkgLTYuMjY3MDcsMTUuNDA4OTYxIC00LjE2ODQ3LC0yLjM4NjA4OSAyLjkwMzU1LC0wLjAyODc1IC0yLjE1NjEsLTEzLjEzNzg2NCAtMzMuMTc1MjYsLTAuNDU5OTY5IDAuMTcyNDgsMC45Nzc0MzQgLTEuMDM0OTMsLTAuNTQ2MjEzIC01LjQ5MDg4LDEzLjU2OTA4NCAtMi4wNDExMSwtMTQuMTQ0MDQ2IC00NC43MzE5OCwtMS4zNzk5MDYgMC4wODYyLDAuODMzNjkzIC0wLjk3NzQzLC0wLjQ1OTk2OSAtNi4xMjMzNCwxMS4wMzkyNTYgLTUuMzQ3MTQsLTQ1LjQ1MDY4NDUgLTg0LjY5MTc4LC0xLjg2ODYyNCAwLjA4NjIsMi40NDM1ODUyIEwgNDAyLjM2MTc5LDIuODE4OTY0IDMyMy43MDcxLDE5Ny4yNDIxIGggNDYwLjkxNzY2IGwgMzYuNTM4NzgsLTk0LjQzNzM4IC0wLjQzMTIyLC0xLjMyMjQxIGggMC4wNTc1IEwgODExLjU2MTY5LDcyLjg3Nzk4OCA3ODcuNDk5NTYsNzIuNjE5MjU2IDc3Ni45NDkwMiw0NS42MjQ4MjcgaCAtMTQuNTQ2NTEgbCAwLjQ4ODcxLDEuMzc5OTA3IC0xLjY2NzM4LC0wLjg2MjQ0MiAtNC4zNjk3MSwxMC44MzgwMTkgLTQuMDUzNDgsLTExLjU1NjcyMSAtMzcuOTQ3NDQsLTAuNjAzNzA5IDAuMzczNzMsMS4yNjQ5MTUgLTEuNDM3NCwtMC43NDc0NSAtMTEuODcyOTUsMjkuMzUxNzcgTCA2ODEuNTYzMDQsNzQuNTE2NjMgNjYxLjg0MTg3LDAuNzc3ODUzNjUgWiIKICAgICAgICAgICBzdHlsZT0iZmlsbDojMDAwMDAwO2ZpbGwtb3BhY2l0eToxO3N0cm9rZTpub25lIiAvPgogICAgICAgIDxwYXRoCiAgICAgICAgICAgaWQ9InBhdGg3NjM0IgogICAgICAgICAgIGQ9Im0gNDAzLjMzOTMxLDEuNjY5MDQzNSAzLjM2MzUyLDkwLjQ3MDE0NzUgOTEuOTY1MDUsMS44MTExMjggLTEwLjYzNjc5LC05MC40MTI2NTE1IHogbSAyMS4yMTYwNiwxOC4xNDAwMjY1IDQ3LjY2NDI5LDEuMDA2MTgyIDUuMDU5NjYsNTQuNzY1MDU2IC00OS40MTc5MiwtMS40OTQ4OTkgeiIKICAgICAgICAgICBzdHlsZT0iZmlsbDojZmZmZmZmO2ZpbGwtb3BhY2l0eToxO3N0cm9rZTojZmZmZmZmO3N0cm9rZS1vcGFjaXR5OjEiIC8+CiAgICAgICAgPHBhdGgKICAgICAgICAgICBpZD0icGF0aDc2MzYiCiAgICAgICAgICAgZD0ibSA1MDAuMzkyNzYsMzcuNTc1MzcyIDYuODEzMjksNTcuMjM3Mzg5IDE3LjkxMDA0LDAuNDAyNDczIC0zLjIxOTc4LC0yNS42MTQ1MjMgMjcuNzk5MzcsMS4xNzg2NzEgLTQuNTcwOTQsLTMxLjgyNDEwNCB6IG0gMTguMjI2MjcsOC4zNjU2ODUgMTguMTY4NzcsMS4xNDk5MjMgMS41ODExNSwxNC4yMDE1NDIgLTE3LjczNzU2LC0wLjU3NDk2MSB6IgogICAgICAgICAgIHN0eWxlPSJmaWxsOiNmZmZmZmY7ZmlsbC1vcGFjaXR5OjE7c3Ryb2tlOiNmZmZmZmY7c3Ryb2tlLW9wYWNpdHk6MSIgLz4KICAgICAgICA8cGF0aAogICAgICAgICAgIGlkPSJwYXRoNzYzOCIKICAgICAgICAgICBkPSJtIDU1My41MjQ3OCwzOS4wOTcxMTMgOS43NjU0Miw1Ni41NDg2NjYgMzUuMDc0NjUsMC42NjUwNjQgLTEuOTQ5ODksLTEwLjQwNzc0NCAtMjIuMTg0ODMsLTAuNTcxMTMgLTEuOTE3NTksLTguODQ3NTUxIDE5LjAwNTQxLC0wLjM3NjM3IEwgNTg4LjY0NzgzLDYzLjU3ODM5NiA1NjkuMjUsNjMuMDg1MjUxIDU2Ny40NzIxMyw1Mi44ODgzOTQgNTg4Ljg0MjY1LDUyLjcwMDE3MSA1ODYuNjgxODMsMzkuNTUxMDEgNTUzLjUyNDc4LDM5LjA5NjgwNyBaIgogICAgICAgICAgIHN0eWxlPSJmaWxsOiNmZmZmZmY7ZmlsbC1vcGFjaXR5OjE7c3Ryb2tlOiNmZmZmZmY7c3Ryb2tlLW9wYWNpdHk6MSIgLz4KICAgICAgICA8cGF0aAogICAgICAgICAgIGlkPSJwYXRoNzY0MCIKICAgICAgICAgICBkPSJtIDU5Ny4zMTI3OSwzOS4yNjI0MTggMTIuNzE3NzcsNTcuNjg0MTg2IDE2LjAxMDc5LDAuMjI3MTQgLTEwLjY5MzM0LC00Mi4zNTEzNTggMTYuODEyMzIsMC41NDgyODggMTAuNTQ0MTMsNDIuMzE1NzQ4IDEzLjc5NDg2LDAuMjQ2NDcxIEwgNjQxLjY1OTY5LDQwLjYyNSA1OTcuMzEyNzksMzkuMjYyMzY3IHYgLTUuMWUtNSAyLjZlLTUgeiIKICAgICAgICAgICBzdHlsZT0iZmlsbDojZmZmZmZmO2ZpbGwtb3BhY2l0eToxO3N0cm9rZTojZmZmZmZmO3N0cm9rZS1vcGFjaXR5OjEiIC8+CiAgICAgICAgPHBhdGgKICAgICAgICAgICBpZD0icGF0aDc2NDIiCiAgICAgICAgICAgZD0iTSA2NDAuMjMwODcsMC40NTczMDk1NSA2NjUuNzUzODIsOTcuOTE0MDQ1IDcyMS4wNTY3OCw5OC44MjI0NzYgNzE0Ljg3OTM1LDc0LjgwNzQwMSA2ODEuNTczMSw3NC41MDI0NTQgNjYxLjg1NDExLDAuNzc4NTg1OTIgNjQwLjIzMDg3LDAuNDU3NDExNjYgWiIKICAgICAgICAgICBzdHlsZT0iZmlsbDojZmZmZmZmO2ZpbGwtb3BhY2l0eToxO3N0cm9rZTojZmZmZmZmO3N0cm9rZS1vcGFjaXR5OjEiIC8+CiAgICAgICAgPHBhdGgKICAgICAgICAgICBpZD0icGF0aDc2NDQiCiAgICAgICAgICAgZD0ibSA3MTQuODUzMjksNDQuODE5ODgzIDE2LjA0MTQyLDU0Ljk2NjI5MiAxMS41ODU0NywwLjI1ODczNSAtNi44OTk1MywtMjIuODI1OTYzIDE3LjE2MjU5LDAuMTE0OTkzIDguMjc5NDQsMjIuODgzNDYgMTEuMDY4LDAuMjAxMjMgLTE5LjI4OTk1LC01NC45OTUwMzggeiBtIDE0LjA4NjU1LDguMzk0NDM0IDE2LjkzMjYxLDAuNDU5OTY5IDQuOTQ0NjcsMTUuMTc4OTc2IC0xNy43NjYzLC0wLjQ1OTk2OSB6IgogICAgICAgICAgIHN0eWxlPSJmaWxsOiNmZmZmZmY7ZmlsbC1vcGFjaXR5OjE7c3Ryb2tlOiNmZmZmZmY7c3Ryb2tlLW9wYWNpdHk6MSIgLz4KICAgICAgICA8cGF0aAogICAgICAgICAgIGlkPSJwYXRoNzY0NiIKICAgICAgICAgICBkPSJtIDc2Mi40MDI1OSw0NS42MjQ4MjkgMTkuNTQ4NjgsNTQuOTY2MjkxIDM4LjgzODYzLDAuODkxMTkgLTkuMjI4MTMsLTI4LjYwNDMyIC0yNC4wNjIxMywtMC4yNTg3MzIgLTEwLjU1MDU0LC0yNi45OTQ0MjkgeiBtIDI3LjUxMTg5LDM1LjE4NzYyNiAxNi4zMjg5LDAuMjU4NzMzIDMuMDQ3MjksMTAuNTUwNTM4IC0xNi4wNDE0MSwtMC40ODg3MTcgeiIKICAgICAgICAgICBzdHlsZT0iZmlsbDojZmZmZmZmO2ZpbGwtb3BhY2l0eToxO3N0cm9rZTojZmZmZmZmO3N0cm9rZS1vcGFjaXR5OjEiIC8+CiAgICAgICAgPHBhdGgKICAgICAgICAgICBpZD0icGF0aDc2NDgiCiAgICAgICAgICAgZD0ibSA1MjkuMjI3MDYsMTA1LjEzMzMxIDkuMzE0MzgsNzguMDc5NzQgMTguMjgzNzYsLTAuMjg3NDggLTMuNzY1OTksLTMyLjM3MDMyIDI1LjgxNTc2LDAuNjYxMjEgNi40OTcwNiwyOS42OTY3NCAxNy4yMjAwOSwwLjcxODcgLTE4LjgwMTI0LC03NS42MzYxNCB6IG0gMTkuMzE4NywxMS44NDQyMSAyMy4xOTk2OSwwLjg5MTE5IDQuODI5NjcsMjEuNTg5NzkgLTI0Ljg2NzA3LC0wLjY2MTIxIHoiCiAgICAgICAgICAgc3R5bGU9ImZpbGw6I2ZmZmZmZjtmaWxsLW9wYWNpdHk6MTtzdHJva2U6I2ZmZmZmZjtzdHJva2Utb3BhY2l0eToxIiAvPgogICAgICAgIDxwYXRoCiAgICAgICAgICAgaWQ9InBhdGg3NjUwIgogICAgICAgICAgIGQ9Im0gNjAwLjU1NjA0LDEwNy40OTMzMSAxMi44Mjc2LDcxLjY5Mzc5IDE5LjczNTE0LDAuNDIxNjEgLTEzLjcxMjAyLC03MS4zMTAxMSB6IgogICAgICAgICAgIHN0eWxlPSJmaWxsOiNmZmZmZmY7ZmlsbC1vcGFjaXR5OjE7c3Ryb2tlOiNmZmZmZmY7c3Ryb2tlLW9wYWNpdHk6MSIgLz4KICAgICAgICA8cGF0aAogICAgICAgICAgIGlkPSJwYXRoNzY1MiIKICAgICAgICAgICBkPSJtIDYzMS43NzE0LDEwNy40MDQ0MSAxMS43MjkyMSw3MS45Mjc2NSAyMi4xNjQ3NSwwLjg5MTE5IC0yLjk4OTgsLTI2Ljc5MzE5IDM1LjEzMDEzLC0wLjU3NDk2IC0xLjc1MzYzLC00My40NjcwNyB6IG0gMjUuOTAyLDEyLjU2MjkgMjIuODgzNDYsMC44NjI0NSAxLjYwOTg5LDE2LjE1NjQxIC0yMi4yNzk3NSwtMS4xNzg2NyB6IgogICAgICAgICAgIHN0eWxlPSJmaWxsOiNmZmZmZmY7ZmlsbC1vcGFjaXR5OjE7c3Ryb2tlOiNmZmZmZmY7c3Ryb2tlLW9wYWNpdHk6MSIgLz4KICAgICAgICA8cGF0aAogICAgICAgICAgIGlkPSJwYXRoNzY1NCIKICAgICAgICAgICBkPSJtIDcxMC4zNDc5LDE4NS44NTM2MiA1MS41NDQxNCwxLjUxNzAxIC0zLjkyNDkxLC0xNi45MjUxIC0yOS42NDg3NCwtMC4yNjAyMyAtMi43NTY2MywtMTMuNTYwNCAyMS43MTcxMywwLjMwMDU0IC0yLjcxNjY3LC0xNy43ODc0OCAtMjEuMDQwMTYsLTAuOTkwNTEgLTIuOTE1ODMsLTE1LjI5MjAxIDI1Ljk2MjM1LDEuMDMzOTggLTIuMjc5MDYsLTE0LjY5NjEgLTQzLjA5MDQ2LC0xLjI5NjAxIHoiCiAgICAgICAgICAgc3R5bGU9ImZpbGw6I2ZmZmZmZjtmaWxsLW9wYWNpdHk6MTtzdHJva2U6I2ZmZmZmZjtzdHJva2Utb3BhY2l0eToxIiAvPgogICAgICAgIDxwYXRoCiAgICAgICAgICAgaWQ9InBhdGg3NjU2IgogICAgICAgICAgIGQ9Im0gNzUwLjM3MjU5LDEwOS41NDE4NyAxMy44MzI5Myw3Ni42MjUxIDIwLjMxNjI4LDAuMjQ5NTEgLTEzLjcxMjAyLC03NS4zMjkzMyAtMjAuNDM3MTksLTEuNTQ1NjcgdiAxLjZlLTQgeiIKICAgICAgICAgICBzdHlsZT0iZmlsbDojZmZmZmZmO2ZpbGwtb3BhY2l0eToxO3N0cm9rZTojZmZmZmZmO3N0cm9rZS1vcGFjaXR5OjEiIC8+CiAgICAgICAgPHBhdGgKICAgICAgICAgICBpZD0icGF0aDc2NTgiCiAgICAgICAgICAgZD0ibSA0MzQuMzg2OTYsMTA1LjU2MzY3IDEuNTk5NzgsMjEuMDI1MTMgMzcuNzA4MSwxLjE0MjY2IDMuNDI4MDIsNjEuNDc1NjQgMjMuNTM4OTcsLTAuNjg1NiAtNS4wMjc3NCwtNjEuMDE4NTggMjguNzk1MjUsMC45MTQxNCAtMy4xOTk0MywtMjIuMzk2MzEgeiIKICAgICAgICAgICBzdHlsZT0iZmlsbDojZmZmZmZmO2ZpbGwtb3BhY2l0eToxO3N0cm9rZTojZmZmZmZmO3N0cm9rZS1vcGFjaXR5OjEiIC8+CiAgICAgIDwvZz4KICAgIDwvZz4KICA8L2c+Cjwvc3ZnPgo=\"> <p>你, 渴望力量嗎?</p><p style=\"color:white\">按下開關!</p><svg width=\"100%\" height=\"100px\" viewBox=\"0 0 400 400\" xmlns=\"http://www.w3.org/2000/svg\"> <path d=\"M 100 100 L 300 100 L 200 300 z\" fill=\"white\" stroke=\"Blue\" stroke-width=\"3\"/> </svg> <svg width=\"100%\" height=\"100px\" viewBox=\"0 0 400 400\" xmlns=\"http://www.w3.org/2000/svg\"> <path d=\"M 100 100 L 300 100 L 200 300 z\" fill=\"white\" stroke=\"Blue\" stroke-width=\"3\"/> </svg> <svg width=\"100%\" height=\"100px\" viewBox=\"0 0 400 400\" xmlns=\"http://www.w3.org/2000/svg\"> <path d=\"M 100 100 L 300 100 L 200 300 z\" fill=\"white\" stroke=\"Blue\" stroke-width=\"3\"/> </svg> <img class=\"btn\" src=\"data:image/svg+xml;base64,PD94bWwgdmVyc2lvbj0iMS4wIiBlbmNvZGluZz0iVVRGLTgiIHN0YW5kYWxvbmU9Im5vIj8+CjxzdmcKICAgeG1sbnM6ZGM9Imh0dHA6Ly9wdXJsLm9yZy9kYy9lbGVtZW50cy8xLjEvIgogICB4bWxuczpjYz0iaHR0cDovL2NyZWF0aXZlY29tbW9ucy5vcmcvbnMjIgogICB4bWxuczpyZGY9Imh0dHA6Ly93d3cudzMub3JnLzE5OTkvMDIvMjItcmRmLXN5bnRheC1ucyMiCiAgIHhtbG5zOnN2Zz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciCiAgIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyIKICAgeG1sbnM6eGxpbms9Imh0dHA6Ly93d3cudzMub3JnLzE5OTkveGxpbmsiCiAgIHdpZHRoPSI1Mi43Mjk5MTZtbSIKICAgaGVpZ2h0PSI1Mi43Mjk5MTZtbSIKICAgdmlld0JveD0iMCAwIDUyLjcyOTkxNiA1Mi43Mjk5MTYiCiAgIHZlcnNpb249IjEuMSIKICAgaWQ9InN2ZzgiPgogIDxkZWZzCiAgICAgaWQ9ImRlZnMyIj4KICAgIDxsaW5lYXJHcmFkaWVudAogICAgICAgaWQ9ImxpbmVhckdyYWRpZW50MjA5MiI+CiAgICAgIDxzdG9wCiAgICAgICAgIHN0eWxlPSJzdG9wLWNvbG9yOiMwMDdjODE7c3RvcC1vcGFjaXR5OjE7IgogICAgICAgICBvZmZzZXQ9IjAiCiAgICAgICAgIGlkPSJzdG9wMjA4OCIgLz4KICAgICAgPHN0b3AKICAgICAgICAgc3R5bGU9InN0b3AtY29sb3I6IzAwN2M4MTtzdG9wLW9wYWNpdHk6MDsiCiAgICAgICAgIG9mZnNldD0iMSIKICAgICAgICAgaWQ9InN0b3AyMDkwIiAvPgogICAgPC9saW5lYXJHcmFkaWVudD4KICAgIDxsaW5lYXJHcmFkaWVudAogICAgICAgaWQ9ImxpbmVhckdyYWRpZW50MjA4NCI+CiAgICAgIDxzdG9wCiAgICAgICAgIHN0eWxlPSJzdG9wLWNvbG9yOiNmZmZmZmY7c3RvcC1vcGFjaXR5OjE7IgogICAgICAgICBvZmZzZXQ9IjAiCiAgICAgICAgIGlkPSJzdG9wMjA4MCIgLz4KICAgICAgPHN0b3AKICAgICAgICAgc3R5bGU9InN0b3AtY29sb3I6I2ZmZmZmZjtzdG9wLW9wYWNpdHk6MDsiCiAgICAgICAgIG9mZnNldD0iMSIKICAgICAgICAgaWQ9InN0b3AyMDgyIiAvPgogICAgPC9saW5lYXJHcmFkaWVudD4KICAgIDxsaW5lYXJHcmFkaWVudAogICAgICAgZ3JhZGllbnRUcmFuc2Zvcm09InRyYW5zbGF0ZSgyMi41MTM1ODUsLTMyMS45NDQzNykiCiAgICAgICB4bGluazpocmVmPSIjbGluZWFyR3JhZGllbnQyMDg0IgogICAgICAgaWQ9ImxpbmVhckdyYWRpZW50MjA4NiIKICAgICAgIHgxPSI3MDYuMjUyNzUiCiAgICAgICB5MT0iMTYxNC45OTE4IgogICAgICAgeDI9Ijc1OC4wNDA3MSIKICAgICAgIHkyPSIxNjcyLjA5MTEiCiAgICAgICBncmFkaWVudFVuaXRzPSJ1c2VyU3BhY2VPblVzZSIgLz4KICAgIDxsaW5lYXJHcmFkaWVudAogICAgICAgZ3JhZGllbnRUcmFuc2Zvcm09InRyYW5zbGF0ZSgyMi41MTM1ODUsLTMyMS45NDQzNykiCiAgICAgICB4bGluazpocmVmPSIjbGluZWFyR3JhZGllbnQyMDkyIgogICAgICAgaWQ9ImxpbmVhckdyYWRpZW50MjA5NCIKICAgICAgIHgxPSI2NTEuMTMzMTIiCiAgICAgICB5MT0iMTY5OC41ODM3IgogICAgICAgeDI9IjgwMy4wMzk0MyIKICAgICAgIHkyPSIxNDc3LjQ0MDQiCiAgICAgICBncmFkaWVudFVuaXRzPSJ1c2VyU3BhY2VPblVzZSIgLz4KICA8L2RlZnM+CiAgPG1ldGFkYXRhCiAgICAgaWQ9Im1ldGFkYXRhNSI+CiAgICA8cmRmOlJERj4KICAgICAgPGNjOldvcmsKICAgICAgICAgcmRmOmFib3V0PSIiPgogICAgICAgIDxkYzpmb3JtYXQ+aW1hZ2Uvc3ZnK3htbDwvZGM6Zm9ybWF0PgogICAgICAgIDxkYzp0eXBlCiAgICAgICAgICAgcmRmOnJlc291cmNlPSJodHRwOi8vcHVybC5vcmcvZGMvZGNtaXR5cGUvU3RpbGxJbWFnZSIgLz4KICAgICAgICA8ZGM6dGl0bGU+PC9kYzp0aXRsZT4KICAgICAgPC9jYzpXb3JrPgogICAgPC9yZGY6UkRGPgogIDwvbWV0YWRhdGE+CiAgPGcKICAgICB0cmFuc2Zvcm09InRyYW5zbGF0ZSgtODcuMzA0NDc2LC0xMDguNDU3MjMpIgogICAgIGlkPSJsYXllcjEiPgogICAgPGcKICAgICAgIGlkPSJnNzY2MiIKICAgICAgIHRyYW5zZm9ybT0ibWF0cml4KDAuMjY0NTgzMzMsMCwwLDAuMjY0NTgzMzMsLTgyLjIyNDIyNywtMTk5LjExMzQpIj4KICAgICAgPGNpcmNsZQogICAgICAgICBzdHlsZT0iZmlsbDp1cmwoI2xpbmVhckdyYWRpZW50MjA4Nik7ZmlsbC1vcGFjaXR5OjE7c3Ryb2tlOnVybCgjbGluZWFyR3JhZGllbnQyMDk0KTtzdHJva2Utd2lkdGg6MzAuMjM2MjAwMzM7c3Ryb2tlLWxpbmVjYXA6cm91bmQ7c3Ryb2tlLWxpbmVqb2luOnJvdW5kO3N0cm9rZS1taXRlcmxpbWl0OjQ7c3Ryb2tlLWRhc2hhcnJheTpub25lO3N0cm9rZS1vcGFjaXR5OjEiCiAgICAgICAgIGlkPSJwYXRoMjA2NyIKICAgICAgICAgY3k9IjEyNjIuMTE4OCIKICAgICAgICAgY3g9Ijc0MC4zODU1IgogICAgICAgICByPSI4NC41Mjg5ODQiIC8+CiAgICA8L2c+CiAgPC9nPgo8L3N2Zz4K\" onclick=\"(function(){var xmlhttp=new XMLHttpRequest();xmlhttp.onreadystatechange=function(){console.log('Rining');};xmlhttp.open('POST','/rining','');xmlhttp.send();alert('Ringing');})();return false;\"></img></body></html>");
  //Page += F("<!DOCTYPE html><html><head><meta charset=\"UTF-8\" /><style>body{margin:0;height:100vh;display:flex;align-items:center;justify-content:center;background:rgb(47,142,138);flex-direction:column}a{margin:0px;text-align:center;padding:20px;list-style-type:none;box-sizing:border-box;width:8em;font-size:4em;font-weight:bold;border-radius:0.5em;box-shadow:0 0 1em rgba(0,0,0,0.2);color:white;font-family:sans-serif;text-transform:capitalize;transition:0.3s;cursor:pointer;margin-left:-100px}a{background:linear-gradient(to left,orange,tomato);text-align:right;padding-right:10%;transform:perspective(400px)rotateY(-45deg)}a:hover{transform:perspective(400px)rotateY(-45deg);padding-right:5%}</style></head><body background=\"\"><a onclick=\"(function(){var xmlhttp=new XMLHttpRequest();xmlhttp.onreadystatechange=function(){console.log('Rining');};xmlhttp.open('POST','/rining','');xmlhttp.send();alert('Ringing');})();return false;\">OpenLab<br>Taipei</a><p style=\"padding-top: 20px;font-weight: bold;font-size: 3em;\">Floss+Art\u76e1\u60c5\u958b\u6e90\u5206\u4eab \u4eab\u53d7\u81ea\u7531\u5275\u4f5c</p></body></html>");
  /*
  Page += F(
            "<html><head></head><body>"
            "<h1>HELLO WORLD!!</h1>");
  if (server.client().localIP() == apIP) {
    Page += String(F("<p>You are connected through the soft AP: ")) + softAP_ssid + F("</p>");
  } else {
    Page += String(F("<p>You are connected through the wifi network: ")) + ssid + F("</p>");
  }
  Page += F("<button onclick=\"(function(){"

    "var xmlhttp=new XMLHttpRequest();"
    "xmlhttp.onreadystatechange=function(){console.log('Rining');};"
    "xmlhttp.open('POST','/rining','');"
    "xmlhttp.send();"
    "alert('Ringing');"
    "})();return false;\">Ring</button>");
  Page += F(
            "<p>You may want to <a href='/wifi'>config the wifi connection</a>.</p>"
            "</body></html>");
  */
  server.send(200, "text/html", Page);
}

void handleRining() {
  if (captivePortal()) { // If caprive portal redirect instead of displaying the page.
    return;
  }

  String Page;
  Serial.println("Request Ring trigger");
  ring_start = 1;
  count = 0;
  server.send(200, "text/html", Page);
}
/** Redirect to captive portal if we got a request for another domain. Return true in that case so the page handler do not try to handle the request again. */
boolean captivePortal() {
  if (!isIp(server.hostHeader()) && server.hostHeader() != (String(myHostname) + ".local")) {
    Serial.println("Request redirected to captive portal");
    server.sendHeader("Location", String("http://") + toStringIp(server.client().localIP()), true);
    server.send(302, "text/plain", "");   // Empty content inhibits Content-length header so we have to close the socket ourselves.
    server.client().stop(); // Stop is needed because we sent no content length
    return true;
  }
  return false;
}

/** Wifi config page handler */
void handleWifi() {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");

  String Page;
  Page += F(
            "<html><head></head><body>"
            "<h1>Wifi config</h1>");
  if (server.client().localIP() == apIP) {
    Page += String(F("<p>You are connected through the soft AP: ")) + softAP_ssid + F("</p>");
  } else {
    Page += String(F("<p>You are connected through the wifi network: ")) + ssid + F("</p>");
  }
  Page +=
    String(F(
             "\r\n<br />"
             "<table><tr><th align='left'>SoftAP config</th></tr>"
             "<tr><td>SSID ")) +
    String(softAP_ssid) +
    F("</td></tr>"
      "<tr><td>IP ") +
    toStringIp(WiFi.softAPIP()) +
    F("</td></tr>"
      "</table>"
      "\r\n<br />"
      "<table><tr><th align='left'>WLAN config</th></tr>"
      "<tr><td>SSID ") +
    String(ssid) +
    F("</td></tr>"
      "<tr><td>IP ") +
    toStringIp(WiFi.localIP()) +
    F("</td></tr>"
      "</table>"
      "\r\n<br />"
      "<table><tr><th align='left'>WLAN list (refresh if any missing)</th></tr>");
  Serial.println("scan start");
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n > 0) {
    for (int i = 0; i < n; i++) {
      Page += String(F("\r\n<tr><td>SSID ")) + WiFi.SSID(i) + ((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? F(" ") : F(" *")) + F(" (") + WiFi.RSSI(i) + F(")</td></tr>");
    }
  } else {
    Page += F("<tr><td>No WLAN found</td></tr>");
  }
  Page += F(
            "</table>"
            "\r\n<br /><form method='POST' action='wifisave'><h4>Connect to network:</h4>"
            "<input type='text' placeholder='network' name='n'/>"
            "<br /><input type='password' placeholder='password' name='p'/>"
            "<br /><input type='submit' value='Connect/Disconnect'/></form>"
            "<p>You may want to <a href='/'>return to the home page</a>.</p>"
            "</body></html>");
  server.send(200, "text/html", Page);
  server.client().stop(); // Stop is needed because we sent no content length
}

/** Handle the WLAN save form and redirect to WLAN config page again */
void handleWifiSave() {
  Serial.println("wifi save");
  server.arg("n").toCharArray(ssid, sizeof(ssid) - 1);
  server.arg("p").toCharArray(password, sizeof(password) - 1);
  server.sendHeader("Location", "wifi", true);
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send(302, "text/plain", "");    // Empty content inhibits Content-length header so we have to close the socket ourselves.
  server.client().stop(); // Stop is needed because we sent no content length
  saveCredentials();
  connect = strlen(ssid) > 0; // Request WLAN connect with new credentials if there is a SSID
}

void handleNotFound() {
  if (captivePortal()) { // If caprive portal redirect instead of displaying the error page.
    return;
  }
  String message = F("File Not Found\n\n");
  message += F("URI: ");
  message += server.uri();
  message += F("\nMethod: ");
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += F("\nArguments: ");
  message += server.args();
  message += F("\n");

  for (uint8_t i = 0; i < server.args(); i++) {
    message += String(F(" ")) + server.argName(i) + F(": ") + server.arg(i) + F("\n");
  }
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send(404, "text/plain", message);
}


void setup() {
  pinMode(2,OUTPUT); //設定GPIO2為OUTPUT
  digitalWrite(2, HIGH); 
  delay(1000);
  Serial.begin(9600);
  Serial.println();
  Serial.println("Configuring access point...");
  /* You can remove the password parameter if you want the AP to be open. */
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(softAP_ssid, softAP_password);
  delay(500); // Without delay I've seen the IP address blank
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  /* Setup the DNS server redirecting all the domains to the apIP */
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);

  /* Setup web pages: root, wifi config pages, SO captive portal detectors and not found. */
  server.on("/", handleRoot);
  server.on("/wifi", handleWifi);
  server.on("/wifisave", handleWifiSave);
  server.on("/generate_204", handleRoot);  //Android captive portal. Maybe not needed. Might be handled by notFound handler.
  server.on("/fwlink", handleRoot);  //Microsoft captive portal. Maybe not needed. Might be handled by notFound handler.
  server.on("/rining",handleRining);
  server.onNotFound(handleNotFound);
  server.begin(); // Web server start
  Serial.println("HTTP server started");
  loadCredentials(); // Load WLAN credentials from network
  connect = strlen(ssid) > 0; // Request WLAN connect if there is a SSID

  flipper.attach(0.5, flip);
}

void connectWifi() {
  Serial.println("Connecting as wifi client...");
  WiFi.disconnect();
  WiFi.begin(ssid, password);
  int connRes = WiFi.waitForConnectResult();
  Serial.print("connRes: ");
  Serial.println(connRes);
}

void loop() {
  if (connect) {
    Serial.println("Connect requested");
    connect = false;
    connectWifi();
    lastConnectTry = millis();
  }
  {
    unsigned int s = WiFi.status();
    if (s == 0 && millis() > (lastConnectTry + 60000)) {
      /* If WLAN disconnected and idle try to connect */
      /* Don't set retry time too low as retry interfere the softAP operation */
      connect = true;
    }
    if (status != s) { // WLAN status change
      Serial.print("Status: ");
      Serial.println(s);
      status = s;
      if (s == WL_CONNECTED) {
        /* Just connected to WLAN */
        Serial.println("");
        Serial.print("Connected to ");
        Serial.println(ssid);
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());

        // Setup MDNS responder
        if (!MDNS.begin(myHostname)) {
          Serial.println("Error setting up MDNS responder!");
        } else {
          Serial.println("mDNS responder started");
          // Add service to MDNS-SD
          MDNS.addService("http", "tcp", 80);
        }
      } else if (s == WL_NO_SSID_AVAIL) {
        WiFi.disconnect();
      }
    }
    if (s == WL_CONNECTED) {
      MDNS.update();
    }
  }
  // Do work:
  //DNS
  dnsServer.processNextRequest();
  //HTTP
  server.handleClient();

  
  
}
