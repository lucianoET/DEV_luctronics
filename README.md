# DEV_luctronics

Monorepo Luctronics. Cada pasta é independente — não há build unificado.

| Pasta | O que é | Repo |
|---|---|---|
| `luctronics_firmware/` | ⭐ firmwares ESP32/ESP32-C3 — ver README próprio | 2 repos + 3 sem git |
| `luctronics-site/` | site institucional (HTML estático + Netlify) | `lucianoET/luctronics-site` |
| `.reference/` | material de consulta e projetos arquivados — ver README próprio |
| `.delete/` | staging de remoção, nada apagado — ver `MANIFEST.md` |

O que **não** mora aqui: firmwares de terceiros sem ligação com o sistema,
drivers e scripts de sistema ficam em `../DEV_ESP32 firmwares/`. As configs do
Home Assistant ficam em `../DEV_HAOS/`.

## `luctronics_firmware/`

Telemetria: nodes ESP32 medindo nível de reservatório (HC-SR04), qualidade do ar,
GPS, paiol. Duas implementações do mesmo hardware, mais nodes à parte:

| Pasta | O que é | Repo |
|---|---|---|
| `esphome/` | **preferido**, roda no HAOS | `lucianoET/workspace_comunicador` |
| `platformio/` | funciona sem HAOS, publica em MQTT cru | `lucianoET/aguada-firmware` |
| `fonoclama/` | avisos wireless — C3 SuperMini + OLED + áudio por WebSocket | ⚠ sem git |
| `sentinela/` | caracterização de sensores IR e som (ADC1, 200 Hz) | ⚠ sem git |
| `RuView-main/` | WiFi sensing por CSI — **de terceiros**, entrega no HA | ⚠ sem git |

O nome do repo do `esphome/` (`workspace_comunicador`) é herança e não bate com o
conteúdo. Detalhes em `luctronics_firmware/README.md`.

## Pendências

- **`fonoclama/` e `sentinela/` não estão em git.** Exemplar único em disco.
- **`luctronics_firmware/esphome/.git` carrega ~722 MB** de objetos órfãos do
  cache ESP-IDF que foi commitado por engano — ver a memória do projeto.
- `.reference/cmasm-erp-*` e `erp-cmms-*` são do CMASM.ERP; se `~/cmms-monorepo`
  virar o lar desse projeto, eles vão junto.
# DEV_luctronics
