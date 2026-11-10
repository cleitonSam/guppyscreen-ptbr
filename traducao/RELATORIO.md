# Tela da impressora em português: relatório final

Impressora: Creality Ender-3 V3 KE com OpenKE. Programa da tela: GuppyScreen v1.5.0-OpenKE (tela pequena, 480x272).
Data: 10/11/2026.

## Em poucas palavras

- **A tela inteira foi traduzida.** São cerca de **590 textos** em **47 arquivos** do programa, mais a tela de calibração do toque e o gráfico da calibração de vibração.
- **Cada texto foi medido em pixels** com as letras reais da tela, para não sair cortado nem empurrar botões para fora.
- **Os nomes seguem o Mainsail em português**, que você já usa: Mesa, Bico, Ventoinha, Calibração, Malha da mesa, Excluir objeto...
- **Os acentos estão resolvidos no projeto**: as letras novas com acento são geradas na montagem. Elas ainda não foram geradas de fato (veja "O que ainda falta").
- **A instalação faz cópia de segurança** e volta sozinha para a tela anterior se a nova não abrir. Existe também um comando para voltar à original em inglês.
- **Nada foi compilado nem instalado ainda.** Para isso, o repositório precisa existir no seu GitHub e você precisa autorizar a montagem e a instalação.

---

## 1. O que foi traduzido

### Quantidade

| O quê | Quanto |
|---|---|
| Textos traduzidos no programa | cerca de 590 |
| Arquivos do programa com tradução | 47 |
| Termos no glossário (a "lista oficial" de palavras) | 251 |
| Calibração do toque da tela | 5 textos |
| Gráfico da calibração de vibração (imagem gerada pela impressora) | 6 textos |

### Telas traduzidas

- **Tela inicial**: os 5 botões, os nomes dos sensores e a caixinha de progresso da impressão.
- **Impressão em andamento**: botões, tempos, resumo do fim da impressão, confirmação de cancelar e janela de excluir objeto.
- **Arquivos (Imprimir)**: lista, detalhes do arquivo, aviso de carretel e de filamento suficiente.
- **Mover**: ir à origem, mover eixos, parar tudo e soltar motores.
- **Filamento**: carregar, descarregar, extrudar, retrair, esfriar e aviso de fim de filamento.
- **Ventoinhas e Luz.**
- **Macros e Console**: categorias, favoritos e recentes.
- **Aba de ajustes (chave inglesa)**: Malha da mesa, Ajuste fino, Limites, Medições TMC, Energia (queda de energia), Retração e Altura do bico.
- **Calibração**: o menu e todos os assistentes.
  - Torção do eixo X.
  - Recalibração completa.
  - Calibração da extrusora (E-steps).
  - Compensação de vibração.
  - Correias.
  - Correção de esquadro.
  - Autoajuste dos motores.
- **Configurações (engrenagem)**: Wi-Fi, IP fixo, Impressoras, Sistema (tema, brilho, apagar tela, redefinições), Reiniciar Klipper, Reiniciar firmware, Reiniciar tela e Atualizar tela.
- **Avisos e mensagens** da própria tela: erros de conexão, "Aguardando a impressora iniciar...", confirmações e janelas de pergunta.

### Antes e depois das telas mais usadas

| Onde | Antes (inglês) | Agora |
|---|---|---|
| Tela inicial | Homing / Extrude / Fans / LED / Print | Mover / Filamento / Ventoinhas / Luz / Imprimir |
| Tela inicial (sensores) | Extruder / Bed / MCU Temp | Bico / Mesa / Placa-mãe |
| Caixinha da impressão | Printing 45% / ETA 1h 23m | Imprimindo 45% / Falta 1h 23min |
| Impressão em andamento | Fine Tune / Objects / Pause / Resume / Cancel / Stop / Back | Ajuste fino / Objetos / Pausar / Retomar / Cancelar / Parar tudo / Voltar |
| Fim da impressão | Print complete / Time 1h 05m / Filament 3.45 m / Done | Impressão concluída / Tempo 1h 05min / Filamento 3.45 m / OK |
| Cancelar | Cancel the print? / Cancel Print / Back | Cancelar a impressão? / Cancelar impressão / Voltar |
| Cancelar durante o aquecimento | Cancel is queued and will run when heating finishes. | O cancelamento fica na fila até o aquecimento terminar. |
| Parar tudo | Do you want to emergency stop? | Fazer parada de emergência? |
| Aba de ajustes | Bed Mesh / Fine Tune / Limits / TMC Metrics / Retraction / Z Offset / Calibration | Malha da mesa / Ajuste fino / Limites / Medições TMC / Retração / Altura do bico / Calibração |
| Configurações | WIFI / Restart Klipper / Update Guppy / Printers / System | Wi-Fi / Reiniciar Klipper / Atualizar tela / Impressoras / Sistema |
| Filamento sem carretel | No spool selected - load anyway? | Sem carretel ativo. Carregar mesmo assim? |
| Fim de filamento | Load / Resume / Cancel | Carregar / Continuar / Cancelar, com o aviso "Cancelar encerra a impressão." |
| Torção do eixo X | Start Calibration / Accept / Save & Restart | Iniciar calibração / Aceitar / Salvar e reiniciar |
| Teste do papel | Paper test | Teste do papel: "Coloque uma folha de papel embaixo do bico. Abaixe até o papel arrastar de leve e toque em Aceitar." |
| Detalhes do arquivo | Modified: 2026-11-10 14:30 | Modificado em: 10/11/2026 14:30 |

