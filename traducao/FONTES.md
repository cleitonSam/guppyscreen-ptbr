# Acentos na tela: as fontes

## Em resumo

A tela da impressora passa a mostrar letras com acento (á, ã, â, é, ê, í, ó, õ, ô, ú, ç e as maiúsculas), além de º, ª, ², ³ e outros símbolos. O resto fica exatamente como está hoje: mesmo desenho de letra, mesmo espaçamento, nada muda de lugar na tela.

## Por que foi preciso

O programa da tela (GuppyScreen) desenha o texto com a letra Montserrat, que já vem pronta dentro do LVGL (a biblioteca que monta a tela). Essas fontes prontas só têm as letras do inglês, o `°` e o `•`. Quando falta uma letra, o LVGL desenha um quadradinho no lugar: "Calibração" sairia como "Calibra□□o". Isso já acontece hoje com o travessão do aviso "Startup in progress — cancel is queued".

## O que foi feito

1. **As mesmas fontes, com mais letras.** O script `scripts/gerar-fontes-ptbr.sh` gera de novo as Montserrat de 8, 10, 12, 14, 16 e 20 pixels:
   - com o **mesmo conversor** que o LVGL usou para as originais (`lv_font_conv`, versão 1.5.2, fixa);
   - a partir dos **mesmos arquivos de fonte** que estão dentro do próprio LVGL do projeto;
   - com as **mesmas opções** (4 bits por ponto, sem compressão) e os **mesmos ícones**.

   A única diferença é a lista de letras. Antes: só o inglês, `°` e `•`. Agora também:
   - todas as letras e símbolos do "Latin-1": `á à â ã ä é ê ë í î ó ô õ ö ú ü ç ñ`, as maiúsculas `Á À Â Ã É Ê Í Ó Ô Õ Ú Ç`, e `º ª ² ³ ± × ÷ µ « » ¿ ¡ © ®`;
   - e, por segurança, `– — ‘ ’ “ ” …`. O glossário pede para a tradução não usar esses sinais, mas, se algum escapar, ele aparece em vez de virar quadradinho.

2. **Mesmos nomes.** As fontes novas ficam na pasta `fonts_ptbr/`, com os mesmos nomes das originais (`lv_font_montserrat_10` etc.). No `lv_conf.h`, as originais desses 6 tamanhos foram desligadas e as novas, declaradas. Por isso **nenhum arquivo de `src/` precisou mudar**.

3. **Uma chave de liga e desliga.** No `lv_conf.h` existe a chave `GUPPY_FONTES_PTBR`: com `1` (o padrão), a tela usa as fontes com acento; com `0`, volta às originais.

4. **O texto sem acento fica idêntico.** O script compara cada fonte nova com a original, letra por letra e par de letras por par de letras (o ajuste de espaço entre pares, como em "AV", chamado *kerning*). Se algo sair diferente, ele avisa.

5. **Largura igual.** Na Montserrat, a letra com acento tem a mesma largura da letra sem acento: "Calibração" ocupa o mesmo espaço que "Calibracao". As medidas do glossário (`traducao/medir_largura.py`) continuam valendo.

## O cuidado com a altura das linhas

As maiúsculas com acento (`Á`, `É`, `Ô`...) são um pouco mais altas que as outras letras, e o `Ç`/`ç` desce um pouco mais. O conversor calcula a altura da linha pela letra mais alta e pela mais funda. Por isso, só de acrescentar os acentos, **cada linha de texto cresceria de 1 a 3 pixels**: pelas contas, nas fontes de 10, 14, 16 e 20. O script mostra o valor exato de cada uma. Na tela de 480x272 isso empurraria tudo para baixo. Textos poderiam quebrar linha ou ser cortados, problema que este projeto já teve antes (veja `docs/dev-notes.md`, "Font and layout tuning").

Por isso o script **mantém a altura da linha igual à das fontes originais**. O acento das maiúsculas e o rabinho do `ç` passam 1 ou 2 pixels da linha, e o LVGL desenha essa sobra normalmente: cada texto (rótulo) tem uma folga de desenho de 1/4 da altura da linha (2 a 5 pixels). O script mede a sobra de cada fonte e avisa se ela passar dessa folga.

