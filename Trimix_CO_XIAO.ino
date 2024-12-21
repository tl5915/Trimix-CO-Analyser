#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_ADS1X15.h>
#include <FlashStorage.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_ADS1115 ads;

// Pin definition
const int displaySwitchPin = 0;  // Display switch button
const int calButtonPin = 1;      // Calibration button
const int coPin = A2;            // CO sensor
const int batteryPin = A8;       // Battery monitoring

// Default calibration values
const float defaultOxygenCalVoltage = 10.0;   // Oxygen voltage in air
const float defaultPureOxygenVoltage = 0.0;   // Oxygen voltage in oxygen
const float defaultHeliumZeroVoltage = 0.0;   // Helium voltage in air
const float defaultHeliumCalVoltage = 582.0;  // Helium voltage in helium
const float COZeroVoltage = 0.43;             // CO voltage in air

// Timing constants
const int calibrationSampleCount = 20;           // Average 20 samples for calibration
const int samplingRateHz = 20;                   // Sampling rate 20 Hz
const unsigned long displayInterval = 500;       // Display refresh rate 2 Hz
const unsigned long longPressDuration = 3000;    // Long press threshold 3 seconds
const unsigned long resetPressDuration = 10000;  // Reset press threshold 10 seconds

// Calibration variables
float oxygencalVoltage = defaultOxygenCalVoltage;
float pureoxygenVoltage = defaultPureOxygenVoltage;
float heliumZeroVoltage = defaultHeliumZeroVoltage;
float heliumcalVoltage = defaultHeliumCalVoltage;
FlashStorage(oxygenCalibrationStorage, float);
FlashStorage(pureOxygenCalibrationStorage, float);
FlashStorage(heliumZeroCalibrationStorage, float);
FlashStorage(heliumCalibrationStorage, float);

// Sensor averaging variables
int sampleCount = 0;
float oxygenSum = 0.0;
float avgOxygenVoltage = 0.0;
float heliumSum = 0.0;
float coSum = 0.0;
float batterySum = 0.0;

// Timing variables
unsigned long lastDisplayUpdate = 0;
unsigned long lastSampleTime = 0;
unsigned long decisionWindowEnd = 0;
unsigned long lastButtonDebounceTime = 0;
unsigned long buttonPressStartTime = 0;
unsigned long lastButtonPressTime = 0;
unsigned int pressCount = 0;
bool lastButtonState = HIGH;
bool isButtonPressed = false;
bool isTwoPointCalibrated = false;
bool isDisplaySwitched = false;

// Oxygen percentage calculation
float calculateOxygenPercentage() {
  if (oxygencalVoltage <= 0 || (isTwoPointCalibrated && pureoxygenVoltage <= oxygencalVoltage)) {
    return 0.0;
  }
  if (!isTwoPointCalibrated) {
    return (avgOxygenVoltage / oxygencalVoltage) * 20.9;  // One-point calibration
  }
  return 20.9 + ((avgOxygenVoltage - oxygencalVoltage) / (pureoxygenVoltage - oxygencalVoltage)) * (100.0 - 20.9);  // Two-point calibration
}

// Oxygen 21% calibration
void performOxygenCalibration() {
  ads.setGain(GAIN_SIXTEEN);
  float oxygenSum = 0.0;
  for (int i = 0; i < calibrationSampleCount; i++) {
    int16_t oxygenRaw = ads.readADC_Differential_0_1();
    float oxygenVoltage = fabs(oxygenRaw * 0.0078125);
    oxygenSum += oxygenVoltage;
    delay(50);
  }
  oxygencalVoltage = oxygenSum / calibrationSampleCount;
  oxygenCalibrationStorage.write(oxygencalVoltage);

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(28, 0);
  display.print(F("Oxygen"));
  display.setCursor(22, 24);
  display.print(F("21% Cal"));
  display.setCursor(22, 48);
  display.print(oxygencalVoltage, 1);
  display.print(F(" mV"));
  display.display();
  delay(3000);
}

