#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define DHTPIN 2
#define DHTTYPE DHT11
#define MQ135_PIN A0
#define DUST_SENSOR_PIN A1
#define RELAY1 4
#define RELAY2 5
#define RELAY3 6
#define RELAY4 7
#define EXHAUST_RELAY 8

DHT dht(DHTPIN, DHTTYPE);

const int MQ135_THRESHOLD = 500;
const int DUST_THRESHOLD = 70;

const unsigned long FAN_RUNTIME = 300000;
const unsigned long FAN_INTERVAL = 600000;
const unsigned long POLLUTION_COOLDOWN = 360000;

unsigned long lastAutoFanRun = 0;
unsigned long fanStartTime = 0;
unsigned long lastPollutionTriggerTime = 0;
unsigned long pollutionCooldownStartTime = 0;

bool fansAreOn = false;
bool fanTriggeredByPollution = false;
bool cooldownInEffect = false;
bool powerOnStart = true;

const char animationFrames[] = {'|', '/', '-', '\\'};
int frameIndex = 0;

const unsigned char smiley_face [] PROGMEM = {
  0x00,0x00,0x3C,0x3C,0x42,0x42,0xA9,0xA9,
  0x85,0x85,0xA9,0xA9,0x91,0x91,0x42,0x42,
  0x3C,0x3C,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

const unsigned char neutral_face [] PROGMEM = {
  0x00,0x00,0x3C,0x3C,0x42,0x42,0xA5,0xA5,
  0x81,0x81,0xBD,0xBD,0x81,0x81,0x42,0x42,
  0x3C,0x3C,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

const unsigned char mask_face [] PROGMEM = {
  0x00,0x00,0x3C,0x3C,0x42,0x42,0x81,0x81,
  0xBD,0xBD,0xBD,0xBD,0xA5,0xA5,0x99,0x99,
  0x42,0x42,0x3C,0x3C,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

void setup() {
  Serial.begin(9600);
  delay(2000);

  dht.begin();

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);
  pinMode(RELAY4, OUTPUT);
  pinMode(EXHAUST_RELAY, OUTPUT);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed");
    while (true);
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(20, 20);
  display.println("OxyWalls");
  display.display();
  delay(2000);

  turnOnFans();
  fanStartTime = millis();
  fanTriggeredByPollution = false;
  powerOnStart = true;
  lastAutoFanRun = millis();
}

void loop() {
  unsigned long currentMillis = millis();

  int mqValue = analogRead(MQ135_PIN);
  int dustValue = analogRead(DUST_SENSOR_PIN);
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  bool pollutionDetected = (mqValue > MQ135_THRESHOLD || dustValue > DUST_THRESHOLD);

  if (cooldownInEffect && (currentMillis - pollutionCooldownStartTime >= POLLUTION_COOLDOWN)) {
    cooldownInEffect = false;
    Serial.println("✅ Cooldown completed");
  }

  if (!fansAreOn && pollutionDetected && !cooldownInEffect) {
    turnOnFans();
    fanTriggeredByPollution = true;
    fanStartTime = currentMillis;
    lastPollutionTriggerTime = currentMillis;
    Serial.println("🚨 Fan ON - Pollution Trigger");
  }

  if (!fansAreOn && !fanTriggeredByPollution && !powerOnStart && (currentMillis - lastAutoFanRun >= FAN_INTERVAL)) {
    turnOnFans();
    fanTriggeredByPollution = false;
    fanStartTime = currentMillis;
    lastAutoFanRun = currentMillis;
    Serial.println("⏰ Fan ON - Auto Timer");
  }

  if (fansAreOn && (currentMillis - fanStartTime >= FAN_RUNTIME)) {
    turnOffFans();

    if (fanTriggeredByPollution) {
      cooldownInEffect = true;
      pollutionCooldownStartTime = currentMillis;
    }

    if (powerOnStart) {
      powerOnStart = false;
    }

    Serial.println("🛑 Fan OFF after 5 mins");
  }

  showOLED(mqValue, dustValue, temp, hum);
  delay(1000);
}

void turnOnFans() {
  digitalWrite(RELAY1, LOW);
  digitalWrite(RELAY2, LOW);
  digitalWrite(RELAY3, LOW);
  digitalWrite(RELAY4, LOW);
  digitalWrite(EXHAUST_RELAY, HIGH);
  fansAreOn = true;
}

void turnOffFans() {
  digitalWrite(RELAY1, HIGH);
  digitalWrite(RELAY2, HIGH);
  digitalWrite(RELAY3, HIGH);
  digitalWrite(RELAY4, HIGH);
  digitalWrite(EXHAUST_RELAY, LOW);
  fansAreOn = false;
  fanTriggeredByPollution = false;
}

void showOLED(int mq, int dust, float temp, float hum) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(0, 0);
  display.print("🌿 OxyWalls Purifier");

  display.setCursor(0, 14);
  display.print("Fan: ");
  if (fansAreOn) {
    display.setTextColor(BLACK, WHITE);
    display.print("ON ");
    display.setTextColor(WHITE);
    display.setCursor(70, 14);
    display.print(animationFrames[frameIndex]);
    frameIndex = (frameIndex + 1) % 4;
  } else {
    display.print("OFF");
  }

  display.setCursor(0, 26);
  display.print("CO2/NH3: ");
  display.print(mq);

  display.setCursor(0, 36);
  display.print("PM2.5:   ");
  display.print(dust);

  if (mq < 300 && dust < 50) {
    display.drawBitmap(100, 30, smiley_face, 16, 16, WHITE);
  } else if ((mq >= 300 && mq <= 500) || (dust >= 50 && dust <= 100)) {
    display.drawBitmap(100, 30, neutral_face, 16, 16, WHITE);
  } else {
    display.drawBitmap(100, 30, mask_face, 16, 16, WHITE);
  }

  display.setCursor(0, 48);
  display.print("Temp: ");
  display.print(temp);
  display.print("C");

  display.setCursor(85, 48);
  display.print("Hum: ");
  display.print(hum);
  display.print("%");

  display.display();
}
