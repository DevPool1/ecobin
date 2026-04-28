# 🚀 ECOBIN — Guia Passo a Passo

Este guia leva-te do zero até ao sistema a funcionar: ESP32-CAM a comunicar com o Raspberry Pi via MQTT, com classificação de resíduos via Gemini.

---

## 📋 O que vais precisar

- [ ] Raspberry Pi 4 (2GB) com Raspberry Pi OS instalado
- [ ] ESP32-CAM (AI-Thinker) com câmara OV2640
- [ ] Adaptador FTDI USB-to-Serial (para programar o ESP32-CAM)
- [ ] Cabo micro-USB para o Raspberry Pi
- [ ] Telemóvel com dados móveis (para hotspot WiFi)
- [ ] Computador com Arduino IDE instalado
- [ ] Conta Google com API key do Gemini

---

## FASE 1: Obter a API Key do Gemini

### Passo 1.1 — Criar API Key

1. Abre o browser e vai a: **https://aistudio.google.com/apikey**
2. Faz login com a tua conta Google
3. Clica em **"Create API Key"**
4. Copia a key (parece algo como: `AIzaSyB...xyz`)
5. **Guarda num local seguro** — vais precisar dela mais tarde

> ⚠️ **NUNCA** partilhes esta key publicamente nem a metas no Git!

---

## FASE 2: Preparar o Raspberry Pi

### Passo 2.1 — Ligar e Aceder ao Raspberry Pi

**Se tiveres monitor + teclado:**
1. Liga o RPi ao monitor via HDMI e liga o teclado USB
2. Liga o cabo de alimentação do RPi
3. Abre o Terminal

**Se NÃO tiveres monitor (headless via SSH):**
1. Liga o RPi à mesma rede que o teu PC (por cabo ethernet ou WiFi)
2. Descobre o IP do RPi (vai ao router ou usa `ping raspberrypi.local`)
3. No teu PC abre o terminal: `ssh pi@<IP_DO_RPI>`
4. Password padrão: `raspberry` (muda depois com `passwd`)

### Passo 2.2 — Instalar Mosquitto (Broker MQTT)

No terminal do Raspberry Pi, executa estes comandos **um de cada vez**:

```bash
sudo apt update
```
```bash
sudo apt install -y mosquitto mosquitto-clients
```
```bash
sudo systemctl enable mosquitto
```
```bash
sudo systemctl start mosquitto
```

**Verificar que está a correr:**
```bash
sudo systemctl status mosquitto
```
Deve mostrar `active (running)` a verde. ✅

### Passo 2.3 — Testar o Mosquitto

Abre **dois terminais** no Raspberry Pi (ou duas sessões SSH):

**Terminal 1** — ficar à escuta:
```bash
mosquitto_sub -t "teste" -v
```

**Terminal 2** — enviar mensagem:
```bash
mosquitto_pub -t "teste" -m "ola mundo"
```

No Terminal 1 deve aparecer:
```
teste ola mundo
```
Se apareceu, o MQTT está a funcionar! ✅ Fecha ambos os terminais (Ctrl+C).

### Passo 2.4 — Instalar Python e Dependências

```bash
sudo apt install -y python3-pip python3-venv git
```

### Passo 2.5 — Clonar o Projeto

```bash
cd ~
git clone https://github.com/DevPool1/ecobin.git
cd ecobin/gateway
```

### Passo 2.6 — Criar Ambiente Virtual Python

```bash
python3 -m venv venv
```
```bash
source venv/bin/activate
```

O terminal deve mostrar `(venv)` no início da linha.

```bash
pip install -r requirements.txt
```

Espera até acabar (pode demorar 2-3 minutos no RPi).

### Passo 2.7 — Criar o Ficheiro .env

```bash
cp .env.example .env
nano .env
```

O editor `nano` abre. Altera as seguintes linhas:

```
GEMINI_API_KEY=COLA_A_TUA_API_KEY_AQUI
ESP32_CAM_IP=VAIS_PREENCHER_DEPOIS
WIFI_SSID=O_NOME_DO_TEU_HOTSPOT
WIFI_PASSWORD=A_PASSWORD_DO_TEU_HOTSPOT
```

**Para guardar e sair do nano:**
1. `Ctrl + O` → Enter (guardar)
2. `Ctrl + X` (sair)

---

## FASE 3: Configurar o WiFi (Hotspot)

### Passo 3.1 — Criar Hotspot no Telemóvel

No telemóvel (Android):
1. **Definições** → **Rede e Internet** → **Ponto de acesso e partilha**
2. **Nome da rede:** `ECOBIN_Hotspot` (ou o que quiseres)
3. **Password:** `ecobin2026` (ou o que quiseres)
4. **Banda:** 2.4 GHz (**IMPORTANTE** — o ESP32 só suporta 2.4 GHz!)
5. **Ativar** o hotspot

