# ECOBIN — Lixo Inteligente com IA

## Visão Geral do Projeto

O **ECOBIN** é um contentor de lixo inteligente com classificação automática de resíduos usando IA (Gemini 2.0 Flash). É o projeto final para a cadeira **Laboratório IoT** na **Universidade do Algarve (UAlg)**, curso LESTI.

O sistema captura uma imagem do resíduo depositado, classifica-o automaticamente via API Gemini, e aciona um carrossel mecânico rotativo para depositar o lixo no compartimento correto (4 categorias). Tudo é coordenado por MQTT, com dashboard web em tempo real.

**Deadline:** 31 de maio de 2026

---

## Arquitetura Distribuída (4 Nós)

O sistema usa **4 nós de hardware** interligados via Wi-Fi e protocolo MQTT:

### 1. Gateway Inteligente — Raspberry Pi 4 (2GB)
- **Papel:** Cérebro central do sistema
- Corre o broker **Mosquitto** (MQTT)
- Pipeline Python de classificação via **API Gemini 2.0 Flash**
- Base de dados **SQLite** com histórico de deposições
- Motor de decisão semântico
- Servidor web com **API REST** e **WebSocket** (dashboard)
- OS: Raspberry Pi OS

### 2. Nó de Visão — ESP32-CAM (OV2640)
- **Papel:** Captura de imagem do resíduo
- Alojado na caixa de controlo superior, aponta para baixo sobre a bandeja
- Serve um endpoint HTTP que o Raspberry Pi chama
- Captura e devolve frames JPEG
- Iluminação branca NeoPixel ativada durante captura

### 3. Nó de Contentor — ESP32-WROOM-32UE
- **Papel:** Controlo mecânico de triagem
- Motor de passo **28BYJ-48** + driver **ULN2003** para rotação do carrossel (90° por classificação)
- **Servo SG90/MG90S** para abrir/fechar a bandeja (trapdoor)
- **Reed Switch** para confirmar estado da bandeja
- **HC-SR04** para medir nível de enchimento
- **Sensor PIR HC-SR501** para deteção de proximidade
- **Fita NeoPixel WS2812B** (8 LEDs) para feedback cromático
- **Buzzer piezoelétrico** para feedback sonoro
- **LDR** para deteção de luz ambiente (modo noturno)

### 4. Nó de Interface — Arduino UNO R4 WiFi
- **Papel:** Interação com o utilizador
- **Display OLED SSD1306** 128x64 (I2C) — mostra estado, classificação, nível
- Cliente MQTT para receber estados do sistema

---

## Design Físico

Design **cilíndrico** com impressão 3D em PLA. Três módulos:

1. **Corpo exterior (fixo):** Cilindro branco com 4 janelas verticais que revelam a cor do compartimento alinhado
2. **Carrossel interior (rotativo):** Cilindro interno dividido em 4 compartimentos por paredes em cruz. Roda em incrementos de 90°
3. **Caixa de controlo (fixa, topo):** Abertura no topo, aloja ESP32-CAM, bandeja de espera com servo, display OLED, anel NeoPixel, sensor PIR

---

## Sequência de Operação

1. Sensor PIR deteta utilizador → sistema acorda do standby
2. OLED mostra "Pronto", NeoPixel ativa branco
3. Utilizador larga resíduo → cai na bandeja de espera (servo fechado)
4. ESP32-CAM captura frame JPEG
5. Raspberry Pi envia imagem à API Gemini 2.0 Flash → classificação JSON
6. Motor de passo roda carrossel até compartimento correto (0°/90°/180°/270°)
7. Servo abre a bandeja → resíduo cai no compartimento
8. Reed Switch confirma fecho → estado regressa a IDLE
9. HC-SR04 mede nível de enchimento → SQLite regista evento

---

## Stack Tecnológica

| Camada | Tecnologia |
|--------|-----------|
| IA / Classificação | Google Gemini 2.0 Flash API |
| Comunicação | MQTT (Mosquitto broker) |
| Base de Dados | SQLite |
| Backend / API | Python (Flask ou FastAPI), REST + WebSocket |
| Dashboard Web | HTML/CSS/JS (tempo real via WebSocket) |
| Firmware ESP32 | Arduino IDE / PlatformIO (C/C++) |
| Firmware Arduino | Arduino IDE (C/C++) |
| Impressão 3D | PLA, design próprio |
| OS Gateway | Raspberry Pi OS |

---

## Componentes Disponíveis

### ✅ Já Disponíveis (custo €0)
- Raspberry Pi 4 (2GB) — Starter Kit HutoPi
- ESP32-WROOM-32UE (Espressif dev board com antena externa)
- ESP32-CAM (OV2640)
- Arduino UNO R4 WiFi
- Módulo Relé 3V (BESTEP 1 Relay Module)
- Sensor Ultrassónico HC-SR04
- Sensor PIR HC-SR501
- Impressora 3D + Filamento PLA

### 🛒 A Comprar/Verificar
- Servo Motor SG90/MG90S (~€3-5)
- Motor de Passo 28BYJ-48 + ULN2003 (~€4-5)
- Display OLED SSD1306 128x64 (~€5-7)
- Fita NeoPixel WS2812B 8 LEDs (~€3-4)
- Buzzer Piezoelétrico Ativo 5V (~€1-2)
- Reed Switch (~€1-2)
- LDR (~€0.50)
- Grove MOSFET Controller (~€5-8)

