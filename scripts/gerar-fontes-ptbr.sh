#!/usr/bin/env bash
# =============================================================================
# Gera as fontes da tela com acentos (português do Brasil).
#
# Cria fonts_ptbr/lv_font_montserrat_{8,10,12,14,16,20}.c: as mesmas fontes que
# vêm embutidas no LVGL 8.3.11 (mesmo conversor, mesmas opções, mesmos ícones),
# agora com á à â ã é ê í ó ô õ ú ü ç º ª etc. Os nomes não mudam, então nenhum
# arquivo de src/ precisa mudar. Explicação completa: traducao/FONTES.md
#
# Precisa de: internet (só para baixar o conversor), Node.js 12 ou mais novo
# (com npm, ou curl/wget) e Python 3.6 ou mais novo. O submódulo lvgl precisa
# estar baixado:  git submodule update --init lvgl
#
# Uso, na pasta do projeto:
#   bash scripts/gerar-fontes-ptbr.sh                   gera, compara e confere
#   bash scripts/gerar-fontes-ptbr.sh --altura-natural  deixa a linha crescer
#                                                       1 a 3 px (mais folga
#                                                       para os acentos, mas
#                                                       as coisas na tela
#                                                       mudam de lugar)
#   bash scripts/gerar-fontes-ptbr.sh --so-conferir     só confere se as letras
#                                                       dos textos de src/
#                                                       existem nas fontes
#
# Sem internet: baixe antes o pacote do conversor e indique o arquivo em
#   LV_FONT_CONV_PACOTE=/caminho/lv_font_conv-1.5.2.tgz bash scripts/gerar-fontes-ptbr.sh
# =============================================================================
set -euo pipefail

# --- Conversor de fontes: versão FIXA -----------------------------------------
# A 1.5.2 é a mesma versão que gerou as fontes embutidas do LVGL 8.3 (o LVGL
# regerou src/font/*.c em 22/11/2021, quando a 1.5.2 era a versão atual). Assim
# as letras sem acento saem idênticas às de hoje. Impressão digital do pacote,
# conferida no registro do npm:
LV_FONT_CONV_VERSAO="1.5.2"
LV_FONT_CONV_SHA512="sha512-0UapRSTkVP/pnB8Z4r2HDHx5p2dJx/xUG1+14u/WXo59mwuC7BahR+Bnx/66jKoDrG1wFQwn9ZzoyMxRHOD9bg=="

# --- Arquivos de fonte: os do próprio submódulo lvgl (v8.3.11, commit 74d0a816)
MONTSERRAT_SHA256="421f26b23e2be6b98373d32acd3cb2897b154d4bf0a77d26534ce476e4cbed53"
FONTAWESOME_SHA256="f4e42f6cd69e5dbdcccc0f2f5be136cebde0e427641e45403bf9173a92da95f4"

TAMANHOS="8 10 12 14 16 20"

# --- Letras de texto ------------------------------------------------------------
# As embutidas têm só 0x20-0x7F,0xB0,0x2022 (ASCII, ° e •). Agora também:
#   0xA0-0xFF      acentos e símbolos do Latin-1 (á à â ã é ê í ó ô õ ú ü ç º ª × ...)
#   0x2013-0x2014  – —   (travessões; o "—" já aparece em textos de src/)
#   0x2018-0x2019  ‘ ’   (aspas simples e apóstrofo)
#   0x201C-0x201D  “ ”   (aspas)
#   0x2026         …     (reticências)
# Nenhuma delas é mais alta ou mais funda que as letras com acento.
FAIXAS_TEXTO="0x20-0x7F,0xA0-0xFF,0x2013-0x2014,0x2018-0x2019,0x201C-0x201D,0x2022,0x2026"

# --- Ícones (FontAwesome): exatamente a lista do LVGL ---------------------------
# Cópia de lvgl/scripts/built_in_font/built_in_font_gen.py (o script confere).
ICONES="61441,61448,61451,61452,61452,61453,61457,61459,61461,61465,61468,61473,61478,61479,61480,61502,61507,61512,61515,61516,61517,61521,61522,61523,61524,61543,61544,61550,61552,61553,61556,61559,61560,61561,61563,61587,61589,61636,61637,61639,61641,61664,61671,61674,61683,61724,61732,61787,61931,62016,62017,62018,62019,62020,62087,62099,62212,62189,62810,63426,63650"

# -----------------------------------------------------------------------------
RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AJUDANTE="$RAIZ/scripts/fontes_ptbr.py"
PASTA_FONTES_LVGL="$RAIZ/lvgl/scripts/built_in_font"
PASTA_EMBUTIDAS="$RAIZ/lvgl/src/font"
SAIDA="$RAIZ/fonts_ptbr"

falha() { echo; echo "ERRO: $*" >&2; exit 1; }
passo() { echo; echo "== $*"; }

