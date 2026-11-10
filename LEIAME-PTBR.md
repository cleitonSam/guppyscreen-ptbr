# Tela da Ender-3 V3 KE em português

Esta é a tela de toque da impressora **Creality Ender-3 V3 KE** traduzida para o português do Brasil.

É o mesmo programa de tela do projeto **OpenKE** (a "GuppyScreen"), com os mesmos botões e recursos. Muda só o idioma: textos naturais e curtos, com acento, e os mesmos nomes que o Mainsail usa em português (Mesa, Ventoinha, Calibração...).

- Feita a partir do **OpenKE v1.5.0**.
- Serve só para a Ender-3 V3 KE com o OpenKE já instalado (tela pequena, 480x272).
- Não mexe nas suas calibrações: a altura do bico (Z-offset), a malha da mesa e o resto continuam no `printer.cfg`, intocados.

---

## Antes de começar

1. **A impressora tem que estar parada**, sem imprimir. O instalador confere sozinho e não roda durante uma impressão.
2. **O OpenKE já instalado** (é o que você tem hoje).
3. **Entrar na impressora pelo terminal (SSH)**, como `root` (o usuário administrador dela):
   - No Windows, abra o **PowerShell** (menu Iniciar, digite "PowerShell").
   - Digite `ssh root@192.168.1.16` (troque pelo endereço da sua impressora) e aperte Enter.
   - Na primeira vez ele pergunta se confia na impressora: digite `yes`.
   - A senha é a do root da impressora. Na KE, a padrão da Creality costuma ser `creality_2023`.

---

## Como instalar (uma vez só)

No terminal da impressora, copie e cole esta linha inteira e aperte Enter:

```sh
wget --no-check-certificate -qO /tmp/instalar-tela-ptbr.sh https://raw.githubusercontent.com/cleitonSam/guppyscreen-ptbr/main/scripts/instalar-tela-ptbr.sh && sh /tmp/instalar-tela-ptbr.sh
```

Leva uns 2 minutos. A tela reinicia (fica parada ou apagada por alguns segundos) e volta em português. No fim aparece **"Pronto! A tela agora está em português."**

O que o instalador faz, nesta ordem (e para no primeiro problema, sem estragar nada):

1. Confere se é a KE com o OpenKE, se a impressora está parada e se há espaço livre.
2. Baixa a versão mais nova daqui do GitHub e confere se chegou inteira (pela "impressão digital" SHA-256 publicada junto).
3. Prepara a versão nova numa pasta separada. A tela atual continua funcionando enquanto isso.
4. Faz uma **cópia de segurança completa** da tela atual em `/usr/data/guppyscreen-backup-AAAAMMDD-HHMMSS` (com a data e a hora).
5. Troca a tela **mantendo as suas configurações** (o arquivo `guppyconfig.json`: calibração do toque, sensores, brilho, tempo para apagar...). Reinicia **só a tela**: o Klipper e o Moonraker continuam rodando.
6. Confere se a tela nova ficou de pé. **Se não ficar, volta sozinho para a anterior.**

Tudo que ele faz fica anotado em `/usr/data/printer_data/logs/instalar-tela-ptbr.log`. Dá para ler pelo Mainsail, em **Máquina**, na pasta de registros (logs).

Outras formas de usar o instalador:

| Comando | O que faz |
|---|---|
| `sh /tmp/instalar-tela-ptbr.sh` | instala a versão oficial mais nova |
| `sh /tmp/instalar-tela-ptbr.sh v1.5.0-OpenKE-ptbr.1` | instala uma versão específica |
| `sh /tmp/instalar-tela-ptbr.sh teste` | instala a versão de teste (não oficial) |
| `sh /tmp/instalar-tela-ptbr.sh /usr/data/guppyscreen-smallscreen.tar.gz` | instala um pacote já copiado para a impressora |

---

## Como atualizar

Quando sair uma versão nova: na tela, toque na aba da engrenagem e depois em **Atualizar tela** (no original em inglês, "Update Guppy"). Ele busca as atualizações aqui, então a tela continua em português. Se a tela já estiver na versão mais nova, nada muda; o motivo fica anotado em `/usr/data/printer_data/logs/guppyscreen-update.log`.

- Ele só pega versões oficiais (ignora a de teste).
- Não atualiza com a impressora imprimindo.
- Confere o pacote antes de mexer e, se a versão nova não subir, volta sozinho para a anterior.
- Como não há onde mostrar mensagens enquanto atualiza, elas ficam em `/usr/data/printer_data/logs/guppyscreen-update.log`.

Também dá para atualizar rodando o instalador de novo (o comando lá de cima). Ele faz outra cópia de segurança antes.

---

## Como voltar para a tela original (em inglês)

No terminal da impressora:

```sh
sh /usr/data/voltar-tela-original.sh
```

- Ele volta para a cópia da tela original, que o instalador guardou na primeira vez.
- As configurações da tela (o arquivo `guppyconfig.json`: toque, brilho, sensores...) voltam a ser as daquela cópia, do dia em que ela foi guardada. As de agora não se perdem: ficam na cópia guardada da versão em português.
- A versão em português não é apagada: vira mais uma cópia guardada, e dá para voltar a ela depois.
- Reinicia só a tela e confere se ela subiu. Se não subir, volta sozinho para a que estava.

