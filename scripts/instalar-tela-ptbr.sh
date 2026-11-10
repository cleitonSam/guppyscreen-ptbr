#!/bin/sh
# =============================================================================
#  instalar-tela-ptbr.sh
#  Instala a tela em português do Brasil (a GuppyScreen do OpenKE, traduzida)
#  na Creality Ender-3 V3 KE.
#
#  Roda NA IMPRESSORA, como root, pelo terminal (SSH). Usa só o /bin/sh e os
#  comandos do BusyBox que já vêm na impressora.
#
#  Jeito mais fácil (copie e cole no terminal da impressora):
#    wget --no-check-certificate -qO /tmp/instalar-tela-ptbr.sh https://raw.githubusercontent.com/cleitonSam/guppyscreen-ptbr/main/scripts/instalar-tela-ptbr.sh && sh /tmp/instalar-tela-ptbr.sh
#
#  Escolhas:
#    sh instalar-tela-ptbr.sh                        versão oficial mais nova
#    sh instalar-tela-ptbr.sh v1.5.0-OpenKE-ptbr.1   uma versão específica
#    sh instalar-tela-ptbr.sh teste                  a versão de teste (não oficial)
#    sh instalar-tela-ptbr.sh /usr/data/guppyscreen-smallscreen.tar.gz
#                                                    um pacote já copiado para a impressora
#
#  O que ele faz, nesta ordem (para no primeiro problema, sem estragar nada):
#    1. confere se é a KE com o OpenKE e se a impressora está parada;
#    2. baixa o pacote do GitHub do dono e confere se chegou inteiro (SHA-256);
#    3. prepara a versão nova numa pasta separada, sem mexer na atual;
#    4. faz uma cópia de segurança completa de /usr/data/guppyscreen em
#       /usr/data/guppyscreen-backup-AAAAMMDD-HHMMSS;
#    5. troca a pasta mantendo as suas configurações (guppyconfig.json:
#       calibração do toque, sensores, brilho...) e reinicia SÓ a tela
#       (/etc/init.d/S99guppyscreen). Klipper e Moonraker continuam rodando;
#    6. confere se a tela nova subiu; se não subir, volta sozinho para a anterior.
#
#  Para voltar à tela original depois:   sh /usr/data/voltar-tela-original.sh
#  Registro de tudo que ele fez:         /usr/data/printer_data/logs/instalar-tela-ptbr.log
# =============================================================================

REPO="cleitonSam/guppyscreen-ptbr"
PACOTE="guppyscreen-smallscreen.tar.gz"

DADOS=/usr/data
GUPPY_DIR=$DADOS/guppyscreen
INIT=/etc/init.d/S99guppyscreen
CONFIG_KLIPPER=$DADOS/printer_data/config
LOG_TELA=$DADOS/printer_data/logs/guppyscreen.log
TRAB=$DADOS/.tela-ptbr-trabalho
DATA=$(date +%Y%m%d-%H%M%S)
BACKUP=$DADOS/guppyscreen-backup-$DATA
ANTIGO=$DADOS/guppyscreen.anterior-$DATA
FALHOU=$DADOS/guppyscreen-falhou-$DATA
# O relógio da KE começa em 01/03/2020 até acertar a hora pela internet: sem
# internet, duas execuções podem gerar o mesmo nome. Nunca reaproveita pasta.
while [ -e "$BACKUP" ] || [ -e "$ANTIGO" ] || [ -e "$FALHOU" ]; do
    DATA="$DATA-$$"
    BACKUP=$DADOS/guppyscreen-backup-$DATA
    ANTIGO=$DADOS/guppyscreen.anterior-$DATA
    FALHOU=$DADOS/guppyscreen-falhou-$DATA
done

# Programa de download (curl) usado pelo próprio instalador do OpenKE: o wget
# da impressora não consegue abrir github.com (falha de TLS), o curl consegue.
# Conferido pelo SHA-256 antes de usar, venha de onde vier.
CURL_SHA256=3a4d38da3b1a5e3cfff3aa471d5c28533ca204d1eac175690a629933fc47f630
CURL_FONTES="https://raw.githubusercontent.com/$REPO/main/scripts/vendor/curl-mipsel
https://raw.githubusercontent.com/coreflake1/guppyscreen/4bf1ffa2e4ed473199805182c855dd61c1022735/scripts/vendor/curl-mipsel"