ALTURA_NATURAL=0
SO_CONFERIR=0
for arg in "$@"; do
  case "$arg" in
    --altura-natural) ALTURA_NATURAL=1 ;;
    --so-conferir) SO_CONFERIR=1 ;;
    -h|--ajuda|--help) sed -n '3,26p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) falha "opção desconhecida: $arg (veja: bash scripts/gerar-fontes-ptbr.sh --ajuda)" ;;
  esac
done

# --- Python (o ajudante usa só a biblioteca padrão) ----------------------------
PY=()
for candidato in python3 python py; do
  command -v "$candidato" >/dev/null 2>&1 || continue
  if [ "$candidato" = py ]; then teste=(py -3); else teste=("$candidato"); fi
  if "${teste[@]}" -c 'import sys; sys.exit(0 if sys.version_info >= (3, 6) else 1)' >/dev/null 2>&1; then
    PY=("${teste[@]}")
    break
  fi
done
[ "${#PY[@]}" -gt 0 ] || falha "não achei o Python 3.6 ou mais novo (python3, python ou py)."
ajudante() { "${PY[@]}" -I "$AJUDANTE" "$@"; }
sha() { ajudante "$1" "$2" | tr -d '\r'; }

if [ "$SO_CONFERIR" = 1 ]; then
  if ajudante conferir --raiz "$RAIZ"; then exit 0; else exit 1; fi
fi

# --- Conferências antes de começar ---------------------------------------------
passo "Conferindo os arquivos do LVGL"
for arquivo in "$PASTA_FONTES_LVGL/Montserrat-Medium.ttf" \
               "$PASTA_FONTES_LVGL/FontAwesome5-Solid+Brands+Regular.woff" \
               "$PASTA_FONTES_LVGL/built_in_font_gen.py"; do
  [ -f "$arquivo" ] || falha "não achei $arquivo. Baixe o submódulo: git submodule update --init lvgl"
done
for n in $TAMANHOS; do
  [ -f "$PASTA_EMBUTIDAS/lv_font_montserrat_$n.c" ] || falha "não achei $PASTA_EMBUTIDAS/lv_font_montserrat_$n.c"
done
[ "$(sha sha256 "$PASTA_FONTES_LVGL/Montserrat-Medium.ttf")" = "$MONTSERRAT_SHA256" ] \
  || falha "Montserrat-Medium.ttf diferente da do LVGL 8.3.11. O submódulo lvgl mudou de versão?"
[ "$(sha sha256 "$PASTA_FONTES_LVGL/FontAwesome5-Solid+Brands+Regular.woff")" = "$FONTAWESOME_SHA256" ] \
  || falha "FontAwesome5-Solid+Brands+Regular.woff diferente da do LVGL 8.3.11. O submódulo lvgl mudou de versão?"
icones_lvgl="$(sed -n 's/^syms *= *"\([0-9,]*\)".*/\1/p' "$PASTA_FONTES_LVGL/built_in_font_gen.py" | tr -d '\r')"
[ "$icones_lvgl" = "$ICONES" ] || falha "a lista de ícones do LVGL mudou (built_in_font_gen.py). Atualize ICONES neste script."
echo "   ok: Montserrat, FontAwesome e lista de ícones iguais às do LVGL 8.3.11"

command -v node >/dev/null 2>&1 || falha "não achei o Node.js (precisa da versão 12 ou mais nova)."
node -e 'process.exit(parseInt(process.versions.node, 10) >= 12 ? 0 : 1)' \
  || falha "Node.js muito antigo ($(node --version)). Precisa da versão 12 ou mais nova."

TEMP="$(mktemp -d 2>/dev/null || mktemp -d -t fontesptbr)"
trap 'rm -rf "$TEMP"' EXIT

# --- Conversor: baixa, confere a impressão digital e abre ----------------------
passo "Preparando o lv_font_conv $LV_FONT_CONV_VERSAO"
PACOTE="$TEMP/lv_font_conv-$LV_FONT_CONV_VERSAO.tgz"
URL="https://registry.npmjs.org/lv_font_conv/-/lv_font_conv-$LV_FONT_CONV_VERSAO.tgz"
if [ -n "${LV_FONT_CONV_PACOTE:-}" ]; then
  cp "$LV_FONT_CONV_PACOTE" "$PACOTE" || falha "não consegui ler $LV_FONT_CONV_PACOTE"
elif command -v npm >/dev/null 2>&1 && (cd "$TEMP" && npm pack --silent "lv_font_conv@$LV_FONT_CONV_VERSAO" >/dev/null 2>&1) && [ -f "$PACOTE" ]; then
  :
elif command -v curl >/dev/null 2>&1; then
  curl -fsSL -o "$PACOTE" "$URL" || falha "não consegui baixar $URL"
elif command -v wget >/dev/null 2>&1; then
  wget -q -O "$PACOTE" "$URL" || falha "não consegui baixar $URL"
else
  falha "não consegui baixar o conversor (sem npm, curl ou wget). Veja LV_FONT_CONV_PACOTE no começo do script."
