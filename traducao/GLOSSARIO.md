# Tradução da tela: glossário e guia de estilo

**Tela:** Creality Ender-3 V3 KE com GuppyScreen v1.5.0-OpenKE (C++ com LVGL 8, 480x272).
**Para quem:** quem traduz os textos de `src/`, e o dono, para conferir os nomes.
**Base:** Mainsail em português (o dono usa), KlipperScreen pt_BR, OrcaSlicer pt-BR, Creality Print pt-BR, tela original da Creality em português, manual da KE em português, Marlin e Fluidd em pt-BR, guias de estilo pt-BR da Microsoft e da Proton, e os termos já usados com o dono. Lista completa no fim.

## Resumo em 12 regras

1. **Primeiro as fontes.** As fontes originais da tela não têm letras com acento; sem as fontes novas (`fonts_ptbr/`), "Calibração" aparece como "Calibra" seguido de quadradinhos. Ver a parte 1.
2. **Mesmo termo do Mainsail** quando ele estiver em português bom. Onde o Mainsail deixou em inglês (Z-Offset, Home all, Pressure Advance) ou traduziu mal ("Consolde Histórico", "Ferramenta" para o cabeçote), vale o português simples que o dono já ouviu: altura do bico, ir à origem, avanço de pressão.
3. **Botão = verbo no infinitivo**: Salvar, Calibrar, Voltar, Iniciar calibração.
4. **Instrução = imperativo**, falando direto com o dono, sem escrever "você": "Abaixe o bico até o papel arrastar de leve e toque em Aceitar."
5. **Andamento = gerúndio com reticências**: Aquecendo..., Medindo a mesa...
6. **Título = substantivo**: Calibração, Malha da mesa, Torção do eixo X.
7. **Só a primeira letra maiúscula**, mais nomes próprios, eixos (X, Y, Z) e siglas.
8. **Curto.** Uma ideia por frase. Botão com ícone: até 62 px por palavra na fonte de 10 px, cerca de 10 letras. Limites de cada tela na parte 2.5.
9. **"Toque em"**, nunca "clique" ou "pressione".
10. **Número com ponto** (0.05 mm), como o teclado da tela e o Klipper. Espaço antes da unidade (100 mm); °C e % colados (240°C, 50%). Minuto é "min".
11. **Não traduzir** nomes próprios, comandos, macros, chaves de configuração e os textos que o código compara ou grava (parte 2.9).
12. **Pontuação simples e igual em todo lugar**: aspas retas "...", três pontos "...", dois-pontos ou frase nova no lugar do " - " do inglês. Hífen só dentro de palavra (Wi-Fi, sub-rede, placa-mãe).

## 1. Acentos: a tradução só aparece com as fontes novas

- A tela usa as fontes Montserrat que vêm dentro do LVGL (`lv_font_montserrat_8/10/12/14/16/20`). Conferido no LVGL que o projeto usa (v8.3.11, commit `74d0a81`): elas só têm os caracteres `0x20-0x7F` (inglês sem acento), o `°` e o `•`, além dos ícones. **Não existem á, ã, â, à, ç, é, ê, í, ó, ô, õ, ú nem as maiúsculas.** Cada letra que falta vira um quadradinho ou some; hoje isso já acontece com o travessão de "Startup in progress — cancel is queued".
- A solução já está no projeto: `scripts/gerar-fontes-ptbr.sh` gera `fonts_ptbr/lv_font_montserrat_{8,10,12,14,16,20}.c` com o mesmo conversor (lv_font_conv 1.5.2) e as mesmas opções das originais, acrescentando a faixa Latin-1 (`0xA0-0xFF`) e `– — ‘ ’ “ ” …`. A chave `GUPPY_FONTES_PTBR` no `lv_conf.h` troca as fontes, com os mesmos nomes, então nada em `src/` muda. Detalhes em `traducao/FONTES.md`.
- As letras sem acento ficam com a mesma largura e o mesmo kerning (o script confere). Por isso as medidas deste guia valem para as fontes novas.
- Com `GUPPY_FONTES_PTBR=0` (fontes antigas) os acentos somem. Não compile a tradução nesse modo.
- Antes de compilar, `bash scripts/gerar-fontes-ptbr.sh --so-conferir` confere se toda letra usada nos textos de `src/` existe nas fontes.
- Com as fontes novas existem também `² ³ ª º`: pode escrever `mm/s²`, `mm³/s` e `1ª camada`.
- Maiúscula com acento (Á, Ã, É, Ú...) é a letra mais alta da fonte. O script mantém a altura de linha original e avisa se alguma letra passar da margem que o LVGL desenha em volta do texto.
- A fonte do console (`assets/dejavusans_mono_14.c`) já tinha acentos. A de ícones (`materialdesign_font_40`) não precisa.
- Texto acentuado nunca pode passar por `KUtils::to_title()`, `toupper` ou `tolower`. Essas funções trabalham byte a byte e estragam as letras com acento.

## 2. Guia de estilo

### 2.1 Tom

- Como um técnico calmo ao lado da impressora: direto, gentil e sem jargão.
- Fale pelo imperativo ("Coloque", "Meça", "Toque em"). Nada de "você deve" ou "o usuário".
- Palavras do dia a dia: "escolher" (não "selecionar"), "veja" (não "consulte"), "de novo", "mexer".
- Na tela de toque, sempre "toque em": "Press Extrude" vira "Toque em Extrudar".
- "Por favor" só quando a frase ficar ríspida sem ele. "Please wait" vira "Aguarde".
- Sem ponto de exclamação, a não ser em alerta de verdade.
- Sem gerundismo ("vai estar salvando"). Use o presente: "Salvar reinicia o Klipper."
- Aviso de risco diz o que acontece e pergunta de forma clara: "A impressora está imprimindo.\nEnviar este comando mesmo assim?"

### 2.2 Forma dos verbos

| Tipo de texto | Forma | Exemplo |
|---|---|---|
| Botão | infinitivo | Save = Salvar; Start Calibration = Iniciar calibração |
| Instrução | imperativo | "Lower until a sheet of paper just drags, then Accept." = "Abaixe até o papel arrastar de leve e toque em Aceitar." |
| Andamento | gerúndio + "..." | "Heating..." = "Aquecendo..." |
| Título e menu | substantivo | Calibration = Calibração |
| Estado | adjetivo ou particípio | Paused = Pausada; Connected = Conectado |
| Pergunta de confirmação | infinitivo + "?" | "Cancel the print?" = "Cancelar a impressão?" |
| "Can't", "Could not", "Unable to" | "Não é possível" + infinitivo ("Não foi possível" se já aconteceu) | "Can't calibrate while printing." = "Não é possível calibrar durante a impressão." |
| "Failed to" | "Falha ao" + infinitivo | "Failed to apply" = "Falha ao aplicar" |
| "... timed out" | "demorou demais" | "Heating timed out." = "O aquecimento demorou demais." |

