#include <Adafruit_GFX.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "game_types.h"

// panel_test.ino
// Teste de validação do painel HUB75 64x32 com chip FM6124DJ
// Biblioteca: ESP32-HUB75-MatrixPanel-DMA (mrcodetastic)
//
// Instale via Arduino IDE:
//   Sketch → Include Library → Manage Libraries → "ESP32 HUB75 LED Matrix Panel DMA Display"
//
// O teste percorre as seguintes etapas em loop:
//   1. Painel inteiro vermelho
//   2. Painel inteiro verde
//   3. Painel inteiro azul
//   4. Painel inteiro branco
//   5. Varredura pixel a pixel (esquerda→direita, cima→baixo) em branco
//   6. Efeito arco-íris percorrendo todas as colunas

// ── Dimensões do painel ───────────────────────────────────────────────────────

#define PANEL_WIDTH  64   // largura em pixels
#define PANEL_HEIGHT 32   // altura em pixels
#define PANELS_NUM    1   // número de painéis encadeados

// ── Pinos HUB75 (padrão mais comum para ESP32) ────────────────────────────────
//
// SE O PAINEL NÃO ACENDER: verifique a pinagem do seu adaptador/cabo.
// Estes são os pinos padrão da biblioteca — podem variar conforme sua placa.
//
// Pino  | Função        | Descrição
// ------+---------------+-----------------------------------
// R1=25 | Red top       | Dado vermelho — metade superior
// G1=26 | Green top     | Dado verde   — metade superior
// B1=27 | Blue top      | Dado azul    — metade superior
// R2=14 | Red bottom    | Dado vermelho — metade inferior
// G2=12 | Green bottom  | Dado verde   — metade inferior
// B2=13 | Blue bottom   | Dado azul    — metade inferior
// A =23 | Row addr A    | Endereço de linha bit 0
// B =19 | Row addr B    | Endereço de linha bit 1
// C = 5 | Row addr C    | Endereço de linha bit 2
// D =17 | Row addr D    | Endereço de linha bit 3
// E =-1 | Row addr E    | Não usado em painéis 1/16 scan (32 linhas)
// LAT=4 | Latch         | Trava os dados no shift register
// OE=15 | Output enable | Habilita a saída (ativo em LOW)
// CLK=16| Clock         | Clock do shift register
//
// ATENÇÃO: O painel 64x32 usa varredura 1/16 (scan 1/16).
// Pino E NÃO é necessário — por isso está como -1.
// Se o painel tiver 64 linhas, E seria necessário.

// ── Configuração da biblioteca ────────────────────────────────────────────────

// HUB75_I2S_CFG agrupa todas as configurações do painel em uma estrutura
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
    -1,  // E  — não usado (painel 1/16 scan)
     4,  // LAT
    15,  // OE
    16   // CLK
};

HUB75_I2S_CFG mxconfig(
    PANEL_WIDTH,   // largura do painel
    PANEL_HEIGHT,  // altura do painel
    PANELS_NUM,    // número de painéis
    _pins          // pinos definidos acima
);

// Ponteiro global para o objeto do painel
// Criado dinamicamente em setup() após configurar o driver
MatrixPanel_I2S_DMA *display = nullptr;

// ── Velocidades de teste ──────────────────────────────────────────────────────

#define DELAY_COR       1500  // ms exibindo cada cor sólida
#define DELAY_PIXEL       10  // ms entre cada pixel na varredura
#define DELAY_ARCO_IRIS    5  // ms entre cada coluna do arco-íris

// #define CHAR_W       6   // largura de cada caractere em pixels (fonte 1:1)
// #define CHAR_H       8   // altura de cada caractere em pixels  (fonte 1:1)

// ── 0. LAYOUTS E MÉTODOS PADRÕES ───────────────────────────────────────────────────────────────────

#define NUM_INPUTS 6

// ── MÁQUINA DE ESTADOS ────────────────────────────────────────────────────────

// Array de funções de tela — adicione novas telas aqui no futuro
typedef void (*ScreenFn)();
ScreenFn reading_screens[] = {
    bem_vindo,
    // instrucoes,   // ← descomente quando criar
    // creditos,     // ← descomente quando criar
};
const int NUM_SCREENS = sizeof(reading_screens) / sizeof(reading_screens[0]);

