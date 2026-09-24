/*
 * MONITOR G6/G7 — diagnostico ao vivo, 1s
 * Nível dos fios + scan I2C no par G6(SDA)/G7(SCL).
 * Leitura:
 *   G6/G7 HIGH + ack 0x3C = TUDO OK, tela acende
 *   pino LOW = linha travada -> display SEM VCC (diodo clampa) ou curto
 *   HIGH sem ack = fio SDA/SCL solto (mas modulo energizado em algum lugar)
 */
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_SDA 6
#define OLED_SCL 7
#define LED_PIN 8

Adafruit_SSD1306 oled(128, 64, &Wire, -1);
bool alive = false;

void setup() {
  Serial.begin(115200);
  delay(500);
  pinMode(LED_PIN, OUTPUT);
  Serial.println("\n=== MONITOR G6/G7 ===");
}

uint32_t n = 0;
void loop() {
  digitalWrite(LED_PIN, n % 2);

  // nivel dos fios (sem pull interno — pullup do modulo deve segurar HIGH)
  Wire.end();
  pinMode(OLED_SDA, INPUT);
  pinMode(OLED_SCL, INPUT);
  delayMicroseconds(100);
  int sda = digitalRead(OLED_SDA), scl = digitalRead(OLED_SCL);

  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setClock(100000);
  Wire.setTimeOut(50);

  uint8_t found = 0;
  for (uint8_t a : {0x3C, 0x3D}) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { found = a; break; }
  }

  const char* diag;
  if (found && sda && scl)      diag = "TUDO OK";
  else if (!sda || !scl)        diag = "linha LOW -> display sem VCC ou curto GND";
  else                          diag = "pullup OK mas sem ack -> SDA/SCL solto ou trocado";

  Serial.printf("SDA(G6)=%s SCL(G7)=%s ack=%s | %s\n",
    sda ? "HIGH" : "LOW", scl ? "HIGH" : "LOW",
    found ? "0x3C" : "nao", diag);

  if (found && !alive) {
    alive = oled.begin(SSD1306_SWITCHCAPVCC, found);
    Serial.printf(">>> init %s\n", alive ? "OK — TELA DEVE ACENDER" : "falhou");
  }
  if (!found) alive = false;

  if (alive) {
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(2);
    oled.setCursor(0, 0);  oled.println("TESTE OK");
    oled.setCursor(0, 24); oled.printf("n=%lu", (unsigned long)n);
    oled.display();
  }
  n++;
  delay(1000);
}
