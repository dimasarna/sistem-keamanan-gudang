#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>

void vSendApiPost(const char * endpoint, void * param) {
  StaticJsonDocument<128> payload;
  db_t * data = (db_t *) param;
  
  WiFiClient client;
  HTTPClient http;
  
  // Your Domain name with URL path or IP address with path
  http.begin(client, endpoint);

  // Specify content-type header
  http.addHeader("Content-Type", "application/json");
  
  // Data to send with HTTP POST
  payload["id"] = DEVICE_ID;
  payload["temperature"] = xDB.sensor.temperature;
  payload["humidity"] = xDB.sensor.humidity;
  payload["smoke_status"] = xDB.sensor.smoke_status;
  payload["fire_status"] = xDB.sensor.fire_status;
  payload["pwm"] = xDB.motor.pwm;
  payload["alarm"] = xDB.alarm;
  
  // Send HTTP POST request
#ifdef _DEBUG_
  serializeJson(payload, Serial);
#endif
  int httpResponseCode = http.POST(payload.as<String>());

  // Free resources
  http.end();
}

void vSendApiGet(const char * endpoint) {
  StaticJsonDocument<64> data;
  WiFiClient client;
  HTTPClient http;
  
  // Your Domain name with URL path or IP address with path
  http.useHTTP10(true);
  http.begin(client, endpoint);
  
  // Send HTTP GET request
  int httpResponseCode = http.GET();
  
  deserializeJson(data, http.getStream());
#ifdef _DEBUG_
  serializeJson(data, Serial);
#endif

  xDB.motor.mode = data["mode"];
  xDB.motor.pwm = data["pwm"];
  
  // Free resources
  http.end();
}