No telemóvel (iPhone):
1. **Definições** → **Partilhar Internet**
2. Mudar o nome e password
3. **Ativar** "Permitir que outros se liguem"
4. Se possível, forçar 2.4 GHz

> ⚠️ **O ESP32 NÃO suporta WiFi 5 GHz!** Certifica-te que o hotspot está em 2.4 GHz.

### Passo 3.2 — Conectar o Raspberry Pi ao Hotspot

```bash
sudo nmcli dev wifi connect "ECOBIN_Hotspot" password "ecobin2026"
```

Se der erro, tenta:
```bash
sudo nmcli dev wifi list
```
Procura o nome do teu hotspot na lista e tenta novamente.

### Passo 3.3 — Descobrir o IP do Raspberry Pi

```bash
hostname -I
```

Anota o IP (ex: `192.168.43.100`). **Vais precisar deste IP nos próximos passos.**

---

## FASE 4: Programar o ESP32-CAM

### Passo 4.1 — Instalar Arduino IDE no teu PC

1. Vai a: **https://www.arduino.cc/en/software**
2. Descarrega e instala o Arduino IDE 2.x

### Passo 4.2 — Adicionar suporte para ESP32

1. Abre o **Arduino IDE**
2. Vai a **File** → **Preferences**
3. Em **"Additional Boards Manager URLs"** cola:
```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```
4. Clica **OK**
5. Vai a **Tools** → **Board** → **Boards Manager**
6. Pesquisa **"esp32"**
7. Instala **"esp32 by Espressif Systems"** (pode demorar)

### Passo 4.3 — Instalar Biblioteca PubSubClient

1. Vai a **Sketch** → **Include Library** → **Manage Libraries**
2. Pesquisa **"PubSubClient"**
3. Instala a de **Nick O'Leary**

### Passo 4.4 — Abrir e Editar o Firmware

1. No Arduino IDE: **File** → **Open**
2. Navega até: `ecobin/firmware/esp32_cam/esp32_cam.ino`
3. **Edita estas 4 linhas** no topo do ficheiro:

```cpp
const char* WIFI_SSID      = "ECOBIN_Hotspot";     // ← Nome do teu hotspot
const char* WIFI_PASSWORD   = "ecobin2026";          // ← Password do teu hotspot
const char* MQTT_BROKER     = "192.168.43.100";      // ← IP do Raspberry Pi (do Passo 3.3)
```

### Passo 4.5 — Ligar o ESP32-CAM ao PC

O ESP32-CAM **não tem USB** — precisas de um **adaptador FTDI**:

```
FTDI          ESP32-CAM
────          ─────────
5V    ───→    5V
GND   ───→    GND
TX    ───→    U0R (GPIO 3)
RX    ───→    U0T (GPIO 1)
```

**IMPORTANTE:** Para entrar em modo de programação:
- Liga **GPIO 0** ao **GND** com um jumper wire
- Carrega no botão **RESET** do ESP32-CAM

### Passo 4.6 — Configurar o Arduino IDE

1. **Tools** → **Board** → **ESP32 Arduino** → **AI Thinker ESP32-CAM**
2. **Tools** → **Port** → Seleciona a porta COM do FTDI (ex: COM3)
3. **Tools** → **Upload Speed** → **115200**

### Passo 4.7 — Upload do Firmware

1. Clica no botão **Upload** (seta →) no Arduino IDE
2. Espera pela compilação e upload
3. Quando disser **"Connecting..."**, carrega no **RESET** do ESP32-CAM
4. Espera até ver **"Hard resetting via RTS pin..."**
5. **Remove o jumper do GPIO 0 ao GND**
6. Carrega no **RESET** novamente

### Passo 4.8 — Verificar no Serial Monitor

1. **Tools** → **Serial Monitor**
2. Baud rate: **115200**
3. Devia ver algo como:

```
============================================
  ♻️  ECOBIN — Nó de Visão (ESP32-CAM)
============================================
[CAM] PSRAM encontrada — VGA 640x480
[CAM] ✅ Câmara inicializada com sucesso!
[WiFi] A conectar a 'ECOBIN_Hotspot'...
[WiFi] ✅ Conectado! IP: 192.168.43.105
[HTTP] ✅ Servidor ativo em http://192.168.43.105/capture
[MQTT] ✅ Conectado ao broker!
[ECOBIN] 🟢 Sistema pronto!
```

**Anota o IP do ESP32-CAM** (ex: `192.168.43.105`).

