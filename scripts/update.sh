#!/bin/sh
#
# update.sh - o botão de atualizar da tela (no original, "Update Guppy").
#
# Versão em português do Brasil: as atualizações vêm do GitHub do dono
# (cleitonSam/guppyscreen-ptbr), e não do OpenKE original (coreflake1), para a
# tela continuar em português depois de atualizar.
#
# Faz o mesmo que o update.sh do OpenKE (confere o espaço livre, só pega
# versões oficiais - a "teste" é ignorada -, preserva guppyconfig.json e
# miniaturas, recopia os scripts de apoio do Klipper e reinicia a tela) e mais:
#   - não atualiza com a impressora imprimindo ou pausada;
#   - confere o pacote (SHA-256 publicado junto e conteúdo) antes de mexer;
#   - guarda a instalação atual e, se a tela nova não subir, volta sozinho.
#
# Apertado na tela, as mensagens vão para
#   /usr/data/printer_data/logs/guppyscreen-update.log
# (dá para ver no Mainsail, em Máquina > logs). Também roda à mão:
#   sh /usr/data/guppyscreen/update.sh

REPO="cleitonSam/guppyscreen-ptbr"
DEFAULT_ASSET_NAME="guppyscreen-smallscreen.tar.gz"

GUPPY_DIR=$(cd "$(dirname "$0")" && pwd -P)
VERSION_FILE=$GUPPY_DIR/.version
CUSTOM_UPGRADE_SCRIPT=$GUPPY_DIR/custom_upgrade.sh
INIT_SCRIPT=/etc/init.d/S99guppyscreen
PREV_DIR=$GUPPY_DIR.antes-da-atualizacao
FAILED_DIR=$GUPPY_DIR.atualizacao-falhou
TMP_PKG=/tmp/guppyscreen.tar.gz
UPDATE_LOG=/usr/data/printer_data/logs/guppyscreen-update.log

# Mesmo programa de download (curl) do instalador do OpenKE: o wget da
# impressora não abre github.com (falha de TLS). Conferido pelo SHA-256.
CURL_SHA256=3a4d38da3b1a5e3cfff3aa471d5c28533ca204d1eac175690a629933fc47f630
CURL_SOURCES="https://raw.githubusercontent.com/$REPO/main/scripts/vendor/curl-mipsel
https://raw.githubusercontent.com/coreflake1/guppyscreen/4bf1ffa2e4ed473199805182c855dd61c1022735/scripts/vendor/curl-mipsel"

# A tela (processo pai) é parada no fim; isto aqui precisa continuar até o fim.
trap '' HUP

# Apertado na tela não há terminal para mostrar mensagens: vão para o log.
if [ ! -t 1 ] && [ -d "$(dirname "$UPDATE_LOG")" ]; then
    exec >>"$UPDATE_LOG" 2>&1
fi
echo "=== $(date '+%Y-%m-%d %H:%M:%S') atualização da tela ($REPO) ==="

sha256_of() { sha256sum "$1" 2>/dev/null | awk '{print $1}'; }

json_field() {   # $1 = arquivo JSON, $2 = campo do primeiro nível
    _val=""
    if command -v jq >/dev/null 2>&1; then
        _val=$(jq -r ".$2 // empty" "$1" 2>/dev/null)
    fi
    if [ -z "$_val" ]; then
        _val=$(sed -n "s/.*\"$2\"[[:space:]]*:[[:space:]]*\"\([^\"]*\)\".*/\1/p" "$1" 2>/dev/null | head -n 1)
    fi
    printf '%s' "$_val"
}

screen_pids() {   # processos rodando o programa da tela desta pasta
    _pids=""
    for _p in /proc/[0-9]*; do
        _exe=$(readlink "$_p/exe" 2>/dev/null) || continue
        [ "$_exe" = "$GUPPY_DIR/guppyscreen" ] && _pids="$_pids ${_p#/proc/}"
    done
    echo $_pids
}

screen_is_up() {   # rodando, e o MESMO processo 10 s depois (não está caindo)
    command -v readlink >/dev/null 2>&1 || { sleep 5; return 0; }
    sleep 5
    _first=$(screen_pids)
    [ -n "$_first" ] || return 1
    sleep 10
    [ "$(screen_pids)" = "$_first" ]
}