GameMode game_mode     = MODE_READING;
int      current_screen = 0;          // índice da tela de leitura atual

short int matriz[32][64];

// ── Cores base do circuito ────────────────────────────────────────────────────
// Centralize aqui para facilitar ajuste de brilho por fase

static uint16_t COR_ATIVO   = 0;  // inicializado em setup()
static uint16_t COR_INATIVO = 0;
static uint16_t COR_FUNDO   = 0;

void inicializarMatriz() {
  for(int y = 0; y < 32; y++) {
    for(int x = 0; x < 64; x++) {
      matriz[y][x] = 2;
    }
  }
}

void iniciarCoresFase() {
    COR_ATIVO   = display->color565(0,   220, 0);    // verde
    COR_INATIVO = display->color565(180, 0,   0);    // vermelho
    COR_FUNDO   = display->color565(0,   0,   0);    // preto
}

// ── Helpers internos ──────────────────────────────────────────────────────────

// Pinta um pixel na matriz booleana
static inline void MP(int col, int row, short int val) {
    if (col >= 0 && col < 64 && row >= 0 && row < 32)
        matriz[row][col] = val;
}

// Linha horizontal na matriz
static void MH(int col_ini, int col_fim, int row, short int val) {
    for (int c = col_ini; c <= col_fim; c++) MP(c, row, val);
}

// Linha vertical na matriz
static void MV(int col, int row_ini, int row_fim, short int val) {
    int step = (row_fim >= row_ini) ? 1 : -1;
    for (int r = row_ini; r != row_fim + step; r += step) MP(col, r, val);
}

// ── Símbolos das portas na matriz ─────────────────────────────────────────────
// Mesmos mapas de pixel das funções desenharAND/OR/NOT,
// mas escrevendo em matriz[row][col] em vez de drawPixel()

void mpAND(int col, int row, short int val) {
    MH(col,   col+3, row,   val);  // topo
    MH(col,   col+3, row+4, val);  // base
    MV(col,   row+1, row+3, val);  // parede esquerda
    MV(col+4, row+1, row+3, val);  // curva direita
}

void mpOR(int col, int row, short int val) {
    MH(col, col+3, row,   val);  // topo
    MH(col, col+3, row+4, val);  // base
    MV(col+1, row+1, row+3, val);  // lateral esquerda

    //curva direita
    MP(col+4, row+1, val);
    MP(col+5, row+2, val);
    MP(col+4, row+3, val);
}

void mpNOT(int col, int row, short int val) {
    // triângulo (7 linhas simétricas)
    MP(col,   row,   val);
    MH(col,   col+1, row+1, val);
    MH(col,   col+2, row+2, val);  // vértice
    MH(col,   col+1, row+3, val);  
    MP(col,   row+4, val);
}

void entradas_leds() {
    for (int i = 3; i < 32; i += 5) {
        MH(0, 5, i, COR_INATIVO);
    }
}

// ── Renderizador: usa COR_ATIVO/INATIVO por célula ───────────────────────────

void renderizarComCores() {
    for (int row = 0; row < 32; row++) {
        for (int col = 0; col < 64; col++) {
            uint16_t cor = 0;
            if (matriz[row][col] == 0) {
                cor = COR_INATIVO;
            } else if (matriz[row][col] == 1) {
                cor = COR_ATIVO;
            } else {
                cor = COR_FUNDO;
            }
            display->drawPixel(col, row, cor);
        }
    }
}

