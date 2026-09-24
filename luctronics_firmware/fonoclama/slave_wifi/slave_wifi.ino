/*
 * SLAVE WIFI — ESP32-C3 SuperMini
 * - Captive Portal para configurar WiFi + MQTT
 * - Integração Home Assistant via MQTT autodiscovery
 * - WebSocket para texto e áudio ao vivo
 *
 * PRIMEIRO USO:
 *   1. Grave o firmware
 *   2. Conecte ao WiFi "Slave-Setup" (sem senha)
 *   3. Abra qualquer URL — redireciona para configuração
 *   4. Preencha WiFi + IP/porta do MQTT + nome do slave
 *   5. Salva na NVS e reinicia
 *
 * HOME ASSISTANT:
 *   - Autodiscovery via MQTT
 *   - Entidade "text" para enviar mensagem ao OLED
 *   - Entidade "select" para modo (Normal/Emergência)
 *   - Sensor de status online/offline (LWT)
 *
 * Tópicos MQTT:
 *   slave/<nome>/text         ← envia texto pro OLED
 *   slave/<nome>/mode         ← "normal" ou "emergency"
 *   slave/<nome>/status       → online/offline (LWT)
 *
 * Pinagem:
 *   OLED SDA→G6  SCL→G7  VCC→3V3
 *   MAX  BCLK→G4 LRC→G5  DIN→G3  SD→3V3  VIN→5V
 *   LED builtin→G8 (invertido)   Botão→G9
 *
 * Dependências:
 *   Adafruit SSD1306, Adafruit GFX
 *   ESPAsyncWebServer + AsyncTCP (me-no-dev/GitHub)
 *   PubSubClient (knolleary)
 *   Preferences (built-in ESP32)
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <driver/i2s_std.h>

// ── Pinos ──────────────────────────────────────────
#define OLED_SDA    6
#define OLED_SCL    7
#define I2S_BCLK    4
#define I2S_LRC     5
#define I2S_DOUT    3
#define LED_PIN     8
#define BTN_PIN     9

// ── OLED ───────────────────────────────────────────
Adafruit_SSD1306 oled(128, 64, &Wire, -1);

// ── Servidores ─────────────────────────────────────
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
DNSServer      dns;

// ── NVS / Preferências ─────────────────────────────
Preferences prefs;
String cfg_ssid, cfg_pass, cfg_mqtt_host, cfg_name;
int    cfg_mqtt_port = 1883;
bool   configured    = false;

// ── MQTT ───────────────────────────────────────────
WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);
bool         mqttConnected = false;
uint32_t     lastMqttRetry = 0;

// ── I2S ────────────────────────────────────────────
i2s_chan_handle_t i2s_tx;

// ── Ring buffer ────────────────────────────────────
#define RING_SIZE 16384
uint8_t  ringBuf[RING_SIZE];
volatile int wPos = 0, rPos = 0;
inline int bufAvail() { return (wPos - rPos + RING_SIZE) % RING_SIZE; }

// ── Estado ─────────────────────────────────────────
enum Mode { NORMAL, EMERGENCY };
volatile Mode mode     = NORMAL;
String        oledText = "Aguardando...";
bool          receiving = false;
uint32_t      lastAudio = 0;

// ══════════════════════════════════════════════════
// LED helpers
// ══════════════════════════════════════════════════
inline void ledOn()  { digitalWrite(LED_PIN, LOW);  }
inline void ledOff() { digitalWrite(LED_PIN, HIGH); }
void blink(uint32_t on, uint32_t off) { ledOn(); delay(on); ledOff(); delay(off); }
void blinkSOS() {
  for (int i=0;i<3;i++) blink(80,  80);  delay(200);
  for (int i=0;i<3;i++) blink(280, 80);  delay(200);
  for (int i=0;i<3;i++) blink(80,  80);  delay(500);
}

// ══════════════════════════════════════════════════
// NVS — carrega / salva config
// ══════════════════════════════════════════════════
void loadConfig() {
  prefs.begin("slave", true);
  cfg_ssid      = prefs.getString("ssid",      "");
  cfg_pass      = prefs.getString("pass",      "");
  cfg_mqtt_host = prefs.getString("mqtt_host", "");
  cfg_mqtt_port = prefs.getInt   ("mqtt_port", 1883);
  cfg_name      = prefs.getString("name",      "slave1");
  prefs.end();
  configured = (cfg_ssid.length() > 0);
}

void saveConfig(String ssid, String pass, String host, int port, String name) {
  prefs.begin("slave", false);
  prefs.putString("ssid",      ssid);
  prefs.putString("pass",      pass);
  prefs.putString("mqtt_host", host);
  prefs.putInt   ("mqtt_port", port);
  prefs.putString("name",      name);
  prefs.end();
}

void clearConfig() {
  prefs.begin("slave", false);
  prefs.clear();
  prefs.end();
}

// ══════════════════════════════════════════════════
// Captive Portal HTML
// ══════════════════════════════════════════════════
const char PORTAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="pt-BR">
<head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Slave Setup</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#111;color:#eee;font-family:sans-serif;display:flex;justify-content:center;align-items:center;min-height:100vh;padding:20px}
.card{background:#1e1e2e;border-radius:14px;padding:24px;width:100%;max-width:380px;display:flex;flex-direction:column;gap:14px}
h1{color:#4a9eff;font-size:1.3em;text-align:center}
label{font-size:.85em;color:#888;margin-bottom:2px;display:block}
input{width:100%;padding:10px;border-radius:8px;border:1px solid #333;background:#111;color:#eee;font-size:1em}
.row{display:flex;gap:8px}
.row input:first-child{flex:2}
.row input:last-child{flex:1}
button{padding:12px;border:none;border-radius:8px;font-size:1em;cursor:pointer;font-weight:bold;background:#4a9eff;color:#fff}
.note{font-size:.75em;color:#555;text-align:center}
#msg{text-align:center;color:#00ff44;font-size:.9em;display:none}
</style></head>
<body><div class="card">
<h1>🔧 Slave Setup</h1>

<div><label>Nome do Slave</label>
<input id="name" placeholder="ex: sala, cozinha" value="slave1"></div>

<div><label>WiFi SSID</label>
<input id="ssid" placeholder="Nome da rede"></div>

<div><label>WiFi Senha</label>
<input id="pass" type="password" placeholder="Senha"></div>

<div><label>MQTT Broker</label>
<div class="row">
  <input id="host" placeholder="192.168.1.x">
  <input id="port" placeholder="1883" value="1883">
</div></div>

<button onclick="save()">Salvar e Reiniciar</button>
<div id="msg">✅ Salvo! Reiniciando...</div>
<p class="note">Após salvar, conecte de volta à sua rede WiFi e acesse o IP do dispositivo.</p>
</div>
<script>
async function save() {
  const body = new URLSearchParams({
    name: document.getElementById('name').value,
    ssid: document.getElementById('ssid').value,
    pass: document.getElementById('pass').value,
    host: document.getElementById('host').value,
    port: document.getElementById('port').value
  });
  await fetch('/save', {method:'POST', body});
  document.getElementById('msg').style.display='block';
}
</script></body></html>
)rawliteral";

// ══════════════════════════════════════════════════
// Interface principal HTML
// ══════════════════════════════════════════════════
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="pt-BR">
<head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Slave Control</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#111;color:#eee;font-family:sans-serif;display:flex;flex-direction:column;align-items:center;padding:20px;gap:14px}
h1{color:#4a9eff;font-size:1.3em}
.card{background:#1e1e2e;border-radius:12px;padding:16px;width:100%;max-width:420px;display:flex;flex-direction:column;gap:10px}
.card h2{font-size:.9em;color:#888;border-bottom:1px solid #333;padding-bottom:6px}
input[type=text]{width:100%;padding:10px;border-radius:8px;border:1px solid #333;background:#111;color:#eee;font-size:1em}
button{padding:12px;border:none;border-radius:8px;font-size:1em;cursor:pointer;font-weight:bold;transition:.2s}
.btn-send{background:#4a9eff;color:#fff}
.btn-mic{background:#2a7a2a;color:#fff}
.btn-mic.active{background:#cc2222;animation:pulse 1s infinite}
.btn-em{background:#cc2222;color:#fff}
.btn-norm{background:#2a5a2a;color:#fff}
.btn-cfg{background:#333;color:#aaa;font-size:.85em;padding:8px}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.6}}
#status{font-size:.8em;color:#666;text-align:center}
#dot{width:10px;height:10px;border-radius:50%;background:#333;display:inline-block;margin-right:5px}
#dot.on{background:#00ff44}
#mqttdot{width:10px;height:10px;border-radius:50%;background:#333;display:inline-block;margin-right:5px}
#mqttdot.on{background:#f0a500}
.row{display:flex;gap:8px}.row button{flex:1}
</style></head>
<body>
<h1>🔊 Slave Control</h1>

<div class="card">
  <h2>Texto → OLED</h2>
  <input id="txt" type="text" placeholder="Digite a mensagem..." maxlength="64">
  <button class="btn-send" onclick="sendText()">Enviar para Display</button>
</div>

<div class="card">
  <h2>Áudio ao vivo</h2>
  <button class="btn-mic" id="micBtn" onclick="toggleMic()">🎤 Iniciar Microfone</button>
  <div id="status">
    <span id="dot"></span>WS: <span id="wstxt">Desconectado</span>
    &nbsp;|&nbsp;
    <span id="mqttdot"></span>MQTT: <span id="mqtttxt">--</span>
  </div>
</div>

<div class="card">
  <h2>Modo</h2>
  <div class="row">
    <button class="btn-em"   onclick="sendCmd('CMD:EMERGENCY')">🚨 Emergência</button>
    <button class="btn-norm" onclick="sendCmd('CMD:NORMAL')">✅ Normal</button>
  </div>
</div>

<div class="card">
  <button class="btn-cfg" onclick="location.href='/setup'">⚙️ Reconfigurar</button>
</div>

<script>
const ws = new WebSocket('ws://' + location.host + '/ws');
ws.binaryType = 'arraybuffer';
const dot=document.getElementById('dot'), wstxt=document.getElementById('wstxt');
const mqttdot=document.getElementById('mqttdot'), mqtttxt=document.getElementById('mqtttxt');
ws.onopen  = ()=>{ dot.className='on'; wstxt.textContent='Conectado'; fetchStatus(); };
ws.onclose = ()=>{ dot.className='';   wstxt.textContent='Desconectado'; };
ws.onmessage = e => {
  try { const d=JSON.parse(e.data);
    mqttdot.className = d.mqtt?'on':'';
    mqtttxt.textContent = d.mqtt?'Conectado':'Offline';
  } catch(e){}
};

function fetchStatus() { setInterval(()=>{ if(ws.readyState===1) ws.send('STATUS'); },3000); }
function sendText()  { const v=document.getElementById('txt').value.trim(); if(v&&ws.readyState===1) ws.send('TXT:'+v); }
function sendCmd(c)  { if(ws.readyState===1) ws.send(c); }
document.getElementById('txt').addEventListener('keydown',e=>{ if(e.key==='Enter') sendText(); });

let audioCtx,processor,stream,micActive=false;
async function toggleMic() {
  const btn=document.getElementById('micBtn');
  if (!micActive) {
    try {
      stream = await navigator.mediaDevices.getUserMedia({audio:{sampleRate:16000,channelCount:1,echoCancellation:true}});
      audioCtx = new AudioContext({sampleRate:16000});
      const src = audioCtx.createMediaStreamSource(stream);
      processor = audioCtx.createScriptProcessor(2048,1,1);
      processor.onaudioprocess = e => {
        if (ws.readyState!==1) return;
        const f=e.inputBuffer.getChannelData(0), i16=new Int16Array(f.length);
        for(let i=0;i<f.length;i++){ let s=Math.max(-1,Math.min(1,f[i])); i16[i]=s<0?s*0x8000:s*0x7FFF; }
        ws.send(i16.buffer);
      };
      src.connect(processor); processor.connect(audioCtx.destination);
      micActive=true; btn.textContent='⏹ Parar'; btn.className='btn-mic active';
    } catch(e){ alert('Erro microfone: '+e.message); }
  } else {
    processor.disconnect(); audioCtx.close(); stream.getTracks().forEach(t=>t.stop());
    micActive=false; btn.textContent='🎤 Iniciar Microfone'; btn.className='btn-mic';
  }
}
</script></body></html>
)rawliteral";

// ══════════════════════════════════════════════════
// I2S setup
// ══════════════════════════════════════════════════
void setupI2S() {
  i2s_chan_config_t ch = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
  ch.auto_clear = true;
  i2s_new_channel(&ch, &i2s_tx, NULL);
  i2s_std_config_t cfg = {
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(16000),
    .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
    .gpio_cfg = {
      .mclk=I2S_GPIO_UNUSED, .bclk=(gpio_num_t)I2S_BCLK,
      .ws=(gpio_num_t)I2S_LRC, .dout=(gpio_num_t)I2S_DOUT,
      .din=I2S_GPIO_UNUSED,
      .invert_flags={false,false,false}
    }
  };
  i2s_channel_init_std_mode(i2s_tx, &cfg);
  i2s_channel_enable(i2s_tx);
}

// ══════════════════════════════════════════════════
// MQTT — Home Assistant autodiscovery
// ══════════════════════════════════════════════════
String topicText()   { return "slave/" + cfg_name + "/text";   }
String topicMode()   { return "slave/" + cfg_name + "/mode";   }
String topicStatus() { return "slave/" + cfg_name + "/status"; }

void mqttPublishDiscovery() {
  // Entidade text (mensagem OLED)
  String tText = "homeassistant/text/slave_" + cfg_name + "_text/config";
  String pText = "{\"name\":\"" + cfg_name + " Mensagem\","
    "\"unique_id\":\"slave_" + cfg_name + "_text\","
    "\"command_topic\":\"" + topicText() + "\","
    "\"availability_topic\":\"" + topicStatus() + "\","
    "\"payload_available\":\"online\","
    "\"payload_not_available\":\"offline\","
    "\"device\":{\"identifiers\":[\"slave_" + cfg_name + "\"],"
    "\"name\":\"Slave " + cfg_name + "\","
    "\"model\":\"ESP32-C3 SuperMini\","
    "\"manufacturer\":\"DIY\"}}";
  mqtt.publish(tText.c_str(), pText.c_str(), true);

  // Entidade select (modo)
  String tMode = "homeassistant/select/slave_" + cfg_name + "_mode/config";
  String pMode = "{\"name\":\"" + cfg_name + " Modo\","
    "\"unique_id\":\"slave_" + cfg_name + "_mode\","
    "\"command_topic\":\"" + topicMode() + "\","
    "\"options\":[\"normal\",\"emergency\"],"
    "\"availability_topic\":\"" + topicStatus() + "\","
    "\"payload_available\":\"online\","
    "\"payload_not_available\":\"offline\","
    "\"device\":{\"identifiers\":[\"slave_" + cfg_name + "\"]}}";
  mqtt.publish(tMode.c_str(), pMode.c_str(), true);
}

void mqttCallback(char *topic, byte *payload, unsigned int len) {
  String t = String(topic);
  String msg = "";
  for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];

  if (t == topicText()) {
    oledText = msg;
    mode = NORMAL;
  } else if (t == topicMode()) {
    if (msg == "emergency") { mode = EMERGENCY; oledText = "!! EMERGENCIA !!"; }
    else                    { mode = NORMAL;     oledText = "Aguardando...";   }
  }
}

void mqttConnect() {
  if (!mqtt.connected() && millis() - lastMqttRetry > 5000) {
    lastMqttRetry = millis();
    String clientId = "slave_" + cfg_name;
    bool ok = mqtt.connect(
      clientId.c_str(),
      NULL, NULL,                          // user/pass (ajuste se necessário)
      topicStatus().c_str(), 0, true, "offline"  // LWT
    );
    if (ok) {
      mqttConnected = true;
      mqtt.publish(topicStatus().c_str(), "online", true);
      mqtt.subscribe(topicText().c_str());
      mqtt.subscribe(topicMode().c_str());
      mqttPublishDiscovery();
      Serial.println("MQTT conectado");
    } else {
      mqttConnected = false;
      Serial.printf("MQTT falhou rc=%d\n", mqtt.state());
    }
  }
}

// ══════════════════════════════════════════════════
// WebSocket handler
// ══════════════════════════════════════════════════
void onWsEvent(AsyncWebSocket *s, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->opcode == WS_TEXT) {
      String msg = String((char *)data).substring(0, len);
      if      (msg == "STATUS")            client->text("{\"mqtt\":" + String(mqttConnected?"true":"false") + "}");
      else if (msg == "CMD:EMERGENCY")   { mode = EMERGENCY; oledText = "!! EMERGENCIA !!"; }
      else if (msg == "CMD:NORMAL")      { mode = NORMAL;    oledText = "Aguardando...";   }
      else if (msg.startsWith("TXT:"))   { oledText = msg.substring(4); mode = NORMAL; }
    } else if (info->opcode == WS_BINARY) {
      for (size_t i = 0; i < len; i++) { ringBuf[wPos] = data[i]; wPos = (wPos+1)%RING_SIZE; }
      receiving = true; lastAudio = millis();
    }
  }
}

// ══════════════════════════════════════════════════
// Modo Captive Portal
// ══════════════════════════════════════════════════
void startCaptivePortal() {
  oled.clearDisplay(); oled.setCursor(0,0);
  oled.println("== SETUP MODE ==");
  oled.println("WiFi: Slave-Setup");
  oled.println("Abra qualquer site");
  oled.println("ou: 192.168.4.1");
  oled.display();

  WiFi.mode(WIFI_AP);
  WiFi.softAP("Slave-Setup");
  dns.start(53, "*", WiFi.softAPIP());

  // Redireciona tudo para o portal
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){ r->send_P(200,"text/html",PORTAL_HTML); });
  server.on("/setup", HTTP_GET, [](AsyncWebServerRequest *r){ r->send_P(200,"text/html",PORTAL_HTML); });
  server.onNotFound([](AsyncWebServerRequest *r){ r->redirect("http://192.168.4.1/"); });

  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *r) {
    String ssid = r->arg("ssid");
    String pass = r->arg("pass");
    String host = r->arg("host");
    int    port = r->arg("port").toInt();
    String name = r->arg("name");
    name.replace(" ", "_");
    if (ssid.length() > 0) {
      saveConfig(ssid, pass, host, port, name);
      r->send(200, "text/plain", "OK");
      delay(1500);
      ESP.restart();
    } else {
      r->send(400, "text/plain", "SSID vazio");
    }
  });

  server.begin();
  Serial.println("Portal iniciado em 192.168.4.1");

  // Loop do portal (DNS precisa de poll)
  while (true) { dns.processNextRequest(); delay(10); }
}

// ══════════════════════════════════════════════════
// Modo Normal — rota setup para reconfigurar
// ══════════════════════════════════════════════════
void setupWebServer() {
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){ r->send_P(200,"text/html",INDEX_HTML); });

  // Página de reconfiguração acessível em /setup
  server.on("/setup", HTTP_GET, [](AsyncWebServerRequest *r){ r->send_P(200,"text/html",PORTAL_HTML); });
  server.on("/save",  HTTP_POST,[](AsyncWebServerRequest *r){
    String ssid=r->arg("ssid"), pass=r->arg("pass"), host=r->arg("host"), name=r->arg("name");
    int port=r->arg("port").toInt();
    name.replace(" ","_");
    if (ssid.length()>0){ saveConfig(ssid,pass,host,port,name); r->send(200,"text/plain","OK"); delay(1500); ESP.restart(); }
    else r->send(400,"text/plain","SSID vazio");
  });

  // Botão físico (>5s) → limpa config e reinicia no portal
  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *r){
    clearConfig(); r->send(200,"text/plain","Resetado"); delay(500); ESP.restart();
  });

  server.begin();
}

// ══════════════════════════════════════════════════
// Tasks FreeRTOS
// ══════════════════════════════════════════════════
void audioTask(void *arg) {
  const int CHUNK = 512; uint8_t buf[CHUNK]; size_t written;
  while (true) {
    if (bufAvail() >= CHUNK) {
      for (int i=0;i<CHUNK;i++){ buf[i]=ringBuf[rPos]; rPos=(rPos+1)%RING_SIZE; }
      i2s_channel_write(i2s_tx, buf, CHUNK, &written, portMAX_DELAY);
    } else vTaskDelay(pdMS_TO_TICKS(2));
  }
}

void ledTask(void *arg) {
  uint32_t lastBlink=0; int cnt=0; bool blinking=false;
  while (true) {
    if (mode==EMERGENCY) { blinkSOS(); }
    else {
      uint32_t now=millis();
      if (!blinking && now-lastBlink>=5000){ blinking=true; cnt=0; }
      if (blinking){ if(cnt<2){ blink(80,120); cnt++; } else { blinking=false; lastBlink=millis(); } }
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
}

void uiTask(void *arg) {
  uint32_t lastDraw=0, btnHeld=0; bool blink_st=false; bool btnWasDown=false;
  while (true) {
    uint32_t now=millis();
    if (receiving && now-lastAudio>400) receiving=false;

    // Botão: curto → modo; longo >5s → reset config
    bool btnDown = (digitalRead(BTN_PIN)==LOW);
    if (btnDown && !btnWasDown) btnHeld=now;
    if (btnDown && now-btnHeld>5000) { clearConfig(); oledText="Resetando..."; oled.clearDisplay(); oled.setCursor(0,0); oled.println("Reset config!"); oled.println("Reiniciando..."); oled.display(); delay(1000); ESP.restart(); }
    if (!btnDown && btnWasDown && now-btnHeld<5000) { mode=(mode==NORMAL)?EMERGENCY:NORMAL; oledText=(mode==EMERGENCY)?"!! EMERGENCIA !!":"Aguardando..."; }
    btnWasDown=btnDown;

    if (now-lastDraw>=200) {
      lastDraw=now; blink_st=!blink_st; oled.clearDisplay();
      if (mode==EMERGENCY) {
        if (blink_st){ oled.fillRect(0,0,128,64,SSD1306_WHITE); oled.setTextColor(SSD1306_BLACK); }
        else oled.setTextColor(SSD1306_WHITE);
        oled.setTextSize(2); oled.setCursor(4,8);  oled.println("EMERGENCIA");
        oled.setTextSize(1); oled.setCursor(28,42); oled.println("!! ATENCAO !!");
      } else {
        oled.setTextColor(SSD1306_WHITE); oled.setTextSize(1);
        oled.setCursor(0,0); oled.print("= "); oled.print(cfg_name); oled.println(" =");
        oled.drawLine(0,9,127,9,SSD1306_WHITE);
        oled.setCursor(0,13);
        String t=oledText;
        for (int l=0;l<3&&t.length()>0;l++){ oled.println(t.substring(0,21)); t=t.length()>21?t.substring(21):""; }
        oled.setCursor(0,48);
        oled.print(receiving?">>AUDIO<< ":"          ");
        oled.setCursor(0,56);
        oled.print(mqttConnected?"MQTT:OK ":"MQTT:-- ");
        oled.print(WiFi.localIP().toString());
      }
      oled.display();
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ══════════════════════════════════════════════════
// Setup
// ══════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT); ledOff();
  pinMode(BTN_PIN, INPUT_PULLUP);

  Wire.begin(OLED_SDA, OLED_SCL);
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  oled.setTextColor(SSD1306_WHITE); oled.setTextSize(1);

  loadConfig();

  // Botão segurado no boot → força portal
  bool forcePortal = (digitalRead(BTN_PIN) == LOW);

  if (!configured || forcePortal) {
    startCaptivePortal(); // não retorna
  }

  // Conecta WiFi
  oled.clearDisplay(); oled.setCursor(0,0);
  oled.println("Conectando WiFi..."); oled.println(cfg_ssid); oled.display();
  WiFi.begin(cfg_ssid.c_str(), cfg_pass.c_str());

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis()-t0 < 15000) {
    delay(300); ledOn(); delay(100); ledOff();
  }
  if (WiFi.status() != WL_CONNECTED) {
    oled.clearDisplay(); oled.setCursor(0,0);
    oled.println("WiFi falhou!"); oled.println("Entrando em setup..."); oled.display();
    delay(2000);
    startCaptivePortal();
  }

  oled.clearDisplay(); oled.setCursor(0,0);
  oled.println("WiFi OK!"); oled.println(WiFi.localIP().toString()); oled.display();

  setupI2S();

  // MQTT
  if (cfg_mqtt_host.length() > 0) {
    mqtt.setServer(cfg_mqtt_host.c_str(), cfg_mqtt_port);
    mqtt.setCallback(mqttCallback);
    mqtt.setBufferSize(512);
    mqttConnect();
  }

  setupWebServer();

  // Splash
  delay(1000);
  oledText = "WiFi OK - " + WiFi.localIP().toString();

  xTaskCreatePinnedToCore(audioTask, "audio", 4096, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(ledTask,   "led",   2048, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(uiTask,    "ui",    4096, NULL, 1, NULL, 1);
}

void loop() {
  ws.cleanupClients();
  if (cfg_mqtt_host.length() > 0) {
    if (!mqtt.connected()) { mqttConnected=false; mqttConnect(); }
    else                   { mqttConnected=true;  mqtt.loop();   }
  }
  delay(10);
}