resync_klipper_helpers() {
    # A atualização pela tela não roda o instalador, então os scripts de apoio
    # (.cfg/.py) são recopiados aqui - mesma origem e destino do installer.sh.
    K1_CONFIG_DIR=/usr/data/printer_data/config
    mkdir -p "$K1_CONFIG_DIR/GuppyScreen/scripts"
    cp "$GUPPY_DIR"/scripts/*.cfg "$K1_CONFIG_DIR/GuppyScreen" 2>/dev/null
    cp "$GUPPY_DIR"/scripts/*.py "$K1_CONFIG_DIR/GuppyScreen/scripts" 2>/dev/null
}

cleanup_tmp() {
    rm -f "$TMP_PKG" "$TMP_PKG.sha256" /tmp/guppy-latest.json
}

# --- espaço livre (mesmos limites do installer.sh do OpenKE) -----------------
# Para antes de baixar/extrair qualquer coisa se alguma partição estiver quase
# cheia, em vez de arriscar deixar a instalação pela metade.
REMAINING_ROOT_DISK=$(df -m / | tail -1 | awk '{print $4}')
if [ "$REMAINING_ROOT_DISK" -lt 25 ]; then
    echo "CRÍTICO: pouco espaço em / ($(df -h / | tail -1 | awk '{print $4}') livres). Nada foi mudado."
    exit 1
fi

REMAINING_TMP_DISK=$(df -m /tmp | tail -1 | awk '{print $4}')
if [ "$REMAINING_TMP_DISK" -lt 25 ]; then
    echo "CRÍTICO: pouco espaço em /tmp ($(df -h /tmp | tail -1 | awk '{print $4}') livres). Nada foi mudado."
    exit 1
fi

REMAINING_DATA_DISK=$(df -m /usr/data | tail -1 | awk '{print $4}')
if [ "$REMAINING_DATA_DISK" -lt 1000 ]; then
    echo "CRÍTICO: pouco espaço em /usr/data ($(df -h /usr/data | tail -1 | awk '{print $4}') livres). Nada foi mudado."
    exit 1
fi

# --- não atualiza no meio de uma impressão -----------------------------------
PRINT_QUERY="http://127.0.0.1:7125/printer/objects/query?print_stats=state"
_state_json=""
if command -v curl >/dev/null 2>&1; then
    _state_json=$(curl -s -m 5 "$PRINT_QUERY" 2>/dev/null)
fi
[ -n "$_state_json" ] || _state_json=$(wget -q -O - "$PRINT_QUERY" 2>/dev/null)
PRINT_STATE=$(printf '%s' "$_state_json" | tr ',{}' '\n\n\n' \
    | sed -n 's/.*"state"[[:space:]]*:[[:space:]]*"\([a-z_]*\)".*/\1/p' | head -n 1)
case "$PRINT_STATE" in
    printing|paused)
        echo "A impressora está imprimindo (ou pausada): a tela não é atualizada agora. Tente quando terminar."
        exit 0 ;;
esac

# --- sem o serviço da tela, não daria para religá-la depois da troca --------
if grep -Fqs "ID=buildroot" /etc/os-release && [ ! -f "$INIT_SCRIPT" ]; then
    echo "Não achei $INIT_SCRIPT (o serviço que liga a tela). Nada foi mudado."
    exit 1
fi

# --- versão instalada --------------------------------------------------------
CURRENT_VERSION=""
ASSET_NAME=""
if [ -f "$VERSION_FILE" ]; then
    CURRENT_VERSION=$(json_field "$VERSION_FILE" version)
    ASSET_NAME=$(json_field "$VERSION_FILE" asset_name)
fi
if [ -z "$ASSET_NAME" ] || [ "$ASSET_NAME" = "null" ]; then
    ASSET_NAME=$DEFAULT_ASSET_NAME
fi
echo "Versão instalada: ${CURRENT_VERSION:-desconhecida}"

