/*
 Basic MQTT example with Authentication
  - connects to an MQTT server, providing username
    and password
  - publishes "hello world" to the topic "outTopic"
  - subscribes to the topic "inTopic"

  original from https://github.com/knolleary/pubsubclient/blob/master/examples/mqtt_auth/mqtt_auth.ino
*/

#include <Arduino.h>
#include <SIM76xx.h>
#include <GSMClientSecure.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <MD5Builder.h>

const char* ssid = "Vision Sensor_F24D0B";

const char* username = "admin";
const char* password = "swd12345";

const char* server = "http://192.168.1.1";
const char* uri = "/vb.htm?language=ie&getppccurnumber";

int count = 0;

unsigned long startMillis;  //some global variables available anywhere in the program
unsigned long startTeleMillis;
unsigned long starSendTeletMillis;
unsigned long currentMillis;
const unsigned long periodSendTelemetry = 60000;  //the value is a number of milliseconds

String deviceToken = "sk2l6IG0U9esIhZSBdcD";
char thingsboardServer[] = "mqtt.thingcontrol.io";

int PORT = 8883;

String json = "";

//void callback(char* topic, byte* payload, unsigned int length) {
//  // handle message arrived
//}

GSMClientSecure gsm_client;
PubSubClient client(gsm_client);

void setup() {
  Serial.begin(115200);
  Serial.println("Hello!");
  
  while(!GSM.begin()) {
    Serial.println("GSM setup fail");
    delay(2000);
  }
  gsm_client.setInsecure();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  
  client.setServer( thingsboardServer, PORT );
  reconnectMqtt();
  // Note - the default maximum packet size is 128 bytes. If the
  // combined length of clientId, username and password exceed this use the
  // following to increase the buffer size:
  // client.setBufferSize(255);
  startMillis = millis();  //initial start time
}

void loop() {
  if ( !client.connected() )
  {
    reconnectMqtt();
  }
  client.loop();
  currentMillis = millis();  //get the current "time" (actually the number of milliseconds since the program started)
  //send
  if (currentMillis - starSendTeletMillis >= periodSendTelemetry)  //test whether the period has elapsed
  {
    getPeopleCount();
    sendtelemetry();
    count = 0;
    starSendTeletMillis = currentMillis;  //IMPORTANT to save the start time of the current LED state.
  }
}

void getPeopleCount() {
  WiFiClient client;
  HTTPClient http;  // must be declared after WiFiClient for correct destruction order, because used by http.begin(client,...)

  Serial.print("[HTTP] begin...\n");

  // configure traged server and url
  http.begin(client, String(server) + String(uri));


  const char* keys[] = { "WWW-Authenticate" };
  http.collectHeaders(keys, 1);

  Serial.print("[HTTP] GET...\n");
  // start connection and send HTTP header
  int httpCode = http.GET();

  if (httpCode > 0) {
    String authReq = http.header("WWW-Authenticate");

    String authorization = getDigestAuth(authReq, String(username), String(password), "GET", String(uri), 1);

    http.end();
    http.begin(client, String(server) + String(uri));

    http.addHeader("Authorization", authorization);

    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      payload = payload.substring(30);
      Serial.print("Count : ");
      Serial.println(payload.toInt());
      count = payload.toInt();
    } else {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
}

String exractParam(String& authReq, const String& param, const char delimit) {
  int _begin = authReq.indexOf(param);
  if (_begin == -1) { return ""; }
  return authReq.substring(_begin + param.length(), authReq.indexOf(delimit, _begin + param.length()));
}

String getCNonce(const int len) {
  static const char alphanum[] = "0123456789"
                                 "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                 "abcdefghijklmnopqrstuvwxyz";
  String s = "";

  for (int i = 0; i < len; ++i) { s += alphanum[rand() % (sizeof(alphanum) - 1)]; }

  return s;
}

String getDigestAuth(String& authReq, const String& username, const String& password, const String& method, const String& uri, unsigned int counter) {
  // extracting required parameters for RFC 2069 simpler Digest
  String realm = exractParam(authReq, "realm=\"", '"');
  String nonce = exractParam(authReq, "nonce=\"", '"');
  String cNonce = getCNonce(8);

  char nc[9];
  snprintf(nc, sizeof(nc), "%08x", counter);

  // parameters for the RFC 2617 newer Digest
  MD5Builder md5;
  md5.begin();
  md5.add(username + ":" + realm + ":" + password);  // md5 of the user:realm:user
  md5.calculate();
  String h1 = md5.toString();

  md5.begin();
  md5.add(method + ":" + uri);
  md5.calculate();
  String h2 = md5.toString();

  md5.begin();
  md5.add(h1 + ":" + nonce + ":" + String(nc) + ":" + cNonce + ":" + "auth" + ":" + h2);
  md5.calculate();
  String response = md5.toString();

  String authorization = "Digest username=\"" + username + "\", realm=\"" + realm + "\", nonce=\"" + nonce + "\", uri=\"" + uri + "\", algorithm=\"MD5\", qop=auth, nc=" + String(nc) + ", cnonce=\"" + cNonce + "\", response=\"" + response + "\"";
  //Serial.println(authorization);

  return authorization;
}

void reconnectMqtt()
{
  if ( client.connect("Thingcontrol_AT", deviceToken.c_str(), NULL) )
  {
    Serial.println( F("Connect MQTT Success."));
    client.subscribe("v1/devices/me/rpc/request/+");
  }
}

void sendtelemetry()
{
  String json = "";
  json.concat("{\"count\":");
  json.concat(count);
  json.concat("}");
  Serial.println(json);
  // Length (with one extra character for the null terminator)
  int str_len = json.length() + 1;
  // Prepare the character array (the buffer)
  char char_array[str_len];
  // Copy it over
  json.toCharArray(char_array, str_len);
  processTele(char_array);
}

void processTele(char jsonTele[])
{
  char *aString = jsonTele;
  Serial.println("OK");
  Serial.print(F("+:topic v1/devices/me/ , "));
  Serial.println(aString);
  client.publish( "v1/devices/me/telemetry", aString);
}