O caminho da calibração que você precisa fazer fica assim na tela em português:
**Ajustes (chave inglesa) → Calibração → Torção do eixo X → Iniciar calibração → Aceitar (5 vezes) → Salvar e reiniciar.**

### Cuidados de qualidade

- **Largura medida, uma por uma.** Usamos as letras reais da tela (fonte Montserrat da biblioteca gráfica LVGL 8.3.11) e a mesma regra de quebra de linha do programa. Quando a tradução natural não cabia, escolhemos uma forma mais curta. Exemplos:
  - "Falta" no lugar de "Faltam", porque "Faltam" cortava 10 px da caixinha da tela inicial.
  - "Tema" no lugar de "Cor do tema".
  - "Compr. da retração".
  - "Mín." e "Máx." no brilho.
  - "30 s", "1 min" e "1 h" no tempo para apagar a tela.
  - "Máscara" no rótulo do IP fixo.
- **Textos que o programa compara não foram mexidos.** Alguns textos não são só para mostrar: o programa procura por eles para saber o que aconteceu. É o caso de "Calibration complete" e "Mesh Bed Leveling Complete", que vêm do Klipper (o programa que controla os motores). Esses ficaram em inglês, senão as calibrações deixariam de reconhecer o fim.
- **Correções que vieram junto:**
  - O botão Parar tudo da tela Mover tinha um texto diferente das outras telas e sempre perguntava. Agora o texto é igual nas 5 telas.
  - A recalibração dizia "4 minutos" de espera, mas o programa espera 8. Agora a tela diz 8.
  - Onde estava escrito "C" agora está "°C". Os "hz" viraram "Hz" e "mm3/s" virou "mm³/s".
  - O estado da impressão aparecia em inglês ("Printing", "N/a"). Agora aparece "Imprimindo", "Pausada", "Concluída", "Cancelada", "Erro", "Em espera" ou "--".
  - Ventoinhas com nome desconhecido agora aparecem como "Ventoinha 1" e não como "Fan1".
- **Seus nomes antigos continuam valendo.** O arquivo de configuração da tela na sua impressora (guppyconfig.json) guarda os nomes "Extruder", "Bed" e "MCU Temp" em inglês. A tela traduz esses nomes só na hora de mostrar e não altera o arquivo. Assim, se você voltar à tela original, nada aparece estranho.
- **Revisão.** Quatro revisores conferiram tudo: português, código, espaço na tela e montagem. Os quatro aprovaram, e as correções que pediram já foram feitas (veja a seção 9).

---

## 2. O que continua em inglês, e por quê

