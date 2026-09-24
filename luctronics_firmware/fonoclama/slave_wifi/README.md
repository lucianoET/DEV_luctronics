# Slave Node — ESP32-C3 SuperMini
### Sistema de Avisos Wireless | Documentação de Montagem e Uso

---

## Índice
1. [Visão Geral](#visão-geral)
2. [Componentes](#componentes)
3. [Pinagem](#pinagem)
4. [Montagem](#montagem)
5. [Firmware](#firmware)
6. [Configuração (Captive Portal)](#configuração-captive-portal)
7. [Interface Web](#interface-web)
8. [Home Assistant](#home-assistant)
9. [Comportamento do LED](#comportamento-do-led)
10. [Botão Builtin](#botão-builtin)
11. [Solução de Problemas](#solução-de-problemas)

---

## Visão Geral

O Slave Node é um dispositivo autônomo que:
- Recebe **texto** e **áudio ao vivo** via WiFi (WebSocket)
- Exibe mensagens no **display OLED**
- Reproduz áudio via **amplificador I2S + speaker**
- Integra com o **Home Assistant** via MQTT
- É configurado sem regravar via **captive portal**

```
Celular / PC / Home Assistant
          │
          │ WiFi (WebSocket / MQTT)
          ▼
   ESP32-C3 SuperMini
     ├── OLED SSD1306   → exibe texto
     ├── MAX98357A      → amplifica áudio
     ├── Speaker        → reproduz áudio
     └── LED builtin    → indica status
```

---

## Componentes

| Componente | Modelo | Qtd | Observação |
|---|---|---|---|
| Microcontrolador | ESP32-C3 SuperMini | 1 | Com LED e botão builtin |
| Display OLED | SSD1306 128×64 I2C | 1 | 3.3V, endereço 0x3C |
| Amplificador I2S | MAX98357A | 1 | Módulo breakout |
| Speaker | 4Ω ou 8Ω, até 3W | 1 | |
| Perfboard | 7×9cm ou maior | 1 | |
| Cabo USB-C | — | 1 | Para gravação e alimentação |
| Fios jumper | — | — | Preferencialmente coloridos |
| Fonte | 5V 1A mínimo | 1 | USB-C ou regulador externo |

> **Opcional para expansão futura:**
> WS2812B (LED RGB), botões extras, módulo SD card.

---

## Pinagem

### ESP32-C3 SuperMini — Mapa Completo

```
              USB-C
         ┌────┤├────┐
  I2S LRC│G5       5V│ → MAX VIN, WS2812B VCC
 I2S BCLK│G4      GND│ → Terra comum
  I2S DIN│G3      G10│ → LED Status externo (futuro)
 WS2812B │G2       G9│ BOOT — não usar
    BTN2 │G1       G8│ LED builtin (invertido)
    BTN1 │G0       G7│ I2C SCL → OLED
      GND│GND      G6│ I2C SDA → OLED
      3V3│3V3      TX │ livre
         └──────────┘
```

> ⚠️ **GPIO 6–11 internos da flash são inacessíveis.**
> Os GPIOs expostos são: 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10.

---

### MAX98357A — Pinagem

| Pino MAX98357A | Conecta em | Cor sugerida |
|---|---|---|
| VIN | 5V (ESP32 pino 5V) | Vermelho |
| GND | GND | Preto |
| **SD_MODE** | **3.3V (obrigatório!)** | Laranja |
| GAIN | Não conectar (NC) | — |
| BCLK | GPIO4 | Verde |
| LRC | GPIO5 | Verde claro |
| DIN | GPIO3 | Verde escuro |
| OUT+ | Speaker (+) | Vermelho |
| OUT− | Speaker (−) | Preto |

> ⚠️ **SD_MODE DEVE ser conectado ao 3.3V.**
> Se deixado flutuando o amplificador entra em shutdown e não emite som.

---

### OLED SSD1306 — Pinagem

| Pino OLED | Conecta em | Cor sugerida |
|---|---|---|
| VCC | 3.3V | Vermelho |
| GND | GND | Preto |
| SCL | GPIO7 | Azul |
| SDA | GPIO6 | Amarelo |

> Endereço I2C padrão: `0x3C`
> Verificar com I2C scanner se o display não inicializar.

---

### LED e Botão Builtin

| Recurso | GPIO | Lógica |
|---|---|---|
| LED builtin | GPIO8 | **Invertido** — LOW = aceso, HIGH = apagado |
| Botão builtin (BOOT) | GPIO9 | INPUT_PULLUP — LOW quando pressionado |

---

## Montagem

### Ordem recomendada

1. **Fixe o ESP32-C3 SuperMini** na perfboard com os pinos soldados
2. **Posicione o MAX98357A** à direita do ESP32 (como na foto de referência)
3. **Posicione o OLED** abaixo do MAX98357A
4. **Faça o barramento GND** — una todos os GNDs em uma trilha horizontal
5. **Faça o barramento 5V** — una os 5V do ESP32 ao VIN do MAX
6. **Conecte SD_MODE → 3.3V** (crítico!)
7. **Conecte os fios I2S** (BCLK, LRC, DIN) entre ESP32 e MAX
8. **Conecte os fios I2C** (SDA, SCL) entre ESP32 e OLED
9. **Conecte o speaker** ao OUT+ e OUT− do MAX98357A
10. **Teste antes de fechar** qualquer caixa

### Diagrama de fios (cores sugeridas)

```
Vermelho  → 5V / VCC
Preto     → GND
Laranja   → 3.3V (SD_MODE do MAX)
Verde     → I2S (BCLK, LRC, DIN)
Azul      → I2C SCL
Amarelo   → I2C SDA
```

### Dicas de soldagem

- **GND único:** una todos os pontos GND em um fio contínuo pela borda da perfboard — evita loops de terra e ruído no áudio
- **Fios curtos:** quanto menor o fio I2S, menos interferência
- **Capacitor de desacoplamento:** 100µF entre 5V e GND próximo ao MAX98357A melhora a qualidade do áudio
- **Cuidado com o calor:** o ESP32-C3 SuperMini é sensível — não deixe o ferro mais de 3s no mesmo pino

---

## Firmware

### Dependências (Arduino IDE)

Instale via **Sketch → Include Library → Manage Libraries**:

| Biblioteca | Autor |
|---|---|
| Adafruit SSD1306 | Adafruit |
| Adafruit GFX Library | Adafruit |
| PubSubClient | Nick O'Leary |

Instale manualmente via ZIP (GitHub):

| Biblioteca | Repositório |
|---|---|
| ESPAsyncWebServer | github.com/me-no-dev/ESPAsyncWebServer |
| AsyncTCP | github.com/me-no-dev/AsyncTCP |

---

### Configuração da board

No Arduino IDE:

```
Ferramentas → Board → ESP32 Arduino → ESP32C3 Dev Module
Ferramentas → USB CDC On Boot → Enabled   ← importante para Serial
Ferramentas → Upload Speed → 921600
```

---

### Gravação

1. Segure o **botão BOOT (GPIO9)** do ESP32-C3
2. Conecte o USB-C
3. Solte o botão
4. Clique em **Upload** no Arduino IDE
5. Após gravar, pressione o botão **RESET** (ou reconecte o USB)

---

## Configuração (Captive Portal)

### Primeiro uso

```
1. Grave o firmware
2. Conecte ao WiFi "Slave-Setup" (sem senha)
3. O browser abre automaticamente (captive portal)
   → Se não abrir, acesse: http://192.168.4.1
4. Preencha os campos:
   - Nome do Slave (ex: sala, cozinha, escritorio)
   - WiFi SSID
   - WiFi Senha
   - IP do MQTT Broker (ex: 192.168.1.100)
   - Porta MQTT (padrão: 1883)
5. Clique em "Salvar e Reiniciar"
6. O ESP32 reinicia e conecta à sua rede
7. O IP aparece no OLED
```

### Reconfigurar

- **Via browser:** acesse `http://<IP_DO_SLAVE>/setup`
- **Via botão físico:**
  - Segurar botão no **boot** → entra direto no portal
  - Segurar **5 segundos em uso** → limpa config e reinicia no portal

---

## Interface Web

Acesse `http://<IP_DO_SLAVE>` no browser.

### Funcionalidades

| Seção | O que faz |
|---|---|
| **Texto → OLED** | Digita mensagem → aparece no display |
| **Microfone** | Streaming de áudio ao vivo via WebSocket → speaker |
| **Emergência** | Ativa modo emergência (LED SOS + OLED piscante) |
| **Normal** | Volta ao modo normal |
| **Reconfigurar** | Abre o captive portal para alterar configurações |

### Status na interface

- 🟢 **WS: Conectado** — WebSocket ativo
- 🟠 **MQTT: Conectado** — Home Assistant integrado
- ⚫ **Offline** — sem conexão

---

## Home Assistant

### Requisito

MQTT habilitado no Home Assistant. Em `configuration.yaml`:

```yaml
mqtt:
  broker: 192.168.1.x   # IP do seu broker (ex: Mosquitto Add-on)
  port: 1883
```

Ou via interface: **Configurações → Dispositivos e Serviços → MQTT**

### Autodiscovery

Assim que o slave conectar ao MQTT, ele aparece automaticamente em:
**Configurações → Dispositivos e Serviços → MQTT → Dispositivos**

Nome do dispositivo: `Slave <nome>` (conforme configurado no portal)

### Entidades criadas automaticamente

| Entidade | Tipo | Função |
|---|---|---|
| `<nome> Mensagem` | Text | Envia texto para o OLED |
| `<nome> Modo` | Select | Alterna Normal / Emergência |
| Status | Sensor | online / offline (LWT) |

### Tópicos MQTT

| Tópico | Direção | Payload |
|---|---|---|
| `slave/<nome>/text` | HA → Slave | Texto livre (até 64 chars) |
| `slave/<nome>/mode` | HA → Slave | `normal` ou `emergency` |
| `slave/<nome>/status` | Slave → HA | `online` / `offline` |

### Exemplo de automação no Home Assistant

```yaml
# Aviso de incêndio via sensor de fumaça
automation:
  - alias: "Aviso incêndio no slave sala"
    trigger:
      - platform: state
        entity_id: binary_sensor.sensor_fumaca
        to: "on"
    action:
      - service: mqtt.publish
        data:
          topic: "slave/sala/mode"
          payload: "emergency"
      - service: mqtt.publish
        data:
          topic: "slave/sala/text"
          payload: "FOGO! Evacue ja!"
```

---

## Comportamento do LED

| Situação | Padrão | Descrição |
|---|---|---|
| Normal | 2 piscadas a cada 5s | Dispositivo OK e aguardando |
| Emergência | SOS Morse em loop | `· · ·  — — —  · · ·` |
| Conectando WiFi | Piscada rápida | Tentando conectar |

### Tabela SOS Morse

```
S = · · ·   (3 pulsos curtos  80ms)
O = — — —   (3 pulsos longos 280ms)
S = · · ·   (3 pulsos curtos  80ms)
```

> LED GPIO8 no ESP32-C3 SuperMini é **invertido**:
> `LOW = aceso` | `HIGH = apagado`

---

## Botão Builtin (GPIO9)

| Ação | Resultado |
|---|---|
| Pressão curta (em uso) | Alterna Normal ↔ Emergência |
| Segurar 5 segundos (em uso) | Limpa configuração + reinicia no portal |
| Segurar no boot | Força entrada no captive portal |

---

## Solução de Problemas

### Display OLED não inicializa
- Verifique SDA → GPIO6 e SCL → GPIO7
- Confirme VCC em 3.3V (não 5V)
- Rode um I2C scanner para verificar o endereço (padrão: 0x3C)

### Sem som no speaker
- **Verifique SD_MODE → 3.3V** (causa mais comum)
- Confirme BCLK→G4, LRC→G5, DIN→G3
- Teste com volume alto — o MAX98357A com GAIN=NC produz 9dB
- Verifique polaridade do speaker

### Não conecta ao WiFi
- Confirme SSID e senha no captive portal (case-sensitive)
- O ESP32-C3 suporta apenas **2.4GHz**
- Se falhar 3x, segure o botão no boot para entrar no portal

### MQTT não aparece no Home Assistant
- Confirme que o broker está acessível na rede
- Verifique o IP e porta no portal de configuração
- No HA: **Configurações → Sistema → Logs** para ver erros MQTT
- Certifique-se que MQTT Discovery está ativado (`discovery: true`)

### Áudio com ruído ou falhas
- Adicione capacitor 100µF entre 5V e GND do MAX98357A
- Encurte os fios I2S
- Verifique GND comum entre ESP32 e MAX98357A
- Aumente o ring buffer no código se houver stutter frequente

### Upload falha
- Segure BOOT (GPIO9) antes de conectar o USB
- Verifique: **USB CDC On Boot → Enabled** no Arduino IDE
- Use cabo USB-C com dados (não só carga)

---

## Roadmap / Expansões Futuras

- [ ] Suporte a múltiplos slaves (broadcast para todos)
- [ ] WS2812B para indicação colorida por zona
- [ ] Botões extras (silenciar, confirmar recebimento)
- [ ] Cartão SD para cache local de mensagens
- [ ] OTA (atualização de firmware pelo ar)
- [ ] TTS via celular Bluetooth → slave retransmite
- [ ] Master físico com microfone PTT (ESP32 WROOM)

---

*Documentação gerada para o projeto Sistema de Avisos Wireless*
*Hardware: ESP32-C3 SuperMini | Firmware: Arduino ESP32 Core v3.x*
