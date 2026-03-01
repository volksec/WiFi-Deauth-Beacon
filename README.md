<div align="center">

# 📡 WiFi Deauth Beacon

### Dispositivo Autônomo de Auditoria Wireless para ESP8266

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform: ESP8266](https://img.shields.io/badge/Platform-ESP8266-blue.svg)](https://www.espressif.com/en/products/socs/esp8266)
[![Arduino IDE](https://img.shields.io/badge/IDE-Arduino%202.x-teal.svg)](https://www.arduino.cc/en/software)
[![Status](https://img.shields.io/badge/Status-Estável-brightgreen.svg)]()

<br/>

> **Ferramenta de hardware hacking para auditoria de redes IEEE 802.11**  
> Desenvolvida por VolkSec com finalidade acadêmica e aplicação prática em segurança ofensiva.

<br/>

</div>

---

## ⚠️ Aviso Legal

> **IMPORTANTE:** Esta ferramenta destina-se **exclusivamente** a testes de penetração **autorizados** e fins educacionais.  
> O uso em redes sem autorização expressa do proprietário é **ilegal** e pode configurar crime conforme:
> - **Brasil:** Lei 12.737/2012 (Lei Carolina Dieckmann) e Marco Civil da Internet
> - **EUA:** Computer Fraud and Abuse Act (CFAA)
> - **UE:** Diretiva 2013/40/UE
>
> O autor **não se responsabilizam** pelo uso indevido desta ferramenta.  
> **Use apenas em ambientes autorizados.**

---

## 📋 Índice

- [Sobre o Projeto](#-sobre-o-projeto)
- [Funcionalidades](#-funcionalidades)
- [Hardware Necessário](#-hardware-necessário)
- [Diagrama de Conexão](#-diagrama-de-conexão)
- [Instalação e Configuração](#-instalação-e-configuração)
- [Como Usar](#-como-usar)
- [Comandos AT (Serial)](#-comandos-at-serial)
- [Casos de Uso em Pentest](#-casos-de-uso-em-pentest)
- [Arquitetura do Código](#-arquitetura-do-código)
- [Solução de Problemas](#-solução-de-problemas)
- [Contribuindo](#-contribuindo)
- [Licença](#-licença)

---

## 🎯 Sobre o Projeto

O **WiFi Deauth Beacon** é um dispositivo de hardware hacking de **baixo custo (~R$90)** construído sobre o microcontrolador **ESP8266 (Wemos D1 Mini)**, capaz de enviar quadros de desautenticação IEEE 802.11 de forma **autônoma**, sem necessidade de computador durante a execução.

### Por que este projeto?

Ferramentas tradicionais como `aircrack-ng` exigem:
- Adaptador Wi-Fi com suporte a modo monitor + injeção
- Computador rodando Linux durante toda a operação
- Configuração complexa de drivers

O **WiFi Deauth Beacon** resolve isso com:
- **Hardware portátil** alimentado por bateria LiPo
- **Interface OLED** com menu navegável via botões
- **Operação standalone** — sem computador no campo
- **Controle remoto** via comandos AT pela serial USB
- **Log persistente** na EEPROM interna

### Casos de uso legítimos

| Cenário | Descrição |
|---------|-----------|
| 🔴 **Red Team Físico** | Drop box autônomo para auditoria wireless em campo |
| 📶 **Pentest Wi-Fi** | Captura de handshakes WPA2 via deauth + airodump-ng |
| 🛡️ **Validação de WIDS** | Verificar se o IDS detecta e alerta sobre frames de deauth |
| 🔐 **Teste de 802.11w** | Confirmar implementação de Management Frame Protection (MFP) |
| 🎓 **Treinamento** | Demonstrações em labs de hardware hacking e CTFs |

---

## ✨ Funcionalidades

```
╔═══════════════════════════════════════════════════════════╗
║                   FUNCIONALIDADES                         ║
╠══════════════════╦════════════════════════════════════════╣
║ 📡 Wi-Fi Scan    ║ Detecta até 20 redes, ordena por RSSI ║
║ 💥 Deauth Bcast  ║ Desconecta TODOS os clientes do AP    ║
║ 🎯 Deauth Ucast  ║ Ataque direcionado a cliente          ║
║ 📺 Display OLED  ║ Interface visual 128x64px em tempo    ║
║ 🕹️  Menu Físico  ║ Navegação por 3 botões (UP/DN/SEL)   ║
║ 🕶️  Stealth Mode ║ LED e display desabilitados           ║
║ 📊 RSSI Ranking  ║ Alvos ordenados por força de sinal    ║
║ 💾 Log EEPROM    ║ Registro persistente de sessões       ║
║ 🖥️  AT Commands  ║ Controle remoto via serial USB        ║
║ 📺 Multi-canal   ║ Suporte a canais 1-13                 ║
║ 🔄 Auto-scan     ║ Re-scan automático a cada 10s         ║
║ 🐕 Watchdog      ║ Auto-reset em caso de travamento      ║
╚══════════════════╩════════════════════════════════════════╝
```

---

## 🛠️ Hardware Necessário

### Lista de Componentes (BOM — Bill of Materials)

| # | Componente | Especificação | Qtd | Aprox. |
|---|-----------|--------------|-----|--------|
| 1 | Microcontrolador | **ESP8266 — Wemos D1 Mini v3** | 1 | R$ 18,00 |
| 2 | Display OLED | **SSD1306 — 0.96" I2C 128x64px** | 1 | R$ 22,00 |
| 3 | Botões Táteis | Push button 6x6mm (THT ou SMD) | 3 | R$ 1,50 |
| 4 | Resistores | 10kΩ 1/4W (pull-up) | 3 | R$ 0,30 |
| 5 | LED | LED vermelho 3mm + resistor 330Ω | 1 | R$ 0,50 |
| 6 | Capacitor | 100µF 16V (desacoplamento) | 1 | R$ 1,00 |
| 7 | Bateria | LiPo 3.7V 1200mAh + módulo TP4056 | 1 | R$ 28,00 |
| 8 | Enclosure | Caixa ABS 50x30x15mm (opcional) | 1 | R$ 8,00 |
| 9 | Protoboard | Mini protoboard 400 furos | 1 | R$ 6,00 |
| 10 | Jumpers | Dupont M-M / M-F | — | R$ 5,00 |
| | | **💰 TOTAL ESTIMADO** | | **R$ 90,30** |

### Alternativas de Hardware

- **NodeMCU v3** (compatível, pinos diferentes — adapte o código)
- **ESP32** (requer adaptações no SDK de injeção de pacotes)
- **Display 1.3" SSD1306** (altere `SCREEN_HEIGHT` e `OLED_ADDRESS` se necessário)

---

## 🔌 Diagrama de Conexão

```
┌─────────────────────────────────────────────────────┐
│              WEMOS D1 MINI — PINAGEM                │
│                                                     │
│  ┌──────────┐        ┌─────────────┐                │
│  │  SSD1306 │        │   BOTÕES    │                │
│  │   OLED   │        │             │                │
│  │          │        │  BTN_UP ──┐ │                │
│  │ VCC ─────┼── 3V3  │  BTN_DN ──┼─┼── GND          │
│  │ GND ─────┼── GND  │  BTN_SEL ─┘ │                │
│  │ SCL ─────┼── D1   │             │                │
│  │ SDA ─────┼── D2   │  D5 (UP)    │                │
│  └──────────┘        │  D6 (DOWN)  │                │
│                      │  D7 (SEL)   │                │
│  ┌──────────┐        └─────────────┘                │
│  │   LED    │                                       │
│  │  (RED)   │        ┌─────────────┐                │
│  │ (+) ─────┼──330Ω──┼── D4        │                │
│  │ (-) ─────┼── GND  │   TP4056    │                │
│  └──────────┘        │             │                │
│                      │ OUT+ ───────┼── 5V/VIN       │
│                      │ BAT+ ── LiPo│                │
│                      └─────────────┘                │
└─────────────────────────────────────────────────────┘
```

### Tabela de Pinos

| Pino ESP8266 | GPIO | Componente | Descrição |
|-------------|------|-----------|-----------|
| D1 | GPIO5 | OLED SCL | I2C Clock |
| D2 | GPIO4 | OLED SDA | I2C Data |
| D5 | GPIO14 | Botão UP | Navegar cima |
| D6 | GPIO12 | Botão DOWN | Navegar baixo |
| D7 | GPIO13 | Botão SELECT | Confirmar |
| D4 | GPIO2 | LED Status | Indicador (active LOW) |
| 3V3 | — | OLED VCC | Alimentação 3.3V |
| GND | — | OLED/Botões GND | Terra comum |
| 5V/VIN | — | TP4056 OUT+ | Alimentação bateria |

---

## 🚀 Instalação e Configuração

### 1. Arduino IDE

Instale o [Arduino IDE 2.x](https://www.arduino.cc/en/software).

### 2. Suporte ao ESP8266

Em `File > Preferences > Additional Board Manager URLs`, adicione:

```
http://arduino.esp8266.com/stable/package_esp8266com_index.json
```

Em `Tools > Board > Boards Manager`, pesquise **ESP8266** e instale **esp8266 by ESP8266 Community**.

Selecione a placa: `Tools > Board > ESP8266 Boards > LOLIN(WEMOS) D1 R2 & mini`

### 3. Bibliotecas

Instale via `Tools > Manage Libraries...`:

| Biblioteca | Versão mínima | Autor |
|-----------|--------------|-------|
| `Adafruit SSD1306` | >= 2.5.7 | Adafruit |
| `Adafruit GFX Library` | >= 1.11.5 | Adafruit |

### 4. Configurações de Upload

```
Board:       LOLIN(WEMOS) D1 R2 & mini
Upload Speed: 921600
CPU Frequency: 80 MHz
Flash Size:   4MB (FS:2MB OTA:~1019KB)
Port:         /dev/ttyUSB0 (Linux) ou COM3 (Windows)
```

### 5. Verificar Endereço I2C

Se o display não inicializar, rode o **I2C Scanner** para confirmar o endereço:

```cpp
// Sketch auxiliar para encontrar endereço I2C
#include <Wire.h>
void setup() {
  Serial.begin(115200);
  Wire.begin(4, 5); // SDA, SCL
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("Encontrado em 0x%02X\n", addr);
    }
  }
}
void loop() {}
```

Se o endereço for `0x3D`, altere no código:
```cpp
#define OLED_ADDRESS 0x3D
```

### 6. Clone e Upload

```bash
git clone https://github.com/volksec/WiFi-Deauth-Beacon.git
cd WiFi-Deauth-Beacon

# Abra o arquivo no Arduino IDE
arduino wifi_deauth_beacon/wifi_deauth_beacon.ino

# Compile e faça upload via IDE (Ctrl+U)
```

---

## 🕹️ Como Usar

### Operação via Interface Física (Standalone)

```
BOOT  →  Splash Screen (2.5s)  →  Scan automático
                                         │
                              ┌──────────┴──────────┐
                              │   MENU DE REDES      │
                              │  [UP]   = Scroll ↑  │
                              │  [DOWN] = Scroll ↓  │
                              │  [SEL]  = Atacar!   │
                              └─────────────────────┘
                                         │
                                   Atacando...
                                [SEL] = Parar + Log
```

**Passo a passo:**

1. **Ligue** o dispositivo (LED pisca 5x no boot)
2. O display mostrará as redes detectadas ordenadas por sinal
3. Use **UP/DOWN** para navegar pela lista
4. Pressione **SELECT** na rede desejada → ataque inicia
5. O display mostra o contador de frames em tempo real
6. Pressione **SELECT** novamente para parar → log salvo na EEPROM

### Display durante o ataque

```
┌────────────────────────┐
│ [ATTACKING]      CH:06 │  ← Header com canal
│────────────────────────│
│ LabWifi     -45dBm     │  ← Rede alvo (destacada)
│ CafeNet     -67dBm     │
│ GuestWifi   -72dBm     │
│ IoTDevice   -80dBm     │
│────────────────────────│
│ Frames: 1.247          │  ← Contador em tempo real
└────────────────────────┘
```

---

## 🖥️ Comandos AT (Serial)

Conecte via USB e abra a serial em **115200 baud**:

```bash
# Linux / macOS
screen /dev/ttyUSB0 115200
# ou
minicom -b 115200 -D /dev/ttyUSB0

# Windows: Use PuTTY ou Arduino Serial Monitor
```

### Referência de Comandos

| Comando | Descrição | Exemplo |
|---------|-----------|---------|
| `AT+HELP` | Lista todos os comandos | `AT+HELP` |
| `AT+SCAN` | Escaneia redes próximas | `AT+SCAN` |
| `AT+LIST` | Lista redes encontradas | `AT+LIST` |
| `AT+TARGET=<idx>` | Define alvo pelo índice | `AT+TARGET=0` |
| `AT+ATTACK=B` | Inicia ataque **broadcast** | `AT+ATTACK=B` |
| `AT+ATTACK=U` | Inicia ataque **unicast** | `AT+ATTACK=U` |
| `AT+STOP` | Para o ataque e salva log | `AT+STOP` |
| `AT+STATUS` | Exibe status atual | `AT+STATUS` |
| `AT+STEALTH=ON` | Ativa modo stealth | `AT+STEALTH=ON` |
| `AT+STEALTH=OFF` | Desativa modo stealth | `AT+STEALTH=OFF` |
| `AT+LOG` | Lê log da EEPROM | `AT+LOG` |
| `AT+LOGCLEAR` | Apaga log da EEPROM | `AT+LOGCLEAR` |
| `AT+CHANNEL=<1-13>` | Fixa canal específico | `AT+CHANNEL=6` |
| `AT+RESET` | Reinicia o dispositivo | `AT+RESET` |

### Sessão de exemplo completa

```
AT+SCAN
[SCN] Iniciando scan de redes...
[SCN] 5 redes encontradas.
[00] LabWifi              BSSID:AA:BB:CC:DD:EE:FF CH:06 RSSI: -45dBm ENC:WPA2
[01] TestNetwork          BSSID:11:22:33:44:55:66 CH:11 RSSI: -67dBm ENC:WPA2
[02] GuestNet             BSSID:DE:AD:BE:EF:CA:FE CH:01 RSSI: -72dBm ENC:OPEN

AT+TARGET=0
[TGT] Alvo definido: [00] LabWifi (AA:BB:CC:DD:EE:FF) CH:6 RSSI:-45dBm

AT+ATTACK=B
[ATK] Broadcast deauth iniciado -> LabWifi (AA:BB:CC:DD:EE:FF) CH:6

# ... aguarda captura de handshake com airodump-ng ...

AT+STOP
[STP] Ataque encerrado. Frames enviados: 1247
[LOG] Entrada salva na EEPROM (offset=0)

AT+LOG
[LOG] === LOG DA EEPROM ===
[00] BSSID: AA:BB:CC:DD:EE:FF  CH:06  Frames:1247
[LOG] === FIM DO LOG ===
```

---

## 🔴 Casos de Uso em Pentest

### 1. Captura de Handshake WPA2

```bash
# Terminal 1 — Inicia captura no canal do AP alvo
airodump-ng -c 6 --bssid AA:BB:CC:DD:EE:FF -w captura wlan0mon

# Terminal 2 (Serial do dispositivo) — Dispara deauth
AT+TARGET=0
AT+ATTACK=B
# Aguarda handshake capturado no airodump-ng
AT+STOP

# Crackear o handshake
aircrack-ng -w /usr/share/wordlists/rockyou.txt captura-01.cap
```

### 2. Validação de 802.11w (Management Frame Protection)

```bash
# Se o AP tiver MFP obrigatório (MFPR), os clientes ignorarão nossos frames
# O ataque vai enviar os frames, mas os clientes NÃO serão desconectados
# → Isso CONFIRMA que o 802.11w está funcionando corretamente

AT+TARGET=0
AT+ATTACK=B
# Observe: clientes não são desconectados = MFP ativo ✓
AT+STOP
```

### 3. Integração com Script Python (Automação)

```python
#!/usr/bin/env python3
"""
Script de controle remoto do WiFi Deauth Beacon via Serial
Requer: pip install pyserial
"""
import serial
import time

def deauth_session(port='/dev/ttyUSB0', target_ssid='TargetWifi', duration=30):
    with serial.Serial(port, 115200, timeout=2) as ser:
        time.sleep(2)  # Aguarda boot

        # Scan
        ser.write(b'AT+SCAN\n')
        time.sleep(5)

        # Lista redes e encontra o alvo
        ser.write(b'AT+LIST\n')
        response = ser.read(2048).decode('utf-8', errors='ignore')
        print(response)

        # Define alvo (ajuste o índice conforme a saída)
        ser.write(b'AT+TARGET=0\n')
        time.sleep(0.5)

        # Inicia ataque
        ser.write(b'AT+ATTACK=B\n')
        print(f"[*] Ataque iniciado por {duration}s...")
        time.sleep(duration)

        # Para e salva log
        ser.write(b'AT+STOP\n')
        time.sleep(0.5)
        ser.write(b'AT+LOG\n')
        log = ser.read(1024).decode('utf-8', errors='ignore')
        print(log)

if __name__ == '__main__':
    deauth_session(target_ssid='LabWifi', duration=30)
```

### 4. Integração com Raspberry Pi Zero W (Headless)

```bash
# No RPi — instala dependências
pip3 install pyserial

# Executa o script de controle
python3 deauth_control.py

# Para automação em red team, adicione ao crontab ou systemd service
```

---

## 🏗️ Arquitetura do Código

```
wifi_deauth_beacon/
├── wifi_deauth_beacon.ino    ← Arquivo principal (Arduino)
│
├── Módulos internos (no mesmo arquivo):
│   ├── [SETUP]     Inicialização de GPIO, I2C, Wi-Fi, EEPROM
│   ├── [DISPLAY]   showSplash(), updateDisplay(), showMessage()
│   ├── [WIFI]      scanNetworks(), sendDeauthFrames()
│   ├── [BUTTONS]   handleButtons() — UP/DOWN/SELECT com debounce
│   ├── [SERIAL]    handleSerial() — Comandos AT completos
│   └── [EEPROM]    logToEEPROM(), readLogFromEEPROM(), clearEEPROM()
│
├── docs/
│   └── demo.gif              ← GIF demonstrativo
│
└── README.md                 ← Esta documentação
```

### Fluxo de execução

```
setup()
  ├── Serial.begin(115200)
  ├── EEPROM.begin()
  ├── Configura GPIOs (botões INPUT_PULLUP, LED OUTPUT)
  ├── Inicializa display OLED (I2C)
  ├── showSplash()
  ├── WiFi.mode(WIFI_STA) + disconnect()
  └── scanNetworks() [scan inicial]

loop()  ← executa continuamente
  ├── handleSerial()    ← processa comandos AT
  ├── handleButtons()   ← lê botões físicos
  ├── if (!attacking && timeout) → scanNetworks()
  └── if (attacking) → sendDeauthFrames() [burst de 10 frames]
```

### Frame 802.11 de Desautenticação

```
Offset  Bytes  Campo              Valor
──────  ─────  ─────              ─────
 0-1      2    Frame Control      0xC0 0x00 (Deauth)
 2-3      2    Duration           0x00 0x00
 4-9      6    Destination MAC    FF:FF:FF:FF:FF:FF (broadcast)
10-15     6    Source MAC         <BSSID do AP> (spoofado)
16-21     6    BSSID              <BSSID do AP>
22-23     2    Sequence Number    incrementado por frame
24-25     2    Reason Code        0x07 0x00 (Class 3 frame)
```

---

## 🔧 Solução de Problemas

| Problema | Causa provável | Solução |
|---------|---------------|---------|
| Display não inicializa | Endereço I2C errado | Execute I2C Scanner; tente `0x3D` |
| Nenhuma rede encontrada | Wi-Fi não inicializado | Verifique modo `WIFI_STA`; reinicie |
| Upload falha | Driver CH340 ausente | Instale [driver CH340](https://www.wch.cn/downloads/CH341SER_ZIP.html) |
| Ataque não desconecta clientes | AP com 802.11w ativo | MFP está funcionando — documento no relatório |
| LED não pisca | Pino errado ou stealth ON | Verifique `LED_STATUS = 2`; `AT+STEALTH=OFF` |
| Botões não respondem | Pull-up não configurado | `INPUT_PULLUP` já definido — verifique fiação |
| Serial sem resposta | Baud rate errado | Configure para **115200** |
| Travamentos frequentes | Watchdog reset | Normal — o `yield()` no loop previne a maioria |

---

## 🔮 Roadmap / Futuras Expansões

- [ ] **Beacon Flood** — Cria centenas de SSIDs falsos para congestionar o espectro
- [ ] **Evil Twin** — AP falso com mesmo SSID do alvo para captura de credenciais
- [ ] **Probe Request Sniffer** — Captura SSIDs procurados por dispositivos próximos
- [ ] **OTA Update** — Atualização de firmware via Wi-Fi
- [ ] **Web Interface** — Interface web via AP para configuração sem serial
- [ ] **MQTT Integration** — Controle remoto via broker MQTT para red team distribuído
- [ ] **PCB Customizada** — Layout profissional para enclosure compacto

---

## 🤝 Contribuindo

Contribuições são bem-vindas! Por favor:

1. Faça um **Fork** do projeto
2. Crie uma branch para sua feature: `git checkout -b feature/nova-funcionalidade`
3. Commit suas mudanças: `git commit -m 'feat: adiciona nova funcionalidade'`
4. Push para a branch: `git push origin feature/nova-funcionalidade`
5. Abra um **Pull Request**

### Convenções de commit

```
feat:     Nova funcionalidade
fix:      Correção de bug
docs:     Documentação
refactor: Refatoração sem mudança de comportamento
test:     Adição ou correção de testes
```

---

## 📄 Licença

Distribuído sob a licença **MIT**. Veja `LICENSE` para mais informações.

```
MIT License — Copyright (c) 2026 VolkSec Offensive Security

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software...
```

---
<div align="center">

**Desenvolvido para a comunidade de segurança ofensiva**

</div>
