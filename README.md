# CyberShot Cam

Firmware para uma câmera DIY baseada em ESP32-S3, com sensor OV2640, display TFT, cartão microSD e interface Wi-Fi.

## O que este projeto faz

O projeto implementa uma câmera digital completa para o ESP32-S3. Ele captura fotos, mostra a interface no display, salva imagens no cartão SD e expõe uma interface web para acessar, editar e baixar fotos.

## Funcionalidades principais

- Captura de fotos com o sensor de câmera do ESP32-S3.
- Visualização no TFT com menu físico.
- Salvamento de imagens em microSD.
- Galeria web para navegar pelos arquivos do cartão SD.
- Editor de imagem via navegador, com opções como:
  - filtros visuais
  - rotação e espelhamento
  - crop
  - pixel sort
  - modo 8-bit
  - ASCII art
  - efeitos glitch no JPEG
- Wi-Fi em dois modos:
  - conexão como station usando credenciais salvas
  - modo Access Point para configuração inicial
- Página de configuração Wi-Fi via portal cativo.

## Hardware esperado

O código está preparado para uma placa ESP32-S3 com:

- sensor OV2640
- display TFT ST7735/ST7789
- microSD via SD_MMC
- botões e joystick para navegação

As definições de pinos estão em [src/main.cpp](src/main.cpp).

## Como compilar e gravar

O projeto usa PlatformIO. O ambiente configurado em [platformio.ini](platformio.ini) é `esp32-s3-cam`.

### Pela linha de comando

```bash
pio run -e esp32-s3-cam
pio run -e esp32-s3-cam -t upload
pio device monitor -b 115200
```

### No VS Code

1. Abra esta pasta no VS Code com a extensão PlatformIO instalada.
2. Conecte a placa ESP32-S3 via USB.
3. Use o comando de Upload do PlatformIO.
4. Depois, abra o monitor serial em `115200` baud.

## Primeira inicialização

- Insira um cartão microSD formatado em FAT32.
- Na primeira configuração de Wi-Fi, o firmware pode abrir o ponto de acesso `CyberShot-Setup`.
- Depois, a interface web fica acessível pela rede local ou pelo IP do AP.

## Estrutura do projeto

- [src/main.cpp](src/main.cpp): firmware principal.
- [scripts/](scripts/): geração dos guias de montagem.
- [platformio.ini](platformio.ini): configuração do build.

## Guias inclusos

O repositório também inclui PDFs de montagem em português e inglês:

- `DIY_CYBERSHOT_Guia_PT-BR.pdf`
- `DIY_CYBERSHOT_Guide_EN-US.pdf`
