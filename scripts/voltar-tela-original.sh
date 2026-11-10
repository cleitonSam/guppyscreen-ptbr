#!/bin/sh
# =============================================================================
#  voltar-tela-original.sh
#  Volta a tela da Ender-3 V3 KE para uma cópia guardada pelo instalador da
#  versão em português (instalar-tela-ptbr.sh). Sem escolher nada, volta para
#  a tela ORIGINAL: a cópia mais recente que não é a versão em português.
#
#  Roda NA IMPRESSORA, como root, pelo terminal (SSH):
#    sh /usr/data/voltar-tela-original.sh             volta para a tela original
#    sh /usr/data/voltar-tela-original.sh --listar    só mostra as cópias guardadas
#    sh /usr/data/voltar-tela-original.sh /usr/data/guppyscreen-backup-AAAAMMDD-HHMMSS
#                                                     volta para essa cópia
#
#  - A cópia escolhida não é apagada (dá para usar de novo).
#  - A versão que estiver instalada agora também não é apagada: vira mais uma
#    cópia guardada (/usr/data/guppyscreen-backup-...), para poder voltar a ela.
#  - Só a tela reinicia (/etc/init.d/S99guppyscreen); Klipper e Moonraker
#    continuam rodando. Não roda com a impressora imprimindo.
#  - Se a tela restaurada não subir, ele volta sozinho para a que estava.
# =============================================================================

REPO="cleitonSam/guppyscreen-ptbr"

DADOS=/usr/data
GUPPY_DIR=$DADOS/guppyscreen
INIT=/etc/init.d/S99guppyscreen
CONFIG_KLIPPER=$DADOS/printer_data/config
LOG_TELA=$DADOS/printer_data/logs/guppyscreen.log
DATA=$(date +%Y%m%d-%H%M%S)
RESTAURANDO=$DADOS/guppyscreen.restaurando-$DATA
GUARDADA=$DADOS/guppyscreen-backup-$DATA
FALHOU=$DADOS/guppyscreen-falhou-$DATA
# O relógio da KE começa em 01/03/2020 até acertar a hora pela internet: sem
# internet, duas execuções podem gerar o mesmo nome. Nunca reaproveita pasta.
while [ -e "$RESTAURANDO" ] || [ -e "$GUARDADA" ] || [ -e "$FALHOU" ]; do
    DATA="$DATA-$$"
    RESTAURANDO=$DADOS/guppyscreen.restaurando-$DATA
    GUARDADA=$DADOS/guppyscreen-backup-$DATA
    FALHOU=$DADOS/guppyscreen-falhou-$DATA
done

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

registrar() { printf '%s %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*" >> "$REGISTRO" 2>/dev/null; return 0; }
msg()       { printf '%s\n' "$*"; registrar "$*"; }
ok()        { printf '%s  ok: %s%s\n' "$VERDE" "$*" "$NORMAL"; registrar "ok: $*"; }
aviso()     { printf '%sAVISO: %s%s\n' "$AMARELO" "$*" "$NORMAL"; registrar "AVISO: $*"; }
erro()      { printf '%sERRO: %s%s\n' "$VERMELHO" "$*" "$NORMAL"; registrar "ERRO: $*"; }
etapa()     { printf '\n%s== %s ==%s\n' "$VERDE" "$*" "$NORMAL"; registrar "== $* =="; }
desistir()  { erro "$*"; exit 1; }

sha256_de() { sha256sum "$1" 2>/dev/null | awk '{print $1}'; }

iguais() {
    [ -f "$1" ] && [ -f "$2" ] || return 1
    if command -v cmp >/dev/null 2>&1; then
        cmp -s "$1" "$2"
    else
        [ "$(sha256_de "$1")" = "$(sha256_de "$2")" ]
    fi
}

tamanho_mb() { du -sk "$1" 2>/dev/null | awk '{print int($1 / 1024) + 1}'; }
livre_mb()   { df -k "$1" 2>/dev/null | tail -n 1 | awk '{print int($(NF-2) / 1024)}'; }

ler_versao() {
    _v=""
    if [ -f "$1" ]; then
        command -v jq >/dev/null 2>&1 && _v=$(jq -r '.version // empty' "$1" 2>/dev/null)
        [ -n "$_v" ] || _v=$(sed -n 's/.*"version"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "$1" 2>/dev/null | head -n 1)
    fi
    printf '%s' "${_v:-desconhecida}"
}

# A pasta é da versão em português? (o pacote em português traz o
# instalar-tela-ptbr.sh, e as versões dele têm "ptbr" no nome)
eh_ptbr() {
    [ -f "$1/instalar-tela-ptbr.sh" ] && return 0
    case "$(ler_versao "$1/.version")" in *ptbr*) return 0 ;; esac
    return 1
}

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
}

