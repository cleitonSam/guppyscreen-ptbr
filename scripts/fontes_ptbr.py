#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Ajudante do scripts/gerar-fontes-ptbr.sh (fontes com acentos, pt-BR).

Usa só a biblioteca padrão do Python 3 (3.6 ou mais novo). Comandos:

  sha512   ARQUIVO                 mostra o "integrity" (sha512-...) do arquivo
  sha256   ARQUIVO                 mostra o SHA-256 do arquivo
  extrair  PACOTE.tgz PASTA        extrai um pacote .tgz com segurança
  ajustar  --tamanho N --gerado G --original O --saida S [--altura-natural]
           ajusta a fonte gerada pelo lv_font_conv (chave de liga/desliga e
           altura da linha), compara com a fonte embutida do LVGL e grava S
  conferir [--raiz PASTA] [--fontes PASTA] [--faixas FAIXAS]
           procura, nos textos entre aspas de src/, letras que as fontes
           novas não têm (elas sumiriam da tela)

Detalhes em traducao/FONTES.md.
"""

import argparse
import base64
import hashlib
import os
import re
import sys
import tarfile
import unicodedata

CHAVE = "GUPPY_FONTES_PTBR"  # chave de liga/desliga (definida no lv_conf.h)
TAM_DESCRITOR = 16  # bytes de cada glifo com LV_FONT_FMT_TXT_LARGE = 1 (lv_conf.h)


class ErroFonte(Exception):
    pass


def _saida_utf8():
    # UTF-8 e fim de linha "\n" também no Windows (o bash compara estas saídas).
    for fluxo in (sys.stdout, sys.stderr):
        try:
            fluxo.reconfigure(encoding="utf-8", errors="replace", newline="\n")
        except (AttributeError, ValueError):
            pass


def ler_texto(caminho):
    with open(caminho, encoding="utf-8") as f:
        return f.read()


def gravar_texto(caminho, texto):
    with open(caminho, "w", encoding="utf-8", newline="\n") as f:
        f.write(texto)


def mostrar_letra(cp):
    """Letra legível para o relatório (espaços e invisíveis viram U+XXXX)."""
    ch = chr(cp)
    if cp <= 0x20 or cp in (0xA0, 0xAD) or unicodedata.category(ch).startswith(("C", "Z")):
        return "U+%04X" % cp
    return ch


# ---------------------------------------------------------------------------
# Leitura de uma fonte no formato do LVGL (arquivo .c do lv_font_conv)
# ---------------------------------------------------------------------------

_RE_COMENTARIO = re.compile(r"/\*\s*U\+([0-9A-Fa-f]{2,6})\b[^\n]*?\*/")
_RE_NUMERO = re.compile(r"-?(?:0x[0-9A-Fa-f]+|\d+)")


def _bloco(texto, nome):
    m = re.search(r"\b" + nome + r"\[\]\s*=\s*\{(.*?)\n\s*\};", texto, re.S)
    return m.group(1) if m else None


def _lista(texto, nome):
    bloco = _bloco(texto, nome)
    if bloco is None:
        return None
    bloco = re.sub(r"/\*.*?\*/", " ", bloco, flags=re.S)
    return [int(x, 0) for x in _RE_NUMERO.findall(bloco)]


def _campo(texto, nome, obrigatorio=True):
    achados = re.findall(r"\." + nome + r"\s*=\s*(-?\d+)", texto)
    if not achados:
        if obrigatorio:
            raise ErroFonte("campo .%s não encontrado" % nome)
        return None
    return int(achados[-1])


class FonteLVGL(object):
    """Lê o que importa de um .c de fonte do LVGL (formato do lv_font_conv)."""

    def __init__(self, caminho):
        self.caminho = caminho
        self.texto = ler_texto(caminho)
        t = self.texto

        m = re.search(r"#ifndef\s+(\w+)\s*\n\s*#define\s+\1\s+1\s*\n\s*#endif\s*\n\s*#if\s+\1\b", t)
        if not m:
            raise ErroFonte("não achei a chave #ifndef/#define/#if no começo da fonte")
        self.chave = m.group(1)

        m = re.search(r"\blv_font_t\s+(\w+)\s*=\s*\{", t)
        if not m:
            raise ErroFonte("não achei a definição lv_font_t da fonte")
        self.nome = m.group(1)

        # Desenhos dos glifos: cada um vem depois do comentário /* U+XXXX "x" */,
        # na mesma ordem dos ids (1, 2, 3...).
        bloco = _bloco(t, "(?:glyph|gylph)_bitmap")
        if bloco is None:
            raise ErroFonte("não achei glyph_bitmap[]")
        partes = _RE_COMENTARIO.split(bloco)
        self.codigos = []
        self.desenho = {}
        self.bytes_desenho = 0
        for i in range(1, len(partes), 2):
            cp = int(partes[i], 16)
            dados = [int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{1,2})\b", partes[i + 1])]
            if cp in self.desenho:
                raise ErroFonte("letra U+%04X repetida" % cp)
            self.codigos.append(cp)
            self.desenho[cp] = dados
            self.bytes_desenho += len(dados)
        if not self.codigos:
            raise ErroFonte("glyph_bitmap[] sem glifos")

        bloco = _bloco(t, "glyph_dsc")
        if bloco is None:
            raise ErroFonte("não achei glyph_dsc[]")
        entradas = []
        for e in re.findall(r"\{([^{}]*)\}", bloco):
            entradas.append(dict((k, int(v)) for k, v in re.findall(r"\.(\w+)\s*=\s*(-?\d+)", e)))
        if len(entradas) != len(self.codigos) + 1:
            raise ErroFonte("glyph_dsc[] tem %d itens, esperava %d" % (len(entradas), len(self.codigos) + 1))
        self.id_de = {}
        self.dsc = {}
        for i, cp in enumerate(self.codigos):
            self.id_de[cp] = i + 1
            self.dsc[cp] = entradas[i + 1]
        self.bpp = _campo(t, "bpp")
        self.bitmap_format = _campo(t, "bitmap_format")
        if self.bitmap_format == 0:  # sem compressão: o desenho tem tamanho exato
            for cp in self.codigos:
                d = self.dsc[cp]
                esperado = (d["box_w"] * d["box_h"] * self.bpp + 7) // 8
                if len(self.desenho[cp]) != esperado:
                    raise ErroFonte("desenho de U+%04X com %d bytes, esperava %d"
                                    % (cp, len(self.desenho[cp]), esperado))

        self.cmaps = []
        for m in re.finditer(
            r"\.range_start\s*=\s*(\d+)\s*,\s*\.range_length\s*=\s*(\d+)\s*,\s*\.glyph_id_start\s*=\s*(\d+)\s*,"
            r"\s*\.unicode_list\s*=\s*(\w+)\s*,\s*\.glyph_id_ofs_list\s*=\s*(\w+)\s*,\s*\.list_length\s*=\s*(\d+)\s*,"
            r"\s*\.type\s*=\s*(\w+)", t):
            self.cmaps.append({
                "inicio": int(m.group(1)), "tamanho": int(m.group(2)), "id_inicio": int(m.group(3)),
                "unicode_list": None if m.group(4) == "NULL" else _lista(t, m.group(4)),
                "ofs_list": None if m.group(5) == "NULL" else _lista(t, m.group(5)),
                "itens": int(m.group(6)), "tipo": m.group(7),
            })
        if not self.cmaps:
            raise ErroFonte("não achei cmaps[]")
        for cp in self.codigos:  # confere a leitura: o mapa de letras tem que bater
            if self.id_pelo_cmap(cp) != self.id_de[cp]:
                raise ErroFonte("mapa de letras não bate para U+%04X" % cp)

        self.line_height = _campo(t, "line_height")
        self.base_line = _campo(t, "base_line")
        self.underline_position = _campo(t, "underline_position", False)
        self.underline_thickness = _campo(t, "underline_thickness", False)
        self.kern_scale = _campo(t, "kern_scale")

        self.kern_classes = _campo(t, "kern_classes")
        self.kern_esq = self.kern_dir = self.kern_val = None
        self.kern_pares = None
        if self.kern_classes == 1:
            self.kern_esq = _lista(t, "kern_left_class_mapping")
            self.kern_dir = _lista(t, "kern_right_class_mapping")
            self.kern_val = _lista(t, "kern_class_values")
            self.n_esq = _campo(t, "left_class_cnt")
            self.n_dir = _campo(t, "right_class_cnt")
            if None in (self.kern_esq, self.kern_dir, self.kern_val):
                raise ErroFonte("tabelas de kerning por classe incompletas")
            if len(self.kern_val) != self.n_esq * self.n_dir:
                raise ErroFonte("kern_class_values com tamanho errado")
        elif _bloco(t, "kern_pair_glyph_ids") is not None:
            ids = _lista(t, "kern_pair_glyph_ids")
            vals = _lista(t, "kern_pair_values")
            self.kern_pares = dict(((ids[2 * i], ids[2 * i + 1]), vals[i]) for i in range(len(vals)))

    def id_pelo_cmap(self, cp):
        for c in self.cmaps:
            rcp = cp - c["inicio"]
            if rcp < 0 or rcp >= c["tamanho"]:
                continue
            if c["tipo"].endswith("FORMAT0_TINY"):
                return c["id_inicio"] + rcp
            if c["tipo"].endswith("FORMAT0_FULL"):
                return c["id_inicio"] + c["ofs_list"][rcp]
            lista = c["unicode_list"] or []
            if rcp in lista:
                i = lista.index(rcp)
                if c["tipo"].endswith("SPARSE_TINY"):
                    return c["id_inicio"] + i
                return c["id_inicio"] + c["ofs_list"][i]
        return None

    def kerning(self, a, b):
        """Valor de kerning do par (a, b), como o LVGL calcula (já com a escala)."""
        ia, ib = self.id_de.get(a), self.id_de.get(b)
        if ia is None or ib is None:
            return 0
        v = 0
        if self.kern_classes == 1:
            ce, cd = self.kern_esq[ia], self.kern_dir[ib]
            if ce > 0 and cd > 0:
                v = self.kern_val[(ce - 1) * self.n_dir + (cd - 1)]
        elif self.kern_pares:
            v = self.kern_pares.get((ia, ib), 0)
        return v * self.kern_scale

    def topo_e_fundo(self, codigos=None):
        """(maior topo, menor fundo) em pixels a partir da linha de base."""
        topo, fundo = None, None
        for cp in (codigos or self.codigos):
            d = self.dsc[cp]
            if d["box_h"] == 0 or d["box_w"] == 0:
                continue
            t, f = d["ofs_y"] + d["box_h"], d["ofs_y"]
            topo = t if topo is None else max(topo, t)
            fundo = f if fundo is None else min(fundo, f)
        return topo, fundo

    def bytes_dados(self):
        """Estimativa dos bytes de dados da fonte dentro do programa."""
        n = len(self.codigos) + 1
        total = self.bytes_desenho + n * TAM_DESCRITOR
        for c in self.cmaps:
            total += 24
            if c["unicode_list"]:
                total += 2 * len(c["unicode_list"])
            if c["ofs_list"]:
                total += (1 if c["tipo"].endswith("FORMAT0_FULL") else 2) * len(c["ofs_list"])
        if self.kern_classes == 1:
            total += len(self.kern_esq) + len(self.kern_dir) + len(self.kern_val)
        elif self.kern_pares:
            total += 3 * len(self.kern_pares)
        return total


# ---------------------------------------------------------------------------
# Comando "ajustar"
# ---------------------------------------------------------------------------

def _trocar_linha(texto, campo, valor, nota):
    padrao = re.compile(r"^(\s*\." + campo + r"\s*=\s*)-?\d+,[^\n]*$", re.M)
    achados = padrao.findall(texto)
    if len(achados) != 1:
        raise ErroFonte("esperava 1 linha .%s, achei %d" % (campo, len(achados)))
    return padrao.sub(lambda m: m.group(1) + "%d,  /*%s*/" % (valor, nota), texto)


def cmd_ajustar(args):
    n = args.tamanho
    nova = FonteLVGL(args.gerado)
    orig = FonteLVGL(args.original)
    nome_esperado = "lv_font_montserrat_%d" % n
    if nova.nome != nome_esperado or orig.nome != nome_esperado:
        raise ErroFonte("nome da fonte inesperado: %s / %s" % (nova.nome, orig.nome))
    if nova.bpp != orig.bpp or nova.bitmap_format != orig.bitmap_format:
        raise ErroFonte("bpp ou compressão diferente da fonte embutida")

    avisos = []
    linhas = []

    # 1) Letras da fonte embutida: têm que continuar todas lá e iguais.
    faltando = [cp for cp in orig.codigos if cp not in nova.desenho]
    if faltando:
        raise ErroFonte("a fonte nova perdeu letras da original: %s" % ", ".join("U+%04X" % c for c in faltando[:10]))
    campos = ("adv_w", "box_w", "box_h", "ofs_x", "ofs_y")
    diferentes = [cp for cp in orig.codigos
                  if any(orig.dsc[cp][k] != nova.dsc[cp][k] for k in campos) or orig.desenho[cp] != nova.desenho[cp]]
    novos = [cp for cp in nova.codigos if cp not in orig.desenho]
    linhas.append("letras: %d iguais às da fonte embutida, %d diferentes, %d novas"
                  % (len(orig.codigos) - len(diferentes), len(diferentes), len(novos)))
    if diferentes:
        avisos.append("%d letras/ícones saíram com desenho diferente do original (%s). Funciona, mas o "
                      "texto sem acento pode mudar alguns pixels; confira a versão do lv_font_conv."
                      % (len(diferentes), " ".join(mostrar_letra(c) for c in diferentes[:12])))

    # 2) Kerning (ajuste entre pares de letras) das letras originais.
    pares_dif = 0
    for a in orig.codigos:
        for b in orig.codigos:
            if orig.kerning(a, b) != nova.kerning(a, b):
                pares_dif += 1
    linhas.append("kerning entre as letras originais: %s" % ("igual" if pares_dif == 0 else "%d pares diferentes" % pares_dif))
    if pares_dif:
        avisos.append("%d pares de letras com espaçamento diferente do original." % pares_dif)

    # 3) Altura da linha. O lv_font_conv 1.5.x calcula a altura pelo glifo mais
    #    alto e pelo mais baixo; com Á, Ç etc. ela cresce 1 a 3 px. Por padrão a
    #    altura volta a ser a da fonte embutida, para nada mudar de lugar na tela.
    alt_nat, base_nat = nova.line_height, nova.base_line
    texto = nova.texto
    if args.altura_natural:
        alt, base = alt_nat, base_nat
        linhas.append("altura da linha: %d (base %d), a natural; a embutida usa %d (base %d)"
                      % (alt, base, orig.line_height, orig.base_line))
    else:
        alt, base = orig.line_height, orig.base_line
        texto = _trocar_linha(texto, "line_height", alt,
                              "pt-BR: igual à fonte embutida do LVGL (natural: %d)" % alt_nat)
        texto = _trocar_linha(texto, "base_line", base,
                              "pt-BR: igual à fonte embutida do LVGL (natural: %d)" % base_nat)
        linhas.append("altura da linha: %d (base %d), igual à embutida; a natural seria %d (base %d)"
                      % (alt, base, alt_nat, base_nat))
    acima = alt - base
    sobra_cima = [(nova.dsc[c]["ofs_y"] + nova.dsc[c]["box_h"] - acima, c) for c in nova.codigos
                  if nova.dsc[c]["box_h"] and nova.dsc[c]["ofs_y"] + nova.dsc[c]["box_h"] > acima]
    sobra_baixo = [(-base - nova.dsc[c]["ofs_y"], c) for c in nova.codigos
                   if nova.dsc[c]["box_h"] and nova.dsc[c]["ofs_y"] < -base]
    margem = alt // 4  # área extra que o rótulo (lv_label) do LVGL 8.3 desenha em volta
    for nome_lado, sobras in (("acima", sobra_cima), ("abaixo", sobra_baixo)):
        if not sobras:
            continue
        maior = max(s for s, _ in sobras)
        letras = " ".join(mostrar_letra(c) for s, c in sorted(sobras, key=lambda x: (-x[0], x[1]))[:16])
        linhas.append("passam da linha %s: até %d px (%s); margem extra do rótulo: %d px" % (nome_lado, maior, letras, margem))
        if maior > margem:
            avisos.append("letras passam %d px %s da linha, mais que a margem de %d px: podem sair cortadas. "
                          "Use --altura-natural." % (maior, nome_lado, margem))

    # 4) Outros números que devem continuar iguais.
    for campo in ("kern_scale", "underline_position", "underline_thickness"):
        if getattr(orig, campo) != getattr(nova, campo):
            avisos.append("%s mudou: %s -> %s" % (campo, getattr(orig, campo), getattr(nova, campo)))

    # 5) Caminho do lvgl.h: "lvgl/lvgl.h" (o padrão do lv_font_conv) funciona a partir
    #    de fonts_ptbr/ com o -I./ do Makefile; o "../../lvgl.h" das embutidas não.
    texto, n_inc = re.subn(
        r'(#ifdef\s+LV_LVGL_H_INCLUDE_SIMPLE\s*\n\s*#include\s+"lvgl\.h"\s*\n\s*#else\s*\n\s*#include\s+)"[^"\n]+"',
        r'\1"lvgl/lvgl.h"', texto, count=1)
    if n_inc != 1:
        raise ErroFonte('não achei o #include do lvgl.h no começo da fonte')

    # 6) Chave de liga/desliga: LV_FONT_MONTSERRAT_N -> GUPPY_FONTES_PTBR.
    texto, trocas = re.subn(r"\b%s\b" % re.escape(nova.chave), CHAVE, texto)
    if trocas < 4 or not re.search(r"#ifndef\s+%s\s*\n\s*#define\s+%s\s+1\s*\n\s*#endif\s*\n\s*#if\s+%s\b" % ((CHAVE,) * 3), texto):
        raise ErroFonte("não consegui trocar a chave %s" % nova.chave)

    # 7) Cabeçalho explicando a origem do arquivo.
    faixa = ", ".join(mostrar_letra(c) for c in novos if c >= 0x2000)
    cab = [
        "/*******************************************************************************",
        " * pt-BR: arquivo GERADO por scripts/gerar-fontes-ptbr.sh. Não edite à mão.",
        " * Montserrat %d px igual à fonte embutida do LVGL 8.3.11 (mesmo conversor," % n,
        " * lv_font_conv %s, e mesmas opções e ícones), com %d letras a mais:" % (args.versao_conversor, len(novos)),
        " * acentos e símbolos do Latin-1 (á é í ó ú â ê ô ã õ à ç º ª ...)%s." % (" e " + faixa if faixa else ""),
        " * Mudanças depois da conversão:",
        " *  - a chave de liga/desliga é %s (veja lv_conf.h e traducao/FONTES.md);" % CHAVE,
    ]
    if args.altura_natural:
        cab.append(" *  - altura da linha natural (%d, base %d); a embutida usa %d (base %d)."
                   % (alt, base, orig.line_height, orig.base_line))
    else:
        cab.append(" *  - altura da linha mantida igual à embutida (%d, base %d) para a tela não"
                   % (alt, base))
        cab.append(" *    mudar de lugar; a natural seria %d (base %d)." % (alt_nat, base_nat))
    cab.append(" ******************************************************************************/")
    m = re.search(r"^ \*{20,}/[ \t]*\n", texto, re.M)  # fim do cabeçalho do lv_font_conv
    if not m:
        raise ErroFonte("não achei o cabeçalho do lv_font_conv")
    texto = texto[:m.end()] + "\n" + "\n".join(cab) + "\n" + texto[m.end():]
    if not texto.endswith("\n"):
        texto += "\n"

    gravar_texto(args.saida, texto)
    conferida = FonteLVGL(args.saida)  # relê o arquivo gravado para garantir que está inteiro
    if (conferida.chave != CHAVE or conferida.line_height != alt or conferida.base_line != base
            or '#include "lvgl/lvgl.h"' not in conferida.texto or conferida.codigos != nova.codigos):
        raise ErroFonte("o arquivo gravado não ficou como esperado")

    antes, depois = orig.bytes_dados(), nova.bytes_dados()
    linhas.append("tamanho dos dados: %.1f KB -> %.1f KB (+%.1f KB)" % (antes / 1024.0, depois / 1024.0, (depois - antes) / 1024.0))

    print("  Montserrat %d px:" % n)
    for l in linhas:
        print("    - " + l)
    for a in avisos:
        print("    ! AVISO: " + a)
    print("  ##DADOS %d %d %d %d" % (n, antes, depois, len(avisos)))
    return 0


# ---------------------------------------------------------------------------
# Comando "conferir": letras usadas nos textos x letras das fontes
# ---------------------------------------------------------------------------

def _letra_de_nome(ch):
    return ch.isalnum() or ch == "_"


def _texto_cru(texto, i):
    """True se a aspa em texto[i] abre um texto cru do C++: R"(...)", u8R"(...)" etc."""
    if i == 0 or texto[i - 1] != "R":
        return False
    for prefixo in ("u8", "u", "U", "L", ""):
        k = i - 1 - len(prefixo)
        if k >= 0 and texto[k:i - 1] == prefixo and (k == 0 or not _letra_de_nome(texto[k - 1])):
            return True
    return False


def _literais(texto):
    """Devolve (linha, conteúdo) de cada texto entre aspas de um arquivo C/C++,
    pulando comentários. Lida com R"(...)" e com o separador 1'000 do C++."""
    i, n, linha = 0, len(texto), 1
    while i < n:
        c = texto[i]
        if c == "\n":
            linha += 1
            i += 1
        elif texto.startswith("//", i):
            fim = texto.find("\n", i)
            i = n if fim < 0 else fim
        elif texto.startswith("/*", i):
            fim = texto.find("*/", i + 2)
            fim = n if fim < 0 else fim + 2
            linha += texto.count("\n", i, fim)
            i = fim
        elif c == '"' and _texto_cru(texto, i):
            abre = texto.find("(", i + 1)
            if abre < 0:
                i += 1
                continue
            delim = texto[i + 1:abre]
            fecha = texto.find(")" + delim + '"', abre + 1)
            fecha = n if fecha < 0 else fecha
            yield linha, texto[abre + 1:fecha]
            linha += texto.count("\n", i, fecha)
            i = fecha + len(delim) + 2
        elif c in "\"'":
            if c == "'" and i > 0 and _letra_de_nome(texto[i - 1]):
                k = i
                while k > 0 and _letra_de_nome(texto[k - 1]):
                    k -= 1
                if texto[k:i] not in ("u", "U", "L", "u8"):
                    i += 1  # separador de dígitos do C++ (1'000), não é letra entre aspas
                    continue
            j = i + 1
            partes = []
            while j < n and texto[j] != c and texto[j] != "\n":
                if texto[j] == "\\" and j + 1 < n:
                    m = re.match(r"\\u([0-9A-Fa-f]{4})|\\U([0-9A-Fa-f]{8})", texto[j:j + 10])
                    if m:
                        partes.append(chr(int(m.group(1) or m.group(2), 16)))
                        j += len(m.group(0))
                        continue
                    partes.append(texto[j:j + 2])
                    j += 2
                    continue
                partes.append(texto[j])
                j += 1
            yield linha, "".join(partes)
            i = j + 1
        else:
            i += 1


