#include <Adafruit_GFX.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "game_types.h"

// ── Dimensões do painel ───────────────────────────────────────────────────────

#define PANEL_WIDTH  64   // largura em pixels
#define PANEL_HEIGHT 32   // altura em pixels
#define PANELS_NUM    1   // número de painéis encadeados

// ── Estado do Background (Matrix Rain) ───────────────────────────────────────
#define MATRIX_COLUMNS (PANEL_WIDTH / 5) // Espaçamento entre colunas

struct MatrixDrop {
    int y;
    int speed;
};

MatrixDrop matrix_drops[MATRIX_COLUMNS];
unsigned long last_matrix_update = 0;
bool matrix_initialized = false;

// Adicione junto às outras variáveis globais no topo
bool exibir_resultado = false; 

// Adicione a nova cor estática
static uint16_t COR_NEUTRA  = 0;

// ── Variáveis Globais de Jogo (Declaradas via extern) ─────────────────────────
extern int vidas;
extern int movimentos;

// ── Configuração da biblioteca ────────────────────────────────────────────────

HUB75_I2S_CFG::i2s_pins _pins = {
    25,  // R1
    26,  // G1
    27,  // B1
    14,  // R2
    12,  // G2
    13,  // B2
    23,  // A
    19,  // B
     5,  // C
    17,  // D
    -1,  // E
     4,  // LAT
    15,  // OE
    16   // CLK
};

HUB75_I2S_CFG mxconfig(
    PANEL_WIDTH,
    PANEL_HEIGHT,
    PANELS_NUM,
    _pins
);

MatrixPanel_I2S_DMA *display = nullptr;

// ── Velocidades de teste ──────────────────────────────────────────────────────

#define DELAY_COR       1500
#define DELAY_PIXEL       10
#define DELAY_ARCO_IRIS    5

// ── 0. LAYOUTS E MÉTODOS PADRÕES ──────────────────────────────────────────────

#define NUM_INPUTS 6

void desenharFaseTutorialAND();
void desenharFaseTutorialOR();
void desenharFaseTutorialNOT();
void desenharFase1();
void desenharFase2();
void desenharFase3();
void desenharFase4();
void desenharFase5();
void desenharFase6();
void desenharFase7();
void desenharFase8();
void desenharFase9();
void desenharFase10();

typedef void (*FaseFn)();
FaseFn fase_screens[] = {
    desenharFaseTutorialAND,
    desenharFaseTutorialOR,
    desenharFaseTutorialNOT,
    desenharFase1,
    desenharFase2,
    desenharFase3,
    desenharFase4,
    desenharFase5,
    desenharFase6,
    desenharFase7,
    desenharFase8,
    desenharFase9,
    desenharFase10
};

typedef void (*ScreenFn)();
ScreenFn reading_screens[] = {
    bem_vindo,
    instrucoes_eixo_x,
    instrucoes_eixo_y,
    instrucoes_botao_sw,
    instrucoes_cores
};
const int NUM_SCREENS = sizeof(reading_screens) / sizeof(reading_screens[0]);

GameMode game_mode     = MODE_READING;
int      current_screen = 0;

short int matriz[32][64];

static uint16_t COR_ATIVO   = 0;
static uint16_t COR_INATIVO = 0;
static uint16_t COR_FUNDO   = 0;
static uint16_t COR_PORTA   = 0;

void inicializarMatriz() {
  for(int y = 0; y < 32; y++) {
    for(int x = 0; x < 64; x++) {
      matriz[y][x] = 3;
    }
  }
}

void iniciarCoresFase() {
    COR_ATIVO   = display->color565(0,   220, 0);    // Verde (Nível Lógico ALTO)
    COR_INATIVO = display->color565(180, 0,   0);    // Vermelho (Nível Lógico BAIXO)
    COR_FUNDO   = display->color565(0,   0,   0);    // Preto
    COR_PORTA   = display->color565(255, 255, 255);  // Branco
    COR_NEUTRA  = display->color565(0,   0, 255);    // Azul (Fios ocultos)
}