iniciar_tela() {
    "$INIT" start >/dev/null 2>&1
}

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

listar_copias() {
    _qtd=0
    for _d in "$DADOS"/guppyscreen-backup-*; do
        [ -f "$_d/guppyscreen" ] || continue
        _qtd=$((_qtd + 1))
        msg "    $_d  (versão $(ler_versao "$_d/.version"), $(tamanho_mb "$_d") MB)"
    done
    [ "$_qtd" -gt 0 ] || msg "    (nenhuma cópia guardada)"
}

# Guarda os arquivos de apoio do Klipper que vão ser repostos ($2 = a pasta
# .config-klipper da cópia escolhida), como estão agora, dentro da cópia
# $1 - assim dá para voltar para a versão atual depois, do jeito que estava.
guardar_config_klipper() {
    _cfgdir=$CONFIG_KLIPPER/GuppyScreen
    [ -d "$_cfgdir" ] && [ -d "$2" ] || return 0
    _dst=$1/.config-klipper
    mkdir -p "$_dst/scripts" || return 1
    for _src in "$2"/*.cfg "$2"/scripts/*.py; do
        [ -f "$_src" ] || continue
        _rel=${_src#"$2"/}
        if [ -f "$_cfgdir/$_rel" ]; then
            cp -a "$_cfgdir/$_rel" "$_dst/$_rel" || return 1
        fi
    done
    # Os que a volta vai apagar (criados pela versão que sai) também ficam guardados.
    if [ -f "$2/novos.txt" ]; then
        while IFS= read -r _rel; do
            case "$_rel" in ""|*..*|/*) continue ;; esac
            [ -f "$_cfgdir/$_rel" ] && cp -a "$_cfgdir/$_rel" "$_dst/$_rel"
        done < "$2/novos.txt"
    fi
    return 0
}

# Repõe os arquivos de apoio do Klipper guardados junto com a cópia escolhida
# (só os que estão diferentes) e apaga os que a versão em português criou.
repor_config_klipper() {   # $1 = pasta .config-klipper da cópia escolhida
    _cfgdir=$CONFIG_KLIPPER/GuppyScreen
    [ -d "$1" ] && [ -d "$_cfgdir" ] || return 0
    _cfg_mudou=0
    for _src in "$1"/*.cfg "$1"/scripts/*.py; do
        [ -f "$_src" ] || continue
        _rel=${_src#"$1"/}
        iguais "$_src" "$_cfgdir/$_rel" && continue
        mkdir -p "$(dirname "$_cfgdir/$_rel")"
        if cp -a "$_src" "$_cfgdir/$_rel"; then
            msg "  reposto: $_cfgdir/$_rel"
            case "$_rel" in *.cfg) _cfg_mudou=1 ;; esac
        fi
    done
    if [ -f "$1/novos.txt" ]; then
        while IFS= read -r _rel; do
            case "$_rel" in ""|*..*|/*) continue ;; esac
            if [ -f "$_cfgdir/$_rel" ]; then
                rm -f "$_cfgdir/$_rel" && msg "  apagado (era da versão em português): $_cfgdir/$_rel"
                case "$_rel" in *.cfg) _cfg_mudou=1 ;; esac
            fi
        done < "$1/novos.txt"
    fi
    if [ "$_cfg_mudou" = 1 ]; then
        aviso "Mudaram atalhos (macros) do Klipper usados pela tela. Eles só valem depois de reiniciar o Klipper: faça isso com a impressora parada (Mainsail > Reiniciar)."
    fi
}

# =============================================================================
ALVO=${1:-}
case "$ALVO" in
    -h|--help|--ajuda|ajuda)
        sed -n '3,19p' "$0" 2>/dev/null | sed 's/^# \{0,1\}//'
        exit 0 ;;
    -l|--listar|listar)
        msg "Cópias guardadas da tela:"
        listar_copias
        exit 0 ;;
esac

registrar "---------------------------------------------------------------"
registrar "voltar-tela-original.sh - pedido: ${ALVO:-tela original}"

etapa "1/4 Conferindo a impressora"
[ "$(id -u)" = "0" ] || desistir "Rode como root (o usuário administrador da impressora)."
for _cmd in awk sed grep cp mv rm mkdir du df readlink killall sleep date; do
    command -v "$_cmd" >/dev/null 2>&1 || desistir "Falta o comando '$_cmd' na impressora."
done
[ -f "$INIT" ] || desistir "Não achei o serviço da tela ($INIT)."
[ -L "$GUPPY_DIR" ] && desistir "$GUPPY_DIR é um atalho (link): instalação fora do padrão, não vou mexer."
VERSAO_ATUAL=$(ler_versao "$GUPPY_DIR/.version")
msg "Versão instalada agora: $VERSAO_ATUAL"

# Escolhe a cópia.
if [ -n "$ALVO" ]; then
    [ -d "$ALVO" ] && [ -f "$ALVO/guppyscreen" ] || desistir "Não achei uma cópia válida da tela em $ALVO."
    ESCOLHIDA=$(cd "$ALVO" && pwd -P)
    [ "$ESCOLHIDA" = "$(cd "$GUPPY_DIR" 2>/dev/null && pwd -P)" ] && desistir "Essa é a pasta da tela instalada agora; escolha uma cópia guardada."
else
    if [ -f "$GUPPY_DIR/guppyscreen" ] && ! eh_ptbr "$GUPPY_DIR"; then
        ok "A tela já está na versão original ($VERSAO_ATUAL). Nada a fazer."
        msg "Para voltar a outra cópia, rode: sh $0 /usr/data/guppyscreen-backup-AAAAMMDD-HHMMSS"
        msg "Cópias guardadas:"
        listar_copias
        exit 0
    fi
    ESCOLHIDA=""
    for _d in "$DADOS"/guppyscreen-backup-*; do
        [ -f "$_d/guppyscreen" ] || continue
        eh_ptbr "$_d" && continue
        ESCOLHIDA=$_d   # a lista vem em ordem de data: fica a mais recente
    done
    if [ -z "$ESCOLHIDA" ]; then
        erro "Não achei nenhuma cópia da tela original (todas as cópias são da versão em português)."
        msg "Cópias guardadas:"
        listar_copias
        msg "Para voltar a uma delas: sh $0 /usr/data/guppyscreen-backup-AAAAMMDD-HHMMSS"
        msg "Ou reinstale o OpenKE original (em inglês) com o instalador oficial dele:"
        msg "  sh -c \"\$(wget --no-check-certificate -qO - https://raw.githubusercontent.com/coreflake1/guppyscreen/main/scripts/installer.sh)\""
        exit 1
    fi
fi
VERSAO_ESCOLHIDA=$(ler_versao "$ESCOLHIDA/.version")
ok "Cópia escolhida: $ESCOLHIDA (versão $VERSAO_ESCOLHIDA)"

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

PRECISA_MB=$(( $(tamanho_mb "$ESCOLHIDA") + 100 ))
LIVRE_MB=$(livre_mb "$DADOS")
[ "${LIVRE_MB:-0}" -ge "$PRECISA_MB" ] || desistir "Pouco espaço livre em $DADOS: ${LIVRE_MB:-?} MB livres, preciso de $PRECISA_MB MB."

ESTADO=inicio
ao_sair() {
    case "$ESTADO" in
        trocando)
            if [ ! -d "$GUPPY_DIR" ] && [ -d "$GUARDADA" ]; then
                mv "$GUARDADA" "$GUPPY_DIR"
            fi
            "$INIT" start >/dev/null 2>&1
            ;;
    esac
    rm -rf "$RESTAURANDO"
}
trap ao_sair EXIT
trap 'erro "Interrompido."; exit 130' INT TERM
trap '' HUP

etapa "2/4 Preparando a cópia (a tela atual continua funcionando)"
cp -a "$ESCOLHIDA" "$RESTAURANDO" || desistir "Não consegui copiar $ESCOLHIDA. Nada foi mudado."
rm -rf "$RESTAURANDO/.config-klipper"
iguais "$ESCOLHIDA/guppyscreen" "$RESTAURANDO/guppyscreen" || desistir "A cópia não ficou igual. Nada foi mudado."
chmod 755 "$RESTAURANDO/guppyscreen" 2>/dev/null
[ -f "$RESTAURANDO/update.sh" ] && chmod 755 "$RESTAURANDO/update.sh"
ok "Pronta"

etapa "3/4 Trocando a tela (só a tela reinicia)"
ESTADO=trocando
parar_tela
if [ -d "$GUPPY_DIR" ] && ! mv "$GUPPY_DIR" "$GUARDADA"; then
    iniciar_tela
    ESTADO=inicio
    desistir "Não consegui trocar as pastas. A tela atual continua como estava."
fi
if ! mv "$RESTAURANDO" "$GUPPY_DIR"; then
    [ -d "$GUARDADA" ] && mv "$GUARDADA" "$GUPPY_DIR"
    iniciar_tela
    ESTADO=inicio
    desistir "Não consegui colocar a cópia no lugar. A tela atual continua como estava."
fi
sync
iniciar_tela
ESTADO=conferindo

etapa "4/4 Conferindo se a tela subiu (uns 15 segundos)"
if ! tela_subiu; then
    erro "A tela restaurada não ficou de pé. Voltando para a que estava..."
    if [ -f "$LOG_TELA" ]; then
        msg "Últimas linhas do registro da tela ($LOG_TELA):"
        tail -n 15 "$LOG_TELA" 2>/dev/null | while IFS= read -r _linha; do msg "    $_linha"; done
    fi
    parar_tela
    mv "$GUPPY_DIR" "$FALHOU" 2>/dev/null
    [ -d "$GUARDADA" ] && mv "$GUARDADA" "$GUPPY_DIR"
    sync
    iniciar_tela
    ESTADO=fim
    if tela_subiu; then
        msg "A tela que estava ($VERSAO_ATUAL) voltou e está funcionando."
    else
        erro "Ela também não subiu. Desligue e ligue a impressora."
    fi
    msg "A cópia que falhou ficou em $FALHOU (pode apagar depois)."
    exit 1
fi
ESTADO=fim
ok "A tela $VERSAO_ESCOLHIDA está rodando"

if [ -d "$GUARDADA" ]; then
    guardar_config_klipper "$GUARDADA" "$ESCOLHIDA/.config-klipper" \
        || aviso "Não consegui guardar os arquivos de apoio do Klipper da versão que saiu."
fi
repor_config_klipper "$ESCOLHIDA/.config-klipper"

etapa "Pronto! A tela voltou para a versão $VERSAO_ESCOLHIDA."
[ -d "$GUARDADA" ] && msg "  A versão que estava ($VERSAO_ATUAL) ficou guardada em $GUARDADA"
if ! eh_ptbr "$GUPPY_DIR"; then
    msg "  O botão de atualizar desta versão busca as atualizações do OpenKE original (inglês)."
    msg "  Para ter a tela em português de novo:"
    msg "    wget --no-check-certificate -qO /tmp/instalar-tela-ptbr.sh https://raw.githubusercontent.com/$REPO/main/scripts/instalar-tela-ptbr.sh && sh /tmp/instalar-tela-ptbr.sh"
fi
msg "  Registro: $REGISTRO"
msg "  Cópias guardadas:"
listar_copias
exit 0