// Oxygen 100% calibration
void calibratePureOxygenVoltage() {
  ads.setGain(GAIN_SIXTEEN);
  float oxygenSum = 0.0;
  for (int i = 0; i < calibrationSampleCount; i++) {
    int16_t oxygenRaw = ads.readADC_Differential_0_1();
    float oxygenVoltage = fabs(oxygenRaw * 0.0078125);
    oxygenSum += oxygenVoltage;
    delay(50);
  }
  pureoxygenVoltage = oxygenSum / calibrationSampleCount;
  pureOxygenCalibrationStorage.write(pureoxygenVoltage);

  if (pureoxygenVoltage > oxygencalVoltage) {
    isTwoPointCalibrated = true;
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(28, 0);
  display.print(F("Oxygen"));
  display.setCursor(16, 24);
  display.print(F("100% Cal"));
  display.setCursor(22, 48);
  display.print(pureoxygenVoltage, 1);
  display.print(F(" mV"));
  display.display();
  delay(3000);
}

// Helium 0% calibration
void calibrateZeroHeliumVoltage() {
  ads.setGain(GAIN_FOUR);
  float heliumSum = 0.0;
  for (int i = 0; i < calibrationSampleCount; i++) {
    int16_t heliumRaw = ads.readADC_Differential_2_3();
    float heliumVoltage = fabs(heliumRaw * 0.03125);
    heliumSum += heliumVoltage;
    delay(50);
  }
  heliumZeroVoltage = (heliumSum / calibrationSampleCount) - 0.62;  // Correction factor at 20.9% O2
  heliumZeroCalibrationStorage.write(heliumZeroVoltage);

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(28, 0);
  display.print(F("Helium"));
  display.setCursor(28, 24);
  display.print(F("0% Cal"));
  display.setCursor(16, 48);
  display.print(heliumZeroVoltage, 1);
  display.print(F(" mV"));
  display.display();
  delay(3000);
}

// Helium 100% calibration
void performHeliumCalibration() {
  ads.setGain(GAIN_FOUR);
  float heliumSum = 0.0;
  for (int i = 0; i < calibrationSampleCount; i++) {
    int16_t heliumRaw = ads.readADC_Differential_2_3();
    float heliumVoltage = fabs(heliumRaw * 0.03125);
    heliumSum += heliumVoltage;
    delay(50);
  }
  heliumcalVoltage = (heliumSum / calibrationSampleCount) - 0.07;  // Correction factor at 0% O2
  heliumCalibrationStorage.write(heliumcalVoltage);

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(28, 0);
  display.print(F("Helium"));
  display.setCursor(16, 24);
  display.print(F("100% Cal"));
  display.setCursor(16, 48);
  display.print(heliumcalVoltage, 1);
  display.print(F(" mV"));
  display.display();
  delay(3000);
}

// Reset calibration
void resetToDefaultCalibration() {
  oxygencalVoltage = defaultOxygenCalVoltage;
  pureoxygenVoltage = defaultPureOxygenVoltage;
  heliumZeroVoltage = defaultHeliumZeroVoltage;
  heliumcalVoltage = defaultHeliumCalVoltage;
  oxygenCalibrationStorage.write(oxygencalVoltage);
  pureOxygenCalibrationStorage.write(pureoxygenVoltage);
  heliumZeroCalibrationStorage.write(heliumZeroVoltage);
  heliumCalibrationStorage.write(heliumcalVoltage);

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10, 0);
  display.print(F("Reset Cal"));
  display.setTextSize(1);
  display.setCursor(10, 24);
  display.print(F(" 21% O2:  "));
  display.print(defaultOxygenCalVoltage, 1);
  display.print(F(" mV"));
  display.setCursor(10, 34);
  display.print(F("100% O2:   "));
  display.print(defaultPureOxygenVoltage, 1);
  display.print(F(" mV"));
  display.setCursor(10, 46);
  display.print(F("  0% He:   "));
  display.print(defaultHeliumZeroVoltage, 1);
  display.print(F(" mV"));
  display.setCursor(10, 56);
  display.print(F("100% He: "));
  display.print(defaultHeliumCalVoltage, 1);
  display.print(F(" mV"));
  display.display();
  delay(5000);
}

void setup() {
  Serial.begin(9600);
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
  ads.begin(0x48);
  analogReadResolution(12);  // Internal ADC to 12-bit

  // Buttons
  pinMode(calButtonPin, INPUT_PULLUP);
  pinMode(displaySwitchPin, INPUT_PULLUP);

  // Start-up screen
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(28, 2);
  display.print(F("Trimix"));
  display.setCursor(16, 24);
  display.print(F("Analyser"));
  display.setTextSize(1);
  display.setCursor(22, 50);
  display.print(F("pre-warming..."));
  display.display();
  delay(5000);

  // Load last calibration values
  oxygencalVoltage = oxygenCalibrationStorage.read();
  if (oxygencalVoltage <= 0.0) {
    oxygencalVoltage = defaultOxygenCalVoltage;
    oxygenCalibrationStorage.write(oxygencalVoltage);
  }

  pureoxygenVoltage = pureOxygenCalibrationStorage.read();
  if (pureoxygenVoltage > oxygencalVoltage) {
    isTwoPointCalibrated = true;
  } else {
    pureoxygenVoltage = 0.0;
  }

  heliumZeroVoltage = heliumZeroCalibrationStorage.read();
  if (heliumZeroVoltage <= 0.0) {
    heliumZeroVoltage = defaultHeliumZeroVoltage;
    heliumZeroCalibrationStorage.write(heliumZeroVoltage);
  }

  heliumcalVoltage = heliumCalibrationStorage.read();
  if (heliumcalVoltage <= 0.0) {
    heliumcalVoltage = defaultHeliumCalVoltage;
    heliumCalibrationStorage.write(heliumcalVoltage);
  }

  // Display calibration values
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(4, 0);
  if (isTwoPointCalibrated) {
    display.print(F("2-pt Calib"));
  } else {
    display.print(F("1-pt Calib"));
  }
  display.setTextSize(1);
  display.setCursor(10, 24);
  display.print(F(" 21% O2: "));
  display.print(oxygencalVoltage, 1);
  display.print(F(" mV"));
  display.setCursor(10, 34);
  display.print(F("100% O2: "));
  display.print(pureoxygenVoltage, 1);
  display.print(F(" mV"));
  display.setCursor(10, 46);
  display.print(F("  0% He: "));
  display.print(heliumZeroVoltage, 1);
  display.print(F(" mV"));
  display.setCursor(10, 56);
  display.print(F("100% He: "));
  display.print(heliumcalVoltage, 1);
  display.print(F(" mV"));
  display.display();
  delay(5000);
}