---

## Estrutura do Projeto (a definir)

```
iot_projeto/
├── CLAUDE.md                    # Este ficheiro
├── Proposta_ECOBIN_final-3.pdf  # Proposta aprovada
├── Lista_de_Material_-_ECOBIN.xlsx
├── firmware/
│   ├── esp32_cam/               # Código ESP32-CAM (Nó de Visão)
│   ├── esp32_contentor/         # Código ESP32-WROOM (Nó de Contentor)
│   └── arduino_interface/       # Código Arduino R4 WiFi (Nó de Interface)
├── gateway/
│   ├── classifier/              # Pipeline de classificação Gemini
│   ├── mqtt/                    # Config e lógica MQTT
│   ├── database/                # Schema SQLite e migrations
│   └── web/                     # Dashboard web (API REST + WebSocket)
├── 3d_models/                   # Ficheiros STL/STEP para impressão 3D
├── docs/                        # Documentação adicional
└── tests/                       # Testes
```

---

## Modo Offline (Resiliência)

O sistema mantém funcionalidade básica sem Internet:
- Monitorização do nível de enchimento
- Deteção de proximidade
- Atuação básica
- Fila de espera de classificações para sincronizar quando a conectividade é restabelecida

---

## Convenções de Desenvolvimento

- **Linguagem dos comentários/commits:** Português
- **Comunicação entre nós:** MQTT (tópicos a definir)
- **Formato de classificação IA:** JSON estruturado
- **Firmware:** Arduino IDE ou PlatformIO (C/C++)
- **Gateway:** Python 3
- **Dashboard:** HTML/CSS/JS vanilla (ou framework leve se necessário)

---

## Notas para o Assistente IA

- O projeto chama-se **ECOBIN** (anteriormente chamado ORÁCULO durante a fase de proposta)
- É um projeto académico para a cadeira **Laboratório IoT** na UAlg
- O aluno tem acesso a componentes do laboratório da universidade
- A impressora 3D é pessoal (em casa)
- O orçamento é mínimo (~€25-35 no total para peças em falta)
- O prazo final é **31 de maio de 2026**
- Priorizar soluções simples e robustas em vez de over-engineering
- Todo o código deve ser bem comentado em português
- O Raspberry Pi é o broker central — toda a inteligência passa por ele

---

## 📌 Diário de Bordo / Estado Atual (17 Abril 2026)

### ✅ O que já está feito e testado a 100%:
1. **Infraestrutura CI/CD:** Repositório GitHub montado com GitHub Actions a validar sintaxe Python (com `black`) e a compilar o Arduino/ESP32. Script de auto-commit (`scripts/`) a funcionar.
2. **Nó de Visão (ESP32-CAM-MB):**
   - **Sucesso de Hardware:** Firmware testado e carregado na ESP32-CAM usando a *motherboard* MB (sem necessidade de jumpers, usando os botões integrados).
   - O servidor HTTP da câmara está robusto, a expor as imagens no endpoint `/capture` e ligado de forma estável através do Wi-Fi de casa.
3. **Gateway Python e Broker (O "Cérebro" no Raspberry Pi / PC):**
   - **Instalação e Teste do Broker:** O broker MQTT embebido (`amqtt`) foi instalado com sucesso no ambiente e está a correr perfeitamente.
   - **Comunicação validada:** A ESP32-CAM-MB conseguiu ligar-se e enviar/receber dados perfeitamente para o Broker.
   - Código central (`gateway/main.py`) perfeitamente funcional.
   - Integração confirmada com o **Gemini 2.5 Flash** (via API) a devolver o JSON estruturado (`categoria`, `descrição curta`, `eco-pontos`).
   - Guardar histórico de classificações numa base de dados `SQLite`.
   - **Teste de pipeline completo validado:** O trigger foi simulado com o script `trigger_test.py`, o Gateway foi à ESP32-CAM buscar a imagem, a IA identificou uma *Garrafa de Água* (Plástico, 50 pts) e emitiu o comando MQTT para o motor (`rotate:0`).

### 🚀 O Próximo Passo Imediato (Onde retomar):
**Fase 3: O Nó de Contentor (ESP32-WROOM)**
Como o hardware do ecrã OLED ainda não está disponível, a próxima sessão de trabalho deve focar-se em programar a **ESP32-WROOM** (`firmware/esp32_contentor/esp32_contentor.ino`). 

Este código deve:
1. Ligar-se ao Wi-Fi e ao Broker MQTT do Gateway.
2. Controlar o **Motor de Passo (28BYJ-48)** com a biblioteca `<Stepper.h>` subscrevendo o tópico `ecobin/motor/command` (ex: rodar para 90º).
3. Controlar o **Servo Motor (SG90)** atuando como a *trapdoor* da bandeja de triagem subscrevendo `ecobin/servo/command`.
4. (*Opcional na 1ª iteração*) Ler o **Sensor Magnético (Reed Switch)** para garantir que o servo fechou.
