#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>

bool dbg = true;

// --- LCD ---
//LCD Pin1 VSS GND
//LCD Pin2 VDD +5V
//LCD Pin3 VDA A4
//LCD Pin4 VCK A5
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- PMS5003 Serial Pins ---
SoftwareSerial pmsSerial(2, 3);  // RX, TX  (PMS TX → pin 2)

// --- Relay ---
const int RELAY_PIN = 10; // Pin connected to relay IN warning lamp

// Air quality data
uint16_t pm0p1 = 0, pm1 = 0, pm2p5 = 0, pm10 = 0;
char buf[7];
uint16_t part_0p3 = 0;
uint16_t part_0p5 = 0;
uint16_t part_1p0 = 0;
uint16_t part_2p5 = 0;
uint16_t part_5p0 = 0;
uint16_t part_10 = 0;

void setup()
{
  if (dbg)
  {
    Serial.begin(9600);
  }
  lcd.begin(16, 2);
  lcd.backlight();
  lcd.print("Starting...");
  delay(200);
  lcd.clear();

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Start with relay off
}

bool readPMS()
{
  if (!pmsSerial.available()) return false;

  if (pmsSerial.read() != 0x42) return false;
  if (pmsSerial.read() != 0x4D) return false;

  uint16_t frameLength = (pmsSerial.read() << 8) | pmsSerial.read();
  if (frameLength != 28) return false;

  uint16_t data[13];
  for (int i = 0; i < 13; i++)
  {
    data[i] = (pmsSerial.read() << 8) | pmsSerial.read();
  }

  // Atmospheric (Environmental) PM concentrations
  pm1   = data[3];
  pm2p5 = data[4];
  pm10  = data[5];

  // --- ADDED: Particle count bins (per 0.1 L of air) ---
  part_0p3 = data[6];
  part_0p5 = data[7];
  part_1p0 = data[8];
  part_2p5 = data[9];
  part_5p0 = data[10];
  part_10  = data[11];

  // --- ADDED: Derive pm0p1 placeholder since PMS does NOT measure 0.1um ---
  pm0p1 = part_0p3 * 0.1; // user wanted pm0.1 variable, keep meaningful

  // checksum (ignored)
  pmsSerial.read();
  pmsSerial.read();

  return true;
}

void loop()
{
  if (readPMS())
  {
    // --- ADDED: Relay triggered by PM2.5 concentration ---
    if (pm2p5 > PM25_WARNING) digitalWrite(RELAY_PIN, HIGH);
    else digitalWrite(RELAY_PIN, LOW);

    lcd.clear();

    // Now show your PM values instead of temperatures
    lcd.setCursor(0, 0);
    lcd.print("PM1:");
    lcd.print(pm1);

    lcd.setCursor(9, 0);
    lcd.print("PM2.5:");
    lcd.print(pm2p5);

    lcd.setCursor(0, 1);
    lcd.print("PM10:");
    lcd.print(pm10);

    // Debug output
    if (dbg)
    {
      Serial.print("PM1: "); Serial.print(pm1);
      Serial.print("  PM2.5: "); Serial.print(pm2p5);
      Serial.print("  PM10: "); Serial.print(pm10);

      Serial.print("  0.3um: "); Serial.print(part_0p3);
      Serial.print("  0.5um: "); Serial.print(part_0p5);
      Serial.print("  1.0um: "); Serial.print(part_1p0);
      Serial.print("  2.5um: "); Serial.print(part_2p5);
      Serial.print("  5.0um: "); Serial.print(part_5p0);
      Serial.print("  10um: "); Serial.println(part_10);
    }
  }

  delay(1000);
}