Único caso em que a pontinha do acento ou do `ç` pode ser cortada (1 ou 2 pixels): os textos que **rolam sozinhos** quando não cabem, que são o nome do arquivo, os nomes de macros e de comandos e o título das janelas de aviso da impressora. Nesses, o LVGL corta o desenho na caixa exata do texto.

Para dar prioridade aos acentos inteiros, mesmo com a tela mudando alguns pixels de lugar, gere com `--altura-natural` (veja "Como gerar").

## Quanto o programa aumenta

| Fonte | A mais (estimativa) |
|---|---|
| Montserrat 8 | ~4,8 KB |
| Montserrat 10 | ~5,6 KB |
| Montserrat 12 | ~6,8 KB |
| Montserrat 14 | ~7,9 KB |
| Montserrat 16 | ~9,1 KB |
| Montserrat 20 | ~11,7 KB |
| **Total** | **~45 KB** |

- O programa da tela de hoje (v1.5.0-OpenKE, tela pequena) tem cerca de 6,2 MB, então são **+0,75%**. O script mostra o número exato ao gerar.
- Memória RAM: praticamente nada a mais. As fontes ficam guardadas dentro do programa e só são lidas quando a letra aparece.
- Ideia para compensar (não foi feita): a Montserrat 40 está ligada no `lv_conf.h` (`LV_FONT_MONTSERRAT_40 1`), mas nada no programa a usa. Desligá-la economiza cerca de 70 KB, mais do que os acentos custam.

## Como gerar as fontes (uma vez)

As fontes geradas (`fonts_ptbr/*.c`) **vão para o Git**, junto com o resto. Assim toda compilação usa exatamente as fontes que foram conferidas. Se a pasta não tiver os 6 arquivos, o `make` para logo no começo com a mensagem "Faltam as fontes com acentos em fonts_ptbr/". Se a montagem do GitHub (`.github/workflows/build.yml`) tiver o passo "fontes com acento", ela gera as fontes sozinha antes do `make` quando elas faltam.

**Jeito 1, no GitHub (não precisa de nada no computador):**
1. Envie o projeto para o GitHub com o arquivo `.github/workflows/fontes-ptbr.yml`. O botão só aparece quando esse arquivo está na branch principal (`main`). Para rodar em outra branch, use `--ref nome-da-branch` no comando do passo 2.
2. No GitHub, abra a aba **Actions**, escolha **gerar fontes pt-BR** e toque em **Run workflow**. Pela linha de comando: `gh workflow run fontes-ptbr.yml`.
3. Quando terminar (1 a 2 minutos), veja o número da execução com `gh run list --workflow fontes-ptbr.yml` e baixe o resultado com `gh run download NUMERO -n fonts_ptbr -D fonts_ptbr`. Confira que são 6 arquivos `.c` e salve no Git. O relatório do script fica no registro da execução.

**Jeito 2, no computador** (Git Bash no Windows, WSL ou Linux). Precisa de Node.js 12 ou mais novo e de Python 3.6 ou mais novo:
```
git submodule update --init lvgl
bash scripts/gerar-fontes-ptbr.sh
```
Opções:
- `--altura-natural`: deixa a altura da linha crescer (acentos sempre inteiros, mas a tela muda alguns pixels de lugar);
- `--so-conferir`: só confere as letras dos textos (veja abaixo), sem gerar nada;
- `--ajuda`: mostra as instruções.

O script faz, nesta ordem:
1. confere que os arquivos de fonte do LVGL são exatamente os da versão 8.3.11, pela impressão digital (SHA-256), e que a lista de ícones é a mesma do LVGL;
2. baixa o conversor `lv_font_conv` 1.5.2 do npm e confere a impressão digital do pacote (SHA-512). Se não bater, para sem mexer em nada;
3. gera as 6 fontes com o mesmo comando do LVGL (`lvgl/scripts/built_in_font/built_in_font_gen.py`), trocando só a lista de letras;
4. ajusta cada fonte (chave de liga e desliga e altura da linha), compara com a original e mostra um relatório;
5. só se tudo der certo, grava os 6 arquivos em `fonts_ptbr/`;
6. confere se as letras usadas nos textos de `src/` existem nas fontes.

Depois de gerar: compile do zero (`make clean` e depois `make`), porque o `lv_conf.h` mudou.

## Como conferir

