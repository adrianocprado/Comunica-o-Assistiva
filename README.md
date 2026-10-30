[README.md](https://github.com/user-attachments/files/34185548/README.md)
# PRIYA \- PROJETO ROBÓTICO DE INCLUSÃO E ACOLHIMENTO

Comunicador assistivo de baixo custo, aberto e sem fins lucrativos. A pessoa aproxima um cartão (ou etiqueta) RFID do leitor, ou aperta um botão de atalho, e o aparelho **fala a frase** e **mostra a imagem** correspondente. Foi pensado para uso em escolas: vários professores configuram tudo pelo navegador, sem regravar o programa.

> Projeto em desenvolvimento. É um recurso educacional de apoio à comunicação, não um dispositivo médico.

## Como funciona

1. Aproxime um cartão (ou etiqueta) do leitor RFID (ou aperte um dos 4 botões de atalho).  
2. O ESP32 encontra no cartão SD o som e a imagem ligados àquele cartão.  
3. A imagem aparece no display e o som toca no alto-falante.  
4. Depois do áudio (mínimo de 3 segundos) a tela volta ao início.

**Painel de configuração (navegador):** o ESP32 cria a própria rede Wi-Fi. Pelo painel você envia sons e imagens (a conversão para o formato do aparelho é automática), cadastra cartões, define texto/som/imagem de cada botão e cartão, e ajusta volume e nome/senha do Wi-Fi.

## Hardware

| Item | Observação |
| :---- | :---- |
| ESP32 DevKit 30 pinos | ESP32-WROOM-32 |
| Display TFT 1,8" SPI 128x160 (ST7735) com slot SD | cartão microSD formatado em FAT32 |
| Leitor RFID RC522 \+ cartões/etiquetas 13,56 MHz | alimentar somente com 3,3 V |
| Amplificador I2S MAX98357A | alimentado em 5 V |
| Alto-falante 4 a 8 ohms, 3 W | preferência blindado para evitar interferência |
| 4 botões de pressão | sem resistor externo |
| Botão liga/desliga | liga e desliga a alimentação do circuito |
| Bateria 18650  | bateria de 3,7 volts responsável por alimentar o circuito quando estiver desligado da energia AC. |
| Módulo Carregador de Bateria de Lítio com Conversor Step-Up DC-DC Integrado  | O módulo serve para carregar a bateria 18650 (via USB Tipo-C) e, ao mesmo tempo, elevar e regular a tensão da bateria para uma saída estável de 5V (ajustada no potenciômetro) para alimentar o seu circuito.  |
| Conector USB Tipo-C | para carregar a bateria/ energizar o circuito. |
| Caixa para o dispositivo | foi utilizado uma caixa de 200x120x55mm. |
| Capacitores 470 µF e 100 µF | evitam oscilação da tela e falhas do SD |

As ligações pino a pino estão em [`docs/`](http://docs/):

- `tabelas_ligacao_comunicador.pdf`: uma tabela por módulo (pino do módulo \-\> pino do ESP32)  
- `diagrama_comunicador.pdf`: diagrama de ligações  
- `esquema_em_blocos_comunicador.pdf`: visão geral em blocos  
- `diagrama_esquematico_completo_j5019.pdf` : ligação do módulo de carregamento e alimentação

Alimentação do circuito:

- A lista de componentes prevê o uso de uma bateria interna para manter o dispositivo portátil  
- se optar por essa disposição, não esqueça de ajustar ou se certificar que a saída do stepup está ajustada para 5 volts, se estiver acima pode danificar os componentes.  
- use um carregador de 5V/2A com conector USB tipo C.  
- Esse módulo step up também protege a bateria de receber carga excessiva e indica o status do processo de carregamento.  
- Uma opção seria não usar uma bateria interna e usar um power bank para manter o dispositivo portátil. Nesse caso o módulo carregador step up não será necessário.


## Cartões e Etiquetas RFID

- **Compatibilidade:** Utilize cartões, chaveiros ou etiquetas adesivas RFID de **13,56 MHz** (padrão Mifare).
- **Sugestão de uso:** Você pode colar as etiquetas adesivas em objetos reais (como brinquedos, copos, talheres ou remédios) ou em cartões com figuras ilustrativas para que o dispositivo faça a leitura e reproduza o áudio correspondente.


## Instalação do firmware

1. Instale a Arduino IDE e o suporte a placas ESP32. Escolha a placa **ESP32 Dev Module**.  
2. Instale as bibliotecas: **Adafruit GFX**, **Adafruit ST7735 and ST7789**, **MFRC522** e **ESP8266Audio**.  
3. Abra `firmware/comunicador_v1/comunicador_v1.ino` e grave no ESP32.  
4. Coloque o cartão SD (as pastas `/audio` e `/img` são criadas automaticamente).

## Primeiro uso

1. Conecte o celular ou o PC na rede Wi-Fi **Comunicador** (senha inicial `comunicador`).  
2. Abra `http://192.168.4.1` no navegador.  
3. Envie os sons e imagens, cadastre os cartões e associe cada um a uma frase.

Para restaurar nome/senha do Wi-Fi e o volume de fábrica, ligue o aparelho segurando o botão 1\. **Troque a senha inicial** ao instalar em uma escola.

## Estrutura do repositório

priya/

├── README.md

├── LICENSE           (a adicionar)

├── firmware/

│   └── comunicador\_v1/

│       └── comunicador\_v1.ino

├── docs/

│   ├── tabelas\_ligacao\_comunicador.pdf

│   ├── diagrama\_comunicador.pdf

│   └── esquema\_em\_blocos\_comunicador.pdf

└── imagens/fotos da montagem e do painel.

## Contribuir

Sugestões, correções e relatos de uso são bem-vindos pelas *Issues* do GitHub.


## Licença
Este projeto está licenciado sob a Licença GPL-3.0 - consulte o arquivo [LICENSE](LICENSE) para mais detalhes.


## Créditos

Idealização e montagem: Adriano Prado. Programação e documentação desenvolvidas com a colaboração do Claude (Anthropic).

## Contato

[adrianocprado@gmail.com](mailto:adrianocprado@gmail.com)