def _faixas_para_conjunto(faixas):
    conjunto = set()
    for item in faixas.split(","):
        item = item.strip()
        if not item:
            continue
        if "-" in item:
            a, b = item.split("-", 1)
            conjunto.update(range(int(a, 0), int(b, 0) + 1))
        else:
            conjunto.add(int(item, 0))
    return conjunto


def _faixas_do_script(raiz):
    caminho = os.path.join(raiz, "scripts", "gerar-fontes-ptbr.sh")
    m = re.search(r'^FAIXAS_TEXTO="([^"]+)"', ler_texto(caminho), re.M)
    if not m:
        raise ErroFonte("não achei FAIXAS_TEXTO em %s" % caminho)
    return m.group(1)


def cmd_conferir(args):
    raiz = os.path.abspath(args.raiz)
    pasta_fontes = os.path.join(raiz, args.fontes)
    arquivos = sorted(f for f in (os.listdir(pasta_fontes) if os.path.isdir(pasta_fontes) else [])
                      if re.match(r"lv_font_montserrat_\d+\.c$", f))
    if arquivos and not args.faixas:
        tem = None
        for f in arquivos:
            codigos = set(int(x, 16) for x in _RE_COMENTARIO.findall(ler_texto(os.path.join(pasta_fontes, f))))
            tem = codigos if tem is None else (tem & codigos)
        origem = "as %d fontes de %s" % (len(arquivos), args.fontes)
    else:
        faixas = args.faixas or _faixas_do_script(raiz)
        tem = _faixas_para_conjunto(faixas)
        origem = "a lista de letras %s (%s)" % ("informada" if args.faixas else "do script", faixas)

    usados = {}
    for pasta, _, nomes in os.walk(os.path.join(raiz, "src")):
        for nome in sorted(nomes):
            if not nome.endswith((".c", ".cc", ".cpp", ".h", ".hpp")):
                continue
            caminho = os.path.join(pasta, nome)
            try:
                texto = ler_texto(caminho)
            except UnicodeDecodeError:
                print("  ! %s não está em UTF-8: os acentos dele vão aparecer errado na tela."
                      % os.path.relpath(caminho, raiz))
                continue
            for linha, conteudo in _literais(texto):
                for ch in conteudo:
                    cp = ord(ch)
                    if cp < 0x80:
                        continue
                    usados.setdefault(cp, []).append("%s:%d" % (os.path.relpath(caminho, raiz).replace(os.sep, "/"), linha))

    icones = sorted(cp for cp in usados if 0xE000 <= cp <= 0xF8FF or cp >= 0xF0000)
    faltam = sorted(cp for cp in usados if cp not in tem and cp not in icones)
    ok = sorted(cp for cp in usados if cp in tem)
    print("  Conferindo os textos de src/ contra %s:" % origem)
    print("    - letras fora do ASCII usadas nos textos: %d (%s)" % (len(ok), " ".join(mostrar_letra(c) for c in ok) or "nenhuma"))
    if icones:
        print("    - ícones (desenhados por outras fontes, não conferidos): %d" % len(icones))
    if not faltam:
        print("    - tudo certo: nenhuma letra dos textos falta nas fontes.")
        return 0
    print("    ! FALTAM nas fontes (iriam sumir da tela):")
    for cp in faltam:
        try:
            nome = unicodedata.name(chr(cp))
        except ValueError:
            nome = "sem nome"
        lugares = usados[cp]
        print("      U+%04X %s (%s) em %s%s" % (cp, mostrar_letra(cp), nome, ", ".join(lugares[:4]),
                                               " e mais %d" % (len(lugares) - 4) if len(lugares) > 4 else ""))
    print("      Troque por uma letra que existe, ou ponha o código em FAIXAS_TEXTO no")
    print("      scripts/gerar-fontes-ptbr.sh e gere as fontes de novo.")
    return 1