### 2.3 Maiúsculas

- Só a primeira palavra: "Malha da mesa", "Iniciar calibração", "Cor do tema", "Reiniciar firmware".
- Sempre com maiúscula: nomes próprios (Klipper, Moonraker, Mainsail, Spoolman, OpenKE, Creality, OrcaSlicer, CR Touch), eixos (X, Y, Z), siglas (TMC, PID, PA, IP, DHCP, LED, MCU) e a unidade Hz.
- O que o original escreve todo em maiúsculas continua assim, com acento: NESTA ORDEM, OUTROS, REDES SALVAS, CANCELAR IMPRESSÃO.
- Depois de dois-pontos, minúscula: "Baixa latência: ativa".
- Botão citado numa frase aparece escrito igual ao botão, com maiúscula e sem aspas: "toque em Aceitar", "toque em Salvar e reiniciar".

### 2.4 Pontuação, números e unidades

- Frase completa termina com ponto. Botão, título e rótulo não levam ponto.
- Reticências: três pontos "..." no andamento ("Salvando...") e no texto de exemplo dos campos ("digite o G-code..."). O caractere único "…" existe nas fontes novas, mas o código inteiro usa três pontos; mantenha.
- O inglês do OpenKE liga frases com " - " ("Calibration timed out - nothing was changed"). Em português, divida em duas frases ou use dois-pontos: "A calibração demorou demais. Nada foi alterado." Se precisar mesmo de travessão, use "—" com espaço dos dois lados (existe nas fontes novas), nunca o hífen.
- Hífen só dentro de palavra (Wi-Fi, sub-rede, placa-mãe) e em faixas de letras (A-Z).
- Aspas retas, como no resto do código: Esquecer a rede "Casa"?
- Ponto decimal: `0.05 mm`, `38.5 Hz`. Os números vêm do código com ponto e o teclado da tela só tem ponto, então misturar com vírgula confunde. Vale também para número escrito dentro da frase: "Within 0.5mm" vira "Até 0.5 mm".
- Espaço entre número e unidade: `100 mm`, `60 mm/s`, `12 g`, `38.5 Hz`, `800 mA`. Sem espaço: `240°C` e `50%`.
- Unidades: mm, mm/s, mm/s², mm³/s, °C, %, Hz (corrigir "hz"), g, m, MB, mA, RPM. O original escreve "mm/s2" e "mm3/s" porque a fonte antiga não tinha ² e ³.
- Tempo: d, h, min, s ("1h 23min 45s"). Minuto é "min"; "m" é metro.
- Medidas com "x" (`5x5`) e faixas com "a" ("Z: 0.120 a -0.300").
- Sem vírgula antes do "e" no fim de uma lista: "a configuração do Klipper, os arquivos e as calibrações".

### 2.5 Espaço na tela (medido)

As larguras foram medidas com as próprias fontes do LVGL do projeto, somando o kerning do jeito que o LVGL soma. Valem para as fontes novas, que mantêm largura e kerning. Letra com acento mede o mesmo que a letra sem acento. Em média, 10 letras dão 62 px na fonte de 10 px.

| Onde | Fonte | Largura por linha | Linhas |
|---|---|---|---|
| Botões com ícone em coluna fixa: tela Filamento, Compensação de vibração, Autoajuste dos motores, Opções de redefinição e Voltar em Sistema, Spoolman, Restaurar dos Limites | 10 px | 62 px | 1 (2 só onde o original já tem `\n`) |
| Barra de botões da tela da impressão | 10 px | cerca de 62 px | 1 |
| Grade das telas Mover e Ajuste fino (5 colunas) | 10 px | cerca de 82 px | 1 (não há altura para 2) |
| Abas Ajustes e Configurações (4 colunas) | 10 px | cerca de 90 px | até 2 |
| Botões da tela inicial (Mover, Filamento, Ventoinhas, Luz, Imprimir) | 10 px | cerca de 100 px | 1 |
| Barra da Malha da mesa e da calibração de correias | 10 px | 88 a 110 px | até 2 |
| Nome do sensor na tela inicial (Bico, Mesa, Placa-mãe) | 10 px | 78 px (corta com "...") | 1 |
| Linhas do menu Calibração | 14 px | cerca de 273 px (corta com "...") | 1 |
| Títulos das telas | 20 px | cerca de 400 px | 1 |
| Caixa de confirmação | 10 px | cerca de 216 px | até 4 |

- O LVGL só quebra a linha em espaço (e em `, . ; : - _`). Palavra maior que a largura é cortada no meio. **Nenhuma palavra sozinha pode passar do limite.**
- Já medidos: cabem no limite exato de 62 px "Descarregar", "Impressoras", "Imprimindo" e "Origem XYZ"; não aumente. Não cabem em 62 px: "Emergência" (63, por isso "Parar tudo"), "Velocidade+" (63, por isso "Veloc.+"), "Configurações" (74) e "Desligar motores" (87, que não cabe nem nos 82 px da tela Mover; por isso "Soltar motores", 74).
- Quebra manual `\n`: só onde o original tem, num ponto natural ("Reiniciar\nKlipper").
- Para medir: `traducao/medir_largura.py` (como usar está no começo do arquivo).

### 2.6 Abreviar só quando não couber

- Primeiro tente uma palavra mais curta. Abreviar é o último recurso.
- Lista fechada: Veloc. (velocidade), Temp. (temperatura), Compr. (comprimento), máx. e mín. Unidades (min, h, s) não levam ponto.
- A abreviação termina em consoante e mantém o acento: "máx.", não "max".
- Nunca abrevie verbo de botão, nome de calibração ou aviso de segurança.

### 2.7 Termos que ficam como estão

- Nomes: Klipper, Moonraker, Mainsail, Fluidd, Spoolman, OpenKE, GuppyScreen, Creality, OrcaSlicer, CR Touch, BLTouch, wpa_supplicant.
- Siglas e padrões: Wi-Fi (sempre com hífen), G-code, TMC, PID, PA, IP, DHCP, DNS, LED, MCU, USB, WPA/WPA2, ID.
- Arquivos e chaves: printer.cfg, guppyconfig.json, [axis_twist_compensation], [skew_correction], [firmware_retraction], z_offset, rotation_distance.
- Nomes de macros e comandos (G28, M600, SAVE_CONFIG...), nomes de objetos do Klipper (extruder, heater_bed, output_pin fan0) e tipos de compensação de vibração (mzv, ei, zv...).
- Em frase para o dono, prefira "arquivo de impressão" a "gcode" e "a tela" a "GuppyScreen". O nome GuppyScreen fica só na versão.