fi
obtido="$(sha sha512 "$PACOTE")"
[ "$obtido" = "$LV_FONT_CONV_SHA512" ] || falha "o pacote do lv_font_conv não confere. Esperado $LV_FONT_CONV_SHA512, veio $obtido."
ajudante extrair "$PACOTE" "$TEMP/conversor"
CONVERSOR="$TEMP/conversor/package/lv_font_conv.js"
[ -f "$CONVERSOR" ] || falha "pacote do lv_font_conv sem lv_font_conv.js"
# Só a saída normal entra na comparação: avisos do Node (stderr) não atrapalham.
versao_real="$(node "$CONVERSOR" --version 2>/dev/null | tr -d '\r' | tail -n 1)" \
  || falha "o lv_font_conv não rodou neste Node.js ($(node --version)): $(node "$CONVERSOR" --version 2>&1 | tail -n 3)"
[ "$versao_real" = "$LV_FONT_CONV_VERSAO" ] || falha "o conversor diz ser a versão $versao_real, esperava $LV_FONT_CONV_VERSAO"
echo "   ok: impressão digital confere ($LV_FONT_CONV_SHA512)"

# Os arquivos de fonte vão para uma pasta de trabalho, e o conversor roda lá com
# nomes curtos: o cabeçalho "Opts:" de cada fonte fica igual ao das embutidas,
# sem caminhos deste computador.
TRAB="$TEMP/trabalho"
PRONTAS="$TEMP/prontas"
mkdir -p "$TRAB" "$PRONTAS"
cp "$PASTA_FONTES_LVGL/Montserrat-Medium.ttf" "$PASTA_FONTES_LVGL/FontAwesome5-Solid+Brands+Regular.woff" "$TRAB/"

extra=()
[ "$ALTURA_NATURAL" = 1 ] && extra=(--altura-natural)

# --- Gera, ajusta e compara cada tamanho ----------------------------------------
passo "Gerando as fontes (pode levar um minuto)"
avisos=0
antes_total=0
depois_total=0
for n in $TAMANHOS; do
  saida_c="lv_font_montserrat_$n.c"
  # Mesmo comando do lvgl/scripts/built_in_font/built_in_font_gen.py, só com
  # FAIXAS_TEXTO no lugar de 0x20-0x7F,0xB0,0x2022.
  if ! (cd "$TRAB" && node "$CONVERSOR" --no-compress --no-prefilter --bpp 4 --size "$n" \
          --font Montserrat-Medium.ttf -r "$FAIXAS_TEXTO" \
          --font FontAwesome5-Solid+Brands+Regular.woff -r "$ICONES" \
          --format lvgl -o "$saida_c" --force-fast-kern-format) > "$TEMP/conversor_$n.log" 2>&1; then
    cat "$TEMP/conversor_$n.log" >&2
    falha "o lv_font_conv falhou no tamanho $n."
  fi
  if ! ajudante ajustar --tamanho "$n" --gerado "$TRAB/$saida_c" \
          --original "$PASTA_EMBUTIDAS/$saida_c" --saida "$PRONTAS/$saida_c" \
          --versao-conversor "$LV_FONT_CONV_VERSAO" ${extra[@]+"${extra[@]}"} > "$TEMP/relatorio_$n.txt"; then
    cat "$TEMP/relatorio_$n.txt"
    falha "a fonte de $n px não passou na conferência."
  fi
  grep -v '##DADOS' "$TEMP/relatorio_$n.txt" || true
  read -r _ _ antes depois avisos_n <<< "$(grep '##DADOS' "$TEMP/relatorio_$n.txt" | tr -d '\r')"
  antes_total=$((antes_total + antes))
  depois_total=$((depois_total + depois))
  avisos=$((avisos + avisos_n))
done

# --- Tudo certo: só agora troca os arquivos de fonts_ptbr/ ----------------------
mkdir -p "$SAIDA"
for n in $TAMANHOS; do
  cp "$PRONTAS/lv_font_montserrat_$n.c" "$SAIDA/lv_font_montserrat_$n.c"
done

passo "Conferindo as letras dos textos da tela (src/)"
if ! ajudante conferir --raiz "$RAIZ"; then
  avisos=$((avisos + 1))
fi

passo "Resumo"
echo "   Fontes gravadas em fonts_ptbr/ (${TAMANHOS// /, } px)."
awk -v a="$antes_total" -v d="$depois_total" 'BEGIN {
  printf "   Dados das fontes: %.1f KB -> %.1f KB (cerca de +%.0f KB no programa).\n", a/1024, d/1024, (d-a)/1024 }'
if [ "$ALTURA_NATURAL" = 1 ]; then
  echo "   Altura da linha: natural (a tela pode mudar alguns pixels de lugar)."
else
  echo "   Altura da linha: igual à das fontes originais (a tela fica no mesmo lugar)."
fi
if [ "$avisos" -gt 0 ]; then
  echo "   Terminou com $avisos aviso(s): leia as linhas com \"!\" acima."
fi
echo
echo "   Próximo passo: compilar do zero (make clean e depois make), porque o"
echo "   lv_conf.h e as fontes mudaram."