| O quê | Por quê |
|---|---|
| Mensagens que chegam do Klipper (erros com "!!", avisos de "Must home axis first", "shutdown", textos de macros com M117) | Vêm prontas da impressora. A tela só mostra. Traduzir exigiria mexer no Klipper. |
| Janelas de pergunta criadas pelas macros (ex.: troca de filamento M600) | O texto está nos arquivos .cfg das macros, não no programa da tela. |
| Nomes de comandos e macros (G28, SAVE_CONFIG, LOAD_FILAMENT...) | São o próprio comando. Se traduzir, deixa de funcionar. |
| Nomes técnicos que você vê no Mainsail: Klipper, Moonraker, Spoolman, printer.cfg, rotation_distance, [axis_twist_compensation], CR Touch | Assim você reconhece o mesmo nome nos dois lugares. |
| Tipos de compensação de vibração (zv, mzv, ei, 2hump_ei...) | São os nomes do Klipper e vão gravados na configuração. |
| Objetivo do autoajuste dos motores (auto, silent, performance, autoswitch) | O texto escolhido é gravado direto no printer.cfg. Para traduzir, seria preciso separar o texto mostrado do valor gravado. |
| Nível de registro (trace, debug, info, warn) | É gravado na configuração da tela. |
| Eixos e sinais (X+, Z-, PA+), números dos botões (0.01, 0.05...) | O programa transforma esses textos em número. |
| Nomes de redes Wi-Fi, de arquivos, de objetos da peça e de perfis da malha | São seus dados. |
| "Status", "OK", "Gateway", "DNS", "IP", "LED", "Manual" | Iguais ou de uso comum em português. |
| A palavra "GuppyScreen" na tela Sistema, ao lado da versão | É o nome do programa. |
| Mensagens de registro (o "diário" interno do programa, que não aparece na tela) | Ninguém vê na tela, e manter facilita pedir ajuda. |

**Teclado da tela:** o teclado de toque não tem teclas com acento. Dá para ver acentos, mas não para digitar (por exemplo, na senha do Wi-Fi). Isso é da biblioteca gráfica e não mudou.

---

## 3. Como os acentos foram resolvidos

**O problema.** As letras que vêm embutidas na biblioteca gráfica da tela (LVGL) têm só os caracteres do inglês. Qualquer "ç", "ã" ou "é" apareceria como um quadradinho.

**A solução.**

1. **Novas letras, mesmo desenho.** Um gerador refaz as 6 fontes usadas (tamanhos 8, 10, 12, 14, 16 e 20) com o mesmo desenho, a mesma ferramenta e a mesma versão que o LVGL usou. Muda só a lista de letras: entram todas as letras acentuadas do português, além de °, ², ³ e as aspas e travessões.
   - Arquivos: `scripts/gerar-fontes-ptbr.sh` e `scripts/fontes_ptbr.py`. A explicação completa está em `traducao/FONTES.md`.
2. **A altura das linhas não muda.** Com isso, nenhum texto da tela muda de lugar. Os acentos das maiúsculas (Á, Ç) usam a pequena folga que o desenho já tem.
   - Exceção: nos 4 textos que rolam sozinhos (nome de arquivo comprido, por exemplo), a pontinha do acento pode sair cortada.
   - Se isso incomodar, existe uma opção para gerar com altura maior.
3. **Conferência automática.** O gerador confere a "impressão digital" (SHA, um código que muda se o arquivo for alterado) de tudo o que baixa. Ele também confere se cada letra antiga ficou idêntica. Depois procura, em todos os textos do programa, letras que as fontes não têm. Se algo não bater, ele para e não grava nada.
4. **Uma chave para desligar.** A chave `GUPPY_FONTES_PTBR` liga ou desliga as letras novas.
5. **Tamanho.** O programa cresce cerca de 45 KB, menos de 1%.

**Situação:** os arquivos de letras (pasta `fonts_ptbr/`) **ainda não foram gerados**. Para gerar, é preciso rodar o Node (programa que executa a ferramenta de conversão), e isso estava bloqueado para nós no seu computador. A montagem no GitHub gera sozinha quando eles faltam.

---

## 4. Como a versão é montada e publicada no seu GitHub

O programa da tela é montado (compilado, ou seja, transformado no arquivo que a impressora executa) na nuvem do GitHub, sem usar nada privado do autor original.

- **Arquivo da montagem:** `.github/workflows/build.yml`.
  1. Baixa as ferramentas de montagem para o processador da KE e confere a impressão digital delas.
  2. Gera as letras com acento, se faltarem.
  3. Monta o programa.
  4. Confere o resultado: se é para o processador certo, se tem os textos em português e se os scripts estão no formato certo.
- **O que ele publica:** o pacote `guppyscreen-smallscreen.tar.gz` e a impressão digital dele, num arquivo `.sha256`.
- **Quando ele roda:**
  - **Envio para a main** (o ramo principal): gera uma versão "teste", que o botão de atualizar ignora.
  - **Versão oficial:** na aba Actions do GitHub, em "montar tela em português", toque em "Run workflow" e digite a versão `v1.5.0-OpenKE-ptbr`.
  - **Proteção:** a versão oficial **não sai** se faltar texto em português ou letra com acento.
- **Numeração:**
  - `v1.5.0-OpenKE-ptbr` é a primeira versão.
  - `.1`, `.2`... são correções.
  - Quando o OpenKE lançar uma versão nova, a base muda, por exemplo para `v1.5.1-OpenKE-ptbr`.