### 2.8 Padrões de frase

| Inglês | Português |
|---|---|
| Printer is printing.\n{X} anyway? | A impressora está imprimindo.\n{verbo} mesmo assim? |
| Can't {X} while printing. | Não é possível {verbo} durante a impressão. |
| Unavailable while printing | Indisponível durante a impressão |
| {Section} isn't enabled.\nAdd [{section}] to printer.cfg. | {Recurso} não está ativado.\nAdicione [{seção}] ao printer.cfg. |
| ... and restart Klipper. | ... e reinicie o Klipper. |
| Saving... Klipper will restart. | Salvando... O Klipper vai reiniciar. |
| This cannot be undone. | Isso não pode ser desfeito. |
| Please wait, do not touch the printer. | Aguarde e não mexa na impressora. |
| Tap {X} / Press {X} | Toque em {X} |
| No {X} yet. | Nenhum {X} ainda. |
| Are you sure you want to delete {}? | Excluir {}? |
| {X} done - recommended {} @ {} Hz. Tap Save to apply. | {X} pronto: recomendado {} em {} Hz. Toque em Salvar para aplicar. |
| Point {} of {} / Step 1 of 2 | Ponto {} de {} / Etapa 1 de 2 |
| {} in progress... | {} em andamento... |
| Connecting to {} ... | Conectando a {}... |
| ... it'll move & shake for ~1 min. Leave it be. | ... ela vai se mexer e vibrar por cerca de 1 minuto. Não mexa. |

Exemplos completos, para dar o tom:

- "Lower until a sheet of paper just drags, then Accept." = "Abaixe até o papel arrastar de leve e toque em Aceitar."
- "Mapping bed... this takes about a minute, no input needed." = "Medindo a mesa... leva cerca de 1 minuto. Não precisa tocar em nada."
- "Corrects left-to-right first-layer error that bed mesh can't fix." = "Corrige a primeira camada esmagada de um lado e alta do outro, que a malha da mesa não resolve."
- "Calibration timed out - nothing was changed. Check the printer and try again." = "A calibração demorou demais e nada foi alterado. Confira a impressora e tente de novo."
- "Within 0.5mm - looks good, you can stop here." = "Diferença de até 0.5 mm: está bom, pode parar aqui."

### 2.9 Cuidados no código: o que quebra se traduzir sem olhar

1. `button_container.cpp:49` compara o texto "Do you want to emergency stop?" para decidir se pergunta antes da parada de emergência. O texto é passado em 4 telas (`homing_panel.cpp:32`, `print_status_panel.cpp:46`, `inputshaper_panel.cpp:71`, `belts_calibration_panel.cpp:35`). Traduza nas 4 e na comparação, ou troque a comparação por uma constante.
2. `axis_twist_panel.cpp:340/351` e `recalibration_wizard_panel.cpp:665/673` procuram "Calibration complete", "Calibration aborted" e "Mesh Bed Leveling Complete" nas respostas do Klipper. Esses `find()` ficam em inglês; só os rótulos da tela mudam.
3. `notification_manager.cpp:93` procura o erro do Klipper "Must home axis first": fica em inglês.
4. `console_panel.cpp:548/549/681/683/806` comparam "Favorites" e "Recent": traduza dos dois lados.
5. `tmc_tune_panel.cpp:157` compara "Not Configured" com o item do menu: traduza dos dois lados. As opções auto/silent/performance/autoswitch e os nomes de motor vão para o printer.cfg: ficam em inglês, a não ser que o código passe a separar o texto mostrado do valor gravado.
6. `sysinfo_panel.cpp`: as cores do tema (blue, red...) viram nome de arquivo e são gravadas; os níveis de registro (trace, debug...) são gravados e lidos pelo spdlog. Traduza só o texto mostrado e mantenha o valor em inglês. As opções de "Apagar tela" comparam com o mesmo vetor e podem ser traduzidas direto.
7. `print_status_panel.cpp:380/540` compara `display_name == "Chamber"`, que vem da configuração: mantenha.
8. "Extruder", "Bed", "MCU Temp", "Toolhead Fan", "Back Fan" e "Side Fan" (`config.cpp`) são gravados no guppyconfig.json na primeira vez. Na impressora do dono já estão em inglês, então traduzir só o código não muda a tela dele. Precisa de uma migração, como a que `config.cpp:121-133` já faz para LOAD_MATERIAL, ou de traduzir na hora de mostrar. Escolha feita: o `config.cpp` continua gravando os nomes em inglês e a tela traduz na hora de mostrar (`main_panel.cpp` para os sensores, `fan_panel.cpp` para as ventoinhas). Assim o arquivo também funciona se o dono voltar para a tela original.
9. `utils.cpp:510-535` (`fan_display_name`) monta os nomes das ventoinhas e pode ser traduzido direto, mas a linha 532 procura "Fan" no nome para acrescentar " Fan". Adapte para "Ventoinha". Feito: o nome de reserva tira o "Fan" e sempre põe "Ventoinha" na frente ("fan1" vira "Ventoinha 1").
10. `mini_print_status.cpp:74` mede a largura da sobreposição da tela inicial pelo texto "ETA  88:88:88". Troque junto com o texto da linha 130. A linha 131 mostra o estado cru do Klipper com `to_title()` ("Printing"): troque por uma tabela (printing = Imprimindo, paused = Pausada...).
11. Nunca traduzir: G-code, nomes de macros, chaves JSON, nomes de objetos do Klipper e estados comparados ("printing", "paused", "complete", "cancelled", "standby", "ready", "shutdown").
12. Mantenha `{}`, `{:.3f}`, `%d`, `%.1f` e `\n` na mesma ordem e quantidade. Frase partida em várias linhas no C++: traduza a frase inteira e redistribua. Mantenha também os ícones `LV_SYMBOL_...` e os espaços depois deles: `LV_SYMBOL_LEFT "  Back"` vira `LV_SYMBOL_LEFT "  Voltar"`.
13. Segurança (não é tradução, mas apareceu): `button_container.cpp:41` trata "chave prompt_emergency_stop ausente" como "não perguntar", enquanto `sysinfo_panel.cpp:246` mostra a opção ligada quando a chave falta. A configuração padrão do OpenKE não tem essa chave. Resultado: o botão de parada de emergência dispara sem perguntar, mesmo com "Confirmar parada de emergência" aparecendo ligado.

### 2.10 O que não está no código da tela

