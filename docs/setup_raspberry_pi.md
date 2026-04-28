# 🍓 Setup do Raspberry Pi — ECOBIN Gateway

## 1. Instalar Mosquitto (MQTT Broker)

```bash
# Instalar Mosquitto e o cliente de teste
sudo apt update
sudo apt install -y mosquitto mosquitto-clients

# Ativar para iniciar automaticamente no boot
sudo systemctl enable mosquitto
sudo systemctl start mosquitto

# Verificar que está a correr
sudo systemctl status mosquitto
```

### Testar Mosquitto

Abrir dois terminais:

```bash
# Terminal 1 — Subscrever
mosquitto_sub -t "ecobin/#" -v

# Terminal 2 — Publicar
mosquitto_pub -t "ecobin/test" -m "hello from RPi"
```

Se no Terminal 1 aparecer `ecobin/test hello from RPi`, o broker está a funcionar! ✅

## 2. Instalar Dependências Python

```bash
# Navegar para a pasta do gateway
cd ~/ecobin/gateway

# Criar ambiente virtual
python3 -m venv venv
source venv/bin/activate

# Instalar dependências
pip install -r requirements.txt
```

## 3. Configurar .env

```bash
# Copiar template
cp .env.example .env

# Editar com os teus valores
nano .env
```

**Campos obrigatórios:**
- `GEMINI_API_KEY` — a tua chave da API Gemini
- `ESP32_CAM_IP` — IP que o ESP32-CAM recebe do hotspot

## 4. Configurar WiFi (Hotspot do Telemóvel)

Para a apresentação, usa o **hotspot do telemóvel**:

1. No telemóvel: **Definições → Ponto de acesso → Ativar**
2. Nome da rede: `ECOBIN_Hotspot`
3. Password: `ecobin2026`
4. No Raspberry Pi:

```bash
# Conectar ao hotspot
sudo nmcli dev wifi connect "ECOBIN_Hotspot" password "ecobin2026"

# Verificar IP
ip addr show wlan0
```

5. Anotar o IP do Raspberry Pi (ex: `192.168.43.100`)
6. Colocar esse IP no firmware do ESP32-CAM como `MQTT_BROKER`

## 5. Descobrir IPs na Rede

Depois de todos os dispositivos estarem ligados ao hotspot:

```bash
# Ver o IP do Raspberry Pi
hostname -I

# Procurar outros dispositivos na rede
sudo apt install -y nmap
nmap -sn 192.168.43.0/24
```

## 6. Arrancar o Gateway

```bash
cd ~/ecobin/gateway
source venv/bin/activate
python main.py
```

## 7. Testar Manualmente

```bash
# Simular ESP32-CAM a notificar resíduo detetado
mosquitto_pub -t "ecobin/cam/ready" -m "waste_detected"

# Ver todas as mensagens do sistema
mosquitto_sub -t "ecobin/#" -v
```

## 8. (Opcional) Arrancar no Boot

Para que o gateway inicie automaticamente:

```bash
# Criar serviço systemd
sudo nano /etc/systemd/system/ecobin.service
```

Conteúdo:
```ini
[Unit]
Description=ECOBIN Gateway
After=network.target mosquitto.service

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi/ecobin/gateway
ExecStart=/home/pi/ecobin/gateway/venv/bin/python main.py
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable ecobin
sudo systemctl start ecobin

# Ver logs em tempo real
sudo journalctl -u ecobin -f
```
