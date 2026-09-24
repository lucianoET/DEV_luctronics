# luctronics_firmware

Firmwares do sistema de telemetria **Luctronics**.

Nodes ESP32/ESP32-C3 medem grandezas em campo e mandam para um ponto central.
O que começou como *Aguada* (nível de reservatório via HC-SR04) virou guarda-chuva
para outros tipos de node: qualidade do ar, GPS, paiol.

## O que tem aqui

| Pasta | O que é | Repo |
|---|---|---|
| `esphome/` | telemetria via ESPHome — **preferido** | `lucianoET/workspace_comunicador` |
| `platformio/` | telemetria em C++, sem HAOS | `lucianoET/aguada-firmware` |
| `fonoclama/` | avisos wireless, node à parte | `DEV_luctronics` |
| `sentinela/` | caracterização de sensores, node à parte | `DEV_luctronics` |
| `RuView-main/` | sensoriamento por CSI de WiFi — **de terceiros**, 165 MB | ⚠ sem git |

## Duas implementações do mesmo sistema

O mesmo hardware é descrito de duas formas. **Não são projetos concorrentes** —
são dois caminhos de deploy para os mesmos nodes.

| | `esphome/` | `platformio/` |
|---|---|---|
| Toolchain | ESPHome (YAML) | PlatformIO / ESP-IDF (C++) |
| Precisa de HAOS? | sim | **não** |
| Status | **preferido** | caminho sem-HAOS |
| Repo git | próprio | próprio |

### `esphome/` — preferido, roda no HAOS

Cada device é um YAML. Integração com Home Assistant é nativa: adicionou o YAML,
apareceu o sensor.

| Arquivo | Node |
|---|---|
| `devices/reservatorio_nivel.yaml` | reservatório, medição de nível |
| `devices/reservatorio_c3_padrao.yaml` | reservatório em ESP32-C3 |
| `devices/reservatorio_ethernet.yaml` | reservatório cabeado |
| `devices/gateway_espnow_c3.yaml` | gateway ESP-NOW |
| `devices/gateway_central.yaml` | gateway central |
| `devices/bomba_controller.yaml` | acionamento de bomba |
| `devices/valvula_controller.yaml` | acionamento de válvula |
| `devices/jardim_irrigacao.yaml` | irrigação |
| `custom_components/uart_distance_sensor.h` | driver HC-SR04 |
| `common/base.yaml` | base compartilhada |
| `home_assistant/` | automações e snippets de config |

### `platformio/` — funciona sem HAOS

Firmware C++ compilado. O node publica em tópicos MQTT crus (`aguada/...`) — o
broker pode ser qualquer um, Mosquitto avulso inclusive. **Só o `tools/bridge.py`
fala Home Assistant**, e apenas para emitir discovery (`homeassistant/.../config`).
Sem HAOS: ignore o bridge, consuma os tópicos `aguada/...` direto.

| Firmware | O que é |
|---|---|
| `firmware/node/` | node HC-SR04 → ESP-NOW |
| `firmware/node-eth/` | node cabeado, ENC28J60 → MQTT direto |
| `firmware/gateway/` | gateway ESP-NOW → serial e variante WiFi/MQTT |
| `firmware/gps_tracker/` | tracker GPS ATGM336H (C3 SuperMini / DevKit) |
| `firmware/rfid_test/` | leitor ID-12 → captive portal |
| `firmware/shared/` | protocolo e headers comuns |
| `firmware/.old/` | tentativas anteriores de gateway |
| `tools/bridge.py` | ponte serial/MQTT + discovery do Home Assistant |

Build: `~/.platformio/penv/bin/pio` (não está no PATH).
Convenções e comandos: `platformio/CLAUDE.md`.

## Em aberto

Manter as duas implementações sincronizadas é trabalho manual — um node novo
precisa nascer nos dois lados. Ainda não há teste que pegue divergência entre o
YAML e o C++.

A ideia é convergir para ESPHome-only. Plano, achados e ressalvas em
[`MIGRACAO-ESPHOME.md`](MIGRACAO-ESPHOME.md) — adiado, mas com a rota mapeada.

## Nodes à parte

Não são telemetria e não falam ESP-NOW — moram aqui por serem firmware ESP32 do
mesmo dono.

### `fonoclama/`

ESP32-C3 SuperMini + OLED que recebe texto e áudio ao vivo por WebSocket para dar
avisos. Não mede nada.

`hw_test/hw_test.ino` é o diagnóstico de G6/G7 (I2C) do C3 SuperMini — mesmo
defeito de GPIO6/7 que aparece no `gps_tracker`. `audio_test/` exercita o I2S.
`slave_wifi/` é o firmware do slave (captive portal + MQTT + WebSocket), que
estava perdido no repo do ESPHome como `slave_test.txt`.

### `sentinela/`

Fase de caracterização, ainda não é firmware de produção. Captura as saídas
analógicas cruas de 3 sensores IR de chama e 2 de som a 200 Hz (ADC1, GPIO32–36)
para decidir duas coisas antes de projetar o node:

1. o IR separa fogo de lâmpada/sol pelo flicker de 5–15 Hz?
2. os dois módulos de som têm ganho parecido o bastante para comparação direcional?

`analise_charact.py` processa a captura; `PROTOCOLO_ENSAIO.md` tem o procedimento
e o alerta de aliasing (lâmpada em 100/120 Hz rebate para dentro da banda a 200 Hz
— montar o RC passa-baixa de ~34 Hz antes de confiar no resultado).

## `RuView-main/` — de terceiros

Não é código próprio e não faz parte da telemetria. Mora aqui porque é firmware
ESP32 e porque entrega no mesmo lugar que o resto: Home Assistant.

Plataforma de *WiFi sensing* — lê Channel State Information (CSI) do rádio para
detectar presença, respiração e movimento através de paredes, sem câmera. O nó é
um ESP32 em `firmware/esp32-csi-node/` (ESP-IDF, com `sdkconfig` para C6, S3 e
DevKitC); o processamento é Python, e a saída vai para o HA por MQTT discovery,
ou por Matter.

Upstream: <https://github.com/ruvnet/ruvector> / <https://cognitum.one>. Para
atualizar, puxar do upstream — não há repo git local, e nada aqui foi modificado.