# --- programa de download ----------------------------------------------------
CURL=$(command -v curl)
if grep -Fqs "ID=buildroot" /etc/os-release; then
    if [ ! -x /tmp/curl ] || [ "$(sha256_of /tmp/curl)" != "$CURL_SHA256" ]; then
        for _src in $CURL_SOURCES; do
            rm -f /tmp/curl.new
            wget -q --no-check-certificate "$_src" -O /tmp/curl.new 2>/dev/null
            if [ "$(sha256_of /tmp/curl.new)" = "$CURL_SHA256" ]; then
                mv -f /tmp/curl.new /tmp/curl
                chmod 755 /tmp/curl
                break
            fi
        done
        rm -f /tmp/curl.new
    fi
    if [ -x /tmp/curl ] && [ "$(sha256_of /tmp/curl)" = "$CURL_SHA256" ]; then
        CURL=/tmp/curl
    fi
fi
if [ -z "$CURL" ]; then
    echo "Sem programa de download (curl). Nada foi mudado."
    exit 1
fi

# --- versão oficial mais nova --------------------------------------------------
# /releases/latest só considera versões oficiais: a "teste" (pré-lançamento)
# fica de fora, como no OpenKE.
rm -f /tmp/guppy-latest.json
$CURL -fsSL --connect-timeout 20 --retry 2 -H "User-Agent: guppyscreen-ptbr-update" \
    -H "Accept: application/vnd.github+json" \
    "https://api.github.com/repos/$REPO/releases/latest" -o /tmp/guppy-latest.json 2>/dev/null
latest_version=$(json_field /tmp/guppy-latest.json tag_name)
if [ -z "$latest_version" ] || [ "$latest_version" = "null" ]; then
    # API fora do ar ou limite de consultas: descobre pelo endereço da página.
    latest_version=$($CURL -sI --connect-timeout 20 "https://github.com/$REPO/releases/latest" 2>/dev/null \
        | tr -d '\r' | sed -n 's#^[Ll]ocation:.*/releases/tag/\([^/?]*\)$#\1#p' | head -n 1)
fi
case "$latest_version" in
    ""|null|*[!A-Za-z0-9._-]*)
        echo "Não consegui descobrir a versão mais nova; nada foi mudado."
        cleanup_tmp
        exit 0 ;;
esac
echo "Versão oficial mais nova: $latest_version"

if [ "$CURRENT_VERSION" = "$latest_version" ]; then
    echo "A versão $CURRENT_VERSION já é a mais nova."
    cleanup_tmp
    exit 0
fi
# Se a instalada for mais nova que a publicada, não "atualiza" para trás.
# (Se o sort da impressora não souber comparar versões, segue e atualiza.)
newest=$(printf '%s\n%s\n' "$CURRENT_VERSION" "$latest_version" | sort -V 2>/dev/null | tail -n 1)
if [ -n "$CURRENT_VERSION" ] && [ "$newest" = "$CURRENT_VERSION" ]; then
    echo "A versão instalada ($CURRENT_VERSION) é mais nova que a publicada ($latest_version); nada a fazer."
    cleanup_tmp
    exit 0
fi

# --- baixa e confere ---------------------------------------------------------
asset_url="https://github.com/$REPO/releases/download/$latest_version/$ASSET_NAME"
echo "Baixando $latest_version: $asset_url"
rm -f "$TMP_PKG" "$TMP_PKG.sha256"
$CURL -fsSL --connect-timeout 20 --retry 3 -o "$TMP_PKG" "$asset_url"
_curl_rc=$?
# Uma falha no meio (conexão caída...) pode deixar um arquivo parcial.
if [ "$_curl_rc" -ne 0 ] || [ ! -s "$TMP_PKG" ]; then
    echo "O download falhou ou veio vazio. Nada foi mudado."
    cleanup_tmp
    exit 1
fi
if $CURL -fsSL --connect-timeout 20 --retry 2 -o "$TMP_PKG.sha256" "$asset_url.sha256" 2>/dev/null \
        && [ -s "$TMP_PKG.sha256" ]; then
    _expected=$(awk '{print $1; exit}' "$TMP_PKG.sha256")
    _actual=$(sha256_of "$TMP_PKG")
    if [ "$_expected" != "$_actual" ]; then
        echo "O pacote chegou diferente do publicado (SHA-256 não bate). Nada foi mudado."
        cleanup_tmp
        exit 1
    fi
    echo "Pacote conferido (SHA-256 ok)."
else
    echo "Aviso: esta versão não tem arquivo de conferência (.sha256); conferindo só o conteúdo."
