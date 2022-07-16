#include <AsyncJson.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

AsyncWebServer xServer(80);

void vMotorController(AsyncWebServerRequest *request) {
  StaticJsonDocument<32> payload;
  String id = request->getParam("id")->value();

  payload["mode"] = (id == "A" ? xDB1.motor.mode : xDB2.motor.mode);
  payload["pwm"] = (id == "A" ? xDB1.motor.pwm : xDB2.motor.pwm);

  request->send(200, "application/json", payload.as<String>());
}

AsyncCallbackJsonWebHandler* xSensorController = new AsyncCallbackJsonWebHandler("/sensor", [](AsyncWebServerRequest *request, JsonVariant &json) {
  const JsonObject& data = json.as<JsonObject>();
  
  if (data["id"] == "A") {
    xDB1.sensor.temperature = data["temperature"].as<float>();
    xDB1.sensor.humidity = data["humidity"].as<float>();
    xDB1.sensor.smoke_status = data["smoke_status"].as<int>();
    xDB1.sensor.fire_status = data["fire_status"].as<int>();
    xDB1.motor.pwm = data["pwm"].as<int>();
    xDB1.alarm = data["alarm"].as<int>();
  } else {
    xDB2.sensor.temperature = data["temperature"].as<float>();
    xDB2.sensor.humidity = data["humidity"].as<float>();
    xDB2.sensor.smoke_status = data["smoke_status"].as<int>();
    xDB2.sensor.fire_status = data["fire_status"].as<int>();
    xDB2.motor.pwm = data["pwm"].as<int>();
    xDB2.alarm = data["alarm"].as<int>();
  }

  request->send(200, "text/plain", "OK.");
});

void vSetupServer() {
  xServer.on("/motor", HTTP_GET, vMotorController);
  xServer.addHandler(xSensorController);
  xServer.begin();
}
