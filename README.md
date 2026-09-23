# Sistema de irrigação com ESP32 + Flask

Projeto Integrador Univesp

Dividido em duas partes

1. Firmware para ESP32 que lê um sensor de umidade do solo, controla uma bomba de água e envia status ao backend.
2. Aplicativo em Flask para exibir a umidade em tempo real, a última irrigação e avisos de reservatório vazio.

## Estrutura

- `esp32_firmware/` - código do ESP32
- `flask_app/` - backend e painel web
- `requirements.txt` - dependências do Flask

## Hardware

- ESP32 DevKit
- Sensor de umidade do solo analogico
- Relé 5V para acionamento da bomba submersa de 5V
- Bomba submersa de 5V em balde/reservatório
- Fonte adequada para a bomba

## Ligação típica

- Sensor de umidade no pino analógico do ESP32 (ex.: GPIO 34 ou 36)
- Relé no GPIO 25
- VCC do relé em 5V do ESP32 ou uma fonte externa compatível
- GND do relé e bomba conectados ao GND comum

> Ajuste as portas conforme seu esquema real.

## Backend Flask

### Instalação

```bash
cd flask_app
python -m venv .venv
source .venv/bin/activate  # Windows: .venv\Scripts\activate
pip install -r ../requirements.txt
python app.py
```

A aplicação fica disponível em:

- http://localhost:5000
Para comunicação do ESP32 com a Interface, utilize o IP da máquina, não o localhost

### Funcionalidades

- Página web com percentagem de umidade
- Última irrigação
- Estado do reservatório
- Botão para indicar que o reservatório foi reabastecido
- Aviso automático quando o sistema detecta reservatório vazio

## Firmware ESP32

### Requisitos

- VS Code + PlatformIO
- Arduino Framework para ESP32

### Configuração

1. Abra a pasta `esp32_firmware/` no PlatformIO.
2. Ajuste o Wi‑Fi e o IP do servidor Flask em `src/main.cpp`.
3. Ajuste os pinos e os limites do sensor.
4. Compile e faça upload.

### Exemplo de ajuste

```cpp
const char* WIFI_SSID = "SEU_WIFI";
const char* WIFI_PASSWORD = "SUA_SENHA";
String serverBase = "http://192.168.1.1:5000";
const int soilPin = 34;
const int relayPin = 25;
```

## Lógica da irrigação

- O ESP32 lê a umidade a cada ciclo.
- Se a umidade estiver abaixo do valor desejado, liga a bomba.
- A bomba permanece ligada por um tempo máximo.
- Se a umidade não subir o suficiente, o firmware considera o reservatório vazio.
- Em seguida envia o aviso ao Flask.
- O usuário, pelo site, confirma que o reservatório foi cheio e o ESP32 retorna a irrigar.

## Observações importantes

- O sensor analógico geralmente precisa de calibração em solo seco e úmido.
- Para bombas de 5V, use um relé ou módulo de transistor com isolamento adequado.
- Não ligue a bomba diretamente no pino do ESP32.
- Use uma fonte estável para a bomba e mantenha a massa comum.

## Exemplo prático de funcionamento

- Umidade atual: 30%
- Meta: 60%
- Sistema liga a bomba
- A bomba funciona por até 18 segundos
- Se a umidade ainda estiver baixa, marca `reservatorio_vazio = true`
- Webapp mostra alerta
- Usuário clica em "Reservatório cheio"
- ESP32 recebe o estado e reativa a irrigação
