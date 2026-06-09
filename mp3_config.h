#pragma once
#include "DFRobotDFPlayerMini.h"

// Declara a existência das variáveis — definição está em dfplayer_config.ino
extern HardwareSerial mp3Serial;
extern DFRobotDFPlayerMini player;

bool dfplayer_is_playing();

// Funções do módulo de áudio
void dfplayer_init();
void dfplayer_play(uint8_t track);
void dfplayer_stop();
void dfplayer_setVolume(uint8_t vol); // 0–30