#!/usr/bin/env bash
# =============================================================================
#  montar-local-wsl.sh
#  Monta, no computador, o mesmo pacote da tela em português que o GitHub monta
#  (guppyscreen-smallscreen.tar.gz), para testar antes de publicar.
#
#  Onde roda: Ubuntu 24.04 dentro do WSL do Windows, como root.
#  Como usar (no terminal do Ubuntu):
#     sudo -i        (se ainda não for root)
#     bash /mnt/c/Users/user/Documents/Projeto/guppyscreen-ptbr/scripts/montar-local-wsl.sh
#  ou escolhendo a versão gravada no pacote (o padrão é local-ptbr-DATA-HORA):
#     bash .../scripts/montar-local-wsl.sh v1.5.0-OpenKE-ptbr.1
#
#  Resultado:  <pasta do projeto>/pacote-local/guppyscreen-smallscreen.tar.gz
#              (e o .sha256 de conferência). A pasta pacote-local/ não vai para o Git.
#  Para instalar na impressora: copie o pacote para /usr/data/ da impressora e,
#  no terminal dela, rode
#     sh /usr/data/guppyscreen/instalar-tela-ptbr.sh /usr/data/guppyscreen-smallscreen.tar.gz
#  (ou o instalar-tela-ptbr.sh baixado do GitHub, com o mesmo caminho do pacote).
#
#  Faz o mesmo que .github/workflows/build.yml:
#    1. instala as ferramentas do Ubuntu (apt), inclusive Node e Python;
#    2. baixa a ferramenta de montagem MIPS da Bootlin e confere o SHA-256
#       (fica guardada em /opt/ferramentas-guppyscreen para as próximas vezes);
#    3. copia o projeto para dentro do Linux (/root/guppyscreen-ptbr-montagem),
#       com o fim de linha certo (o Git do Windows troca LF por CRLF);
#    4. baixa os submódulos (lvgl, lv_drivers, libhv, spdlog);
#    5. aplica os remendos, prepara as fontes com acento e monta;
#    6. empacota com o scripts/release.sh e confere o pacote.
#  A pasta do projeto no Windows não é alterada (só ganha a pasta pacote-local/).
# =============================================================================
set -Eeuo pipefail

ASSET=guppyscreen-smallscreen
MIPS_TOOLCHAIN=mips32el--musl--stable-2024.02-1
MIPS_URL=https://toolchains.bootlin.com/downloads/releases/toolchains/mips32el/tarballs/$MIPS_TOOLCHAIN.tar.bz2
MIPS_SHA256=25c0b3217df1bf1a7bae2cc4f56cdeab9fec98b172bbf0b336b2e8fe41d3ee4e
FERRAMENTAS=/opt/ferramentas-guppyscreen
ORIGEM="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TRABALHO="${TRABALHO:-/root/guppyscreen-ptbr-montagem}"
SAIDA="$ORIGEM/pacote-local"
VERSAO="${1:-local-ptbr-$(date +%Y%m%d-%H%M)}"

passo() { printf '\n\033[1;32m==> %s\033[0m\n' "$*"; }
aviso() { printf '\033[1;33mAVISO: %s\033[0m\n' "$*"; }
falha() { printf '\n\033[1;31mERRO: %s\033[0m\n' "$*" >&2; exit 1; }
trap 'falha "a montagem parou na linha $LINENO (veja a mensagem logo acima)."' ERR

