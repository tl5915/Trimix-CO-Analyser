#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_ADS1X15.h> 
#include <FlashStorage.h>

Adafruit_SSD1306 display(128, 64, &Wire, -1);
Adafruit_ADS1115 ads;

const int SamplingRate = 100; // Sampling rate 100 Hz
const unsigned long DisplayInterval = 200; // Refresh rate 5 Hz
unsigned long LastSampleTime = 0;
unsigned long LastDisplayUpdate = 0;

const float DefaultAirVoltage = 10.0; // Oxygen voltage in air
const float DefaultPureVoltage = 0.0; // Oxygen voltage in oxygen
float AirVoltage = DefaultAirVoltage;
float PureVoltage = DefaultPureVoltage;
FlashStorage(AirVoltageStorage, float);
FlashStorage(PureVoltageStorage, float);
bool TwoPointCalibration = false;

int SampleCount = 0;
float OxygenSum = 0.0;
float AvgOxygenVoltage = 0.0;

// Calibration
void PerformCalibration() {
  const int CalSampleCount = 25; // Average 25 samples for calibration

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(8, 24);
  display.print(F("Calibrating..."));
  display.display();

  for (int i = 0; i < CalSampleCount; i++) {
    int16_t OxygenRaw = ads.readADC_Differential_0_1();
    float OxygenVoltage = fabs(OxygenRaw * 0.0078125);
    OxygenSum += OxygenVoltage;
    delay(15);
  }
  AvgOxygenVoltage = OxygenSum / CalSampleCount;

  if (AvgOxygenVoltage < 35.0) {
    AirVoltage = AvgOxygenVoltage;
    AirVoltageStorage.write(AirVoltage);

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(8, 10);
    display.print(F("20.9% Calibration"));
    display.setTextSize(2);
    display.setCursor(8, 36);
    display.print(AirVoltage, 1);
    display.print(F(" mV"));
    display.display();
    delay(1000);

  } else {
    PureVoltage = AvgOxygenVoltage;
    PureVoltageStorage.write(PureVoltage);

    if (PureVoltage > AirVoltage) {
      TwoPointCalibration = true;
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(8, 10);
    display.print(F("100% Calibration"));
    display.setTextSize(2);
    display.setCursor(8, 36);
    display.print(PureVoltage, 1);
    display.print(F(" mV"));
    display.display();
    delay(1000);
  }
}

// Reset function
void ResetCalibration() {
  AirVoltage = DefaultAirVoltage;
  PureVoltage = DefaultPureVoltage;
  AirVoltageStorage.write(AirVoltage);
  PureVoltageStorage.write(PureVoltage);
  TwoPointCalibration = false;

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(34, 12);
  display.print(F("Reset"));
  display.setCursor(34, 36);
  display.print(F("Calib"));
  display.display();
  delay(2000);
}

// Oxygen percentage calculation
float CalculateOxygenPercentage() {
  if (!TwoPointCalibration) {
  return (AvgOxygenVoltage / AirVoltage) * 20.9; // One-point calibration
  }
  return 20.9 + ((AvgOxygenVoltage - AirVoltage) / (PureVoltage - AirVoltage)) * (99.9 - 20.9); // Two-point calibration
}

void setup() {
  pinMode(7, INPUT_PULLDOWN); // Reset read
  pinMode(8, OUTPUT); // Reset output
  digitalWrite(8, HIGH); // Short pin 7 and 8 to reset
  pinMode(3, OUTPUT); // ADS1115 address pin
  digitalWrite(3, LOW); // Set I2C address to 0x48
  ads.begin(0x48);
  ads.setDataRate(RATE_ADS1115_250SPS); // Data rate 250 SPS
  ads.setGain(GAIN_SIXTEEN); // Gain 16, 256 mV
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(SSD1306_WHITE);

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(28, 12);
  display.print(F("Nitrox"));
  display.setCursor(16, 36);
  display.print(F("Analyser"));
  display.display();
  delay(500);

  AirVoltage = AirVoltageStorage.read();
  PureVoltage = PureVoltageStorage.read();
  TwoPointCalibration = (PureVoltage > AirVoltage);

  display.clearDisplay();
  display.setTextSize(2);
  if (TwoPointCalibration) {
    display.setCursor(4, 8);
    display.print(F("2-Pt Calib"));
    display.setCursor(22, 40);
    display.print(PureVoltage, 1);
    display.print(F(" mV"));
  } else {
    display.setCursor(4, 24);
    display.print(F("1-Pt Calib"));
  }
  display.display();
  delay(1000);

  PerformCalibration();
}

void loop() {
  unsigned long CurrentTime = millis();

  if (digitalRead(7) == HIGH) {
    ResetCalibration();
  }

  if (CurrentTime - LastSampleTime >= (1000 / SamplingRate)) {
    LastSampleTime = CurrentTime;
    if (SampleCount == 0) {
      OxygenSum = 0.0;
    }

    int16_t OxygenRaw = ads.readADC_Differential_0_1();
    float OxygenVoltage = fabs(OxygenRaw * 0.0078125);
    OxygenSum += OxygenVoltage;
    SampleCount++;
  }

  if (CurrentTime - LastDisplayUpdate >= DisplayInterval) {
    LastDisplayUpdate = CurrentTime;

    // Calculate average voltage
    AvgOxygenVoltage = (SampleCount > 0) ? (OxygenSum / SampleCount) : 0.0;
    SampleCount = 0;

    // Calculate percentage
    float OxygenPercentage = max(0.0, CalculateOxygenPercentage());

    // Calculate MOD
    int MOD14 = (OxygenPercentage > 0) ? min(99, (int)((1400.0 / OxygenPercentage) - 10.0)) : 0; // MOD at ppO2 1.4
    int MOD16 = (OxygenPercentage > 0) ? min(99, (int)((1600.0 / OxygenPercentage) - 10.0)) : 0; // MOD at ppO2 1.6

    // Display
    display.clearDisplay();
    display.setTextSize(3);
    display.setCursor(10, 2);
    display.print(OxygenPercentage, 1);
    display.print(F("%"));
    display.setTextSize(1);
    display.setCursor(4, 42);
    display.print(AvgOxygenVoltage, 1);
    display.print(F("mV"));
    display.setCursor(80, 34);
    display.print(F("MOD"));
    display.setTextSize(2);
    display.setCursor(56, 46);
    display.print((int)MOD14);
    display.print(F("/"));
    display.print((int)MOD16);
    display.print(F("m"));
    display.display();
  }
}