Para ver as cópias guardadas: `sh /usr/data/voltar-tela-original.sh --listar`
Para voltar para uma cópia específica: `sh /usr/data/voltar-tela-original.sh /usr/data/guppyscreen-backup-AAAAMMDD-HHMMSS`

Depois de voltar para a original, o botão de atualizar dela busca as atualizações do OpenKE original (em inglês). Para ter o português de novo, é só rodar o instalador outra vez.

**Não apague a cópia da tela original**: é a que o `--listar` mostra com uma versão sem "ptbr" no nome. Não confie na data do nome da pasta: enquanto a impressora não acerta a hora pela internet, o relógio dela marca 01/03/2020. As outras cópias você pode apagar quando quiser, para liberar espaço:

```sh
rm -rf /usr/data/guppyscreen-backup-AAAAMMDD-HHMMSS
```

---

## Se algo der errado

- **A tela ficou apagada ou travada:** espere 30 segundos. Se continuar, desligue e ligue a impressora. Se ainda assim não voltar, entre pelo terminal e rode `sh /usr/data/voltar-tela-original.sh`.
- **"A impressora está imprimindo":** espere a impressão terminar e rode de novo.
- **"Não consegui baixar":** confira a internet da impressora. Se ainda não existir versão oficial publicada, teste com `sh /tmp/instalar-tela-ptbr.sh teste`.
- **"Pouco espaço livre":** apague arquivos antigos (gcodes velhos, cópias `guppyscreen-backup-...` que não precisa mais) e rode de novo.
- **Algum texto cortado ou estranho na tela:** anote em qual tela e qual texto, para corrigir na próxima versão.
- **Os registros** (para entender o que aconteceu): `/usr/data/printer_data/logs/instalar-tela-ptbr.log`, `guppyscreen-update.log` e `guppyscreen.log`, na mesma pasta.
- **Não rode o instalador completo do OpenKE original** (o de `coreflake1`): ele põe a tela em inglês de volta. Se rodar sem querer, rode o instalador em português de novo.

---

## Como as versões são numeradas

| Versão | O que é |
|---|---|
| `v1.5.0-OpenKE-ptbr` | primeira versão em português, feita do OpenKE v1.5.0 |
| `v1.5.0-OpenKE-ptbr.1`, `.2`, `.3`... | correções da tradução sobre o mesmo OpenKE |
| `v1.5.1-OpenKE-ptbr` | quando o OpenKE lançar a 1.5.1 e ela for traduzida |
| `teste-ptbr-xxxxxxx` | versão de teste, feita sozinha a cada mudança (não oficial) |
| `local-ptbr-AAAAMMDD-HHMM` | versão montada no computador (veja abaixo) |

O botão de atualizar compara por esse esquema: a `.10` vem depois da `.9`, e a de teste ou a local sempre dão lugar à oficial mais nova.

O pacote sempre se chama `guppyscreen-smallscreen.tar.gz` (o mesmo nome do OpenKE, gravado no arquivo `.version` de cada instalação). Junto vai o `guppyscreen-smallscreen.tar.gz.sha256`, a impressão digital para conferir.

---

## Para quem cuida do projeto

### O repositório

- O repositório **cleitonSam/guppyscreen-ptbr** precisa ser **público**. A licença GPL-3.0 do GuppyScreen exige o código aberto, e num repositório privado a impressora não consegue baixar as atualizações.
- Os scripts têm que ficar com fim de linha do Linux (LF). O arquivo `.gitattributes` garante isso, mesmo quando o projeto é baixado no Windows.

### Montagem automática no GitHub

Fica em `.github/workflows/build.yml` (aba **Actions** do GitHub, "montar tela em português"). Roda sozinha, sem nada instalado no computador:

- **A cada envio para a `main`:** monta e atualiza a **versão de teste** (a etiqueta `teste`).
- **Para publicar uma versão oficial**, um destes dois jeitos:
  - na aba **Actions**, escolha "montar tela em português", toque em **Run workflow**, preencha a versão (por exemplo `v1.5.0-OpenKE-ptbr.1`) e confirme; ou
  - crie a etiqueta e envie: `git tag v1.5.0-OpenKE-ptbr.1` e depois `git push origin v1.5.0-OpenKE-ptbr.1`.
- **Pedidos de mudança (pull requests):** só monta, para conferir.

Antes de publicar, a montagem confere sozinha:

- o programa saiu para MIPS e estático (o tipo que roda na KE);
- a versão ficou gravada no programa;
- há textos em português no programa e todas as letras usadas nos textos existem nas fontes (se faltar alguma, ela viraria um quadradinho na tela);
- o pacote tem tudo, o `.version` está certo, os scripts estão com fim de linha do Linux e o botão de atualizar aponta para este repositório.

Se algo disso falhar, a versão oficial **não é publicada**.