// Função utilitária para escrever textos simples
// void escreverTextoCentralizado(const char* texto, uint16_t cor) {
//     int total_chars    = strlen(texto);
//     int chars_per_line = PANEL_WIDTH / CHAR_W;          // quantos chars cabem por linha
//     int num_lines      = (total_chars + chars_per_line - 1) / chars_per_line; // ceil
//     // Altura total do bloco de texto
//     int block_h = num_lines * CHAR_H + (num_lines - 1) * LINE_SPACING;
//     // Y do topo do bloco — centralizado verticalmente
//     int y_start = (PANEL_HEIGHT - block_h) / 2;
//     display->setTextSize(1);
//     display->setTextWrap(false); // controlamos a quebra manualmente
//     display->setTextColor(cor);
//     for (int line = 0; line < num_lines; line++) {
//         int offset      = line * chars_per_line;          // índice do primeiro char da linha
//         int line_chars  = min(chars_per_line, total_chars - offset); // chars desta linha
//         int line_w      = line_chars * CHAR_W;            // largura em px desta linha
//         // X centralizado horizontalmente para esta linha
//         int x_start = (PANEL_WIDTH - line_w) / 2;
//         // Copia o fragmento da linha para um buffer temporário
//         char line_buf[chars_per_line + 1];
//         strncpy(line_buf, texto + offset, line_chars);
//         line_buf[line_chars] = '\0';
//         display->setCursor(x_start, y_start + line * (CHAR_H + LINE_SPACING));
//         display->print(line_buf);
//     }
// }

// Avança tela de leitura; na última, entra no modo jogo
void reading_next() {
    current_screen++;
    if (current_screen >= NUM_SCREENS) {
        // Todas as telas exibidas — entra no modo jogo
        game_mode      = MODE_PLAYING;
        current_screen = 0;           // reseta para próxima vez
        display->clearScreen();
        // Desenha a fase 1 — a lógica do jogo já foi carregada no setup()
        desenharFase1();
    } else {
        display->clearScreen();
        reading_screens[current_screen]();
    }
}

// Renderiza a tela de leitura atual (chamado no setup e ao voltar para leitura)
void reading_show_current() {
    display->clearScreen();
    reading_screens[current_screen]();
}

void borda_branca() {
    uint16_t branco = display->color565(255, 255, 255);
    display->drawRect(0, 0, display->width(), display->height(), branco);
}

// ── 1. BEM-VINDO ────────────────────────────────────────────────────────────────────
void bem_vindo() {
    uint16_t branco = display->color565(255, 255, 255);

    // Cada linha: { texto, x centralizado }
    const char* linhas[] = { "CAMINHO", "DOS", "BITS" };
    const int num_linhas = 3;

    const int CHAR_H_LOCAL   = 8;  // altura do glyph com fonte 1
    const int LINE_SPACING   = 2;  // px entre linhas
    const int block_h        = num_linhas * CHAR_H_LOCAL
                               + (num_linhas - 1) * LINE_SPACING; // = 28

    int y_start = (PANEL_HEIGHT - block_h) / 2;  // = 2

    display->setTextSize(1);
    display->setTextWrap(false);
    display->setTextColor(branco);

    for (int i = 0; i < num_linhas; i++) {
        int len   = strlen(linhas[i]);
        int x     = (PANEL_WIDTH - len * 6) / 2;
        int y     = y_start + i * (CHAR_H_LOCAL + LINE_SPACING);
        display->setCursor(x, y);
        display->print(linhas[i]);
    }
}

// // ── Setup ─────────────────────────────────────────────────────────────────────

void inicializar_display() {
    // FM6124DJ exige sequência especial de inicialização.
    // Sem isso, o painel pode não acender ou mostrar cores erradas.
    // A biblioteca detecta e aplica automaticamente ao setar o driver.
    mxconfig.driver = HUB75_I2S_CFG::FM6124;

    mxconfig.clkphase = false;

    mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;

    // Cria o objeto do painel com as configurações definidas
    display = new MatrixPanel_I2S_DMA(mxconfig);

    // Inicializa o DMA e os pinos — retorna false se falhar
    if (!display->begin()) {
        Serial.println("ERRO: falha ao inicializar o painel!");
        Serial.println("Verifique a fiacao e os pinos definidos.");
        while (true) delay(1000); // trava — não tenta continuar
    }

    // Brilho: 0 (apagado) a 255 (máximo)
    // 128 = 50% — bom para testes sem sobrecarregar a fonte
    display->setBrightness8(128);

    // Garante que o painel começa apagado
    display->clearScreen();

    Serial.println("Painel inicializado. Iniciando testes...");
    Serial.println();
}