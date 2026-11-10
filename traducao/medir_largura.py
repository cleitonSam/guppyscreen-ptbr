"""Mede quantos pixels um texto ocupa na tela, com as fontes Montserrat do LVGL.

Serve para saber se uma tradução cabe no botão antes de compilar.
Conta a largura de cada letra e o kerning do jeito que o LVGL 8 conta
(cada letra vale arredondar((largura + kerning) / 16) px). Letra com acento
mede o mesmo que a letra sem acento (assim é na Montserrat).

Como usar (só Python, sem instalar nada):
  python medir_largura.py 10 "Descarregar" "Parar tudo"
  python medir_largura.py 14 --arquivo lista.txt        (um texto por linha, em UTF-8)
  python medir_largura.py 10 --fontes C:/caminho/das/fontes "Calibração"

O primeiro número é o tamanho da fonte (8, 10, 12, 14, 16 ou 20).
Escreva \\n no texto para medir cada linha de um rótulo de 2 linhas.
Por padrão as fontes são lidas de ../lvgl/src/font (submódulo do LVGL baixado).

Limites principais (ver GLOSSARIO.md, parte 2.5): botão com ícone 62 px na fonte 10;
telas Mover e Ajuste fino ~82 px; abas Ajustes e Configurações ~90 px.
"""
import os
import re
import sys
import unicodedata


def _array(txt, nome):
    m = re.search(nome + r'\[\]\s*=\s*\{(.*?)\};', txt, re.S)
    if not m:
        return []
    corpo = re.sub(r'/\*.*?\*/', '', m.group(1), flags=re.S)
    return [int(x, 0) for x in re.findall(r'-?(?:0x[0-9a-fA-F]+|\d+)', corpo)]


def carregar(pasta, tamanho):
    txt = open(os.path.join(pasta, f'lv_font_montserrat_{tamanho}.c'), encoding='utf-8').read()
    adv = [int(x) for x in re.findall(r'\.adv_w\s*=\s*(\d+)', txt)]
    esq = _array(txt, 'kern_left_class_mapping')
    dir_ = _array(txt, 'kern_right_class_mapping')
    vals = _array(txt, 'kern_class_values')
    rc = int(re.search(r'\.right_class_cnt\s*=\s*(\d+)', txt).group(1))
    ks = int(re.search(r'\.kern_scale\s*=\s*(\d+)', txt).group(1))
    return adv, esq, dir_, vals, rc, ks


def _gid(ch):
    c = ord(ch)
    if 32 <= c <= 126:
        return c - 31
    base = unicodedata.normalize('NFD', ch)[0]
    if base != ch and 32 <= ord(base) <= 126:
        return ord(base) - 31
    if ch == '°':
        return 96
    return ord('o') - 31  # caractere fora da fonte: aproxima pela largura do "o"


def medir(fonte, texto):
    adv, esq, dir_, vals, rc, ks = fonte
    total = 0
    for i, ch in enumerate(texto):
        g = _gid(ch)
        kv = 0
        if i + 1 < len(texto):
            g2 = _gid(texto[i + 1])
            if g < len(esq) and g2 < len(dir_):
                lc, rcl = esq[g], dir_[g2]
                if lc > 0 and rcl > 0:
                    kv = vals[(lc - 1) * rc + (rcl - 1)]
        total += (adv[g] + ((kv * ks) >> 4) + 8) >> 4
    return total


def main(args):
    if len(args) < 2:
        print(__doc__)
        return 1
    tam = args[0]
    resto = args[1:]
    pasta = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'lvgl', 'src', 'font')
    if resto[0] == '--fontes':
        pasta, resto = resto[1], resto[2:]
    if resto and resto[0] == '--arquivo':
        textos = [l.rstrip('\n') for l in open(resto[1], encoding='utf-8') if l.strip()]
    else:
        textos = resto
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8')
    try:
        fonte = carregar(pasta, tam)
    except OSError:
        print(f'Não achei lv_font_montserrat_{tam}.c em {pasta}. Baixe o submódulo lvgl ou use --fontes.')
        return 1
    for t in textos:
        larguras = [medir(fonte, linha) for linha in t.split('\\n')]
        print(f'{max(larguras):4d} px  {t}')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