umask 022

if [ -d "$DADOS/printer_data/logs" ]; then
    REGISTRO=$DADOS/printer_data/logs/instalar-tela-ptbr.log
else
    REGISTRO=$DADOS/instalar-tela-ptbr.log
fi

if [ -t 1 ]; then
    VERDE=$(printf '\033[1;32m'); AMARELO=$(printf '\033[1;33m')
    VERMELHO=$(printf '\033[1;31m'); NORMAL=$(printf '\033[0m')
else
    VERDE=""; AMARELO=""; VERMELHO=""; NORMAL=""
fi

# ----------------------------------------------------------------- mensagens --
registrar() { printf '%s %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*" >> "$REGISTRO" 2>/dev/null; return 0; }
msg()       { printf '%s\n' "$*"; registrar "$*"; }
ok()        { printf '%s  ok: %s%s\n' "$VERDE" "$*" "$NORMAL"; registrar "ok: $*"; }
aviso()     { printf '%sAVISO: %s%s\n' "$AMARELO" "$*" "$NORMAL"; registrar "AVISO: $*"; }
erro()      { printf '%sERRO: %s%s\n' "$VERMELHO" "$*" "$NORMAL"; registrar "ERRO: $*"; }
etapa()     { printf '\n%s== %s ==%s\n' "$VERDE" "$*" "$NORMAL"; registrar "== $* =="; }
desistir()  { erro "$*"; exit 1; }

uso() {
    cat <<EOF
Uso: sh $0 [versão | teste | /caminho/do/pacote.tar.gz]
  (sem nada)                         instala a versão oficial mais nova
  v1.5.0-OpenKE-ptbr.1               instala essa versão
  teste                              instala a versão de teste (não oficial)
  $DADOS/$PACOTE   instala um pacote já copiado para a impressora
EOF
}

# ------------------------------------------------------------------ ajudantes --
sha256_de() { sha256sum "$1" 2>/dev/null | awk '{print $1}'; }

iguais() {   # $1 e $2 têm o mesmo conteúdo?
    [ -f "$1" ] && [ -f "$2" ] || return 1
    if command -v cmp >/dev/null 2>&1; then
        cmp -s "$1" "$2"
    else
        [ "$(sha256_de "$1")" = "$(sha256_de "$2")" ]
    fi
}

tamanho_mb() { du -sk "$1" 2>/dev/null | awk '{print int($1 / 1024) + 1}'; }
livre_mb()   { df -k "$1" 2>/dev/null | tail -n 1 | awk '{print int($(NF-2) / 1024)}'; }

ler_versao() {   # $1 = arquivo .version
    _v=""
    if [ -f "$1" ]; then
        command -v jq >/dev/null 2>&1 && _v=$(jq -r '.version // empty' "$1" 2>/dev/null)
        [ -n "$_v" ] || _v=$(sed -n 's/.*"version"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "$1" 2>/dev/null | head -n 1)
    fi
    printf '%s' "${_v:-desconhecida}"
}

# Números dos processos rodando o programa da tela.
#   $1 = caminho exato do programa; vazio = qualquer "guppyscreen".
pids_tela() {
    _lista=""
    for _p in /proc/[0-9]*; do
        _exe=$(readlink "$_p/exe" 2>/dev/null) || continue
        _exe=${_exe% (deleted)}
        if [ -n "$1" ]; then
            [ "$_exe" = "$1" ] || continue
        else
            case "$_exe" in */guppyscreen) ;; *) continue ;; esac
        fi
        _lista="$_lista ${_p#/proc/}"
    done
    echo $_lista
}

parar_tela() {
    "$INIT" stop >/dev/null 2>&1
    killall -q guppyscreen 2>/dev/null
    _n=0
    while [ -n "$(pids_tela)" ] && [ "$_n" -lt 10 ]; do
        sleep 1
        _n=$((_n + 1))
    done
    if [ -n "$(pids_tela)" ]; then
        killall -q -9 guppyscreen 2>/dev/null
        sleep 1
    fi
    acender_luz
}