// ── Helpers internos ──────────────────────────────────────────────────────────

static inline void MP(int col, int row, short int val) {
    if (col >= 0 && col < 64 && row >= 0 && row < 32)
        matriz[row][col] = val;
}

static void MH(int col_ini, int col_fim, int row, short int val) {
    for (int c = col_ini; c <= col_fim; c++) MP(c, row, val);
}

static void MV(int col, int row_ini, int row_fim, short int val) {
    int step = (row_fim >= row_ini) ? 1 : -1;
    for (int r = row_ini; r != row_fim + step; r += step) MP(col, r, val);
}

void mpAND(int col, int row, short int val) {
    MH(col,   col+3, row,   val);
    MH(col,   col+3, row+4, val);
    MV(col,   row+1, row+3, val);
    MV(col+4, row+1, row+3, val);
}

void mpOR(int col, int row, short int val) {
    MH(col, col+4, row,   val);
    MH(col, col+4, row+6, val);
    MV(col+1, row+1, row+6, val);

    MP(col+5, row+1, val);
    MP(col+6, row+2, val);
    MP(col+7, row+3, val);
    MP(col+6, row+4, val);
    MP(col+5, row+5, val);
}

void mpNOT(int col, int row, short int val) {
    MP(col,   row,   val);
    MH(col,   col+1, row+1, val);
    MH(col,   col+2, row+2, val);
    MH(col,   col+1, row+3, val);  
    MP(col,   row+4, val);
}

void renderizarComCores() {
    for (int row = 0; row < 32; row++) {
        for (int col = 0; col < 64; col++) {
            uint16_t cor = COR_FUNDO;
            
            // Fios (0 = inativo/falso, 1 = ativo/verdadeiro)
            if (matriz[row][col] == 0 || matriz[row][col] == 1) {
                if (exibir_resultado) {
                    // Revela a propagação real do circuito
                    cor = (matriz[row][col] == 1) ? COR_ATIVO : COR_INATIVO;
                } else {
                    // Mascara tudo como azul enquanto o jogador pensa
                    cor = COR_NEUTRA;
                }
            } 
            // Portas lógicas
            else if (matriz[row][col] == 2) {
                cor = COR_PORTA;
            }

            display->drawPixel(col, row, cor);
        }
    }
}

void reading_next() {
    current_screen++;
    if (current_screen >= NUM_SCREENS) {
        game_mode      = MODE_PLAYING;
        current_screen = 0;
        display->clearScreen();
        desenharFaseAtual();
    } else {
        display->clearScreen();
        reading_screens[current_screen]();
    }
}

void reading_prev() {
    if (current_screen > 0) {
        current_screen--;
        display->clearScreen();
        reading_screens[current_screen]();
    }
}

void reading_show_current() {
    display->clearScreen();
    reading_screens[current_screen]();
}

void borda_branca() {
    uint16_t branco = display->color565(255, 255, 255);
    display->drawRect(0, 0, display->width(), display->height(), branco);
}

void desenharFaseAtual() {
    display->clearScreen();
    if (current_phase >= 1 && current_phase <= NUM_PHASES) {
        fase_screens[current_phase - 1]();
    }
}

void desenharNumeroFase(int fase) {
    uint16_t branco = display->color565(255, 255, 255);
    
    display->setFont(&TomThumb);
    display->setTextSize(1);
    display->setTextWrap(false);
    display->setTextColor(branco);

    int digits = (fase >= 10) ? 2 : 1;
    int x = PANEL_WIDTH - (digits * 4);
    int y = 5;

    display->setCursor(x, y);
    display->print(fase);

    display->setFont(NULL);
}