- As janelas abertas por macros (`action:prompt`) estão nos `.cfg` da impressora: `GuppyScreen/M600-support.cfg` (troca de filamento: "Filament change detected!", Unload, Load, Purge More, CANCEL PRINT, IGNORE, RESUME) e `GuppyScreen/useful-macros.cfg` (backup e restauração). Traduza lá, sabendo que uma atualização do OpenKE pode sobrescrever. Sugestão: Troca de filamento detectada, Descarregar, Carregar, Purgar mais, CANCELAR IMPRESSÃO, IGNORAR, RETOMAR.
- As mensagens do Klipper (erros, "Must home axis first", estado "shutdown") e a saída do console chegam em inglês.
- Nomes de arquivos, de macros e de comandos não mudam.

## 3. Nomes para usar nas instruções ao dono

- As abas da esquerda só têm ícone. Chame assim: Início (casa), Macros, Console, Ajustes (ícone de ajuste) e Configurações (engrenagem).
- Torção do eixo X, caminho completo depois da tradução: aba Ajustes → Calibração → 1 Torção do eixo X → Iniciar calibração → Aceitar nos 5 pontos → Salvar e reiniciar.
- **A instrução já enviada ao dono ("Tune → Axis Twist → Start Calibration") pula um passo.** Hoje, em inglês, o caminho é Tune → Calibration → Axis Twist → Start Calibration → Accept (5 vezes) → Save & Restart.
- O resto do menu Calibração, na ordem: 2 Altura do bico e malha da mesa, 3 Compensação de vibração, 4 Calibração da extrusora, 5 Correção de esquadro, 6 Autoajuste dos motores (opcional). Em OUTROS: Malha da mesa.

## 4. Glossário

Colunas: o texto em inglês como está no código, o texto em português para a tela e as observações (de onde vem o termo e, nos rótulos curtos, a largura medida).

### Ações e botões gerais

| Inglês (no código) | Português (na tela) | Observações |
|---|---|---|
| Back | Voltar | Igual ao Marlin e ao Orca. [30 px, fonte 10] |
| Cancel | Cancelar | Mainsail. [45 px, fonte 10] |
| Confirm | Confirmar | Mainsail. [52 px, fonte 10] |
| Continue | Continuar | [51 px, fonte 10] |
| OK | OK |  |
| Yes / No | Sim / Não | Mainsail. |
| Close | Fechar | Mainsail. [35 px, fonte 10] |
| Save | Salvar | Mainsail. [30 px, fonte 10] |
| Save & Restart / Save/Restart | Salvar e reiniciar | Mainsail ("Salvar e Reiniciar"; aqui só a 1ª maiúscula). Nunca usar "&". Em 1 linha mede 84 px: cabe nos botões largos (torção do eixo X). No botão de 62 px (Autoajuste dos motores), 2 linhas: "Salvar e`\n`reiniciar" (39 e 42 px). |
| Apply / Apply & Save | Aplicar / Aplicar e salvar | Mainsail e Orca. [36 px / 77 px, fonte 10] |
| Accept | Aceitar | Mainsail (Sonda manual, Parafusos da mesa). Na torção do eixo X o dono toca 5 vezes. [36 px, fonte 10] |
| Abort | Abortar | Mainsail, Orca e KlipperScreen. Calibration aborted = Calibração abortada. [38 px, fonte 10] |
| Start / Start Calibration | Iniciar / Iniciar calibração |  |
| Stop (parar uma ação) | Parar | Ex.: parar de carregar filamento. "Press Stop to halt." = "Toque em Parar para interromper." [27 px, fonte 10] |
| Stop (botão vermelho de parada de emergência) | Parar tudo | O botão faz parada de emergência (M112). "Emergência" mede 63 px e seria cortada no meio no botão de 62 px. A pergunta usa o termo completo: "Fazer parada de emergência?" [54 px, fonte 10] |
| Emergency stop | Parada de emergência | Mainsail e KlipperScreen; termo que o dono já viu. Prompt: "Do you want to emergency stop?" = "Fazer parada de emergência?" |
| Done | Concluir (no resumo da impressão: OK) | Fim de assistente (calibração da extrusora). Na torção do eixo X o botão com seta ← só fecha a tela sem salvar: lá usar "Voltar". No resumo de fim de impressão o botão só fecha o aviso: lá ficou "OK". [43 px, fonte 10] |
| Discard | Descartar | "Discard, Keep Previous" = "Descartar e manter o anterior"; "Discard Both, Keep Original" = "Descartar os dois e manter o original". [49 px, fonte 10] |
| Reset (painel Limites) | Restaurar | Volta o limite ao valor do printer.cfg. [49 px, fonte 10] |
| Reset (configurações, de forma geral) | Redefinir | Padrão Microsoft pt-BR. "Reset Options" = "Opções de redefinição". [47 px, fonte 10] |
| Reload | Recarregar | Spoolman e lista de arquivos. [56 px, fonte 10] |
| Refresh | Atualizar | [45 px, fonte 10] |
| Remove | Remover (no Wi-Fi: Esquecer) | [45 px, fonte 10]. No Wi-Fi o botão apaga a rede salva: ficou "Esquecer", com a pergunta "Esquecer a rede ...?" (mesmo termo de Forget). |
| Delete | Excluir | Mainsail. Não usar "Deletar". [35 px, fonte 10] |
| Clear | Limpar | Mainsail. [37 px, fonte 10] |
| Forget / Forget This Network | Esquecer / Esquecer esta rede | KlipperScreen. |
| Switch (trocar de impressora) | Trocar | [32 px, fonte 10] |
| Retry | Tentar de novo |  |
| Verify Again | Conferir de novo |  |
| Dismiss | Ignorar | Só esconde a oferta de retomar após queda de energia. [37 px, fonte 10] |
| Calibrate | Calibrar | Mainsail. [40 px, fonte 10] |
| Next | Avançar | Não aparece hoje no código; padrão Windows/Android pt-BR. [41 px, fonte 10] |
| Home (botão do aviso de origem) | Ir à origem | Com crase. [56 px, fonte 10] |
| Send | Enviar | Console. [33 px, fonte 10] |

### Telas, abas e menus