- **Relatório do script**, para cada tamanho: quantas letras ficaram iguais às originais (devem ser todas), quantas são novas (102), se o espaçamento entre pares ficou igual, a altura da linha, quanto os acentos passam da linha e o tamanho.
- **Letras dos textos:** depois que a tradução de `src/` terminar, rode `bash scripts/gerar-fontes-ptbr.sh --so-conferir` (só precisa de Python). Ele lê todos os textos entre aspas de `src/` e avisa se algum usa uma letra que as fontes não têm, com arquivo e linha. Comentários do código são ignorados.
- **Na tela** (simulador ou impressora), olhe com atenção:
  - palavras com maiúscula acentuada ou Ç no começo, como "Ação", "Água", "É", "Ç";
  - textos com ç e ã, como "Calibração", "Extrusão", "Configuração";
  - um nome de arquivo com acento rolando na lista de arquivos;
  - a tela de rede Wi-Fi, que mostra nomes de rede com acento, se houver.

## Como desfazer

- **Só numa compilação, sem apagar nada:** `make clean` e depois `make GUPPY_FONTES_PTBR=0`.
- **De vez, sem apagar nada:** no `lv_conf.h`, troque `#define GUPPY_FONTES_PTBR 1` por `0` nos **dois** lugares (a parte da impressora e a parte do simulador). Deixe a pasta `fonts_ptbr/` onde está, porque com a chave em 0 ela vira arquivo vazio na compilação. Depois, `make clean` e `make`.
- **Tirar tudo:** apague `fonts_ptbr/`, `scripts/gerar-fontes-ptbr.sh`, `scripts/fontes_ptbr.py`, `.github/workflows/fontes-ptbr.yml` e este arquivo, e volte o `lv_conf.h` e o `Makefile` à versão anterior pelo Git.

Em qualquer caso, a tela volta a mostrar quadradinhos no lugar das letras acentuadas.

## Limites e cuidados

- **Letras fora da lista não aparecem** (viram quadradinho): emojis, setas como `→`, `≈`, `≠`, letras gregas e assim por diante. O `--so-conferir` avisa. Para incluir uma letra, acrescente o código dela em `FAIXAS_TEXTO`, no começo do script, e gere de novo.
- **Arquivos de `src/` em UTF-8.** O LVGL está configurado para ler texto em UTF-8 (`LV_TXT_ENC_UTF8`, nas duas partes do `lv_conf.h`). Basta salvar os arquivos em UTF-8, o padrão dos editores. O `--so-conferir` avisa se algum arquivo não estiver.
- **Maiúsculas e minúsculas:** texto com acento não pode passar por `KUtils::to_title()`, `toupper` ou `tolower`, porque essas funções estragam letras acentuadas (regra do glossário).
- **Teclado da tela:** o teclado do LVGL não tem teclas com acento. A tela mostra acentos, mas não dá para digitá-los. Isso só importa em senha de Wi-Fi com acento, o que é raro.
- **Bônus:** nomes de arquivo, nomes de macros e mensagens que vêm da impressora (Klipper) com acento também passam a aparecer certos.
- **Outras fontes do programa:** o console usa a DejaVu Sans Mono 14 (`assets/dejavusans_mono_14.c`), que já tinha acentos. Os ícones das abas usam `assets/materialdesign_font_40.c`, que não precisa de acentos. Nenhuma das duas foi mexida.
- **Temas:** os arquivos de tema (`themes/*.json` e `releases/guppyscreen/themes/*.json`) só definem cores. Nenhum cita fonte.
- **Se o LVGL mudar de versão** (submódulo), gere as fontes de novo. O script se recusa a gerar se os arquivos de fonte do LVGL não forem os esperados, e aí é preciso atualizar as impressões digitais e a versão no começo dele.

## Para quem for mexer no código

- **LVGL:** 8.3.11 (submódulo `lvgl` no commit `74d0a816a440eea53e030c4f1af842a94f7ce3d3`). As fontes embutidas foram regeradas pela última vez em 22/11/2021 (commit `004b7ddd65` do LVGL), com o `lv_font_conv` 1.5.2. A 1.5.3 (2024) também funcionaria, mas escreve `.fallback`/`.user_data` a mais e usa outra versão do FreeType. A 1.5.2 reproduz as letras de hoje sem mudança.
- **Comando** (igual ao `built_in_font_gen.py`, só mudando o `-r` da Montserrat):
  ```
  lv_font_conv --no-compress --no-prefilter --bpp 4 --size N
    --font Montserrat-Medium.ttf -r 0x20-0x7F,0xA0-0xFF,0x2013-0x2014,0x2018-0x2019,0x201C-0x201D,0x2022,0x2026
    --font FontAwesome5-Solid+Brands+Regular.woff -r <ícones do LVGL>
    --format lvgl -o lv_font_montserrat_N.c --force-fast-kern-format
  ```