// ── Função: HUD de Vidas (Corações) e Movimentos ──────────────────────────────
void desenharHUD() {
    uint16_t cor_coracao = display->color565(255, 0, 0);     // Vermelho
    uint16_t cor_vazio   = display->color565(40, 0, 0);      // Vermelho escuro/apagado
    uint16_t cor_mov     = display->color565(100, 100, 255); // Azul claro

    int heart_y = PANEL_HEIGHT - 6; // Base inferior para o sprite de 5px

    // Desenha 3 corações (ícone pixel art 5x5)
    for (int i = 0; i < 3; i++) {
        int heart_x = 1 + (i * 7); // Espaçamento horizontal entre corações
        uint16_t cor = (vidas > i) ? cor_coracao : cor_vazio;

        // Linha 0: topo das duas "orelhas" do coração
        display->drawPixel(heart_x + 1, heart_y + 0, cor);
        display->drawPixel(heart_x + 3, heart_y + 0, cor);
        // Linha 1: largura máxima superior
        display->drawLine(heart_x + 0, heart_y + 1, heart_x + 4, heart_y + 1, cor);
        // Linha 2: meio
        display->drawLine(heart_x + 0, heart_y + 2, heart_x + 4, heart_y + 2, cor);
        // Linha 3: afunilamento
        display->drawLine(heart_x + 1, heart_y + 3, heart_x + 3, heart_y + 3, cor);
        // Linha 4: base do coração
        display->drawPixel(heart_x + 2, heart_y + 4, cor);
    }

    // Desenhando Movimentos após os corações
    display->setFont(&TomThumb);
    display->setTextSize(1);
    display->setTextWrap(false);
    display->setTextColor(cor_mov);

    // Posição ajustada para a direita dos corações
    display->setCursor(24, PANEL_HEIGHT - 1);
    display->print("M:");
    display->print(movimentos);

    display->setFont(NULL);
}

void initMatrixBackground() {
    for (int i = 0; i < MATRIX_COLUMNS; i++) {
        matrix_drops[i].y = random(-40, 0);
        matrix_drops[i].speed = random(1, 4);
    }
    matrix_initialized = true;
}

void drawMatrixBackground() {
    if (!matrix_initialized) initMatrixBackground();

    unsigned long now = millis();
    if (now - last_matrix_update > 60) {
        last_matrix_update = now;

        display->setFont(&TomThumb);
        display->setTextSize(1);
        display->setTextWrap(false);

        for (int i = 0; i < MATRIX_COLUMNS; i++) {
            int x = i * 5 + 1;
            int speed = matrix_drops[i].speed;

            int tail_length = speed * 4 * 5; 
            display->setTextColor(0x0000); 
            display->setCursor(x, matrix_drops[i].y - tail_length);
            
            display->print("0"); 

            matrix_drops[i].y += speed;
            
            display->setTextColor(display->color565(150, 255, 150));
            display->setCursor(x, matrix_drops[i].y);
            display->print(random(2) ? "1" : "0");

            display->setTextColor(display->color565(0, 180, 0));
            display->setCursor(x, matrix_drops[i].y - (speed * 5));
            display->print(random(2) ? "1" : "0");

            display->setTextColor(display->color565(0, 60, 0));
            display->setCursor(x, matrix_drops[i].y - (speed * 10));
            display->print(random(2) ? "1" : "0");

            if (matrix_drops[i].y - tail_length > PANEL_HEIGHT) {
                matrix_drops[i].y = random(-20, -5);
                matrix_drops[i].speed = random(1, 4);
            }
        }
        display->setFont(NULL);
    }
}

// ── Setup do painel ───────────────────────────────────────────────────────────

void inicializar_display() {
    mxconfig.driver = HUB75_I2S_CFG::FM6124;
    mxconfig.clkphase = false;
    mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;

    display = new MatrixPanel_I2S_DMA(mxconfig);

    if (!display->begin()) {
        Serial.println("ERRO: falha ao inicializar o painel!");
        while (true) delay(1000);
    }

    display->setBrightness8(128);
    display->clearScreen();

    Serial.println("Painel inicializado. Iniciando testes...");
    Serial.println();
}