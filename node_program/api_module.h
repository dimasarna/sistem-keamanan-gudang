#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>

void vSendApiPost(const char * endpoint, void * param) {
  StaticJsonDocument<128> payload;
  db_t * data = (db_t *) param;
  
  WiFiClient client;
  HTTPClient http;
  
  // Your Domain name with URL path or IP address with path
  http.useHTTP10(true);
  http.begin(client, endpoint);

  // Specify content-type header
  http.addHeader("Content-Type", "application/json");
  
  // Data to send with HTTP POST
  payload["id"] = DEVICE_ID;
  payload["temperature"] = data->sensor.temperature;
  payload["humidity"] = data->sensor.humidity;
  payload["smoke_status"] = data->sensor.smoke_status;
  payload["fire_status"] = data->sensor.fire_status;
  payload["pwm"] = data->motor.pwm;
  payload["alarm"] = data->alarm;
  
  // Send HTTP POST request
#ifdef _DEBUG_
  serializeJson(payload, Serial);
  _PL();
#endif
  int httpResponseCode = http.POST(payload.as<String>());

  // Free resources
  http.end();
}

void vSendApiGet(const char * endpoint) {
  StaticJsonDocument<96> data;
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
  _PL();
#endif

  xDB.motor.mode = data["mode"].as<int>();
  xDB.motor.pwm = data["pwm"].as<int>();
  sSetPoint = data["step"].as<int>();
  
  // Free resources
  http.end();
}
