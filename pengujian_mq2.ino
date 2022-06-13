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
  int sensor1 = analogRead(MQ2_PIN1);
  int sensor2 = analogRead(MQ2_PIN2);
  Serial.println(sensor1);
  Serial.println(sensor2);

  if (sensor1 > 350 || sensor2 > 150)
    Serial.println("Terdeteksi Asap!");

  delay(100);
}