# ---------------------------------------------------------------------------
# Comandos pequenos
# ---------------------------------------------------------------------------

def cmd_sha512(args):
    h = hashlib.sha512()
    with open(args.arquivo, "rb") as f:
        for pedaco in iter(lambda: f.read(1 << 16), b""):
            h.update(pedaco)
    print("sha512-" + base64.b64encode(h.digest()).decode("ascii"))
    return 0


def cmd_sha256(args):
    h = hashlib.sha256()
    with open(args.arquivo, "rb") as f:
        for pedaco in iter(lambda: f.read(1 << 16), b""):
            h.update(pedaco)
    print(h.hexdigest())
    return 0


def cmd_extrair(args):
    destino = os.path.abspath(args.pasta)
    with tarfile.open(args.pacote, "r:gz") as tar:
        membros = []
        for m in tar.getmembers():
            alvo = os.path.abspath(os.path.join(destino, m.name))
            if not (alvo == destino or alvo.startswith(destino + os.sep)):
                raise ErroFonte("caminho suspeito no pacote: %s" % m.name)
            if not (m.isfile() or m.isdir()):
                raise ErroFonte("item não esperado no pacote: %s" % m.name)
            membros.append(m)
        for m in membros:
            alvo = os.path.join(destino, m.name)
            if m.isdir():
                os.makedirs(alvo, exist_ok=True)
                continue
            os.makedirs(os.path.dirname(alvo), exist_ok=True)
            origem = tar.extractfile(m)
            with open(alvo, "wb") as f:
                f.write(origem.read())
    return 0