Como ela monta, sem o contêiner do OpenKE: usa o Ubuntu do próprio GitHub, instala as mesmas ferramentas que o `docker/Dockerfile` instala e baixa a mesma ferramenta de montagem para MIPS da Bootlin (`mips32el--musl--stable-2024.02-1`), conferida pelo SHA-256. Depois segue igual ao OpenKE: remendos (`patches/`), `make`, `guppybeep` e `scripts/release.sh`.

Os outros dois fluxos do OpenKE (`pull_request.yml` e `wiki.yml`) ficam desligados aqui: só rodam no repositório original.

### Fontes com acento

As letras com acento vêm das fontes em `fonts_ptbr/`, geradas pelo `scripts/gerar-fontes-ptbr.sh`. Se elas não estiverem salvas no Git, a montagem gera na hora. Explicação completa em `traducao/FONTES.md`.

### Montar no próprio computador (WSL)

Para testar antes de publicar, dá para montar o mesmo pacote no Windows, pelo Ubuntu 24.04 do WSL, como root:

```sh
bash /mnt/c/Users/user/Documents/Projeto/guppyscreen-ptbr/scripts/montar-local-wsl.sh
```

- Na primeira vez ele instala as ferramentas e baixa a ferramenta da Bootlin (uns 130 MB). Nas próximas, reaproveita.
- O pacote sai em `pacote-local/guppyscreen-smallscreen.tar.gz` (essa pasta não vai para o Git).
- Para instalar na impressora, primeiro copie o pacote para `/usr/data/` dela. No próprio Ubuntu do WSL, dentro da pasta do projeto:
  - `scp -O pacote-local/guppyscreen-smallscreen.tar.gz root@192.168.1.16:/usr/data/`
  - se o `scp` não funcionar com a impressora: `ssh root@192.168.1.16 "cat > /usr/data/guppyscreen-smallscreen.tar.gz" < pacote-local/guppyscreen-smallscreen.tar.gz`

  Depois, no terminal da impressora: `wget --no-check-certificate -qO /tmp/instalar-tela-ptbr.sh https://raw.githubusercontent.com/cleitonSam/guppyscreen-ptbr/main/scripts/instalar-tela-ptbr.sh && sh /tmp/instalar-tela-ptbr.sh /usr/data/guppyscreen-smallscreen.tar.gz`. Se a tela em português já estiver instalada, basta `sh /usr/data/guppyscreen/instalar-tela-ptbr.sh /usr/data/guppyscreen-smallscreen.tar.gz`.

### Arquivos desta parte

| Arquivo | Para quê |
|---|---|
| `scripts/instalar-tela-ptbr.sh` | instala a tela em português na impressora (com cópia de segurança e volta automática) |
| `scripts/voltar-tela-original.sh` | volta para a tela original (ou para outra cópia guardada) |
| `scripts/update.sh` | o botão de atualizar da tela, buscando aqui |
| `scripts/montar-local-wsl.sh` | monta o pacote no computador (WSL) |
| `scripts/release.sh` | empacota (agora inclui os dois scripts acima no pacote) |
| `scripts/installer.sh` | instalador completo do OpenKE; aqui ele baixa a tela deste repositório |
| `.github/workflows/build.yml` | montagem e publicação automáticas |
| `.gitattributes` | fim de linha do Linux para os scripts |

---

## Créditos

Este trabalho só existe por causa de quem veio antes:

- **GuppyScreen**, de ballaswag ([ballaswag/guppyscreen](https://github.com/ballaswag/guppyscreen)), com contribuições de [probielodan/guppyscreen](https://github.com/probielodan/guppyscreen), [pellcorp/grumpyscreen](https://github.com/pellcorp/grumpyscreen) e a malha 3D de [prestonbrown/guppyscreen](https://github.com/prestonbrown/guppyscreen).
- **OpenKE**, a versão da GuppyScreen para a Ender-3 V3 KE, de coreflake1 ([coreflake1/guppyscreen](https://github.com/coreflake1/guppyscreen)), com o instalador, as melhorias de impressão e os guias.
- Bibliotecas usadas: [LVGL](https://lvgl.io) (licença MIT), [libhv](https://github.com/ithewei/libhv) (BSD-3-Clause), [spdlog](https://github.com/gabime/spdlog) (MIT) e o cliente do wpa_supplicant (BSD).
- Letras da tela: Montserrat (SIL Open Font License 1.1) e os ícones do Font Awesome Free, que vêm junto com o LVGL.
- Ferramenta de montagem para MIPS: [Bootlin](https://toolchains.bootlin.com).
- Os módulos do Klipper que vêm no OpenKE mantêm as licenças e os créditos próprios (veja os arquivos `NOTICE.md` e `LICENSE` em `k1/k1_mods/klipper_mods/`).

## Licença

**GPL-3.0**, a mesma do GuppyScreen e do OpenKE. O texto completo está no arquivo [`LICENSE`](LICENSE).

Esta é uma **versão modificada** do OpenKE v1.5.0: em novembro de 2026 foram feitas a tradução da tela para o português do Brasil, as fontes com acento e os scripts de montagem, instalação, atualização e volta à tela original. Quem receber o pacote tem direito ao código-fonte completo, que está publicado neste repositório, sob a mesma licença.
