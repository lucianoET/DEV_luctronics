# Migração para ESPHome-only — backlog

**Status:** adiado (14/08/2026). Decisão: vale ir, mas não é migração de um dia.
Enquanto não acontece, `platformio/` continua sendo o caminho que roda sem HAOS.

**Regra:** aposentar um firmware do `platformio/` só quando o YAML equivalente
estiver validado no hardware. Nunca antes.

---

## Por que vale

Verificado na doc atual do ESPHome (não é suposição):

- `espnow:` é **componente core** — não precisa de external component. Tem
  `packet_transport: platform: espnow` com encryption e lista de peers. Cobre a
  arquitetura node → gateway.
- `gps:` core, NMEA por UART.
- `ultrasonic:` core, cobre o HC-SR04.

## O que está quebrado hoje no `esphome/`

| # | Achado | Ação |
|---|---|---|
| 1 | `custom_components/uart_distance_sensor.h` usa `#include "esphome.h"` — API removida no ESPHome **2025.2.0**; a pasta `custom_components/` some no **2026.6.0** | migrar para `external_components` ou apagar |
| 2 | `devices/reservatorio_ethernet.yaml` linha 9: bloco `ethernet:` está comentado | não faz Ethernet, só tem o nome |
| 3 | nenhum YAML declara `external_components:` | — |
| 4 | `esphome` não está instalado nesta máquina | os YAMLs nunca foram validados aqui, só (talvez) dentro do add-on do HAOS |

## Dependência de HAOS está no lado errado

Hoje é o `platformio/` que roda sem HAOS, e o `esphome/` que não roda.

`common/base.yaml` traz:

- `api:` — API nativa do Home Assistant
- `time: platform: homeassistant` — hora vem do HA

E `devices/reservatorio_nivel.yaml` tem 3 sensores `platform: homeassistant`, que
deixam de existir sem HA.

**Troca para standalone:** `api:` → `mqtt:`, `time: homeassistant` → `sntp`,
sensores `homeassistant` → `number`/`globals` locais. Depois disso roda com
Mosquitto avulso. O compilador ESPHome pode viver em pip/Docker — não exige HAOS.

## Cobertura: faltam firmwares

| `platformio/firmware/` | contraparte YAML |
|---|---|
| `node` (HC-SR04) | `reservatorio_nivel` ✓ |
| `gateway` | `gateway_espnow_c3`, `gateway_central` ✓ |
| `node-eth` (ENC28J60) | `reservatorio_ethernet` ✗ ethernet comentado |
| `gps_tracker` | ✗ nenhum |
| `rfid_test` | ✗ nenhum |

## Ordem sugerida

1. Instalar `esphome` local e rodar `esphome config` nos 9 YAMLs — barato,
   independente do resto, e mostra quantos realmente compilam.
2. Consertar os achados 1 e 2.
3. Tirar a dependência de HAOS do `base.yaml`.
4. Portar `node-eth` e `rfid_test`.
5. Por último `gps_tracker` — ver ressalva abaixo.

## Ressalva do gps_tracker

O componente `gps:` do ESPHome usa TinyGPS++ cru, que **tem o mesmo bug de
rollover de 1024 semanas** (2006 → 2026) corrigido em 14/08/2026 no
`platformio/firmware/gps_tracker/src/gps_reader.cpp` (compensação de 7168 dias).

Migrar o tracker significa reimplementar essa compensação como lambda ou
external_component. É o único item onde o YAML custa mais que o C++.