void loop() {
  unsigned long currentTime = millis();

  // Check display switch button state
  if (digitalRead(displaySwitchPin) == LOW) {
    isDisplaySwitched = true;
  } else {
    isDisplaySwitched = false;
  }

  // Check calibration button state
  bool currentButtonState = digitalRead(calButtonPin) == LOW;

  if (currentButtonState != lastButtonState) {
    lastButtonDebounceTime = currentTime;
  }

  if ((currentTime - lastButtonDebounceTime) > 25) {  // Debounce delay 25 ms
    if (currentButtonState && !isButtonPressed) {
      isButtonPressed = true;
      buttonPressStartTime = currentTime;
      if (currentTime - lastButtonPressTime < longPressDuration) {
        pressCount++;
      } else {
        pressCount = 1;
      }
      lastButtonPressTime = currentTime;
      decisionWindowEnd = currentTime + longPressDuration;
    } else if (!currentButtonState && isButtonPressed) {
      isButtonPressed = false;
      unsigned long pressDuration = currentTime - buttonPressStartTime;
      if (pressDuration >= resetPressDuration) {
        resetToDefaultCalibration();  // Reset press
        pressCount = 0;
        decisionWindowEnd = 0;
      } else if (pressDuration >= longPressDuration) {
        performHeliumCalibration();  // Long press
        pressCount = 0;
        decisionWindowEnd = 0;
      }
    }
  }

  if (isButtonPressed) {
    unsigned long pressDuration = currentTime - buttonPressStartTime;
    if (pressDuration >= resetPressDuration) {
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(10, 16);
      display.print(F("Calibrate"));
      display.setCursor(34, 40);
      display.print(F("Reset"));
      display.display();
    } else if (pressDuration >= longPressDuration) {
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(10, 16);
      display.print(F("Calibrate"));
      display.setCursor(28, 40);
      display.print(F("Helium"));
      display.display();
    } else {
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(10, 16);
      display.print(F("Calibrate"));
      display.setCursor(28, 40);
      display.print(F("Oxygen"));
      display.display();
    }
  }

  if (!isButtonPressed && (currentTime - lastButtonPressTime > longPressDuration) && decisionWindowEnd > 0) {
    if (pressCount == 5) {
      calibrateZeroHeliumVoltage();  // Quintuple press
    } else if (pressCount == 3) {
      calibratePureOxygenVoltage();  // Triple press
    } else if (pressCount == 1) {
      performOxygenCalibration();  // Short press
    }
    pressCount = 0;
    decisionWindowEnd = 0;
  }
  lastButtonState = currentButtonState;

  // Sensor sampling
  if (currentTime - lastSampleTime >= (1000 / samplingRateHz)) {
    lastSampleTime = currentTime;

    if (sampleCount == 0) {
      oxygenSum = heliumSum = coSum = batterySum = 0.0;
    }

    // Oxygen
    ads.setGain(GAIN_SIXTEEN);
    int16_t oxygenRaw = ads.readADC_Differential_0_1();  // Oxygen sensor A0-A1
    float oxygenVoltage = fabs(oxygenRaw * 0.0078125);   // Gain 16, 256 mV
    oxygenSum += oxygenVoltage;

    // Helium
    ads.setGain(GAIN_FOUR);
    int16_t heliumRaw = ads.readADC_Differential_2_3();  // Helium sensor A2-A3
    float heliumVoltage = fabs(heliumRaw * 0.03125);     // Gain 4, 1024 mV
    heliumSum += heliumVoltage;

    // Carbon Monoxide
    int coRaw = analogRead(coPin);
    float coVoltage = coRaw * (3.3 / 4095.0);  // 12-bit ADC
    coSum += coVoltage;

    // Battery
    int batteryRaw = analogRead(batteryPin);
    float batteryVoltage = batteryRaw * (3.3 / 4095.0) * 3.0;  // 12-bit ADC, 1:2 voltage divider
    batterySum += batteryVoltage;

    sampleCount++;
  }

  // Update cycle
  if (decisionWindowEnd == 0 && currentTime - lastDisplayUpdate >= displayInterval) {
    lastDisplayUpdate = currentTime;

    // Calculate average voltages
    avgOxygenVoltage = (sampleCount > 0) ? (oxygenSum / sampleCount) : 0.0;
    float avgHeliumVoltage = (sampleCount > 0) ? (heliumSum / sampleCount) : 0.0;
    float avgBatteryVoltage = (sampleCount > 0) ? (batterySum / sampleCount) : 0.0;
    float avgCOVoltage = (sampleCount > 0) ? (coSum / sampleCount) : 0.0;
    sampleCount = 0;

    // Calculate Oxygen percentage
    float oxygenPercentage = max(0.0, calculateOxygenPercentage());

    // Calculate Helium percentage
    float correctedHeliumVoltage = max(0.0, avgHeliumVoltage - (17.0 / (1 + exp(-0.105 * (oxygenPercentage - 52.095)))));
    float heliumPercentage = (correctedHeliumVoltage > heliumZeroVoltage) ? ((correctedHeliumVoltage - heliumZeroVoltage) / (heliumcalVoltage - heliumZeroVoltage)) * 100.0 : 0.0;
    if (heliumPercentage < 2.0) {
      heliumPercentage = 0.0;
    }

    // Calculate MOD
    int mod14 = (oxygenPercentage > 0) ? min(999, (int)((1400.0 / oxygenPercentage) - 10.0)) : 0;  // MOD at ppO2 1.4
    int mod16 = (oxygenPercentage > 0) ? min(999, (int)((1600.0 / oxygenPercentage) - 10.0)) : 0;  // MOD at ppO2 1.6

    // Calculate END
    int end = (mod14 + 10.0) * (1 - heliumPercentage / 100.0) - 10.0;  // END at MOD 1.4

    // Calculate Density
    float den = (oxygenPercentage * 0.1756 - heliumPercentage * 1.0582 + 123.46) * (mod14 + 10) / 1000;  // Density at MOD 1.4

    // Calculate CO concentration
    float coConcentration = (avgCOVoltage > COZeroVoltage) ? 500 / (2.0 - COZeroVoltage) * (avgCOVoltage - COZeroVoltage) : 0.0;  // 500 ppm = 2 V

    // Serial Monitor
    Serial.println(avgOxygenVoltage, 2);
    Serial.println(correctedHeliumVoltage, 2);
    Serial.println(avgCOVoltage, 3);
    Serial.println(avgBatteryVoltage, 3);

    // Display
    display.clearDisplay();

    if (isDisplaySwitched) {
      display.setTextSize(2);
      display.setCursor(12, 0);
      display.print(oxygenPercentage, 1);
      display.print(F("/"));
      display.print(heliumPercentage, 1);
      display.setTextSize(1);
      display.setCursor(0, 20);
      display.print(F("MOD: "));
      display.print((int)mod14);
      display.print(F(" m / "));
      display.print((int)mod16);
      display.print(F(" m"));
      display.setCursor(0, 32);
      display.print(F("END ("));
      display.print((int)mod14);
      display.print(F("): "));
      display.print((int)end);
      display.print(F(" m"));
      display.setCursor(0, 44);
      display.print(F("Den: "));
      display.print(den, 1);
      display.print(F(" g/L"));
      display.setCursor(0, 56);
      display.print(F("CO: "));
      display.print(coConcentration, 1);
      display.print(F(" ppm"));
      if (coConcentration > 5.0) {
        display.print(F("  High!"));
      }
    }
    else {
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.print(F("O2:"));
      display.setCursor(0, 12);
      display.print(avgOxygenVoltage, 1);
      display.print(F("mV"));
      display.setTextSize(2);
      display.setCursor(52, 3);
      display.print(oxygenPercentage, 1);
      display.print(F("%"));
      display.setTextSize(1);
      display.setCursor(0, 28);
      display.print(F("He:"));
      display.setCursor(0, 40);
      display.print(correctedHeliumVoltage, 1);
      display.print(F("mV"));
      display.setTextSize(2);
      display.setCursor(52, 29);
      display.print(heliumPercentage, 1);
      display.print(F("%"));
      display.setTextSize(1);
      display.setCursor(0, 56);
      display.print(F("Battery: "));
      display.print(avgBatteryVoltage, 1);
      display.print(F("V"));
      if (avgBatteryVoltage < 7.0) {
        display.print(F("  Low!"));
      }
    }
    display.display();
  }
}