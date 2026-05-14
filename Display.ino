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

#define NUM_INPUTS 6

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

// ── FASE 1: XOR(A,E) = (A·~E) + (~A·E) ──────────────────────────────────────
//
// Topologia de nós (do código do jogo):
//   0=A  1=E  2=NOT_A  3=NOT_E  4=AND(A,NE)  5=AND(NA,E)  6=OR  7=S
//
// Layout no painel 64×32 (esq→dir):
//
//   Col  0..7  → linhas de entrada  (A=row10, E=row21 — yEntrada com N=2)
//   Col  8     → fio de entroncamento para NOT e AND
//   Col  9..14 → NOT_A (centrado em row10) e NOT_E (centrado em row21)
//   Col 15..17 → fios de saída dos NOTs e roteamento vertical para os ANDs
//   Col 18..23 → AND1(A,NE) centrado em row14; AND2(NA,E) centrado em row19
//   Col 24..26 → fios convergindo para OR (coluna 26 = vertical de junção)
//   Col 27..32 → OR centrado em row16
//   Col 33..38 → fio de saída
//   Col 39..41 → LED indicador de saída (3×3)

void desenharFase1() {
    inicializarMatriz();
    iniciarCoresFase();
    entradas_leds();

    // Recupera valores lógicos da engine do jogo
    short int vA    = values[0];  // entrada A
    short int vE    = values[1];  // entrada E
    short int vNA   = values[2];  // NOT_A
    short int vNE   = values[3];  // NOT_E
    short int vAND1 = values[4];  // AND(A,NE)
    short int vAND2 = values[5];  // AND(NA,E)
    short int vOR   = values[6];  // OR — saída final
    // values[7] = S = vOR (buffer)

    // ── 1. Linhas de entrada ──────────────────────────────────────────────
    //    yEntrada(i) com num_inputs=2: A=row3, E=row23
    int rowA = 3;
    int rowE = 23; 

    int rowNOTA = 1;   // row 7
    int colNOTA = 9;

    int rowNOTE = 21;   // row 18
    int colNOTE = 9;

    int rowAND1 = 8;  // topo do AND1 (rows 12..16)
    int colAND1 = 23;

    int rowAND2 = 18;  // topo do AND2 (rows 18..22)
    int colAND2 = 23;

    int rowOR = 13;  // row12
    int colOR = 42;

    int rowSaidaAND1 = rowAND1 + 2;  // row14
    int rowSaidaAND2 = rowAND2 + 2;  // row20

    int rowSaidaOR = rowOR + 2;   // centro do OR — entre row14 e row20

    // ─── A e E lines ───────────────────────
    MH(0, colNOTE-1, rowA, vA);
    MH(0, colNOTE-1, rowE, vE);

    // ── 2. NOT_A (col9, row7) e NOT_E (col9, row18) ───────────────────────
    //    NOT 7 linhas de altura: centralizado em row10 → topo=row7
    //    NOT 7 linhas de altura: centralizado em row21 → topo=row18

    mpNOT(colNOTA, rowNOTA, vNA);  // NOT_A
    mpNOT(colNOTE, rowNOTE, vNE);  // NOT_E

    // ── 3. Roteamento NOT→AND ─────────────────────────────────────────────
    //    Saída do NOT está em col+5,row+3 = col14, row10 (NA) e col14, row21 (NE)
    //    AND1 está em col18, precisa de NE na entrada inferior (row16) e A na entrada superior (row12)
    //    AND2 está em col18, precisa de NA na entrada superior (row17) e E  na entrada inferior (row21)

    // Layout dos ANDs — cada AND(4px alto) centralizado:
    // AND1 entre rowA(10) e meio: centrado em row13 → topo=row13, base=row17
    // AND2 entre meio e rowE(21): centrado em row18 → topo=row18, base=row22


    // entradas do AND: pino superior=topo, pino inferior=base

    // fio NOT_A saída (col14,row10) → horizontal até col17, depois desce até topAND2
    MH(colNOTA+3, colNOTA+5, rowA, vNA);           // NA sai do NOT e vai até col17
    MV(colNOTA+5, rowA+1, rowAND2+3, vNA);    // desce col17 de row11 até row18 (entrada AND2 topo)
    MH(colNOTA+6, colAND2-1, rowAND2+3, vNA);

    // fio NOT_E saída (col14,row21) → horizontal até col17, depois sobe até topAND1+4
    MH(colNOTE+3, colNOTE+8, rowE, vNE);           // NE sai do NOT e vai até col17
    MV(colNOTE+8, rowE-1, rowAND1+3, vNE);  // sobe col17 de row16 até row20 (entrada AND1 base)
    MH(colNOTE+9, colAND1-1, rowAND1+3, vNE);

    // fio A→AND1 (entrada superior AND1=topo=row12): A vem de col8, desce col16
    MV(4, rowA+1, rowAND1+1, vA);   // desce col16 de row11 até row12
    MH(5, colAND1-1, rowAND1+1, vA);            // A horizontal col8..16

    // fio E→AND2 (entrada inferior AND2=base=row22): E vem de col8
    MV(4, rowE-1, rowAND2+1, vE); // sobe col16 de row22 até row20
    MH(5, colAND2-1, rowAND2+1, vE);

    // ── 4. AND1 e AND2 ───────────────────────────────────────────────────────
    mpAND(colAND1, rowAND1, vAND1);   // AND(A, NE)  col18, rows12..16
    mpAND(colAND2, rowAND2, vAND2);   // AND(NA, E)  col18, rows18..22

    MH(colAND1+5, colAND1+8, rowSaidaAND1, vAND1);   // AND1→junção horizontal
    MV(colAND1+8, rowSaidaAND1+1, rowSaidaOR-1, vAND1); // vertical subindo
    MH(colAND1+9, colOR, rowSaidaOR-1, vAND1);   // AND1→junção horizontal

    MH(colAND2+5, colAND2+8, rowSaidaAND2, vAND2);   // AND2→junção horizontal
    MV(colAND2+8, rowSaidaAND2-1, rowSaidaOR+1, vAND2); // vertical descendo
    MH(colAND2+9, colOR, rowSaidaOR+1, vAND2);   // AND1→junção horizontal

    // ── 6. OR (col27, centrado em rowOR=15, topo=row12) ──────────────────
    
    mpOR(colOR, rowOR, vOR);

    // ── 7. Saída OR→S ─────────────────────────────────────────────────────
    // saída do OR: col32, rowOR=15
    MH(colOR+6 , colOR+10, rowSaidaOR, vOR);

    // LED indicador 3×3 em col39..41, rows 14..16
    for (int r = rowSaidaOR-1; r <= rowSaidaOR+1; r++)
        MH(colOR+11, colOR+13, r, vOR);

    renderizarComCores();
}

