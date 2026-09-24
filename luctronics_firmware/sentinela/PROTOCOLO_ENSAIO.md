# Protocolo de ensaio — sensores IR e som do node Sentinela

## Objetivo

Decidir duas coisas antes de projetar o firmware:

1. O IR de chama consegue separar **fogo** de **lâmpada/sol** pelo flicker de 5–15 Hz?
2. Os dois módulos de som têm ganho parecido o bastante para **comparação direcional**?

Se (1) falhar, o arranjo em Y de IR não presta e o nó precisa de outro princípio de
detecção. Se (2) falhar, abandona-se só o Y de som — o de IR é independente.

## Montagem

| Sensor | GPIO |
|---|---|
| IR1 AO | 32 |
| IR2 AO | 33 |
| IR3 AO | 34 |
| SOM1 AO | 35 |
| SOM2 AO | 36 |

GND comum. Módulos em 3V3. **Se alimentar em 5 V**, a saída AO pode ultrapassar
3,3 V e danificar o ADC — nesse caso use divisor 2:1 em cada AO.

Rode a **primeira bateria sem RC** e a **segunda com RC** (10 kΩ + 470 nF por canal,
fc ≈ 34 Hz), para ver o efeito do anti-aliasing nos próprios dados.

Os três IR devem ficar lado a lado apontando para o mesmo alvo nesta fase.
A geometria em Y só se testa depois que a detecção se provar.

## Captura

```bash
# grave o sketch, depois:
stty -F /dev/ttyUSB0 921600 raw
cat /dev/ttyUSB0 > ensaio_semRC.csv
```

Durante cada estímulo, envie o dígito correspondente pela serial para marcar o trecho.

## Estímulos

| Marca | Estímulo | Duração |
|---|---|---|
| 0 | escuro, ambiente em repouso (baseline) | 30 s |
| 1 | isqueiro ou vela a 1 m | 30 s |
| 2 | isqueiro ou vela a 2 m | 30 s |
| 3 | isqueiro ou vela a 3 m | 30 s |
| 4 | chama com corrente de ar (ventilador fraco) | 30 s |
| 5 | luz solar direta pela janela | 30 s |
| 6 | lâmpada fluorescente a 2 m | 30 s |
| 7 | lâmpada LED a 2 m | 30 s |
| 8 | lanterna de celular a 2 m | 30 s |
| 9 | incandescente/halógena, se houver | 30 s |

Para o som, com os dois módulos separados ~30 cm apontando para lados opostos:

| Marca | Estímulo |
|---|---|
| 1 | fonte de ruído centralizada, equidistante |
| 2 | fonte deslocada 90° para o lado do SOM1 |
| 3 | fonte deslocada 90° para o lado do SOM2 |
| 4 | fonoclama acionado, se disponível |

Ajuste os dois trimpots de som com a **mesma fonte, mesma distância**, antes de
começar. Sem isso o ensaio de casamento não significa nada.

## Análise

```bash
pip install numpy scipy pandas matplotlib
python analise_charact.py ensaio_semRC.csv --mark 1 --out ir_1m
python analise_charact.py ensaio_semRC.csv --mark 6 --out lampada
```

## Critérios de aprovação

**IR** — RMS na banda 5–15 Hz da chama a 3 m ≥ **3×** o da lâmpada fluorescente
e do sol. Abaixo de 2× não vale a pena: o índice de alarme falso inviabiliza.

Se a coluna `alias?` marcar SIM, há 100/120 Hz rebatendo para dentro da banda.
Monte o RC e repita — não conclua nada com o dado contaminado.

**SOM** — razão de envelope som1/som2 com a fonte centralizada entre **0,8 e 1,25**,
e dispersão p90/p10 abaixo de **1,5**. Fora disso os dois canais não são comparáveis
e o Y de som cai.

**Direcional (só se o casamento passar)** — com a fonte a 90°, a razão precisa se
afastar de 1,0 de forma consistente (algo como >1,4 ou <0,7). Se ficar perto de 1,0,
confirma-se o que a física já dizia: eletreto é omnidirecional e apontar não adianta.

## O que a decisão destrava

| Resultado | Consequência |
|---|---|
| IR passa | flicker no firmware → precisa de amostragem contínua → **firmware próprio** |
| IR falha, DO basta | ESPHome volta a ser a escolha, muito mais barato de manter |
| Som passa | Y de som mantido, direção por comparação de envelope |
| Som falha | Y de som cai; para teste do fonoclama use mic I2S (INMP441) |