- **Impressões digitais:** pacote `lv_font_conv-1.5.2.tgz` `sha512-0UapRSTkVP/pnB8Z4r2HDHx5p2dJx/xUG1+14u/WXo59mwuC7BahR+Bnx/66jKoDrG1wFQwn9ZzoyMxRHOD9bg==` (igual ao registro do npm); `Montserrat-Medium.ttf` SHA-256 `421f26b2…cbed53`; `FontAwesome5-Solid+Brands+Regular.woff` SHA-256 `f4e42f6c…da95f4`.
- **Ajustes depois da conversão** (`scripts/fontes_ptbr.py ajustar`):
  - a chave `LV_FONT_MONTSERRAT_N` de cada arquivo vira `GUPPY_FONTES_PTBR`. Sem isso, o próprio `lv_conf.h`, que desliga a embutida com `LV_FONT_MONTSERRAT_N = 0`, desligaria também a nova;
  - `line_height` e `base_line` voltam aos valores da fonte embutida, lidos do submódulo;
  - um cabeçalho explica a origem do arquivo.
- **Verificações:** desenho e medidas de cada letra original idênticos; kerning de todos os pares originais idêntico; `kern_scale` e sublinhado iguais; mapa de letras conferido. Kerning por classes: 61x49 hoje, cerca de 73x62 com os acentos. Isso foi calculado reproduzindo o opentype.js 1.3.3 do conversor, e o cálculo dá os mesmos 61x49 das fontes de hoje. A escala do kerning não muda.
- **`lv_conf.h`:** `GUPPY_FONTES_PTBR` (padrão 1); `LV_FONT_MONTSERRAT_8/10/12/14/16/20` = `(!GUPPY_FONTES_PTBR)`; `LV_FONT_CUSTOM_DECLARE` declara as 6 fontes quando a chave está ligada. Tudo isso nas duas partes do arquivo (impressora e simulador). A demo de música do LVGL, ligada no simulador, usa `lv_font_montserrat_12/16` pelo nome e continua achando as fontes pela declaração.
- **Configurações conferidas, sem mudança:** `LV_TXT_ENC = LV_TXT_ENC_UTF8`, `LV_FONT_FMT_TXT_LARGE = 1` (16 bytes por letra), `LV_USE_USER_DATA = 1`. A fonte padrão continua `lv_font_montserrat_10` na tela pequena e `_16` nas outras.
- **`Makefile`:** compila `fonts_ptbr/*.c`; `make GUPPY_FONTES_PTBR=0` desliga; os objetos de fonte dependem do `lv_conf.h`, para não juntar objetos velhos sem `make clean`; e há um aviso claro se faltarem os 6 arquivos (só na compilação principal, não em `make clean`, `make libhv.a` etc.).
- **Tamanhos de linha** (altura/base, iguais aos das originais): 8 → 10/2, 10 → 11/2, 12 → 15/3, 14 → 16/3, 16 → 18/3, 20 → 22/4. Folga de desenho do rótulo (altura/4): 2, 2, 3, 4, 4 e 5 px.

## Arquivos

| Arquivo | Para quê |
|---|---|
| `scripts/gerar-fontes-ptbr.sh` | gera, compara e grava as fontes; confere os textos |
| `scripts/fontes_ptbr.py` | ajudante do script (só Python padrão): impressões digitais, ajuste, comparação e conferência dos textos |
| `fonts_ptbr/lv_font_montserrat_{8,10,12,14,16,20}.c` | as fontes geradas (vão para o Git) |
| `lv_conf.h` | chave `GUPPY_FONTES_PTBR`, desliga as embutidas e declara as novas |
| `Makefile` | compila `fonts_ptbr/`, chave `GUPPY_FONTES_PTBR=0` e avisos |
| `.github/workflows/fontes-ptbr.yml` | gera as fontes no GitHub, quando acionado à mão |
| `traducao/FONTES.md` | este texto |
