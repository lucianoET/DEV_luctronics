/*
 * Sentinela - sketch de caracterizacao de sensores IR e som
 *
 * Objetivo: capturar as saidas ANALOGICAS (AO) cruas a 200 Hz para decidir:
 *   1) o IR de chama separa fogo de lampada/sol pelo flicker de 5-15 Hz?
 *   2) os dois modulos de som tem ganho parecido o bastante para comparacao direcional?
 *
 * Ligacoes (todas em ADC1 - ADC2 nao funciona com WiFi, mas aqui WiFi fica desligado):
 *   IR1  AO -> GPIO32      IR2  AO -> GPIO33      IR3  AO -> GPIO34
 *   SOM1 AO -> GPIO35      SOM2 AO -> GPIO36
 *   GND comum obrigatorio. Modulos alimentados em 3V3 (se usar 5V, AO pode passar
 *   de 3,3 V e danificar o ADC - use divisor 2:1 nesse caso).
 *
 * IMPORTANTE - filtro anti-aliasing:
 *   Lampada fluorescente e LED com PWM piscam em 100/120 Hz. Amostrando a 200 Hz,
 *   120 Hz rebate para 80 Hz e 100 Hz cai na borda de Nyquist. O script de analise
 *   avisa quando isso aparece, mas o certo e montar um RC passa-baixa em cada AO:
 *      AO --[ 10k ]--+-- GPIO
 *                    |
 *                  [470n]
 *                    |
 *                   GND        -> fc ~ 34 Hz
 *   Rode uma bateria de testes SEM o RC e outra COM, e compare.
 *
 * Uso:
 *   1) Grave, abra a serial a 921600.
 *   2) Aperte uma tecla 0-9 durante o estimulo para marcar o trecho no CSV
 *      (ex.: 1 = isqueiro 1 m, 2 = isqueiro 2 m, 5 = sol, 6 = fluorescente...).
 *   3) Salve a saida em um arquivo .csv e rode analise_charact.py.
 *
 * Captura direto para arquivo (Linux):
 *   stty -F /dev/ttyUSB0 921600 raw && cat /dev/ttyUSB0 > ensaio.csv
 */

#include <Arduino.h>
#include "driver/adc.h"
#include "esp_timer.h"

#define FS_HZ      200
#define PERIOD_US  (1000000 / FS_HZ)
#define N_CH       5
#define BUF_SZ     512

static const adc1_channel_t CH[N_CH] = {
  ADC1_CHANNEL_4,  // GPIO32 - IR1
  ADC1_CHANNEL_5,  // GPIO33 - IR2
  ADC1_CHANNEL_6,  // GPIO34 - IR3
  ADC1_CHANNEL_7,  // GPIO35 - SOM1
  ADC1_CHANNEL_0,  // GPIO36 - SOM2
};

struct Sample {
  uint32_t t_us;
  uint16_t v[N_CH];
  uint8_t  mark;
};

static Sample buf[BUF_SZ];
static volatile uint16_t head = 0, tail = 0;
static volatile uint8_t  mark_pending = 0;
static volatile uint32_t overflows = 0;

// Roda na task do esp_timer (nao e ISR), entao adc1_get_raw() e seguro aqui.
static void on_tick(void *arg) {
  uint16_t h = head;
  uint16_t next = (h + 1) % BUF_SZ;
  if (next == tail) { overflows++; return; }

  buf[h].t_us = (uint32_t)esp_timer_get_time();
  for (int i = 0; i < N_CH; i++) buf[h].v[i] = adc1_get_raw(CH[i]);
  buf[h].mark = mark_pending;
  mark_pending = 0;
  head = next;
}

void setup() {
  Serial.begin(921600);
  delay(300);

  adc1_config_width(ADC_WIDTH_BIT_12);
  for (int i = 0; i < N_CH; i++) {
    // 11 dB = faixa util ~0..3,1 V. Em IDF 5.x a constante virou ADC_ATTEN_DB_12.
    adc1_config_channel_atten(CH[i], ADC_ATTEN_DB_11);
  }

  Serial.println("# sentinela charact fs=200Hz adc=12bit atten=11dB");
  Serial.println("# marcar trecho: enviar tecla 0-9 pela serial");
  Serial.println("t_us,ir1,ir2,ir3,som1,som2,mark");

  const esp_timer_create_args_t args = {
    .callback = &on_tick,
    .arg = NULL,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "adc_tick",
    .skip_unhandled_events = true,
  };
  esp_timer_handle_t t;
  esp_timer_create(&args, &t);
  esp_timer_start_periodic(t, PERIOD_US);
}

void loop() {
  if (Serial.available()) {
    int c = Serial.read();
    if (c >= '0' && c <= '9') mark_pending = (uint8_t)(c - '0');
  }

  static uint32_t last_report = 0;
  if (millis() - last_report > 5000) {
    last_report = millis();
    if (overflows) Serial.printf("# overflow=%u\n", overflows);
  }

  char line[80];
  while (tail != head) {
    Sample &s = buf[tail];
    int n = snprintf(line, sizeof(line), "%lu,%u,%u,%u,%u,%u,%u\n",
                     (unsigned long)s.t_us, s.v[0], s.v[1], s.v[2],
                     s.v[3], s.v[4], s.mark);
    Serial.write((uint8_t *)line, n);
    tail = (tail + 1) % BUF_SZ;
  }
}