| Inglês (no código) | Português (na tela) | Observações |
|---|---|---|
| (aba com ícone de casa) | Início | A aba não tem texto; nome para usar nas instruções ao dono. |
| Macros | Macros | Mainsail. [37 px, fonte 10] |
| Console | Console | Mainsail. [40 px, fonte 10] |
| (aba "Tune", só ícone) | Ajustes | Sem texto na tela; nome para instruções. Tem: Malha da mesa, Ajuste fino, Limites, Medições TMC, Energia, Retração, Altura do bico, Calibração. |
| (aba com ícone de engrenagem) | Configurações | Sem texto na tela; nome para instruções. "Configurações" mede 74 px: não usar em botão de 62 px. |
| Homing (botão da tela inicial) | Mover | Abre a tela com origem, setas X/Y/Z, soltar motores e parada. Igual ao KlipperScreen. [31 px, fonte 10] |
| Extrude (botão da tela inicial) | Filamento | Abre a tela de carregar/descarregar/extrudar. Dentro dela, o botão Extrude é "Extrudar". [52 px, fonte 10] |
| Fans | Ventoinhas | [58 px, fonte 10] |
| LED | Luz | Marlin pt-BR. [18 px, fonte 10] |
| Print | Imprimir | Mainsail. [46 px, fonte 10] |
| Status | Status | Mainsail. [32 px, fonte 10] |
| Fine Tune | Ajuste fino | KlipperScreen. [55 px, fonte 10] |
| Limits | Limites | [38 px, fonte 10] |
| TMC Metrics | Medições TMC | Cabe em 1 linha na aba Ajustes (~90 px). [75 px, fonte 10] |
| Power Settings | Energia | Mainsail: "Dispositivos de energia". [40 px, fonte 10] |
| Retraction | Retração | Mainsail e Orca. [45 px, fonte 10] |
| Firmware Retraction | Retração pelo firmware | Fluidd. |
| Z Offset / Z-Offset | Altura do bico | Termo que o dono conhece. No Mainsail aparece "Z-Offset": em título pode ir "Altura do bico (Z-Offset)". A chave técnica z_offset fica. [72 px, fonte 10] |
| Calibration | Calibração | Mainsail e Orca (a Creality usa "Calibragem"). [54 px, fonte 10] |
| System | Sistema | [41 px, fonte 10] |
| Network | Rede | [26 px, fonte 10] |
| WIFI / wifi | Wi-Fi | Grafia oficial. [27 px, fonte 10] |
| Printers | Impressoras | No limite de 62 px. [62 px, fonte 10] |
| Objects | Objetos | [39 px, fonte 10] |
| Exclude Object / Exclude | Excluir objeto / Excluir | Mainsail e Orca. Confirmação: "Excluir \"{}\" da impressão?`\n`Isso não pode ser desfeito." |
| Favorites | Favoritos | Atenção: texto comparado no código (ver Cuidados). [45 px, fonte 10] |
| Recent | Recentes | Atenção: texto comparado no código. [47 px, fonte 10] |
| All Macros | Todas as macros |  |
| Commands / All commands | Comandos / Todos os comandos |  |
| Other / OTHER | Outros / OUTROS |  |
| IN ORDER | NESTA ORDEM | Cabeçalho do menu Calibração. |
| Recalibration Wizard | Assistente de recalibração | "Wizard" = "Assistente" (Windows pt-BR). |
| Reset Options | Opções de redefinição | Título. No botão de 62 px, 2 linhas como o original: "Opções de`\n`redefinição" (54 e 58 px). |
| Power Loss Recovery | Queda de energia | Título. "Resume Print" = "Retomar impressão". |

### Movimento e partes da impressora

| Inglês (no código) | Português (na tela) | Observações |
|---|---|---|
| Home / homing (ação) | ir à origem / levar à origem | "Homing..." = "Indo à origem...". O Mainsail deixou "Home all" em inglês. |
| Home All | Origem XYZ | 62 px; a célula da tela Mover tem ~82 px. [62 px, fonte 10] |
| Home XY | Origem XY | [55 px, fonte 10] |
| Homing required | Ir à origem primeiro | Título do aviso. "The printer must be homed first." = "Leve a impressora à origem antes." Botões: Ir à origem / Cancelar. |
| Must home axis first | Leve os eixos à origem primeiro | Mensagem da tela. O texto do Klipper continua comparado em inglês no código. |
| Motor Off | Soltar motores | Cabe na tela Mover (~82 px); "Desligar motores" (87 px) não cabe. Mainsail: "Motores off". [74 px, fonte 10] |
| X Axis / Y Axis / Z Axis | Eixo X / Eixo Y / Eixo Z |  |
| Move Distance (mm) | Distância (mm) |  |
| Move Speed (mm/s) | Velocidade (mm/s) |  |
| Invert Y Direction / Invert Z Direction | Inverter direção do Y / Inverter direção do Z | Mainsail. |
| Raise (farther from bed) | Subir (mais longe da mesa) | Botão da tela Altura do bico. |
| Lower (closer to bed) | Descer (mais perto da mesa) |  |
| Toolhead | cabeçote | Orca (o Mainsail diz "Ferramenta"). |
| Nozzle / Hotend | bico | "Heating the hotend..." = "Aquecendo o bico..." |
| Extruder (sensor de temperatura) | Bico | Nome do sensor na tela inicial (largura 78 px). [23 px, fonte 10] |
| Extruder (mecanismo que empurra o filamento) | extrusora | Mainsail e Orca, no feminino. |
| Bed | Mesa | Mainsail. [27 px, fonte 10] |
| Chamber | Câmara | Mainsail. Atenção: "Chamber" é comparado no código. [40 px, fonte 10] |
| MCU Temp | Placa-mãe | Temperatura da placa-mãe. [55 px, fonte 10] |
| Motion (categoria do console) | Movimento |  |
| Probe / probing (verbo) | medir / medindo | "Homing, wiping, and probing..." = "Indo à origem, limpando o bico e medindo..." |
| Probe (peça) | sensor de nivelamento | Na KE é o CR Touch. O Mainsail usa "sonda". |
| Load Sensor | sensor de pressão | Sensor que sente o bico encostar na mesa (KE). |
| Accelerometer | acelerômetro |  |
| Calipers / ruler / tape / mark | paquímetro / régua / fita crepe / marca | Ferramentas citadas nas calibrações. |
| Sheet of paper / drags | folha de papel / arrastar de leve | Mesmas palavras já ditas ao dono. |

### Temperatura

| Inglês (no código) | Português (na tela) | Observações |
|---|---|---|
| Heating / Heating... | Aquecendo / Aquecendo... | [59 px / 65 px, fonte 10] |
| Cooling | Esfriando | [49 px, fonte 10] |
| Preheat | Preaquecer | Mainsail; grafia sem hífen (Priberam). Não aparece hoje no código. [59 px, fonte 10] |
| Cooldown | Esfriar | Mainsail (painel de temperaturas) e Marlin. [33 px, fonte 10] |
| Target | Alvo | Mainsail. |
| Def. Temp | Temp. padrão | Temperatura do bico usada para carregar e descarregar filamento. |
| Heater | aquecedor |  |
| Temperature / Fans (categoria do console) | Temperatura / ventoinhas |  |

### Ventoinhas e luz

