#include <Adafruit_GFX.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

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

#define CHAR_W       6   // largura de cada caractere em pixels (fonte 1:1)
#define CHAR_H       8   // altura de cada caractere em pixels  (fonte 1:1)
#define LINE_SPACING 1   // pixels extras entre linhas

uint16_t branco = display->color565(255, 255, 255);

// ── 0. LAYOUTS E MÉTODOS PADRÕES ───────────────────────────────────────────────────────────────────

static inline void _px(int16_t x, int16_t y, uint16_t cor) {
    display->drawPixel(x, y, cor);
}

void desenharAND(int16_t x, int16_t y, uint16_t cor) {
    // Topo e base (linha horizontal esquerda)
    for (int c = 0; c < 4; c++) _px(x+c, y+0, cor);  // row 0
    for (int c = 0; c < 4; c++) _px(x+c, y+4, cor);  // row 4

    // Borda esquerda (coluna 0)
    for (int r = 1; r < 4; r++) _px(x+0, y+r, cor);

    // Curva direita (D)
    for (int l = 1; l < 4; l++) _px(x+4, y+l, cor);
}

void desenharOR(int16_t x, int16_t y, uint16_t cor) {
    // Topo e base
    for (int c = 0; c < 4; c++) _px(x+c, y+0, cor);  // row 0
    for (int c = 0; c < 4; c++) _px(x+c, y+4, cor);  // row 4

    // Laterais arredondada (esquerda)
    for (int e = 1; e < 4; e++) _px(x+1, y+e, cor);

    // Laterais arredondada (esquerda)
    _px(x+4, y+1, cor);
    _px(x+5, y+2, cor);
    _px(x+4, y+3, cor);
}

void desenharNOT(int16_t x, int16_t y, uint16_t cor) {
    // Triângulo apontando para a direita
    _px(x+0, y+0, cor);
    _px(x+0, y+1, cor); _px(x+1, y+1, cor);
    _px(x+0, y+2, cor); _px(x+1, y+2, cor); _px(x+2, y+2, cor);
    _px(x+0, y+3, cor); _px(x+1, y+3, cor); 
    _px(x+0, y+4, cor); 

}

void borda_branca() {
    display->drawRect(0, 0, display->width(), display->height(), branco);
}

// Função utilitária para escrever textos simples
void escreverTextoCentralizado(const char* texto, uint16_t cor) {
    int total_chars    = strlen(texto);
    int chars_per_line = PANEL_WIDTH / CHAR_W;          // quantos chars cabem por linha
    int num_lines      = (total_chars + chars_per_line - 1) / chars_per_line; // ceil

    // Altura total do bloco de texto
    int block_h = num_lines * CHAR_H + (num_lines - 1) * LINE_SPACING;

    // Y do topo do bloco — centralizado verticalmente
    int y_start = (PANEL_HEIGHT - block_h) / 2;

    display->setTextSize(1);
    display->setTextWrap(false); // controlamos a quebra manualmente
    display->setTextColor(cor);

    for (int line = 0; line < num_lines; line++) {
        int offset      = line * chars_per_line;          // índice do primeiro char da linha
        int line_chars  = min(chars_per_line, total_chars - offset); // chars desta linha
        int line_w      = line_chars * CHAR_W;            // largura em px desta linha

        // X centralizado horizontalmente para esta linha
        int x_start = (PANEL_WIDTH - line_w) / 2;

        // Copia o fragmento da linha para um buffer temporário
        char line_buf[chars_per_line + 1];
        strncpy(line_buf, texto + offset, line_chars);
        line_buf[line_chars] = '\0';

        display->setCursor(x_start, y_start + line * (CHAR_H + LINE_SPACING));
        display->print(line_buf);
    }
}

// ── 1. BEM-VINDO ────────────────────────────────────────────────────────────────────

void bem_vindo() {
    uint16_t branco = display->color565(255, 255, 255);
    escreverTextoCentralizado("Caminho dos BITS", branco);
}

// ── Setup ─────────────────────────────────────────────────────────────────────

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

    borda_branca();
    desenharAND(5, 5, branco);
    desenharOR(15, 5, branco);
    desenharNOT(25, 5, branco);
}
