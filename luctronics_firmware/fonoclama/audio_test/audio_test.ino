/*
 * TESTE MAX98357A + SPEAKER — ESP32-C3 SuperMini
 * BCLK=G4  LRC/WS=G5  DIN=G3  |  SD_MODE do amp DEVE estar em 3.3V
 *
 * Ciclo: 440Hz -> 880Hz -> 1760Hz -> silencio, 1s cada, repetindo.
 * Leitura:
 *   ouve os 3 tons subindo         = amp e speaker OK
 *   silencio total                 = SD_MODE flutuando, DIN solto ou speaker aberto
 *   chiado continuo no gap         = GND ruim / alimentacao do amp fraca
 *   som so num tom                 = speaker com bobina danificada
 */
#include <ESP_I2S.h>

#define I2S_BCLK 4
#define I2S_LRC  5
#define I2S_DIN  3
#define LED_PIN  8

#define SAMPLE_RATE 16000
#define AMPLITUDE   8000  // 0..32767 — knob de volume, baixe se distorcer

I2SClass i2s;
const int TONES[] = {440, 880, 1760, 0};
int16_t buf[SAMPLE_RATE / 10];  // 100ms por escrita

void setup() {
  Serial.begin(115200);
  delay(500);
  pinMode(LED_PIN, OUTPUT);
  Serial.println("\n=== TESTE AUDIO I2S ===");

  i2s.setPins(I2S_BCLK, I2S_LRC, I2S_DIN);
  bool ok = i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  Serial.printf("i2s.begin: %s\n", ok ? "OK" : "FALHOU");
  if (!ok) {
    Serial.println("driver nao subiu — pinos em conflito, reinicie");
    while (1) { digitalWrite(LED_PIN, millis() / 100 % 2); }
  }
  Serial.println("SD_MODE do amp em 3.3V? sem isso o chip fica em shutdown e nao sai som.");
}

uint32_t phase = 0;  // fase acumulada, evita clique entre buffers

void loop() {
  for (int freq : TONES) {
    Serial.printf("%s\n", freq ? String(String(freq) + " Hz — deve sair tom").c_str()
                               : "silencio — nao deve sair nada");
    digitalWrite(LED_PIN, freq ? LOW : HIGH);  // LED invertido: aceso durante o tom

    for (int chunk = 0; chunk < 10; chunk++) {  // 10 x 100ms = 1s
      for (size_t i = 0; i < sizeof(buf) / sizeof(buf[0]); i++) {
        buf[i] = freq ? (int16_t)(AMPLITUDE * sinf(2.0f * PI * phase / SAMPLE_RATE)) : 0;
        phase = freq ? (phase + freq) % SAMPLE_RATE : 0;
      }
      i2s.write((uint8_t *)buf, sizeof(buf));
    }
  }
}