| Inglês (no código) | Português (na tela) | Observações |
|---|---|---|
| Fan | Ventoinha | Orca, Marlin, Creality e Fluidd. [53 px, fonte 10] |
| Part Cooling Fan | Ventoinha da peça | Orca: "Ventoinha de resfriamento de peças". Na KE é a fan0. |
| Hotend Fan | Ventoinha do bico | Na KE se chama nozzle_fan no printer.cfg. |
| Mainboard Fan | Ventoinha da placa-mãe |  |
| Controller Fan | Ventoinha da eletrônica |  |
| Toolhead Fan | Ventoinha da peça | Nome padrão do K1, gravado no guppyconfig.json para a fan0, que na KE é a ventoinha da peça. A tela traduz na hora de mostrar (fan_panel.cpp). |
| Back Fan / Side Fan | Ventoinha traseira / Ventoinha lateral | Termos da própria Creality em português. |
| Off (botão) | Desligar | Botão da barra da ventoinha e da luz. [42 px, fonte 10] |
| Max (botão) | Máximo | [42 px, fonte 10] |
| On / Off (estado) | Ligada / Desligada | No feminino (ventoinha, luz). [35 px / 51 px, fonte 10] |

### Filamento e carretel

| Inglês (no código) | Português (na tela) | Observações |
|---|---|---|
| Filament | Filamento | [52 px, fonte 10] |
| Load | Carregar | Mainsail ("Carregar filamento"). [44 px, fonte 10] |
| Unload | Descarregar | Mainsail. Exatamente no limite de 62 px; se quebrar, usar "Retirar" (34 px). [62 px, fonte 10] |
| Extrude (botão) | Extrudar | Fluidd e KlipperScreen; o Priberam registra "extrudar". O Mainsail mostra "Extrusão". [45 px, fonte 10] |
| Extruding... | Extrudando... |  |
| Retract | Retrair | Mainsail e Marlin. [34 px, fonte 10] |
| Purge / Purge More | Purgar / Purgar mais | Empurrar filamento até sair limpo (Orca: "purga"). "Purge More" é botão da janela do M600, que é larga. [35 px / 63 px, fonte 10] |
| Purge line | linha de limpeza | Termo já usado com o dono. |
| Filament runout | Fim do filamento | Título. Runout sensor = sensor de filamento. |
| Filament change | Troca de filamento | Orca. |
| Spool | carretel | Orca e Creality Print. |
| Remain`\n`Weight / Remain`\n`Length | Peso`\n`restante / Compr.`\n`restante | Cabeçalhos da tabela do Spoolman (fonte de 10 px). [42 px / 42 px, fonte 10] |
| MAT (coluna) | Tipo | PLA, PETG... |
| Show Archived | Mostrar arquivados |  |
| enough / not enough | suficiente / insuficiente | "Print needs {} g" = "A impressão usa {} g". |
| (active) | (ativo) | Carretel ativo. |
| Mark the Filament / Enter the Measurement | Marque o filamento / Digite a medida | Títulos da calibração da extrusora. |

### Calibração

| Inglês (no código) | Português (na tela) | Observações |
|---|---|---|
| Bed Mesh | Malha da mesa | Mainsail. Cabe em 1 linha na aba Ajustes. [79 px, fonte 10] |
| Bed Mesh / Leveling (categoria do console) | Malha / nivelamento |  |
| Bed leveling | Nivelamento da mesa | Orca. |
| Mesh | malha | "Mesh: 5x5" = "Malha: 5x5". |
| Save Profile | Salvar perfil | [60 px, fonte 10] |
| Clear Profile | Limpar malha | Faz BED_MESH_CLEAR (descarrega, não apaga). Mainsail: "Limpar malha da mesa". [73 px, fonte 10] |
| 3D View | Vista 3D | [42 px, fonte 10] |
| Profile | perfil |  |
| Algorithm / Tension | Algoritmo / Tensão | Dados da malha. |
| Axis Twist | Torção do eixo X | Termo já usado com o dono. Linha 1 do menu Calibração. [117 px, fonte 14] |
| Axis Twist Compensation | Compensação da torção do eixo X | Título (fonte de 20 px, cabe). [344 px, fonte 20] |
| Z-Offset & Bed Mesh | Altura do bico e malha da mesa | Linha 2 do menu Calibração. [229 px, fonte 14] |
| Input Shaper | Compensação de vibração | Creality Print pt-BR. O Orca diz "Modelagem de entrada", que não diz nada ao dono. [191 px, fonte 14] |
| Resonance | ressonância |  |
| Frequency Response | Resposta em frequência | "X Frequency Response" = "Resposta em frequência X". |
| E-Steps Calibration | Calibração da extrusora | No título pode ir "Calibração da extrusora (E-steps)". [169 px, fonte 14] |
| rotation_distance | rotation_distance | Nome do ajuste no Klipper; fica. |
| Skew Correction | Correção de esquadro | Termo já usado com o dono (esquadro). [161 px, fonte 14] |
| TMC Autotune (optional) | Autoajuste dos motores (opcional) | Linha 6 do menu Calibração. [250 px, fonte 14] |
| Tuning Goal | Objetivo do ajuste | As opções auto/silent/performance/autoswitch são gravadas no printer.cfg: ficam em inglês, a não ser que o código separe texto e valor. |
| Sensorless Threshold | Limiar sensorless | Ajusta quando o motor "sente" o fim do eixo na ida à origem sem chave de fim de curso. Não chamar de "sensibilidade": conforme o driver, número maior pode ser mais ou menos sensível. |
| Not Configured | Não configurado | Atenção: comparado no código. |
| Motor | Motor |  |
| Belts / Shake Belts | correias / Testar correias | [40 px / 73 px, fonte 10] |
| Excitate | Vibrar | [31 px, fonte 10] |
| Excite Frequency Control | Frequência de vibração |  |
| Paper test | teste do papel |  |
| Keep Paper Value / Use Sensor Value | Manter valor do papel / Usar valor do sensor |  |
| Refine with Load Sensor (optional) | Refinar com o sensor de pressão (opcional) |  |
| Calibration complete | Calibração concluída | Só no texto da tela; quando vem do Klipper, o código compara em inglês. |
| Step 1 of 2 / Point 1 of 5 | Etapa 1 de 2 / Ponto 1 de 5 |  |
| Ready to review / Result | Pronto para revisar / Resultado |  |

### Ajuste fino e limites