// // Calcula o Y centralizado da entrada i usando divisão inteira
// // Fórmula: y = HEIGHT * (i+1) / (N+1)
// int yEntrada(int i) {
//     return (PANEL_HEIGHT * (i + 1)) / (NUM_INPUTS + 1);
//     // Resultados: 4, 9, 13, 18, 22, 27
// }

// // Struct que descreve uma linha de entrada
// struct LinhaEntrada {
//     int16_t x_fim;   // até onde a linha vai horizontalmente
//     uint16_t cor;    // cor da linha (reflete valor lógico: verde=1, vermelho=0)
// };

// // Array com o estado de cada linha — você preenche antes de chamar desenharLinhasEntrada()
// LinhaEntrada linhas[NUM_INPUTS];

// // Desenha todas as 6 linhas horizontais da borda esquerda até x_fim de cada uma
// void desenharLinhasEntrada() {
//     for (int i = 0; i < NUM_INPUTS; i++) {
//         int16_t y = yEntrada(i);
//         // drawFastHLine(x_inicio, y, comprimento, cor)
//         display->drawFastHLine(0, y, linhas[i].x_fim, linhas[i].cor);
//     }
// }

// static void desenharLinha(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t cor) {
//     display->drawLine(x0, y0, x1, y1, cor);
// }

// static inline void _px(int16_t x, int16_t y, uint16_t cor) {
//     display->drawPixel(x, y, cor);
// }

// void desenharAND(int16_t x, int16_t y, uint16_t cor) {
//     // Topo e base (linha horizontal esquerda)
//     for (int c = 0; c < 4; c++) _px(x+c, y+0, cor);  // row 0
//     for (int c = 0; c < 4; c++) _px(x+c, y+4, cor);  // row 4

//     // Borda esquerda (coluna 0)
//     for (int r = 1; r < 4; r++) _px(x+0, y+r, cor);

//     // Curva direita (D)
//     for (int l = 1; l < 4; l++) _px(x+4, y+l, cor);
// }

// void desenharOR(int16_t x, int16_t y, uint16_t cor) {
//     // Topo e base
//     for (int c = 0; c < 4; c++) _px(x+c, y+0, cor);  // row 0
//     for (int c = 0; c < 4; c++) _px(x+c, y+4, cor);  // row 4

//     // Laterais arredondada (esquerda)
//     for (int e = 1; e < 4; e++) _px(x+1, y+e, cor);

//     // Laterais arredondada (esquerda)
//     _px(x+4, y+1, cor);
//     _px(x+5, y+2, cor);
//     _px(x+4, y+3, cor);
// }

// void desenharNOT(int16_t x, int16_t y, uint16_t cor) {
//     // Triângulo apontando para a direita
//     _px(x+0, y+0, cor);
//     _px(x+0, y+1, cor); _px(x+1, y+1, cor);
//     _px(x+0, y+2, cor); _px(x+1, y+2, cor); _px(x+2, y+2, cor);
//     _px(x+0, y+3, cor); _px(x+1, y+3, cor); 
//     _px(x+0, y+4, cor); 

// }

// void borda_branca() {
//     display->drawRect(0, 0, display->width(), display->height(), branco);
// }

// // Função utilitária para escrever textos simples
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

// // ── 1. BEM-VINDO ────────────────────────────────────────────────────────────────────

// void bem_vindo() {
//     uint16_t branco = display->color565(255, 255, 255);
//     escreverTextoCentralizado("Caminho dos BITS", branco);
// }

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