///LIBRARY FUNCTIONS
#include <Wire.h>                 //used for I2C communication
#include <SPI.h>                  //used for SPI communication
#include <MFRC522.h>              //controls RFID module
#include <OneWire.h>              //DS18B20 uses OneWire protocol
#include <DallasTemperature.h>    //used for DS18B20 temperature sensor
#include <Adafruit_MPU6050.h>     //used for MPU6050 IMU sensor
#include <Adafruit_BMP085.h>      //used for BMP180 sensor
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>     //controls OLED display

//OLED CONFIGURATION
#define SCN_W 128
#define SCN_H 64

//OLED DISPLAY OBJECT
Adafruit_SSD1306 display(
  SCN_W,
  SCN_H,
  &Wire,
  -1
);

//DS18B20 CONFIGURATION
#define DS18B20_DATA 5
OneWire oneWire(DS18B20_DATA);
DallasTemperature ds18b20(&oneWire);

//MQ2 CONFIGURATION
#define MQ2_PIN 1

//PIR SENSOR CONFIGURATION
#define PIR_PIN 4

//LED CONFIGURATION
#define GREEN_LED 16
#define YELLOW_LED 17
#define RED_LED 18

//BUZZER CONFIGURATION
#define BUZ_PIN 15

//RFID CONFIGURATION
#define SS_PIN 10
#define RST_PIN 14

MFRC522 rfid(SS_PIN, RST_PIN);

//MPU6050 CONFIGURATION
Adafruit_MPU6050 mpu;

//BMP180 CONFIGURATION
Adafruit_BMP085 bmp;

//GLOBAL VARIABLES
String statusText = "SAFE";

bool danger = false;
bool warning = false;

//AUTHORIZED RFID UID
byte authorizedUID[4] = {0x01, 0x02, 0x03, 0x04};

//SETUP FUNCTION
void setup()
{
  Serial.begin(115200);

  //PIN MODES
  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  pinMode(BUZ_PIN, OUTPUT);

  pinMode(PIR_PIN, INPUT);

  //INITIAL LED STATES
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(RED_LED, LOW);

  //I2C START
  Wire.begin(8, 9);

  //DS18B20 START
  ds18b20.begin();

  //RFID START
  SPI.begin();
  rfid.PCD_Init();

  Serial.println("RFID READY");

  //MPU6050 START
  if (!mpu.begin())
  {
    Serial.println("MPU6050 NOT DETECTED");

    while (1);
  }

  //BMP180 START
  if (!bmp.begin())
  {
    Serial.println("BMP180 NOT DETECTED");

    while (1);
  }

  //OLED START
  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        0x3C
      ))
  {
    Serial.println("OLED FAILED");

    while (1);
  }

  //STARTUP SCREEN
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(15, 10);
  display.println("MEDICAL");

  display.setCursor(20, 25);
  display.println("COLD CHAIN");

  display.setCursor(28, 40);
  display.println("VAULT");

  display.display();

  delay(3000);

  Serial.println("SYSTEM READY");
}