| Inglês (no código) | Português (na tela) | Observações |
|---|---|---|
| Speed | Velocidade | [57 px, fonte 10] |
| Speed+ / Speed- | Veloc.+ / Veloc.- | "Velocidade+" mede 63 px. [36 px / 34 px, fonte 10] |
| Speed Reset | Veloc. 100% | Volta a velocidade a 100% (M220 S100). "Velocidade 100%" (86 px) não cabe. [59 px, fonte 10] |
| Flow | Fluxo | Mainsail e Orca (o Marlin diz "Vazão"). [27 px, fonte 10] |
| Flow+ / Flow- | Fluxo+ / Fluxo- | [34 px / 32 px, fonte 10] |
| Flow Reset | Fluxo 100% | Volta o fluxo a 100% (M221 S100). [56 px, fonte 10] |
| Pressure Advance (PA) | Avanço de pressão (PA) | Mainsail e Orca deixam em inglês. PA+ e PA- ficam. |
| Reset PA | PA padrão | Volta ao valor do printer.cfg. [53 px, fonte 10] |
| Reset Z | Zerar Z | Zera o ajuste de altura feito na impressão (SET_GCODE_OFFSET Z=0). [36 px, fonte 10] |
| Z+ / Z- | Z+ / Z- | No Ajuste fino, Z+ afasta o bico da mesa. |
| Step (mm) | Passo (mm) | Quanto cada toque move. |
| Multipler Step (%) | Passo do ajuste (%) | O original tem erro de digitação ("Multipler"). |
| Velocity (mm/s) | Velocidade máx. (mm/s) |  |
| Acceleration (mm/s2) | Aceleração máx. (mm/s²) | As fontes novas (fonts_ptbr) têm o ²; o original escreve "mm/s2" porque a fonte antiga não tinha. |
| Acceleration to Deceleration (mm/s2) | Aceleração para desaceleração (mm/s²) | Mainsail: "Aceleração máx. para desacel." |
| Square Corner Velocity (mm/s) | Velocidade nos cantos (mm/s) | Mainsail: "Velocidade em canto". |
| Retract length / Retract speed | Compr. da retração / Velocidade da retração | No rótulo da tela Retração ficou "Compr. da retração" para caber em 1 linha (abreviação da lista fechada). |
| Unretract extra / Unretract speed | Extra ao voltar / Velocidade ao voltar | Mainsail: "Compr. Extra ao Voltar Retração". |

### Impressão e arquivos

| Inglês (no código) | Português (na tela) | Observações |
|---|---|---|
| Printing | Imprimindo | No limite de 62 px. [62 px, fonte 10] |
| Paused | Pausada | A impressão (feminino). [44 px, fonte 10] |
| Pause / Resume | Pausar / Retomar | Mainsail ("Pausar impressão" / "Retomar impressão"). [35 px / 44 px, fonte 10] |
| Cancel Print / Cancelling print... | Cancelar impressão / Cancelando a impressão... | Mainsail. |
| Print complete / Print cancelled | Impressão concluída / Impressão cancelada |  |
| Print Job (categoria do console) | Impressão |  |
| Layer | Camada | Mainsail. |
| ETA (tempo que falta) | Falta | Tempo restante ("Falta  1h 23min"). No código ficou "Falta" (singular, mais curto que "Faltam"); a medida da largura usa "Falta  88:88:88" (mini_print_status.cpp). [fonte 16] |
| Time | Tempo |  |
| Last print: | Última impressão: |  |
| Filament Weight / Print Time / Size / Modified | Peso do filamento / Tempo de impressão / Tamanho / Modificado em | Detalhes do arquivo. |
| Modified (botão de ordenar) | Data | Ordena a lista pela data. [24 px, fonte 10] |
| No metadata available | Sem informações do arquivo |  |
| object labels (opção do fatiador) | "Etiquetar objetos" | Nome da opção no OrcaSlicer pt-BR. |

### Estados vindos do Klipper

| Inglês (no código) | Português (na tela) | Observações |
|---|---|---|
| printing / paused / complete / cancelled / error / standby | Imprimindo / Pausada / Concluída / Cancelada / Erro / Em espera | Só para exibir. O valor em inglês continua nas comparações do código. |
| ready / startup / shutdown | Pronto / Iniciando / Parado | Estado do Klipper (masculino: o Klipper está pronto). "shutdown" é parada por erro, não "desligado". Em texto que cita o estado, deixar em inglês entre aspas, como o Mainsail mostra: "se aparecer 'shutdown'..." |

### Rede e Wi-Fi

| Inglês (no código) | Português (na tela) | Observações |
|---|---|---|
| KNOWN NETWORKS / OTHER NETWORKS IN RANGE | REDES SALVAS / OUTRAS REDES POR PERTO |  |
| Signal: Excellent / Good / Fair / Weak | Sinal: Excelente / Bom / Razoável / Fraco |  |
| Security: Open (unsecured) / Secured (WPA/WPA2) | Segurança: Aberta (sem senha) / Protegida (WPA/WPA2) |  |
| Static IP / Configure Static IP | IP fixo / Configurar IP fixo |  |
| Revert to DHCP | Voltar ao automático (DHCP) |  |
| Netmask / Gateway / subnet | Máscara de rede / Gateway / sub-rede | "sub-rede" com hífen. No rótulo curto da tela de IP fixo ficou só "Máscara" (falta espaço); nas mensagens de erro, "Máscara de rede". |
| IP address | Endereço IP |  |
| Password / Enter password for {} | Senha / Digite a senha de {} |  |
| Scanning for networks... | Procurando redes... |  |
| Connected / Connecting to {} ... | Conectado / Conectando a {}... |  |
| Low Latency: ON / OFF | Baixa latência: ativa / inativa | Wi-Fi mais estável, sem economia de energia. |

### Sistema e configurações

| Inglês (no código) | Português (na tela) | Observações |
|---|---|---|
| Restart`\n`Klipper | Reiniciar`\n`Klipper | Mainsail. [45 px, fonte 10] |
| Restart`\n`Firmware | Reiniciar`\n`firmware | Mainsail. [46 px, fonte 10] |
| Restart`\n`Guppy | Reiniciar`\n`tela | Guppy/GuppyScreen é o programa da tela. [45 px, fonte 10] |
| Update`\n`Guppy | Atualizar`\n`tela | Atualiza o programa da tela. [45 px, fonte 10] |
| Display Sleep | Apagar tela | Opções na lista: Nunca, 30 s, 1 min, 5 min, 10 min, 30 min, 1 h (formas curtas para caber na lista). |
| Brightness: Low / Dim / Medium / Bright / Max | Brilho: Mín. / Baixo / Médio / Alto / Máx. | Manter o % entre parênteses: "Mín. (10%)", "Máx. (100%)". Abreviado para caber na lista (abreviações da lista fechada). |
| Log Level | Nível de registro | Os valores trace/debug/info/warn/error ficam (são gravados). |
| Prompt Emergency Stop | Confirmar parada de emergência | KlipperScreen. |
| Theme Color: blue / red / green / purple / pink / yellow | Tema: Azul / Vermelho / Verde / Roxo / Rosa / Amarelo | Rótulo ficou "Tema": "Cor do tema" não cabe antes da lista. Só na exibição; o valor gravado e o nome do arquivo do tema ficam em inglês (a escolha é pela posição na lista). |
| Touch Beep | Som ao tocar |  |
| Reset GuppyScreen settings | Redefinir as configurações da tela |  |
| Factory Reset Printer | Restaurar a impressora de fábrica | Mainsail: "Reset de Fábrica". |
| Reset Touch Calibration | Refazer a calibração do toque |  |
| This cannot be undone. | Isso não pode ser desfeito. |  |
| Version | Versão |  |
| Printer Name / Moonraker IP Address / Moonraker Port | Nome da impressora / Endereço IP do Moonraker / Porta do Moonraker |  |
| Unavailable while printing | Indisponível durante a impressão |  |
| Waiting for printer to initialize... | Aguardando a impressora iniciar... |  |

