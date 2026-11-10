#!/bin/bash

RELEASES_DIR=./releases/guppyscreen
rm -rf $RELEASES_DIR
mkdir -p $RELEASES_DIR

ASSET_NAME=$1

"$CROSS_COMPILE"strip ./build/bin/guppyscreen
cp ./build/bin/guppyscreen $RELEASES_DIR/guppyscreen
if [ -f ./build/bin/guppybeep ]; then
    "$CROSS_COMPILE"strip ./build/bin/guppybeep
    cp ./build/bin/guppybeep $RELEASES_DIR/guppybeep
fi
cp -r ./k1/k1_mods $RELEASES_DIR
cp -r ./k1/scripts $RELEASES_DIR
cp -r ./themes $RELEASES_DIR
cp ./scripts/installer-deb.sh $RELEASES_DIR
cp ./scripts/installer.sh $RELEASES_DIR
cp ./scripts/update.sh $RELEASES_DIR
# Licença GPL-3.0: quem recebe o programa recebe também o texto dela.
cp ./LICENSE $RELEASES_DIR/LICENSE
# Versão em português: o instalador da tela em português e o script que volta
# para a tela original também vão no pacote (o instalador copia o segundo para
# /usr/data/voltar-tela-original.sh, para funcionar mesmo sem internet).
for extra in instalar-tela-ptbr.sh voltar-tela-original.sh; do
    if [ -f ./scripts/$extra ]; then
        cp ./scripts/$extra $RELEASES_DIR/$extra
        chmod 755 $RELEASES_DIR/$extra
    fi
done
# O botão de atualizar da tela executa o update.sh direto: ele precisa da
# permissão de execução mesmo se o checkout a tiver perdido (ex.: Windows).
chmod 755 $RELEASES_DIR/update.sh $RELEASES_DIR/installer.sh $RELEASES_DIR/installer-deb.sh
if [ -f ./custom_upgrade.sh ]; then
    cp ./custom_upgrade.sh $RELEASES_DIR
fi
# debian/ must land before kd_graphic_mode: the binary is placed inside it
# (see debian/disable_blinking_cursor.service, which execs debian/kd_graphic_mode).
# Copying it to $RELEASES_DIR/debian directly (old behavior) collided with this
# cp -r, which cp then silently refused (won't overwrite a dir with a file) —
# every release ended up shipping that stray binary instead of the debian/ dir.
cp -r ./debian $RELEASES_DIR
if [ -f ./build/bin/kd_graphic_mode ]; then
    "$CROSS_COMPILE"strip ./build/bin/kd_graphic_mode
    cp ./build/bin/kd_graphic_mode $RELEASES_DIR/debian/kd_graphic_mode
fi


echo "{\"version\": \"$GUPPYSCREEN_VERSION\", \"theme\": \"$GUPPY_THEME\", \"asset_name\": \"$ASSET_NAME.tar.gz\"}" > $RELEASES_DIR/.version
tar --owner=0 --group=0 --numeric-owner -czf $ASSET_NAME.tar.gz -C releases .
