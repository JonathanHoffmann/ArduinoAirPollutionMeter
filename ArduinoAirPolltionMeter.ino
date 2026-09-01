#include <LiquidCrystal_I2C.h>
#include <sps30.h>

const bool DEBUG_ENABLED = true;

// --- LCD ---
// LCD Pin1 VSS GND
// LCD Pin2 VDD +5V
// LCD Pin3 VDA A4
// LCD Pin4 VCK A5
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- Relay ---
const int RELAY_PIN = 10;
const int ACKNOWLEDGE_BUTTON_PIN = 8; // Button wired between D8 and GND
const unsigned long ACKNOWLEDGE_DURATION_MS = 10UL * 60UL * 60UL * 1000UL;
const unsigned long BUTTON_DEBOUNCE_MS = 50UL;
const unsigned long MEASUREMENT_INTERVAL_MS = 1000UL;

// WHO 2021 Global Air Quality Guidelines, 24-hour mean values:
// https://www.who.int/publications/i/item/9789240034228
// WHO does not publish PM1.0 or PM4.0 limits. PM2.5's limit is used as a
// conservative project threshold for those measurements.
const float PM1P0_WARNING = 15.0f;  // ug/m3, PM2.5 proxy
const float PM2P5_WARNING = 15.0f;  // ug/m3, WHO 24-hour guideline
const float PM4P0_WARNING = 15.0f;  // ug/m3, PM2.5 proxy
const float PM10P0_WARNING = 45.0f; // ug/m3, WHO 24-hour guideline

// Air quality data, in ug/m3
float pm1p0 = 0.0f;
float pm2p5 = 0.0f;
float pm4p0 = 0.0f;
float pm10p0 = 0.0f;

bool displaySecondPage = false;
bool particleWarningActive = false;
bool warningAcknowledged = false;
unsigned long acknowledgementStartedAt = 0;
unsigned long lastMeasurementAt = 0;
bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;
unsigned long buttonLastChangedAt = 0;

void showSensorError(const char* message)
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SPS30 error");
  lcd.setCursor(0, 1);
  lcd.print(message);
}

void setup()
{
  if (DEBUG_ENABLED)
  {
    Serial.begin(9600);
  }

  lcd.begin(16, 2);
  lcd.backlight();
  lcd.print("Starting...");

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  pinMode(ACKNOWLEDGE_BUTTON_PIN, INPUT_PULLUP);

  sensirion_i2c_init();

  while (sps30_probe() != 0)
  {
    showSensorError("Check I2C wiring");
    if (DEBUG_ENABLED)
    {
      Serial.println("SPS30 probe failed");
    }
    delay(1000);
  }

  if (sps30_start_measurement() != 0)
  {
    showSensorError("Start failed");
    if (DEBUG_ENABLED)
    {
      Serial.println("SPS30 measurement start failed");
    }
    while (true)
    {
      delay(1000);
    }
  }

  lcd.clear();
  lcd.print("SPS30 ready");
  delay(1000);
}

bool readSPS30()
{
  uint16_t dataReady;
  struct sps30_measurement measurement;

  if (sps30_read_data_ready(&dataReady) != 0 || !dataReady)
  {
    return false;
  }

  if (sps30_read_measurement(&measurement) != 0)
  {
    showSensorError("Read failed");
    if (DEBUG_ENABLED)
    {
      Serial.println("SPS30 measurement read failed");
    }
    return false;
  }

  pm1p0 = measurement.mc_1p0;
  pm2p5 = measurement.mc_2p5;
  pm4p0 = measurement.mc_4p0;
  pm10p0 = measurement.mc_10p0;
  return true;
}

void updateDisplay()
{
  lcd.clear();

  if (displaySecondPage)
  {
    lcd.setCursor(0, 0);
    lcd.print("PM4.0: ");
    lcd.print(pm4p0, 1);
    lcd.setCursor(0, 1);
    lcd.print("PM10: ");
    lcd.print(pm10p0, 1);
  }
  else
  {
    lcd.setCursor(0, 0);
    lcd.print("PM1.0: ");
    lcd.print(pm1p0, 1);
    lcd.setCursor(0, 1);
    lcd.print("PM2.5: ");
    lcd.print(pm2p5, 1);
  }
}

void printDebugData()
{
  if (!DEBUG_ENABLED)
  {
    return;
  }

  Serial.print("PM1.0: ");
  Serial.print(pm1p0, 1);
  Serial.print("  PM2.5: ");
  Serial.print(pm2p5, 1);
  Serial.print("  PM4.0: ");
  Serial.print(pm4p0, 1);
  Serial.print("  PM10.0: ");
  Serial.println(pm10p0, 1);
}

bool hasParticleWarning()
{
  bool pm1p0Warning = pm1p0 > PM1P0_WARNING;
  bool pm2p5Warning = pm2p5 > PM2P5_WARNING;
  bool pm4p0Warning = pm4p0 > PM4P0_WARNING;
  bool pm10p0Warning = pm10p0 > PM10P0_WARNING;

  if (DEBUG_ENABLED && (pm1p0Warning || pm2p5Warning || pm4p0Warning || pm10p0Warning))
  {
    Serial.print("WARNING:");
    if (pm1p0Warning) Serial.print(" PM1.0");
    if (pm2p5Warning) Serial.print(" PM2.5");
    if (pm4p0Warning) Serial.print(" PM4.0");
    if (pm10p0Warning) Serial.print(" PM10.0");
    Serial.println();
  }

  return pm1p0Warning || pm2p5Warning || pm4p0Warning || pm10p0Warning;
}

bool acknowledgeButtonPressed()
{
  bool buttonReading = digitalRead(ACKNOWLEDGE_BUTTON_PIN);
  unsigned long now = millis();

  if (buttonReading != lastButtonReading)
  {
    buttonLastChangedAt = now;
  }

  if ((now - buttonLastChangedAt) >= BUTTON_DEBOUNCE_MS && buttonReading != stableButtonState)
  {
    stableButtonState = buttonReading;
    if (stableButtonState == LOW)
    {
      lastButtonReading = buttonReading;
      return true;
    }
  }

  lastButtonReading = buttonReading;
  return false;
}

void updateWarningLamp(bool warningActive)
{
  unsigned long now = millis();
  bool buttonPressed = acknowledgeButtonPressed();

  if (!warningActive)
  {
    warningAcknowledged = false;
    digitalWrite(RELAY_PIN, LOW);
    return;
  }

  if (buttonPressed)
  {
    warningAcknowledged = true;
    acknowledgementStartedAt = now;
    if (DEBUG_ENABLED)
    {
      Serial.println("Warning acknowledged for up to 10 hours");
    }
  }

  if (warningAcknowledged && (now - acknowledgementStartedAt) >= ACKNOWLEDGE_DURATION_MS)
  {
    warningAcknowledged = false;
    if (DEBUG_ENABLED)
    {
      Serial.println("Warning acknowledgement expired");
    }
  }

  digitalWrite(RELAY_PIN, warningAcknowledged ? LOW : HIGH);
}

void loop()
{
  unsigned long now = millis();

  updateWarningLamp(particleWarningActive);

  if ((now - lastMeasurementAt) < MEASUREMENT_INTERVAL_MS)
  {
    return;
  }

  lastMeasurementAt = now;
  if (!readSPS30())
  {
    return;
  }

  particleWarningActive = hasParticleWarning();
  updateWarningLamp(particleWarningActive);
  updateDisplay();
  printDebugData();
  displaySecondPage = !displaySecondPage;
}