### Console e macros

| Inglês (no código) | Português (na tela) | Observações |
|---|---|---|
| type gcode... / filter... | digite o G-code... / filtrar... | Texto de exemplo dentro do campo. |
| Probe / Calibration | Medição / calibração | Categoria do console. |
| Extruder / Filament | Extrusora / filamento | Categoria do console. |
| TMC / Drivers | TMC / drivers | Categoria do console. |
| Camera | Câmera | Categoria do console. |
| Query / Debug | Consulta / depuração | Categoria do console. |
| showing %d of %d  -  filter to narrow | Mostrando %d de %d: use o filtro para ver menos | Manter os dois %d na mesma ordem. |
| No favorites yet. / No recent commands yet. | Nenhum favorito ainda. / Nenhum comando recente. |  |

### Frases repetidas (usar igual em todos os arquivos)

| Inglês (no código) | Português (na tela) | Observações |
|---|---|---|
| Do you want to emergency stop? | Fazer parada de emergência? | 4 telas + a comparação em button_container.cpp:49 (as 5 precisam ficar iguais). |
| Cancel is queued and will run when heating finishes. | O cancelamento fica na fila até o aquecimento terminar. | 6 lugares. |
| Startup in progress — cancel is queued / and will run when startup finishes. | A impressão está começando. O cancelamento fica na fila / e acontece quando essa etapa terminar. | Dividida em duas frases: fica mais clara que com travessão. |
| Extruder heating: {:.0f}/{:.0f}°C / Bed heating: {:.0f}/{:.0f}°C | Aquecendo o bico: {:.0f}/{:.0f}°C / Aquecendo a mesa: {:.0f}/{:.0f}°C |  |
| Extruder {:.0f}/{:.0f}°C + Bed {:.0f}/{:.0f}°C | Bico {:.0f}/{:.0f}°C + mesa {:.0f}/{:.0f}°C |  |
| Homing and probing... | Indo à origem e medindo... | 3 lugares. |
| Lower until a sheet of paper just drags, then Accept. | Abaixe até o papel arrastar de leve e toque em Aceitar. | Mesmas palavras já ditas ao dono. |
| Mapping bed... this takes about a minute, no input needed. | Medindo a mesa... leva cerca de 1 minuto. Não precisa tocar em nada. |  |
| Saving... Klipper will restart. | Salvando... O Klipper vai reiniciar. |  |
| Can't calibrate while printing. / Can't recalibrate while printing. | Não é possível calibrar durante a impressão. / Não é possível recalibrar durante a impressão. |  |
| Load new filament, then Continue. | Carregue o filamento novo e toque em Continuar. | "Carregue" casa com o botão Carregar da tela Filamento. |
| Use this filament? / Use the same filament? | Usar este filamento? / Usar o mesmo filamento? |  |
| Stopping load... | Parando o carregamento... |  |
| {}, {:.0f} g left | {}, restam {:.0f} g |  |
| No interrupted print to recover. | Nenhuma impressão interrompida para retomar. |  |
| Please select your wifi network | Escolha sua rede Wi-Fi |  |
| Printer is printing.`\n`Run this macro anyway? (e Send this command / Adjust limits / Change power devices) | A impressora está imprimindo.`\n`Executar esta macro mesmo assim? (Enviar este comando / Ajustar os limites / Mudar os dispositivos de energia) |  |
| Bigger gap than normal (0.1mm) - paper test (green) is the safer pick. | Diferença maior que o normal (0.1 mm): o teste do papel (verde) é a escolha mais segura. |  |
| Close agreement - sensor reading (green) is usually the more precise pick. | Valores parecidos: a leitura do sensor (verde) costuma ser a mais precisa. |  |

## 5. Fontes consultadas

- Mainsail em português (`pt.json`), cruzado chave a chave com o inglês do Mainsail v2.19 (1.028 textos).
- KlipperScreen pt_BR: `ks_includes/locales/pt_BR/LC_MESSAGES/KlipperScreen.po` (GitHub, ramo master).
- OrcaSlicer pt-BR: `pt_BR.po` (9.850 textos).
- Creality Print pt-BR: `CrealityOfficial/CrealityPrint`, `localization/i18n/pt_BR/CrealityPrint_pt_BR.po` (5.376 textos).
- Tela original da Creality em português: textos do programa `display-server` do K1 (mesma família de tela da KE), do repositório `Guilouz/Creality-K1-Extracted-Firmwares`. Traz "Deslocamento Z", "Autonivelamento", "Ventoinha do modelo", mas também erros como "Tráfego" (Flow) e "rede de cama" (Bed mesh).
- Manual da Ender-3 V3 KE em português (V1.6): "bico", "mesa aquecida", "Pré-aqueça o bocal", "Calibração".
- Marlin pt-BR (`language_pt_br.h`): menus de tela pequena ("Esfriar", "Vazão", "Indo para origem", "PARADA DE EMERGÊNCIA").
- Fluidd pt-BR (`pt-BR.yaml`).
- Microsoft, Portuguese (Brazil) Localization Style Guide: abreviação como último recurso e com acento, "Não é possível" + infinitivo, "Falha ao" + infinitivo, palavras curtas do dia a dia, sem gerundismo.
- Proton, "Guidance for Portuguese, Brazilian": só a primeira letra maiúscula, tratamento por "você", poucos anglicismos, sem ponto final em botões.
- Dicionário Priberam: "preaquecer" (sem hífen) e "extrudir = extrudar".
- Fontes do LVGL no commit `74d0a816` (`lv_font_montserrat_10/12/14/16/20.c`), usadas nas medidas.
- Código do projeto (`src/`) e cópia local da configuração da impressora do dono.