fi
if ! tar tf "$TMP_PKG" 2>/dev/null | grep -Eq '^(\./)?guppyscreen/guppyscreen$'; then
    echo "O pacote está corrompido ou não tem o programa da tela. Nada foi mudado."
    cleanup_tmp
    exit 1
fi

# --- guarda a instalação atual para poder voltar ------------------------------
rm -rf "$PREV_DIR" "$FAILED_DIR"
HAVE_PREV=0
if cp -a "$GUPPY_DIR" "$PREV_DIR"; then
    HAVE_PREV=1
else
    rm -rf "$PREV_DIR"
    echo "Aviso: não consegui guardar a versão atual; se a nova falhar, use o voltar-tela-original.sh."
fi

restore_previous() {
    [ "$HAVE_PREV" = 1 ] && [ -d "$PREV_DIR" ] || return 1
    rm -rf "$FAILED_DIR"
    mv "$GUPPY_DIR" "$FAILED_DIR" && mv "$PREV_DIR" "$GUPPY_DIR"
}

## substitui a instalação
# Uma extração anterior corrompida pode deixar uma destas pastas como arquivo
# comum, e o tar se recusa a sobrescrever ("File exists"). As quatro são
# conteúdo do pacote, recriado inteiro a cada vez - guppyconfig.json,
# miniaturas etc. NÃO são tocados.
rm -rf "$GUPPY_DIR/k1_mods" "$GUPPY_DIR/scripts" "$GUPPY_DIR/themes" "$GUPPY_DIR/debian" \
       "$GUPPY_DIR/instalar-tela-ptbr.sh" "$GUPPY_DIR/voltar-tela-original.sh"
if ! tar xf "$TMP_PKG" -C "$GUPPY_DIR/.."; then
    echo "Falhou ao extrair o pacote."
    if restore_previous; then
        echo "A instalação anterior foi reposta."
    fi
    cleanup_tmp
    exit 1
fi
chmod 755 "$GUPPY_DIR/guppyscreen" "$GUPPY_DIR/update.sh" 2>/dev/null
# Mantém em dia o script de volta à tela original (fica fora desta pasta).
if [ -f "$GUPPY_DIR/voltar-tela-original.sh" ]; then
    cp "$GUPPY_DIR/voltar-tela-original.sh" /usr/data/voltar-tela-original.sh 2>/dev/null \
        && chmod 755 /usr/data/voltar-tela-original.sh
fi

resync_klipper_helpers

if [ -f "$CUSTOM_UPGRADE_SCRIPT" ]; then
    echo "Rodando custom_upgrade.sh da versão $latest_version"
    "$CUSTOM_UPGRADE_SCRIPT"
fi

cleanup_tmp
echo "Tela atualizada para a versão $latest_version; reiniciando a tela."
if grep -Fqs "ID=buildroot" /etc/os-release
then
    [ -f "$INIT_SCRIPT" ] && "$INIT_SCRIPT" stop >/dev/null 2>&1
    killall -q guppyscreen
    "$INIT_SCRIPT" restart >/dev/null 2>&1
    if screen_is_up; then
        echo "A tela $latest_version subiu e está funcionando."
        rm -rf "$PREV_DIR"
    elif [ "$HAVE_PREV" = 1 ]; then
        echo "A versão $latest_version não ficou de pé; voltando para ${CURRENT_VERSION:-a anterior}."
        "$INIT_SCRIPT" stop >/dev/null 2>&1
        killall -q guppyscreen
        if restore_previous; then
            resync_klipper_helpers
            "$INIT_SCRIPT" start >/dev/null 2>&1
            if screen_is_up; then
                echo "A versão anterior voltou e está funcionando. A que falhou ficou em $FAILED_DIR."
            else
                echo "A versão anterior também não subiu: desligue e ligue a impressora, ou rode sh /usr/data/voltar-tela-original.sh"
            fi
        else
            "$INIT_SCRIPT" start >/dev/null 2>&1
            echo "Não consegui repor a versão anterior: rode sh /usr/data/voltar-tela-original.sh"
        fi
        exit 1
    else
        echo "A versão $latest_version não ficou de pé e não havia cópia para voltar: rode sh /usr/data/voltar-tela-original.sh"
        exit 1
    fi
fi

exit 0
