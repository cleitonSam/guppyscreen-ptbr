#include "avisos_pt.h"

#include <cctype>
#include <vector>

namespace AvisosPt {

namespace {

bool tem(const std::string &s, const char *p) {
  return s.find(p) != std::string::npos;
}

bool comeca(const std::string &s, const char *p) {
  return s.rfind(p, 0) == 0;
}

std::string aparar(const std::string &s) {
  size_t a = 0, b = s.size();
  while (a < b && std::isspace((unsigned char)s[a])) a++;
  while (b > a && std::isspace((unsigned char)s[b - 1])) b--;
  return s.substr(a, b - a);
}

// desfaz \n, \t, \", \', \\ (a Creality manda o texto escapado dentro do JSON)
std::string desescapar(const std::string &s) {
  std::string o;
  o.reserve(s.size());
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '\\' && i + 1 < s.size()) {
      char c = s[i + 1];
      if (c == 'n') { o += '\n'; i++; continue; }
      if (c == 't') { o += ' '; i++; continue; }
      if (c == '"' || c == '\'' || c == '\\' || c == '/') { o += c; i++; continue; }
    }
    o += s[i];
  }
  return o;
}

// {"code":"key1", "msg": "..."} -> "..." (sem depender de o JSON ser válido)
std::string tirar_embrulho(const std::string &s) {
  std::string t = aparar(s);
  if (t.empty() || t[0] != '{') {
    return t;
  }
  size_t k = t.find("\"msg\"");
  if (k == std::string::npos) {
    return t;
  }
  size_t a = t.find('"', t.find(':', k + 5) + 1);
  size_t b = t.rfind('"');
  if (a == std::string::npos || b == std::string::npos || b <= a) {
    return t;
  }
  return desescapar(t.substr(a + 1, b - a - 1));
}

// pega o nome entre aspas simples depois de 'chave' (ex.: Heater 'extruder')
std::string nome_aquecedor(const std::string &s) {
  if (tem(s, "heater_bed") || tem(s, "bed")) return "a mesa";
  if (tem(s, "extruder")) return "o bico";
  return "um aquecedor";
}

std::string eixo_de(const std::string &s) {
  // "No trigger on x after full movement", "Endstop y still triggered"
  for (const char *e : {" x ", " y ", " z ", " x", " y", " z"}) {
    size_t p = s.find(e);
    if (p != std::string::npos) {
      char c = (char)std::toupper((unsigned char)e[1]);
      return std::string("eixo ") + c;
    }
  }
  return "eixo";
}

}  // namespace

std::string limpar(const std::string &linha) {
  std::string t = tirar_embrulho(linha);

  // corta o "Once the underlying issue is corrected..." e o "Printer is shutdown"
  for (const char *fim : {"\nOnce the underlying issue", "\nThis generally indicates",
                          "\nThis often indicates", "\nPrinter is shutdown"}) {
    size_t p = t.find(fim);
    if (p != std::string::npos) {
      t = t.substr(0, p);
    }
  }

  // tira os prefixos técnicos das macros
  for (const char *pre : {"gcode.CommandError: ", "CommandError: ", "Internal error on command:"}) {
    size_t p = t.rfind(pre);
    if (p != std::string::npos) {
      t = t.substr(p + std::string(pre).size());
    }
  }
  if (comeca(t, "Error evaluating '")) {
    size_t p = t.find("': ");
    if (p != std::string::npos) {
      t = t.substr(p + 3);
    }
  }

  // uma linha só
  t = aparar(t);
  size_t nl = t.find('\n');
  if (nl != std::string::npos) {
    t = aparar(t.substr(0, nl));
  }
  if (t.size() > 160) {
    t = t.substr(0, 157) + "...";
  }
  return t;
}

