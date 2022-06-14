const int MQ2_PIN1 = 35;
const int MQ2_PIN2 = 33;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(20 * 1000);
  Serial.println("Ready!");
}

void loop() {
  // put your main code here, to run repeatedly:
  int sensor_val = 0;

  for (int i = 0; i < 20; i++) {
    // Comment pin yang tidak ingin diukur
    sensor_val += analogRead(MQ2_PIN1);
    //sensor_val += analogRead(MQ2_PIN2);
  }

  sensor_val /= 20;
  
  float sensor_volt = ((float)sensor_val / 4095) * 3300;
  float rs = (5000 - sensor_volt) / sensor_volt;
  float r0 = rs / 9.8;

  Serial.print("Sensor value: ");
  Serial.println(sensor_val);
  Serial.print("Sensor value (in millivolts): ");
  Serial.println(sensor_volt);
  Serial.print("RS of air: ");
  Serial.println(rs);
  Serial.print("R0: ");
  Serial.println(r0);

  delay(1000);
}