# A tela parada dormindo deixa a luz em 0 e a nova subia apagada (o toque
# continuava valendo: em 10/11 o dono ligou o pre-aquecimento as cegas).
acender_luz() {
    for _d in /sys/class/backlight/*; do
        if [ -w "$_d/brightness" ] && [ -r "$_d/max_brightness" ]; then
            cat "$_d/max_brightness" > "$_d/brightness" 2>/dev/null
        fi
    done
}

iniciar_tela() {
    "$INIT" start >/dev/null 2>&1
}

# A tela "subiu" se o programa da pasta certa está rodando e continua sendo o
# MESMO processo 10 segundos depois (se ele cair, o supervise-daemon do OpenKE
# reinicia e o número muda; depois de 10 quedas ele desiste).
tela_subiu() {
    _alvo=$(cd "$GUPPY_DIR" 2>/dev/null && pwd -P)/guppyscreen
    sleep 5
    _p1=$(pids_tela "$_alvo")
    [ -n "$_p1" ] || return 1
    sleep 10
    _p2=$(pids_tela "$_alvo")
    [ -n "$_p2" ] && [ "$_p1" = "$_p2" ]
}

estado_impressao() {
    _url="http://127.0.0.1:7125/printer/objects/query?print_stats=state"
    _resp=""
    if command -v curl >/dev/null 2>&1; then
        _resp=$(curl -s -m 5 "$_url" 2>/dev/null)
    fi
    if [ -z "$_resp" ]; then
        _resp=$(wget -q -O - "$_url" 2>/dev/null)
    fi
    printf '%s' "$_resp" | tr ',{}' '\n\n\n' \
        | sed -n 's/.*"state"[[:space:]]*:[[:space:]]*"\([a-z_]*\)".*/\1/p' | head -n 1
}

# Confere pelo cabeçalho se o programa é ELF 32 bits, little-endian, MIPS.
conferir_programa() {
    _hex=$(dd if="$1" bs=20 count=1 2>/dev/null | od -An -tx1 2>/dev/null | tr -d ' \n')
    case "$_hex" in
        "")
            aviso "Não deu para conferir o tipo do programa (falta o comando od); seguindo." ;;
        7f454c460101*)
            if [ "$(printf '%s' "$_hex" | cut -c37-40)" = "0800" ]; then
                ok "O programa novo é para MIPS (serve na KE)"
            else
                desistir "O programa do pacote não é para MIPS: não serve nesta impressora. Nada foi mudado."
            fi ;;
        *)
            desistir "O programa do pacote não é para esta impressora (não é MIPS 32 bits). Nada foi mudado." ;;
    esac
}

preparar_curl() {
    CURL=""
    if [ -x /tmp/curl ] && [ "$(sha256_de /tmp/curl)" = "$CURL_SHA256" ] && /tmp/curl --version >/dev/null 2>&1; then
        CURL=/tmp/curl
        return 0
    fi
    for _u in $CURL_FONTES; do
        rm -f /tmp/curl.novo
        wget -q --no-check-certificate "$_u" -O /tmp/curl.novo 2>/dev/null
        if [ "$(sha256_de /tmp/curl.novo)" = "$CURL_SHA256" ]; then
            mv -f /tmp/curl.novo /tmp/curl
            chmod 755 /tmp/curl
            if /tmp/curl --version >/dev/null 2>&1; then
                CURL=/tmp/curl
                return 0
            fi
        fi
    done
    rm -f /tmp/curl.novo
    # Último recurso: o curl do sistema, se ele souber falar https.
    if command -v curl >/dev/null 2>&1 && curl --version 2>/dev/null | grep -qi 'https'; then
        CURL=$(command -v curl)
        return 0
    fi
    return 1
}

baixar() {   # $1 = endereço, $2 = arquivo de destino
    rm -f "$2"
    "$CURL" -fsSL --connect-timeout 20 --retry 3 --retry-delay 3 \
        -H "User-Agent: instalar-tela-ptbr" -o "$2" "$1" 2>/dev/null && [ -s "$2" ]
}