- **Alternativa no seu computador:** `scripts/montar-local-wsl.sh` faz a mesma montagem no Ubuntu dentro do Windows (WSL). O pacote sai na pasta `pacote-local/`. Só use se você autorizar a montagem no computador.

---

## 5. Como instalar na impressora (com cópia de segurança e volta automática)

**Antes:** a impressora tem que estar **parada, sem imprimir**. Entre nela pelo terminal: no PowerShell, digite `ssh root@192.168.1.16`.

Cole esta linha e aperte Enter:

```sh
wget --no-check-certificate -qO /tmp/instalar-tela-ptbr.sh https://raw.githubusercontent.com/cleitonSam/guppyscreen-ptbr/main/scripts/instalar-tela-ptbr.sh && sh /tmp/instalar-tela-ptbr.sh
```

Enquanto não existir a primeira versão oficial, use a de teste: troque o fim da linha por `sh /tmp/instalar-tela-ptbr.sh teste`.

O que o instalador faz (`scripts/instalar-tela-ptbr.sh`):

1. Confere se é a KE com OpenKE, se a impressora está parada e se há espaço livre.
2. Baixa o pacote e confere se chegou inteiro.
3. Prepara a versão nova numa pasta separada. A tela atual continua funcionando enquanto isso.
4. Faz a **cópia de segurança completa** em `/usr/data/guppyscreen-backup-AAAAMMDD-HHMMSS`.
5. Troca a tela **mantendo suas configurações** (calibração do toque, brilho, sensores) e reinicia **só a tela**. O Klipper continua rodando.
6. Espera 15 segundos e confere se a tela nova ficou de pé. **Se não ficou, volta sozinho para a anterior.**

Tudo fica anotado em `/usr/data/printer_data/logs/instalar-tela-ptbr.log`. Dá para ler pelo Mainsail.

## 6. Como atualizar pelo botão da tela

Toque na **engrenagem → Atualizar tela**. O botão agora busca no **seu** repositório, então a tela continua em português (`scripts/update.sh`).

- Ele só pega versões oficiais.
- Não faz nada se a impressora estiver imprimindo.
- Confere o pacote e guarda a versão atual. Se a nova não abrir, volta sozinho.
- A tela não mostra mensagens durante a atualização (já era assim no original). O que acontece fica anotado em `/usr/data/printer_data/logs/guppyscreen-update.log`.

## 7. Como voltar à tela original, em inglês

No terminal da impressora:

```sh
sh /usr/data/voltar-tela-original.sh
```

- Ele volta para a cópia da tela original que o instalador guardou.
- Reinicia só a tela e confere se ela abriu. Se não abriu, volta sozinho para a que estava.
- A versão em português não é apagada: vira mais uma cópia guardada.
- Para ver as cópias guardadas: `sh /usr/data/voltar-tela-original.sh --listar`.
- **Não apague a cópia original.** No `--listar`, ela é a que aparece com a versão **sem "ptbr"** no nome. Não confie na data do nome da pasta: enquanto a impressora não acerta a hora pela internet, o relógio dela marca 01/03/2020.

Para ter o português de novo, rode o instalador outra vez.

**Atenção:** não rode o instalador completo do OpenKE original. Ele põe a tela em inglês de volta.

O manual completo para o dono está em `LEIAME-PTBR.md`. O README do projeto ganhou um aviso no topo apontando para ele.

---

## 8. O que depende de você

1. **Criar o repositório público `cleitonSam/guppyscreen-ptbr` no GitHub.** Hoje o endereço responde "não encontrado".
   - Tem que ser **público**: a impressora baixa os arquivos sem senha.
   - Se for criado como cópia (fork), ligue o GitHub Actions na aba Actions.
2. **Escolher como montar:**
   - pela **montagem do GitHub**, recomendada, que não usa seu computador;
   - ou **autorizar a montagem no seu computador** (WSL).
3. **Publicar a primeira versão oficial** (`v1.5.0-OpenKE-ptbr`). Antes disso, só funciona a instalação "teste".
4. **Autorizar a instalação e acompanhar a primeira vez**, com a impressora parada. Nada foi testado na impressora ainda.
5. **Responder: a parada de emergência deve perguntar antes?** Veja o item 1 da seção 9.

---

## 9. Pendências que os revisores deixaram

**Já corrigidas:**