### Passo 4.9 — Testar a Câmara no Browser

Abre o browser (no PC ou telemóvel ligado ao mesmo hotspot) e vai a:
```
http://192.168.43.105
```
Deves ver a página com uma imagem da câmara. ✅

---

## FASE 5: Configurar o IP do ESP32-CAM no Gateway

### Passo 5.1 — Atualizar o .env no Raspberry Pi

Agora que sabes o IP do ESP32-CAM, atualiza o `.env`:

```bash
cd ~/ecobin/gateway
nano .env
```

Altera a linha:
```
ESP32_CAM_IP=192.168.43.105
```

(Usa o IP que anotaste no Passo 4.8)

Guardar: `Ctrl+O` → Enter → `Ctrl+X`

---

## FASE 6: Arrancar o Sistema! 🚀

### Passo 6.1 — Iniciar o Gateway

No terminal do Raspberry Pi:

```bash
cd ~/ecobin/gateway
source venv/bin/activate
python main.py
```

Deves ver:
```
♻️  ECOBIN Gateway — Configuração
========================================
  MQTT Broker:  localhost:1883
  ESP32-CAM:    http://192.168.43.105:80/capture
  Gemini Model: gemini-2.0-flash
  API Key:      ✅ Definida
  Database:     ./database/ecobin.db
========================================
✅ Conectado ao broker MQTT!
🟢 Gateway ativo — à espera de resíduos...
```

### Passo 6.2 — Testar!

Coloca a mão ou um objeto em frente à câmara do ESP32-CAM. O Smart Gate deve detetar a mudança e:

1. ESP32-CAM publica `ecobin/cam/ready` → `waste_detected`
2. Gateway captura imagem via HTTP
3. Gemini classifica o resíduo
4. Resultado aparece no terminal do Gateway:

```
==================================================
🔄 NOVA CLASSIFICAÇÃO
==================================================
📷 A capturar imagem do ESP32-CAM (tentativa 1/3)...
✅ Imagem capturada: 45230 bytes (44.2 KB)
🔍 A classificar imagem (640x480)...
✅ Classificação: plastico (87%) — Garrafa de plástico transparente
⚙️  Motor → rotate:0
💾 Registado: ID=1
==================================================
```

### Passo 6.3 — Testar Manualmente (se o Smart Gate não disparar)

Num segundo terminal no RPi:
```bash
mosquitto_pub -t "ecobin/cam/ready" -m "waste_detected"
```

Isto simula o ESP32-CAM a detetar um resíduo e dispara todo o pipeline.

---

## 🔧 Resolução de Problemas

### "WiFi não conecta"
- Verifica que o hotspot está em **2.4 GHz** (não 5 GHz)
- Verifica o nome e password do WiFi no código
- Reinicia o ESP32-CAM (botão RESET)

### "MQTT não conecta"
- Verifica que o Mosquitto está a correr: `sudo systemctl status mosquitto`
- Verifica que o IP do RPi está correto no firmware
- Testa com: `mosquitto_pub -t "teste" -m "ola"` + `mosquitto_sub -t "teste" -v`

### "Câmara não captura"
- Verifica que a câmara está bem encaixada no conector
- Acede a `http://<IP_ESP32>/capture` no browser
- Se der erro 500: reinicia o ESP32-CAM

### "Gemini API erro"
- Verifica que a API key está correta no `.env`
- Verifica que tens internet no RPi: `ping google.com`
- Verifica que o hotspot tem dados móveis ativos

### "Gateway não recebe mensagens MQTT"
- Verifica os tópicos com: `mosquitto_sub -t "ecobin/#" -v`
- Verifica se o ESP32-CAM está conectado: deve publicar em `ecobin/cam/status`

---

## 📊 Verificar Resultados na Base de Dados

```bash
cd ~/ecobin/gateway
sqlite3 database/ecobin.db
```

No SQLite:
```sql
-- Ver últimas classificações
SELECT * FROM classifications ORDER BY id DESC LIMIT 10;

-- Contar por categoria
SELECT category, COUNT(*) FROM classifications GROUP BY category;

-- Sair
.quit
```

---

## ✅ Checklist Final

- [ ] Hotspot ativo no telemóvel (2.4 GHz)
- [ ] Raspberry Pi conectado ao hotspot
- [ ] Mosquitto a correr no RPi
- [ ] `.env` preenchido com API key e IP do ESP32-CAM
- [ ] ESP32-CAM programado com WiFi e IP do broker corretos
- [ ] ESP32-CAM conectado ao WiFi e MQTT
- [ ] Gateway Python a correr (`python main.py`)
- [ ] Resíduo na bandeja → classificação automática! 🎉