# Guarda, dentro da cópia de segurança, os arquivos de apoio do Klipper que o
# pacote novo pode trocar (os mesmos que o botão de atualizar recopia:
# GuppyScreen/*.cfg e GuppyScreen/scripts/*.py), para o voltar-tela-original.sh
# poder repor exatamente como estavam.
guardar_config_klipper() {   # $1 = pasta da cópia, $2 = pasta da versão nova
    _cfgdir=$CONFIG_KLIPPER/GuppyScreen
    [ -d "$_cfgdir" ] || return 0
    _dst=$1/.config-klipper
    mkdir -p "$_dst/scripts" || return 1
    for _src in "$2"/scripts/*.cfg; do
        [ -f "$_src" ] || continue
        _nome=$(basename "$_src")
        if [ -f "$_cfgdir/$_nome" ]; then
            cp -a "$_cfgdir/$_nome" "$_dst/$_nome" || return 1
        else
            echo "$_nome" >> "$_dst/novos.txt"
        fi
    done
    for _src in "$2"/scripts/*.py; do
        [ -f "$_src" ] || continue
        _nome=$(basename "$_src")
        if [ -f "$_cfgdir/scripts/$_nome" ]; then
            cp -a "$_cfgdir/scripts/$_nome" "$_dst/scripts/$_nome" || return 1
        else
            echo "scripts/$_nome" >> "$_dst/novos.txt"
        fi
    done
    return 0
}

# Mesma recópia que o botão de atualizar (update.sh) faz, mas só do que mudou.
sincronizar_config_klipper() {
    _cfgdir=$CONFIG_KLIPPER/GuppyScreen
    if [ ! -d "$_cfgdir" ]; then
        aviso "Não achei $_cfgdir; pulei os arquivos de apoio do Klipper."
        return 0
    fi
    mkdir -p "$_cfgdir/scripts"
    _cfg_mudou=0
    _py_mudou=0
    for _src in "$GUPPY_DIR"/scripts/*.cfg; do
        [ -f "$_src" ] || continue
        _dst=$_cfgdir/$(basename "$_src")
        iguais "$_src" "$_dst" && continue
        cp "$_src" "$_dst" && _cfg_mudou=1 && msg "  atualizado: $_dst"
    done
    for _src in "$GUPPY_DIR"/scripts/*.py; do
        [ -f "$_src" ] || continue
        _dst=$_cfgdir/scripts/$(basename "$_src")
        iguais "$_src" "$_dst" && continue
        cp "$_src" "$_dst" && _py_mudou=1 && msg "  atualizado: $_dst"
    done
    if [ "$_cfg_mudou" = 1 ]; then
        aviso "Mudaram atalhos (macros) do Klipper usados pela tela. Eles só valem depois de reiniciar o Klipper: faça isso com a impressora parada (Mainsail > Reiniciar)."
    elif [ "$_py_mudou" = 0 ]; then
        ok "Arquivos de apoio do Klipper já estavam iguais (nada mudou no Klipper)"
    fi
}

instalar_script_de_volta() {
    _dst=$DADOS/voltar-tela-original.sh
    if [ -f "$GUPPY_DIR/voltar-tela-original.sh" ]; then
        cp "$GUPPY_DIR/voltar-tela-original.sh" "$_dst" && chmod 755 "$_dst" && return 0
    fi
    rm -f "$_dst.novo"
    if wget -q --no-check-certificate -O "$_dst.novo" \
            "https://raw.githubusercontent.com/$REPO/main/scripts/voltar-tela-original.sh" 2>/dev/null \
        && head -n 1 "$_dst.novo" | grep -q '^#!/bin/sh'; then
        mv -f "$_dst.novo" "$_dst" && chmod 755 "$_dst" && return 0
    fi
    rm -f "$_dst.novo"
    return 1
}

listar_copias() {
    _qtd=0
    for _d in "$DADOS"/guppyscreen-backup-*; do
        [ -f "$_d/guppyscreen" ] || continue
        _qtd=$((_qtd + 1))
        msg "    $_d  (versão $(ler_versao "$_d/.version"), $(tamanho_mb "$_d") MB)"
    done
    if [ "$_qtd" -gt 3 ]; then
        msg "  Você tem $_qtd cópias guardadas. Pode apagar as que não precisar com"
        msg "  rm -rf /usr/data/guppyscreen-backup-AAAAMMDD-HHMMSS  (guarde a mais antiga: é a original)."
    fi
}

voltar_para_anterior() {
    erro "A tela nova não ficou de pé. Voltando sozinho para a anterior..."
    if [ -f "$LOG_TELA" ]; then
        msg "Últimas linhas do registro da tela ($LOG_TELA):"
        tail -n 15 "$LOG_TELA" 2>/dev/null | while IFS= read -r _linha; do msg "    $_linha"; done
    fi
    parar_tela
    mv "$GUPPY_DIR" "$FALHOU" 2>/dev/null
    if [ -d "$ANTIGO" ]; then
        mv "$ANTIGO" "$GUPPY_DIR"
    else
        cp -a "$BACKUP" "$GUPPY_DIR"
        rm -rf "$GUPPY_DIR/.config-klipper"
    fi
    sync
    iniciar_tela
    if tela_subiu; then
        msg "A tela anterior ($VERSAO_ATUAL) voltou e está funcionando. Nada foi perdido."
    else
        erro "A tela anterior também não subiu. Desligue e ligue a impressora."
        erro "Se continuar sem tela, rode: sh $DADOS/voltar-tela-original.sh $BACKUP"
    fi
    msg "A versão que falhou ficou em $FALHOU (pode apagar depois)."
    msg "Registro desta tentativa: $REGISTRO"
}

# --------------------------------------------- limpeza se algo interromper ---
ESTADO=inicio
ao_sair() {
    case "$ESTADO" in
        trocando)
            # Interrompido no meio da troca: garante que exista uma tela no lugar.
            if [ ! -d "$GUPPY_DIR" ]; then
                if [ -d "$ANTIGO" ]; then
                    mv "$ANTIGO" "$GUPPY_DIR"
                elif [ -d "$BACKUP" ]; then
                    cp -a "$BACKUP" "$GUPPY_DIR"
                    rm -rf "$GUPPY_DIR/.config-klipper"
                fi
            fi
            "$INIT" start >/dev/null 2>&1
            ;;
        conferindo)
            aviso "Interrompido durante a conferência: a tela nova ficou instalada."
            aviso "A anterior ficou em $ANTIGO e a cópia completa em $BACKUP."
            ;;
    esac
    rm -rf "$TRAB"
}
trap ao_sair EXIT
trap 'erro "Interrompido."; exit 130' INT TERM
# Se a conexão SSH cair no meio, continua até o fim (e registra tudo no log).
trap '' HUP

# =============================================================================
ALVO=${1:-}
case "$ALVO" in
    -h|--help|--ajuda|ajuda) uso; exit 0 ;;
esac

registrar "---------------------------------------------------------------"
registrar "instalar-tela-ptbr.sh (repositório $REPO) - pedido: ${ALVO:-versão oficial mais nova}"

# --------------------------------------------------------- 1. conferências ---
etapa "1/6 Conferindo a impressora"
[ "$(id -u)" = "0" ] || desistir "Rode como root (o usuário administrador da impressora)."
case "$(uname -m)" in
    mips*) ;;
    *) desistir "Este instalador roda na impressora (Ender-3 V3 KE), não no computador." ;;
esac
for _cmd in tar sha256sum awk sed grep cp mv rm mkdir du df dd tr cut head tail readlink killall sleep date wget; do
    command -v "$_cmd" >/dev/null 2>&1 || desistir "Falta o comando '$_cmd' na impressora."
done
[ -L "$GUPPY_DIR" ] && desistir "$GUPPY_DIR é um atalho (link): instalação fora do padrão, não vou mexer."
[ -f "$GUPPY_DIR/guppyscreen" ] || desistir "Não achei a tela do OpenKE em $GUPPY_DIR. Instale o OpenKE primeiro."
[ -f "$INIT" ] || desistir "Não achei o serviço da tela ($INIT)."
if [ -r /sys/class/graphics/fb0/virtual_size ]; then
    _res=$(cat /sys/class/graphics/fb0/virtual_size)
    _x=${_res%,*}
    _y=${_res#*,}
    if [ "$_x" -ge 800 ] 2>/dev/null || [ "$_y" -ge 800 ] 2>/dev/null; then
        desistir "Esta tela tem ${_x}x${_y}; o pacote é para a tela pequena (480x272) da KE."
    fi
fi
VERSAO_ATUAL=$(ler_versao "$GUPPY_DIR/.version")
ok "Ender-3 V3 KE com a tela do OpenKE (versão atual: $VERSAO_ATUAL)"

ESTADO_IMP=$(estado_impressao)
case "$ESTADO_IMP" in
    printing|paused)
        desistir "A impressora está imprimindo (ou pausada). Rode de novo quando a impressão terminar. Nada foi mudado." ;;
    standby|complete|cancelled|error)
        ok "Impressora parada (estado: $ESTADO_IMP)" ;;
    *)
        aviso "Não consegui confirmar se a impressora está parada (o Moonraker não respondeu)."
        if [ -t 0 ]; then
            printf 'A impressora está PARADA, sem imprimir? Digite s e Enter para continuar: '
            read -r _resp
            case "$_resp" in
                s|S|sim|Sim|SIM) registrar "dono confirmou que a impressora está parada" ;;
                *) desistir "Cancelado. Nada foi mudado." ;;
            esac
        else
            desistir "Sem como confirmar que a impressora está parada. Nada foi mudado."
        fi ;;
esac

TAM_MB=$(tamanho_mb "$GUPPY_DIR")
PRECISA_MB=$((${TAM_MB:-50} * 3 + 150))
LIVRE_MB=$(livre_mb "$DADOS")
[ "${LIVRE_MB:-0}" -ge "$PRECISA_MB" ] || desistir "Pouco espaço livre em $DADOS: ${LIVRE_MB:-?} MB livres, preciso de $PRECISA_MB MB. Apague arquivos antigos (gcodes, cópias guppyscreen-backup-*) e tente de novo."
_tmp_mb=$(livre_mb /tmp)
[ "${_tmp_mb:-0}" -ge 5 ] || desistir "Pouco espaço livre em /tmp (${_tmp_mb:-?} MB)."
ok "Espaço livre: $LIVRE_MB MB (preciso de $PRECISA_MB MB)"

# ------------------------------------------------------------- 2. pacote -----
etapa "2/6 Pegando o pacote da tela em português"
rm -rf "$TRAB"
mkdir -p "$TRAB" || desistir "Não consegui criar $TRAB."
ARQ=$TRAB/$PACOTE

if [ -n "$ALVO" ] && [ -f "$ALVO" ]; then
    cp "$ALVO" "$ARQ" || desistir "Não consegui ler $ALVO."
    [ -f "$ALVO.sha256" ] && cp "$ALVO.sha256" "$ARQ.sha256"
    ok "Usando o pacote $ALVO"
else
    case "$ALVO" in
        "")
            BASE_URL="https://github.com/$REPO/releases/latest/download"
            DESCRICAO="versão oficial mais nova" ;;
        teste)
            BASE_URL="https://github.com/$REPO/releases/download/teste"
            DESCRICAO="versão de teste" ;;
        v[0-9]*)
            case "$ALVO" in *[!A-Za-z0-9._-]*) desistir "Nome de versão inválido: $ALVO" ;; esac
            BASE_URL="https://github.com/$REPO/releases/download/$ALVO"
            DESCRICAO="versão $ALVO" ;;
        *)
            uso
            desistir "Não entendi '$ALVO': não é uma versão nem um arquivo que exista na impressora." ;;
    esac
    preparar_curl || desistir "Não consegui preparar o programa de download (curl). Confira a internet da impressora."
    msg "Baixando a $DESCRICAO de github.com/$REPO ..."
    if ! baixar "$BASE_URL/$PACOTE" "$ARQ"; then
        if [ -z "$ALVO" ]; then
            desistir "Não consegui baixar a versão oficial. Confira a internet da impressora. Se ainda não existe versão oficial publicada, experimente a de teste: sh $0 teste"
        fi
        desistir "Não consegui baixar $BASE_URL/$PACOTE. Confira a internet e o nome da versão."
    fi
    baixar "$BASE_URL/$PACOTE.sha256" "$ARQ.sha256" || rm -f "$ARQ.sha256"
    ok "Pacote baixado ($(tamanho_mb "$ARQ") MB)"
fi

if [ -s "$ARQ.sha256" ]; then
    _esperado=$(awk '{print $1; exit}' "$ARQ.sha256")
    _obtido=$(sha256_de "$ARQ")
    [ "$_esperado" = "$_obtido" ] || desistir "O pacote chegou diferente do publicado (a conferência SHA-256 não bate). Tente de novo. Nada foi mudado."
    ok "Pacote conferido (SHA-256 $_obtido)"
else
    aviso "Sem o arquivo de conferência (.sha256); vou conferir só o conteúdo do pacote."
fi

tar tf "$ARQ" > "$TRAB/lista.txt" 2>/dev/null || desistir "O pacote está corrompido (não abre). Nada foi mudado."
grep -Eq '^(\./)?guppyscreen/guppyscreen$' "$TRAB/lista.txt" || desistir "O pacote não tem o programa da tela. Nada foi mudado."
grep -Eq '^(\./)?guppyscreen/\.version$' "$TRAB/lista.txt" || desistir "O pacote não tem o arquivo .version. Nada foi mudado."
if grep -Eq '^/|(^|/)\.\.(/|$)' "$TRAB/lista.txt"; then
    desistir "O pacote tem caminhos estranhos (fora da pasta). Nada foi mudado."
fi
if grep -Ev '^(\./)?$|^(\./)?guppyscreen(/|$)' "$TRAB/lista.txt" | grep -q .; then
    desistir "O pacote tem arquivos fora da pasta guppyscreen. Nada foi mudado."
fi

# ------------------------------------------------------- 3. versão nova ------
etapa "3/6 Preparando a versão nova (a atual continua funcionando)"
MONTAGEM=$TRAB/montagem
NOVO=$MONTAGEM/guppyscreen
mkdir -p "$MONTAGEM"
# Parte de uma cópia da instalação atual (guppyconfig.json, miniaturas e o que
# mais houver) e põe o pacote por cima - o mesmo que o update.sh do OpenKE faz.
cp -a "$GUPPY_DIR" "$NOVO" || desistir "Não consegui copiar a instalação atual para preparar a nova. Nada foi mudado."
rm -rf "$NOVO/k1_mods" "$NOVO/scripts" "$NOVO/themes" "$NOVO/debian" "$NOVO/.config-klipper" \
       "$NOVO/instalar-tela-ptbr.sh" "$NOVO/voltar-tela-original.sh"
tar xf "$ARQ" -C "$MONTAGEM" || desistir "Não consegui abrir o pacote (download corrompido?). Nada foi mudado."
[ -s "$NOVO/guppyscreen" ] || desistir "O programa da tela não saiu do pacote. Nada foi mudado."
# As suas configurações ficam exatamente como estão.
if [ -f "$GUPPY_DIR/guppyconfig.json" ]; then
    cp -a "$GUPPY_DIR/guppyconfig.json" "$NOVO/guppyconfig.json" \
        || desistir "Não consegui copiar o guppyconfig.json. Nada foi mudado."
    iguais "$GUPPY_DIR/guppyconfig.json" "$NOVO/guppyconfig.json" \
        || desistir "O guppyconfig.json não ficou igual. Nada foi mudado."
fi
# Modelo de configuração com os caminhos da KE (como o instalador do OpenKE faz).
if [ -f "$NOVO/debian/guppyconfig.json" ]; then
    sed -i "s|<GUPPY_DIR>|$GUPPY_DIR|g; s|<PRINTER_DATA_DIR>|$DADOS/printer_data|g" "$NOVO/debian/guppyconfig.json"
fi
for _f in guppyscreen guppybeep update.sh installer.sh installer-deb.sh instalar-tela-ptbr.sh \
          voltar-tela-original.sh debian/kd_graphic_mode k1_mods/respawn/supervise-daemon \
          k1_mods/S50dropbear k1_mods/S58factoryreset k1_mods/S99guppyscreen \
          k1_mods/cleanup/cleanup-files.sh k1_mods/cleanup/S45cleanup; do
    [ -f "$NOVO/$_f" ] && chmod 755 "$NOVO/$_f"
done
conferir_programa "$NOVO/guppyscreen"
VERSAO_NOVA=$(ler_versao "$NOVO/.version")
if ! grep -Eq '^(\./)?guppyscreen/instalar-tela-ptbr\.sh$' "$TRAB/lista.txt"; then
    case "$VERSAO_NOVA" in
        *ptbr*) ;;
        *) aviso "Este pacote ($VERSAO_NOVA) não parece ser o da tela em português (é o original?)." ;;
    esac
fi
[ "$VERSAO_NOVA" = "$VERSAO_ATUAL" ] && msg "A versão $VERSAO_NOVA já está instalada; vou reinstalar."
ok "Versão nova pronta: $VERSAO_NOVA"

# ---------------------------------------------------- 4. cópia de segurança --
etapa "4/6 Cópia de segurança completa"
cp -a "$GUPPY_DIR" "$BACKUP" || { rm -rf "$BACKUP"; desistir "Não consegui fazer a cópia de segurança. Nada foi mudado."; }
for _f in guppyscreen guppyconfig.json .version; do
    if [ -f "$GUPPY_DIR/$_f" ] && ! iguais "$GUPPY_DIR/$_f" "$BACKUP/$_f"; then
        rm -rf "$BACKUP"
        desistir "A cópia de segurança não ficou igual à original ($_f). Nada foi mudado."
    fi
done
guardar_config_klipper "$BACKUP" "$NOVO" \
    || { rm -rf "$BACKUP"; desistir "Não consegui guardar os arquivos de apoio do Klipper. Nada foi mudado."; }
sync
ok "Cópia guardada em $BACKUP ($(tamanho_mb "$BACKUP") MB)"

# ------------------------------------------------------------ 5. troca -------
etapa "5/6 Instalando e reiniciando só a tela"
ESTADO=trocando
parar_tela
if ! mv "$GUPPY_DIR" "$ANTIGO"; then
    iniciar_tela
    ESTADO=inicio
    desistir "Não consegui trocar as pastas. A tela atual continua como estava."
fi
if ! mv "$NOVO" "$GUPPY_DIR"; then
    mv "$ANTIGO" "$GUPPY_DIR"
    iniciar_tela
    ESTADO=inicio
    desistir "Não consegui colocar a versão nova no lugar. A tela atual continua como estava."
fi
# A tela antiga pode ter salvo alguma mudança de configuração enquanto a nova
# era preparada: leva a versão final (gravada antes de ela parar).
if [ -f "$ANTIGO/guppyconfig.json" ]; then
    cp -a "$ANTIGO/guppyconfig.json" "$GUPPY_DIR/guppyconfig.json"
fi
sync
iniciar_tela
ESTADO=conferindo

# ---------------------------------------------------------- 6. conferência ---
etapa "6/6 Conferindo se a tela nova subiu (uns 15 segundos)"
if ! tela_subiu; then
    ESTADO=voltando
    voltar_para_anterior
    ESTADO=fim
    exit 1
fi
ESTADO=fim
ok "A tela nova está rodando"
if [ -f "$LOG_TELA" ] && tail -n 400 "$LOG_TELA" 2>/dev/null | grep -qF "Guppy Screen Version: $VERSAO_NOVA"; then
    ok "A própria tela confirmou a versão $VERSAO_NOVA no registro dela"
fi
rm -rf "$ANTIGO"
sincronizar_config_klipper
if instalar_script_de_volta; then
    ok "Script para voltar à tela original: $DADOS/voltar-tela-original.sh"
else
    aviso "Não consegui deixar o $DADOS/voltar-tela-original.sh (baixe do GitHub do projeto se precisar)."
fi

etapa "Pronto! A tela agora está em português."
msg "  Versão instalada  : $VERSAO_NOVA  (antes: $VERSAO_ATUAL)"
msg "  Cópia de segurança: $BACKUP"
msg "  Voltar à original : sh $DADOS/voltar-tela-original.sh"
msg "  Atualizar depois  : botão de atualizar a tela, em Configurações"
msg "  Registro          : $REGISTRO"
msg "  Cópias guardadas:"
listar_copias
exit 0