- "OU" em maiúsculas.
- "Excluí-lo" trocado por "Excluir este objeto".
- "Conectado a" igual nos 3 lugares do Wi-Fi.
- "se mexer / Não mexa" sem repetir o verbo.
- Linha de resultado da vibração encurtada ("vib.").
- Frase da fila de cancelamento quebrada no lugar certo, nas duas janelas.
- Aviso de favoritos do console em 2 linhas.
- Nomes de ventoinha.
- Nomes padrão em inglês no arquivo de configuração, traduzidos só na tela.
- Comentário da linha do tema.
- Regra da montagem para trocar a chave das letras.
- `__pycache__` e `*.pyc` fora do Git.
- Nomes de pasta repetidos nas cópias de segurança.
- Glossário atualizado com as escolhas que fugiram dele, com o motivo.
- Data no formato brasileiro.

**Ainda abertas:**

1. **Segurança: o botão Parar tudo dispara sem perguntar.**
   - Na configuração padrão do OpenKE falta a opção "perguntar antes da parada de emergência".
   - Sem ela, o programa entende "não perguntar" (`src/button_container.cpp`, linha 41).
   - Mas a tela Sistema mostra a opção **ligada**. Isso já era assim no original e não foi mexido, porque é lógica e não tradução.
   - **Sugestão:** tratar a falta da opção como "perguntar". Basta trocar `false` por `true` naquela linha, se você quiser.
2. **As letras com acento ainda não foram geradas** (pasta `fonts_ptbr/`). O ideal é gerar uma vez pelo GitHub e guardar no repositório.
3. **Nada foi compilado nem testado na impressora.** Os scripts foram conferidos por leitura e por testes simulados.
4. **Pontos com pouca folga na tela:**
   - Na lista de arquivos, a fileira de cima "Recarregar | Data | A-Z" já passava da borda no original e continua igual.
   - Em impressões de mais de 1 dia, o tempo ("1d 23h 59min 59s") pode encostar no ícone.
   - Acentos de maiúsculas em textos que rolam sozinhos podem perder a pontinha.
5. **Peça do projeto que pode sumir.** O driver da tela (`lv_drivers`) aponta para um repositório que mudou de dono (NebulaOS-KE). Recomendo fazer uma cópia (fork) na sua conta e trocar o endereço no arquivo `.gitmodules`.
6. **Arquivos soltos na pasta do projeto que não devem ir para o GitHub:** `bocchi.html`, `d558.html`, `entware_pkgs.html`, `pr34.html` e `kb_install.sh`.
7. **Ao salvar no Git, marcar os scripts como executáveis:**
   - `git update-index --chmod=+x scripts/instalar-tela-ptbr.sh scripts/voltar-tela-original.sh scripts/montar-local-wsl.sh`
8. **Arquivos de fim de linha.** O Git deste Windows troca o fim das linhas dos arquivos. O arquivo `.gitattributes` novo garante que os scripts vão no formato certo para a impressora. Não copie scripts direto desta pasta para a impressora: use sempre o pacote publicado.
9. **Futuro do OpenKE.** O autor do OpenKE parece ter passado para outro projeto (NebulaOS). A v1.5.0 pode ser a última versão para traduzir.
10. **Se a impressora não acertar a hora pela internet,** as cópias de segurança ficam com data de 2020. Identifique a original pelo `--listar`.

---

## Arquivos principais

| Arquivo | Para quê |
|---|---|
| `src/*.cpp` (47 arquivos) | Os textos da tela, traduzidos |
| `lv_touch_calibration/` | Tela de calibração do toque, traduzida |
| `k1/scripts/calibrate_shaper.py` | Gráfico da calibração de vibração, traduzido |
| `traducao/GLOSSARIO.md` e `glossario.json` | A lista de termos (251) e as regras de escrita |
| `traducao/FONTES.md` | Explicação das letras com acento |
| `traducao/medir_largura.py` | Medidor de largura dos textos |
| `scripts/gerar-fontes-ptbr.sh`, `scripts/fontes_ptbr.py` | Geram e conferem as letras com acento |
| `lv_conf.h`, `Makefile` | Ligam as letras novas na montagem |
| `.github/workflows/build.yml` | Montagem e publicação no GitHub |
| `scripts/instalar-tela-ptbr.sh` | Instalador na impressora |
| `scripts/update.sh` | Botão "Atualizar tela" |
| `scripts/voltar-tela-original.sh` | Volta para a tela original |
| `scripts/montar-local-wsl.sh` | Montagem no computador (WSL) |
| `LEIAME-PTBR.md` | Manual para o dono |
