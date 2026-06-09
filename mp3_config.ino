#include "mp3_config.h"

// Definição real — existe uma única vez em toda a compilação
HardwareSerial mp3Serial(2);
DFRobotDFPlayerMini player;

bool dfplayer_is_playing();

void dfplayer_init() {
  // RX = GPIO3, TX = GPIO1 — mantidas conforme original
  mp3Serial.begin(9600, SERIAL_8N1, 3, 1);

  Serial.println("Iniciando DFPlayer...");

  if (!player.begin(mp3Serial)) {
    Serial.println("DFPlayer nao encontrado!");
    while (true); // halt — sem player o jogo não tem sentido
  }

  Serial.println("DFPlayer conectado!");
  player.volume(15);
  delay(1000);
}

void dfplayer_play(uint8_t track) {
  player.play(track);
}

void dfplayer_stop() {
  player.stop();
}

void dfplayer_setVolume(uint8_t vol) {
  player.volume(vol);
}