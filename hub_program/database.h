typedef struct {
  float temperature;
  float humidity;
  int smoke_status;
  int fire_status;
} sensor_t;

typedef struct {
  int mode;
  int pwm;
  int step;
} motor_t;

typedef struct {
  sensor_t sensor;
  motor_t motor;
  int alarm;
} db_t;

static db_t xDB1;
static db_t xDB2;

// Populate default value
void populateDBWithEmptyValue()
{
  xDB1.sensor.temperature = 0;
  xDB1.sensor.humidity = 0;
  xDB1.sensor.smoke_status = 0;
  xDB1.sensor.fire_status = 0;
  xDB1.motor.mode = 0;
  xDB1.motor.pwm = 0;
  xDB1.motor.step = 0;
  xDB1.alarm = 0;

  xDB2.sensor.temperature = 0;
  xDB2.sensor.humidity = 0;
  xDB2.sensor.smoke_status = 0;
  xDB2.sensor.fire_status = 0;
  xDB2.motor.mode = 0;
  xDB2.motor.pwm = 0;
  xDB2.motor.step = 0;
  xDB2.alarm = 0;
}

void vShowDB()
{
#ifdef _DEBUG_
  StaticJsonDocument<384> db;
  
  JsonObject A = db.createNestedObject("A");
  
  JsonObject A_sensor = A.createNestedObject("sensor");
  A_sensor["temperature"] = xDB1.sensor.temperature;
  A_sensor["humidity"] = xDB1.sensor.humidity;
  A_sensor["smoke_status"] = xDB1.sensor.smoke_status;
  A_sensor["fire_status"] = xDB1.sensor.fire_status;
  
  JsonObject A_motor = A.createNestedObject("motor");
  A_motor["mode"] = xDB1.motor.mode;
  A_motor["pwm"] = xDB1.motor.pwm;
  A_motor["step"] = xDB1.motor.step;
  A["alarm"] = xDB1.alarm;
  
  JsonObject B = db.createNestedObject("B");
  
  JsonObject B_sensor = B.createNestedObject("sensor");
  B_sensor["temperature"] = xDB2.sensor.temperature;
  B_sensor["humidity"] = xDB2.sensor.humidity;
  B_sensor["smoke_status"] = xDB2.sensor.smoke_status;
  B_sensor["fire_status"] = xDB2.sensor.fire_status;
  
  JsonObject B_motor = B.createNestedObject("motor");
  B_motor["mode"] = xDB2.motor.mode;
  B_motor["pwm"] = xDB2.motor.pwm;
  B_motor["step"] = xDB2.motor.step;
  B["alarm"] = xDB2.alarm;
  
  serializeJson(db, Serial);
  _PL();
#endif
}