def main(argv=None):
    _saida_utf8()
    p = argparse.ArgumentParser(description="Ajudante das fontes com acentos (pt-BR) do GuppyScreen.")
    sub = p.add_subparsers(dest="comando")

    s = sub.add_parser("sha512")
    s.add_argument("arquivo")
    s.set_defaults(funcao=cmd_sha512)

    s = sub.add_parser("sha256")
    s.add_argument("arquivo")
    s.set_defaults(funcao=cmd_sha256)

    s = sub.add_parser("extrair")
    s.add_argument("pacote")
    s.add_argument("pasta")
    s.set_defaults(funcao=cmd_extrair)

    s = sub.add_parser("ajustar")
    s.add_argument("--tamanho", type=int, required=True)
    s.add_argument("--gerado", required=True)
    s.add_argument("--original", required=True)
    s.add_argument("--saida", required=True)
    s.add_argument("--altura-natural", action="store_true")
    s.add_argument("--versao-conversor", default="1.5.2")
    s.set_defaults(funcao=cmd_ajustar)

    s = sub.add_parser("conferir")
    s.add_argument("--raiz", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
    s.add_argument("--fontes", default="fonts_ptbr")
    s.add_argument("--faixas", default=None)
    s.set_defaults(funcao=cmd_conferir)

    args = p.parse_args(argv)
    if not getattr(args, "funcao", None):
        p.print_help()
        return 2
    try:
        return args.funcao(args)
    except (ErroFonte, OSError, tarfile.TarError, ValueError) as e:
        print("  ERRO: %s" % e, file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