# --- conferências --------------------------------------------------------------
[ "$(id -u)" -eq 0 ] || falha "rode como root (no Ubuntu: sudo -i, e depois o comando de novo)."
[ -f "$ORIGEM/Makefile" ] && [ -f "$ORIGEM/scripts/release.sh" ] || falha "não achei o projeto em $ORIGEM."
case "$TRABALHO" in
    /|/root|/root/|/mnt/*|"$ORIGEM"|"$ORIGEM"/*) falha "pasta de trabalho inválida: $TRABALHO" ;;
esac
[[ "$VERSAO" =~ ^[A-Za-z0-9._-]+$ ]] || falha "versão inválida: '$VERSAO' (use letras, números, ponto, hífen)."
if [[ "$VERSAO" == v* ]] && ! [[ "$VERSAO" =~ ^v[0-9]+\.[0-9]+\.[0-9]+-OpenKE-ptbr(\.[0-9]+)?$ ]]; then
    falha "versão fora do padrão: use vX.Y.Z-OpenKE-ptbr ou vX.Y.Z-OpenKE-ptbr.N (ou deixe em branco)."
fi
if [ -r /etc/os-release ]; then
    # shellcheck disable=SC1091
    . /etc/os-release
    [ "${VERSION_ID:-}" = "24.04" ] || aviso "feito para o Ubuntu 24.04; este é ${PRETTY_NAME:-desconhecido}. Sigo assim mesmo."
fi
grep -qi microsoft /proc/version 2>/dev/null || aviso "não parece ser o WSL (tudo bem, sigo igual)."

# --- 1. ferramentas do Ubuntu --------------------------------------------------
passo "1/6 Instalando as ferramentas do Ubuntu"
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends \
    build-essential cmake git wget ca-certificates xz-utils bzip2 file jq python3 nodejs npm
node -e 'process.exit(parseInt(process.versions.node, 10) >= 12 ? 0 : 1)' \
    || falha "Node.js muito antigo ($(node --version)); o gerador de fontes pede 12 ou mais novo."
echo "Node $(node --version), npm $(npm --version), $(python3 --version), $(cmake --version | head -n 1)"

# --- 2. ferramenta MIPS (Bootlin) ----------------------------------------------
passo "2/6 Ferramenta de montagem MIPS (Bootlin, $MIPS_TOOLCHAIN)"
mkdir -p "$FERRAMENTAS"
ARQ="$FERRAMENTAS/$MIPS_TOOLCHAIN.tar.bz2"
PRONTO="$FERRAMENTAS/$MIPS_TOOLCHAIN/.conferida"
if [ ! -f "$PRONTO" ]; then
    if ! { [ -s "$ARQ" ] && echo "$MIPS_SHA256  $ARQ" | sha256sum -c --status -; }; then
        rm -f "$ARQ"
        wget --tries=3 --timeout=120 -O "$ARQ" "$MIPS_URL"
        echo "$MIPS_SHA256  $ARQ" | sha256sum -c --status - \
            || falha "a ferramenta baixada não bate com o SHA-256 esperado ($MIPS_SHA256)."
    fi
    rm -rf "${FERRAMENTAS:?}/$MIPS_TOOLCHAIN"
    tar -xjf "$ARQ" -C "$FERRAMENTAS"
    touch "$PRONTO"
fi
export PATH="$FERRAMENTAS/$MIPS_TOOLCHAIN/bin:$PATH"
export CROSS_COMPILE=mipsel-buildroot-linux-musl-
command -v "${CROSS_COMPILE}gcc" >/dev/null || falha "não achei ${CROSS_COMPILE}gcc em $FERRAMENTAS."
"${CROSS_COMPILE}gcc" --version | head -n 1

# --- 3. cópia do projeto, com o fim de linha certo ------------------------------
passo "3/6 Copiando o projeto para dentro do Linux ($TRABALHO)"
rm -rf "${TRABALHO:?}"
mkdir -p "$TRABALHO"
# --no-same-owner: tudo fica do root (senão o Git recusa a pasta: "dubious ownership").
tar -C "$ORIGEM" --exclude=./build --exclude=./pacote-local --exclude='./guppyscreen-*.tar.gz*' -cf - . \
    | tar -C "$TRABALHO" --no-same-owner -xf -
cd "$TRABALHO"
if [ -d .git ]; then
    # O próprio Git decide o que é texto: guarda tudo (com as mudanças ainda
    # não salvas) convertendo CRLF -> LF e reescreve os arquivos a partir dali,
    # já com LF e com as permissões de execução registradas no Git.
    rm -f .git/index.lock
    git config --unset core.fsmonitor 2>/dev/null || true
    git config core.autocrlf false
    git config core.ignorecase false
    git -c core.autocrlf=true -c core.filemode=false -c core.symlinks=false add -A .
    git config core.filemode true
    git config core.symlinks true
    git checkout-index -a -f
else
    aviso "a pasta não é um repositório Git; convertendo CRLF -> LF nos arquivos de texto."
    { grep -rIl --exclude-dir=.git $'\r' . || true; } | while IFS= read -r f; do sed -i 's/\r$//' "$f"; done
fi
if grep -q $'\r' scripts/release.sh scripts/update.sh; then
    falha "os scripts continuam com fim de linha do Windows (CRLF)."
fi

# --- 4. submódulos -------------------------------------------------------------
passo "4/6 Baixando os submódulos (lvgl, lv_drivers, libhv, spdlog)"
git submodule sync --recursive
# --force: refaz sempre os arquivos dos submódulos (com fim de linha do Linux e
# sem remendos já aplicados), mesmo que tenham vindo do Windows já no commit certo.
git submodule update --init --recursive --force --depth 1 \
    || git submodule update --init --recursive --force
for d in lvgl lv_drivers libhv spdlog; do
    [ -n "$(ls -A "$d" 2>/dev/null)" ] || falha "o submódulo $d veio vazio."
done

# --- 5. remendos, fontes e montagem -------------------------------------------
passo "5/6 Remendos, fontes com acento e montagem (versão $VERSAO)"
(cd spdlog && git apply ../patches/0002-spdlog_fmt_initializer_list.patch)
(cd lvgl && git apply ../patches/0003-lvgl-dpi-text-scale.patch)

[ -f scripts/gerar-fontes-ptbr.sh ] || falha "falta scripts/gerar-fontes-ptbr.sh (as fontes com acento)."
QTD_FONTES=0
if [ -d fonts_ptbr ]; then
    QTD_FONTES=$(find fonts_ptbr -maxdepth 1 -name 'lv_font_montserrat_*.c' | wc -l)
fi
if [ "$QTD_FONTES" -eq 6 ]; then
    echo "Usando as 6 fontes com acento da pasta do projeto (fonts_ptbr/)."
else
    echo "Fontes com acento na pasta do projeto: $QTD_FONTES de 6. Gerando agora."
    npm_config_yes=true npm_config_update_notifier=false npm_config_fund=false npm_config_audit=false \
        bash scripts/gerar-fontes-ptbr.sh
    mkdir -p "$SAIDA/fonts_ptbr"
    cp fonts_ptbr/lv_font_montserrat_*.c "$SAIDA/fonts_ptbr/"
    aviso "as fontes geradas também foram copiadas para $SAIDA/fonts_ptbr/ (ponha em fonts_ptbr/ e salve no Git)."
fi
bash scripts/gerar-fontes-ptbr.sh --so-conferir \
    || aviso "há letras nos textos da tela que não existem nas fontes (virariam quadradinhos)."

export CC="${CROSS_COMPILE}gcc" CXX="${CROSS_COMPILE}g++"
export GUPPY_THEME=material GUPPY_SMALL_SCREEN=true EVDEV_CALIBRATE=true
export GUPPYSCREEN_VERSION="$VERSAO"

# Tarefas em paralelo: no máximo uma por processador e uma a cada ~700 MB livres
# (o WSL costuma ter só metade da memória do computador).
NPROC=$(nproc)
MEM_MB=$(awk '/MemAvailable/ {print int($2 / 1024)}' /proc/meminfo)
JOBS=$(( MEM_MB / 700 ))
[ "$JOBS" -ge 1 ] || JOBS=1
[ "$JOBS" -le "$NPROC" ] || JOBS=$NPROC
echo "Montando com $JOBS tarefas em paralelo ($NPROC processadores, $MEM_MB MB livres)."

# Mesmos alvos do Makefile (make wpaclient / libhv.a / libspdlog.a), chamados
# direto para limitar as tarefas em paralelo - o Makefile usa -j sem limite.
make -C wpa_supplicant/wpa_supplicant -j"$JOBS" libwpa_client.a
make -C libhv -j"$JOBS" libhv
mkdir -p spdlog/build
cmake -B spdlog/build -S spdlog/ -DCMAKE_CXX_COMPILER="$CXX"
make -C spdlog/build -j"$JOBS"

make -j"$JOBS" || { aviso "a primeira passada falhou; tentando de novo só o que faltou."; make -j"$JOBS"; }
make kd_graphic_mode
"${CROSS_COMPILE}gcc" -O2 -static -o build/bin/guppybeep k1/k1_mods/buzzer/guppybeep.c

BIN=build/bin/guppyscreen
file "$BIN"
file "$BIN" | grep -Eq 'ELF 32-bit LSB (pie )?executable, MIPS' || falha "o programa não saiu para MIPS 32 bits."
file "$BIN" | grep -Eq 'statically linked|static-pie linked' || falha "o programa não saiu estático."
LC_ALL=C grep -aqF -- "$VERSAO" "$BIN" || falha "a versão $VERSAO não ficou gravada no programa."
# Palavras do glossário da tradução (só "ç"/"ã" daria falso positivo nos dados).
if LC_ALL=C grep -aqF -e 'Calibração' -e 'Cancelar' "$BIN"; then
    echo "Textos em português encontrados no programa."
else
    aviso "não achei textos em português no programa (Calibração, Cancelar): a tradução ainda não entrou?"
fi

# --- 6. pacote -----------------------------------------------------------------
passo "6/6 Empacotando e conferindo"
bash ./scripts/release.sh "$ASSET"
PKG="$ASSET.tar.gz"
tar -tzf "$PKG" > lista-pacote.txt
for f in ./guppyscreen/guppyscreen ./guppyscreen/guppybeep ./guppyscreen/.version \
         ./guppyscreen/update.sh ./guppyscreen/instalar-tela-ptbr.sh ./guppyscreen/voltar-tela-original.sh \
         ./guppyscreen/k1_mods/S99guppyscreen ./guppyscreen/k1_mods/respawn/supervise-daemon \
         ./guppyscreen/debian/guppyconfig.json; do
    grep -qxF -- "$f" lista-pacote.txt || falha "falta $f no pacote."
done
rm -rf conferencia && mkdir conferencia
tar -xzf "$PKG" -C conferencia ./guppyscreen/.version ./guppyscreen/update.sh \
    ./guppyscreen/instalar-tela-ptbr.sh ./guppyscreen/voltar-tela-original.sh ./guppyscreen/k1_mods/S99guppyscreen
cat conferencia/guppyscreen/.version; echo
jq -e --arg v "$VERSAO" --arg a "$PKG" '.version == $v and .asset_name == $a' conferencia/guppyscreen/.version > /dev/null \
    || falha "o .version do pacote não bate com a versão $VERSAO e o nome $PKG."
for s in update.sh instalar-tela-ptbr.sh voltar-tela-original.sh k1_mods/S99guppyscreen; do
    if grep -q $'\r' "conferencia/guppyscreen/$s"; then
        falha "$s está com fim de linha do Windows (CRLF) e não rodaria na impressora."
    fi
    [ -x "conferencia/guppyscreen/$s" ] || falha "$s está sem permissão de execução no pacote."
done
sha256sum "$PKG" > "$PKG.sha256"
mkdir -p "$SAIDA"
cp "$PKG" "$PKG.sha256" "$SAIDA/"

trap - ERR
passo "Pronto!"
echo "  Pacote : $SAIDA/$PKG"
echo "  SHA-256: $(cut -d' ' -f1 "$PKG.sha256")"
echo "  Versão : $VERSAO"
echo
echo "  Para instalar na impressora (com ela parada):"
echo "    1. copie o pacote para /usr/data/ da impressora (aqui mesmo, no Ubuntu):"
echo "         scp -O \"$SAIDA/$PKG\" root@192.168.1.16:/usr/data/"
echo "       se o scp não funcionar com a impressora:"
echo "         ssh root@192.168.1.16 \"cat > /usr/data/$PKG\" < \"$SAIDA/$PKG\""
echo "    2. no terminal da impressora:"
echo "         sh /usr/data/guppyscreen/instalar-tela-ptbr.sh /usr/data/$PKG"
echo "       (se a tela instalada ainda for a original, baixe antes o instalador:"
echo "        wget --no-check-certificate -qO /tmp/instalar-tela-ptbr.sh https://raw.githubusercontent.com/cleitonSam/guppyscreen-ptbr/main/scripts/instalar-tela-ptbr.sh"
echo "        e rode: sh /tmp/instalar-tela-ptbr.sh /usr/data/$PKG)"
echo
echo "  A cópia de trabalho ficou em $TRABALHO (pode apagar quando quiser)."