//MAIN LOOP
void loop()
{
  //RESET FLAGS
  danger = false;
  warning = false;

  statusText = "SAFE";

  //RFID DETECTION
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial())
  {
    Serial.println("CARD DETECTED");

    Serial.print("UID: ");

    for (byte i = 0; i < rfid.uid.size; i++)
    {
      Serial.print(rfid.uid.uidByte[i], HEX);
      Serial.print(" ");
    }

    Serial.println();

    bool accessGranted = true;

    //UID COMPARISON
    for (byte i = 0; i < 4; i++)
    {
      if (rfid.uid.uidByte[i] != authorizedUID[i])
      {
        accessGranted = false;
      }
    }

    //ACCESS GRANTED
    if (accessGranted)
    {
      Serial.println("ACCESS GRANTED");

      digitalWrite(GREEN_LED, HIGH);

      tone(BUZ_PIN, 2500);
      delay(200);
      noTone(BUZ_PIN);
    }

    //ACCESS DENIED
    else
    {
      Serial.println("ACCESS DENIED");

      digitalWrite(RED_LED, HIGH);

      tone(BUZ_PIN, 500);
      delay(1000);
      noTone(BUZ_PIN);
    }

    //STOP RFID COMMUNICATION
    rfid.PICC_HaltA();
  }

  //READ DS18B20
  ds18b20.requestTemperatures();

  float coldTemp =
    ds18b20.getTempCByIndex(0);

  //READ MQ2
  int gasValue =
    analogRead(MQ2_PIN);

  //READ BMP180
  float pressure =
    bmp.readPressure() / 100.0;

  //READ PIR SENSOR
  int motionDetected =
    digitalRead(PIR_PIN);

  //READ MPU6050
  sensors_event_t a, g, temp;

  mpu.getEvent(&a, &g, &temp);

  //acceleration values
  float accelX =
    a.acceleration.x;

  float accelY =
    a.acceleration.y;

  float accelZ =
    a.acceleration.z;
  
  //gyroscope values
  float gyroX=
    g.gyro.x*57.2958;
  
  float gyroY=
    g.gyro.y*57.2958;

  float gyroZ=
    g.gyro.z*57.2958;

  //TOTAL ACCELERATION
  float totalAccel = sqrt(
    accelX * accelX +
    accelY * accelY +
    accelZ * accelZ
  );

  //TEMPERATURE ANALYSIS
  if (coldTemp > 8 || coldTemp < 2)
  {
    danger = true;

    statusText = "TEMP ALERT";
  }
  
  //PRESSURE ANALYSIS
  if (pressure < 990 || pressure > 1035)
  {
    warning = true;

    statusText = "PRESS ALERT";
  }

  //GAS ANALYSIS
  if (gasValue > 2000)
  {
    danger = true;

    statusText = "GAS ALERT";
  }

  //TILT / SHOCK ANALYSIS
  if (totalAccel > 20)
  {
    warning = true;

    statusText = "TILT ALERT";
  }

  //PIR MOTION ANALYSIS
  if (motionDetected == HIGH)
  {
    warning = true;

    statusText = "MOTION ALERT";
  }

  //LED CONTROL
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(RED_LED, LOW);

  noTone(BUZ_PIN);

  if (danger)
  {
    digitalWrite(RED_LED, HIGH);

    tone(BUZ_PIN, 1000);
  }
  else if (warning)
  {
    digitalWrite(YELLOW_LED, HIGH);
  }
  else
  {
    digitalWrite(GREEN_LED, HIGH);
  }

  //OLED DISPLAY
  display.clearDisplay();

  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print("Cold Temp: ");

  display.print(coldTemp);

  display.println(" C");

  display.setCursor(0, 12);
  display.print("Gas Value: ");

  display.println(gasValue);

  display.setCursor(0, 24);
  display.print("Pressure: ");

  display.print(pressure);

  display.println(" hPa");

  display.setCursor(0, 36);
  display.print("Motion: ");

  if (motionDetected == HIGH)
  {
    display.println("YES");
  }
  else
  {
    display.println("NO");
  }

  display.setCursor(0, 48);
  display.print("Status: ");

  display.println(statusText);

  display.display();

  //SERIAL MONITOR OUTPUT
  Serial.print("Cold Temp: ");
  Serial.print(coldTemp);
  Serial.println(" C");

  Serial.print("Gas Value: ");
  Serial.println(gasValue);

  Serial.print("Pressure: ");
  Serial.print(pressure);
  Serial.println(" hPa");

  Serial.print("Motion: ");

  if (motionDetected == HIGH)
  {
    Serial.println("DETECTED");
  }
  else
  {
    Serial.println("NO MOTION");
  }

  //ACCELERATION VALUES
  Serial.print("Accel X: ");
  Serial.println(accelX);

  Serial.print("Accel Y: ");
  Serial.println(accelY);

  Serial.print("Accel Z: ");
  Serial.println(accelZ);

  //GYROSCOPE VALUES
  Serial.print("Gyro X: ");
  Serial.println(gyroX);

  Serial.print("Gyro Y: ");
  Serial.println(gyroY);

  Serial.print("Gyro Z: ");
  Serial.println(gyroZ);

  Serial.print("Total Accel: ");
  Serial.println(totalAccel);

  Serial.print("System Status: ");
  Serial.println(statusText);

  Serial.println("----------------------");

  //LOOP DELAY
  delay(100);
}