Aviso traduzir(const std::string &linha) {
  std::string t = limpar(linha);
  Aviso a{t, LEVE, false};

  if (t.empty()) {
    a.nivel = IGNORAR;
    return a;
  }

  // passageiro ou repetido por outro caminho
  if (tem(t, "is not ready") || tem(t, "attempting to connect") ||
      t == "Printer is shutdown" || tem(t, "Klippy Disconnected") ||
      tem(t, "Klipper state: Shutdown")) {
    a.nivel = IGNORAR;
    return a;
  }

  if (tem(t, "Must home axis first") || tem(t, "Must home")) {
    a.pede_home = true;
    a.texto = "Primeiro leve os eixos ao início (botão Início).";
    return a;
  }

  // ---- graves: param a impressora ----
  if (tem(t, "Shutdown due to webhooks request") || tem(t, "Emergency stop") ||
      tem(t, "M112")) {
    a.texto = "Parada de emergência. Para voltar, toque em Reiniciar firmware.";
    a.nivel = GRAVE;
    return a;
  }
  if (tem(t, "not heating at expected rate")) {
    a.texto = "Parei por segurança: " + nome_aquecedor(t) +
              " não esquentou como deveria. Confira o cabo e o sensor.";
    a.nivel = GRAVE;
    return a;
  }
  if (tem(t, "ADC out of range") || tem(t, "Thermocouple") || tem(t, "temperature sensor")) {
    a.texto = "Parei por segurança: temperatura fora do limite. Confira os sensores.";
    a.nivel = GRAVE;
    return a;
  }
  if (tem(t, "Timer too close") || tem(t, "timer in the past") ||
      tem(t, "too far in past") || tem(t, "Missed scheduling")) {
    a.texto = "A placa da impressora perdeu o ritmo e parou. Desligue e ligue a impressora.";
    a.nivel = GRAVE;
    return a;
  }
  if (tem(t, "Lost communication with MCU") || tem(t, "Unable to connect") ||
      tem(t, "serialqueue")) {
    a.texto = "Perdi contato com a placa da impressora. Desligue e ligue a impressora.";
    a.nivel = GRAVE;
    return a;
  }
  if (tem(t, "Unhandled exception")) {
    a.texto = "O sistema da impressora teve um erro interno. Desligue e ligue a impressora.";
    a.nivel = GRAVE;
    return a;
  }
  if (tem(t, "Probe triggered prior to movement")) {
    a.texto = "O sensor de nível já estava acionado antes de descer. Confira o pino do sensor.";
    a.nivel = GRAVE;
    return a;
  }
  if (tem(t, "No trigger on probe")) {
    a.texto = "O sensor de nível não encostou na mesa. Confira se a chapa está no lugar.";
    a.nivel = GRAVE;
    return a;
  }
  if (tem(t, "Probe samples exceed")) {
    a.texto = "As medições da mesa variaram demais. Limpe a ponta do bico e tente de novo.";
    a.nivel = GRAVE;
    return a;
  }
  if (tem(t, "Timeout during endstop homing") || tem(t, "No trigger on") ||
      tem(t, "still triggered after retract")) {
    a.texto = "O " + eixo_de(t) + " não achou o fim do curso ao voltar ao início. Confira se nada trava o caminho.";
    a.nivel = GRAVE;
    return a;
  }
  if (tem(t, "TMC") && (tem(t, "error") || tem(t, "Unable to read"))) {
    a.texto = "Um driver de motor deu erro e a impressora parou. Desligue e ligue a impressora.";
    a.nivel = GRAVE;
    return a;
  }
  if (comeca(t, "MCU '") && tem(t, "shutdown")) {
    a.texto = "A placa da impressora parou (" + t + "). Desligue e ligue a impressora.";
    a.nivel = GRAVE;
    return a;
  }

  // ---- leves: aparecem e somem ----
  if (tem(t, "Move out of range")) {
    a.texto = "Esse movimento passa do limite da mesa. Nada foi movido.";
    return a;
  }
  if (tem(t, "Extrude below minimum temp")) {
    a.texto = "Bico frio: aqueça o bico (mínimo 170 °C) antes de mexer no filamento.";
    return a;
  }
  if (tem(t, "Extrude only move too long") || tem(t, "Move exceeds maximum extrusion")) {
    a.texto = "Movimento de filamento grande demais de uma vez. Use uma distância menor.";
    return a;
  }
  if (tem(t, "out of range") && tem(t, "emperature")) {
    a.texto = "Temperatura pedida fora do limite da impressora.";
    return a;
  }
  if (comeca(t, "Unknown command")) {
    size_t p = t.find('"');
    size_t q = p == std::string::npos ? p : t.find('"', p + 1);
    std::string cmd = (p != std::string::npos && q != std::string::npos) ? t.substr(p + 1, q - p - 1) : "";
    a.texto = "Comando desconhecido" + (cmd.empty() ? std::string(".") : (": " + cmd));
    return a;
  }
  if (tem(t, "SD busy") || tem(t, "already printing")) {
    a.texto = "Já existe uma impressão em andamento.";
    return a;
  }
  if (tem(t, "Unable to open file") || tem(t, "File not found")) {
    a.texto = "Não consegui abrir o arquivo de impressão.";
    return a;
  }
  if (tem(t, "not paused") || tem(t, "already paused") || tem(t, "Already paused")) {
    a.texto = tem(t, "not paused") ? "Não há impressão pausada." : "A impressão já está pausada.";
    return a;
  }

  // qualquer outro: mostra o texto limpo e some sozinho
  return a;
}

}  // namespace AvisosPt
