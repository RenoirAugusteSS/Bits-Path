#include <Adafruit_GFX.h>
#include <Adafruit_GrayOLED.h>
#include <Adafruit_SPITFT.h>
#include <Adafruit_SPITFT_Macros.h>
#include <gfxfont.h>
#include "game_types.h"
#include <Fonts/TomThumb.h>

// ── Variáveis externas declaradas em Display.ino ──────────────────────────────
extern GameMode game_mode;
extern int      current_screen;
extern void     reading_next();
extern void     reading_prev(); 
extern void     reading_show_current();
extern void     desenharFaseAtual();
extern void     alterar_cor_joystick();


// logic_gate_game_v6.ino
// 10 fases de portas lógicas — ESP32
// Base: v3 (dois botões + serial em paralelo, vitória sem travar)
//
// Hardware:
//   BTN_CYCLE  (GPIO 18) INPUT_PULLUP — cicla entradas / avança fase ao vencer
//   BTN_TOGGLE (GPIO 19) INPUT_PULLUP — alterna valor da entrada selecionada
//
// Serial:
//   'a'..'f' — alterna a entrada correspondente (se existir na fase atual)
//   'n'      — avança para a próxima fase (só após vencer)

// ── Pinos ─────────────────────────────────────────────────────────────────────

#define JOY_VRX 33  // ADC apropriado para leitura analógica
#define JOY_VRY 32  // ADC apropriado para leitura analógica
#define JOY_SW  21  // Input digital (mantém-se com INPUT_PULLUP)
#define JOY_COOLDOWN_MS 300 // Tempo de supressão de transientes (300ms engole o snap-back)
#define JOY_SETTLE_TIME 300 // ms contínuos no centro para considerar a mola parada
#define JOY_CENTER_ZONE 150

#define JOY_DEADZONE 700


// ── Dimensões máximas ─────────────────────────────────────────────────────────

// Fase 10 tem 18 nós — MAX_N=20 dá margem para novas fases
#define MAX_N      20
#define MAX_INPUTS  6
#define NUM_PHASES 10

// Animação da borda
#define PERIMETER       188      // número de pixels na borda externa (2*(64+32)-4)
#define SNAKE_LENGTH    16          // comprimento da cobrinha (8 pixels)
#define BORDER_SPEED_MS 60       // intervalo entre cada movimento da cobrinha (ms)

#define RGB_R 22
#define RGB_G 2

void rgb_verde()    { digitalWrite(RGB_R, LOW);  digitalWrite(RGB_G, HIGH); }
void rgb_vermelho() { digitalWrite(RGB_R, HIGH); digitalWrite(RGB_G, LOW);  }

// ── Tipos de porta ────────────────────────────────────────────────────────────

typedef enum {
    GATE_BUFFER, // f(x) = x  (entradas e saída)
    GATE_AND,    // f = true se TODOS os predecessores forem true
    GATE_OR,     // f = true se ALGUM predecessor for true
    GATE_NOT     // f = !predecessor
} GateKind;

// ── Estado global do circuito ─────────────────────────────────────────────────

// Matriz de adjacência MAX_N×MAX_N — topologia do circuito atual
// adjacency[i][j]=true → aresta de i para j (j depende de i)
// Resetada e recarregada a cada troca de fase por load_phase()
bool     adjacency[MAX_N][MAX_N];
bool     values[MAX_N];     // valor lógico atual de cada nó
GateKind kinds[MAX_N];      // tipo funcional de cada nó
int      input_ids[MAX_INPUTS];    // índices dos nós de entrada
char     input_labels[MAX_INPUTS]; // letra de cada entrada ('A'..'F')
int      num_nos;      // total de nós da fase atual
int      num_inputs;   // total de entradas da fase atual
int      output_id;    // índice do nó de saída S

int joy_center_x = 2048, joy_center_y = 2048;
unsigned long last_sw_press = 0;
unsigned long x_center_time = 0;
unsigned long y_center_time = 0;
bool prev_sw = HIGH;
bool axis_x_active = false;
bool axis_y_active = false;
unsigned long last_joy_action = 0;
unsigned long last_debug_print = 0;
static unsigned long last_reading_action = 0;

bool display_dirty = true;  // true = precisa redesenhar

// ── Temporização do Efeito Pisca (Blink) ──────────────────────────────────
unsigned long last_blink_time = 0;
const unsigned long BLINK_INTERVAL = 300; // Tempo em milissegundos (f = 1Hz)
bool blink_state = true;                  // true = renderiza entrada, false = oculta

// No topo do arquivo, junto com as outras globais
uint16_t rainbow_text_color = 0;      // cor atual para os textos
unsigned long last_rainbow_update = 0;
const unsigned long RAINBOW_INTERVAL = 80; // ms entre cada mudança de cor

int       snake_head = 0;             // posição atual da cabeça (0..187)
int       snake_body[SNAKE_LENGTH];   // armazena as posições de cada segmento
unsigned long last_snake_move = 0;
bool      snake_initialized = false;

// ── Estado do jogo ────────────────────────────────────────────────────────────

int  current_phase  = 1;
int  selected_input = 0;
bool phase_won      = false; // true após S=1; BTN_CYCLE e 'n' avançam fase

// ── Fórmulas para exibição ────────────────────────────────────────────────────

const char* phase_formulas[NUM_PHASES] = {
    "(A~E)+(~AE)",
    "(A~B)+(BF)",
    "(~AC)+(~A~E)",
    "(AB)+((D+E)~F)",
    "((~AB)(~C~D))(EF)",
    "(((~AB)((C+D)~(CD)))E)~F",
    "(~(~AB))(~CD)(~EF)",
    "((AB)(~(CD)))(E+F)",
    "(~A+(BC))(~A+(F(DE)))",
    "(((A~B)C)+(~C(~AB)))+((DE)F)"
};

// ── Debounce ──────────────────────────────────────────────────────────────────

#define DEBOUNCE_MS 50
unsigned long last_cycle_press  = 0;
unsigned long last_toggle_press = 0;
bool prev_cycle  = HIGH;
bool prev_toggle = HIGH;

// ── Forward declaration ───────────────────────────────────────────────────────

void print_state();

#define FILTER_SIZE 5
int x_buffer[FILTER_SIZE];
int x_idx = 0;

int readFilteredX() {
    x_buffer[x_idx] = analogRead(JOY_VRX);
    x_idx = (x_idx + 1) % FILTER_SIZE;
    long sum = 0;
    for (int i = 0; i < FILTER_SIZE; i++) sum += x_buffer[i];
    return sum / FILTER_SIZE;
}

int y_buffer[FILTER_SIZE];
int y_idx = 0;

int readFilteredY() {
    y_buffer[y_idx] = analogRead(JOY_VRY);
    y_idx = (y_idx + 1) % FILTER_SIZE;
    long sum = 0;
    for (int i = 0; i < FILTER_SIZE; i++) sum += y_buffer[i];
    return sum / FILTER_SIZE;
}

// ── Helpers de construção do circuito ────────────────────────────────────────

// Limpa todos os arrays e reseta o estado da interface
void clear_circuit() {
    for (int i = 0; i < MAX_N; i++) {
        values[i] = false;
        kinds[i]  = GATE_BUFFER;
        for (int j = 0; j < MAX_N; j++) adjacency[i][j] = false;
    }
    for (int i = 0; i < MAX_INPUTS; i++) {
        input_ids[i]    = 0;
        input_labels[i] = '?';
    }
    selected_input = 0;
    phase_won      = false;
}

// Atalho para definir uma aresta na matriz de adjacência
void edge(int from, int to) {
    adjacency[from][to] = true;
}

// ── Inicializadores de cada fase ──────────────────────────────────────────────

// FASE 01: (A~E)+(~AE)  →  XOR(A,E)
// Nós: 0=A  1=E  2=NOT_A  3=NOT_E  4=AND(A,NE)  5=AND(NA,E)  6=OR  7=S
void init_fase_1() {
    num_nos = 8; num_inputs = 2; output_id = 7;
    input_ids[0]=0; input_labels[0]='A';
    input_ids[1]=1; input_labels[1]='E';

    kinds[2]=GATE_NOT; kinds[3]=GATE_NOT;
    kinds[4]=GATE_AND; kinds[5]=GATE_AND;
    kinds[6]=GATE_OR;

    edge(0,2);            // A   → NOT_A
    edge(1,3);            // E   → NOT_E
    edge(0,4); edge(3,4); // A, NOT_E → AND1
    edge(2,5); edge(1,5); // NOT_A, E → AND2
    edge(4,6); edge(5,6); // AND1, AND2 → OR
    edge(6,7);            // OR → S
}

// FASE 02: (A~B)+(BF)
// Nós: 0=A  1=B  2=F  3=NOT_B  4=AND(A,NB)  5=AND(B,F)  6=OR  7=S
void init_fase_2() {
    num_nos = 8; num_inputs = 3; output_id = 7;
    input_ids[0]=0; input_labels[0]='A';
    input_ids[1]=1; input_labels[1]='B';
    input_ids[2]=2; input_labels[2]='F';

    kinds[3]=GATE_NOT;
    kinds[4]=GATE_AND; kinds[5]=GATE_AND;
    kinds[6]=GATE_OR;

    edge(1,3);            // B → NOT_B
    edge(0,4); edge(3,4); // A, NOT_B → AND1
    edge(1,5); edge(2,5); // B, F → AND2
    edge(4,6); edge(5,6); // AND1, AND2 → OR
    edge(6,7);
}

// FASE 03: (~AC)+(~A~E)
// Nós: 0=A  1=C  2=E  3=NOT_A  4=NOT_E  5=AND(NA,C)  6=AND(NA,NE)  7=OR  8=S
void init_fase_3() {
    num_nos = 9; num_inputs = 3; output_id = 8;
    input_ids[0]=0; input_labels[0]='A';
    input_ids[1]=1; input_labels[1]='C';
    input_ids[2]=2; input_labels[2]='E';

    kinds[3]=GATE_NOT; kinds[4]=GATE_NOT;
    kinds[5]=GATE_AND; kinds[6]=GATE_AND;
    kinds[7]=GATE_OR;

    edge(0,3);            // A → NOT_A
    edge(2,4);            // E → NOT_E
    edge(3,5); edge(1,5); // NOT_A, C → AND1
    edge(3,6); edge(4,6); // NOT_A, NOT_E → AND2
    edge(5,7); edge(6,7); // AND1, AND2 → OR
    edge(7,8);
}

// FASE 04: (AB)+((D+E)~F)
// Nós: 0=A 1=B 2=D 3=E 4=F | 5=AND(A,B) 6=OR(D,E) 7=NOT_F
//      8=AND(OR,NF) 9=OR2 10=S
void init_fase_4() {
    num_nos = 11; num_inputs = 5; output_id = 10;
    input_ids[0]=0; input_labels[0]='A';
    input_ids[1]=1; input_labels[1]='B';
    input_ids[2]=2; input_labels[2]='D';
    input_ids[3]=3; input_labels[3]='E';
    input_ids[4]=4; input_labels[4]='F';

    kinds[5]=GATE_AND; kinds[6]=GATE_OR; kinds[7]=GATE_NOT;
    kinds[8]=GATE_AND; kinds[9]=GATE_OR;

    edge(0,5); edge(1,5); // A, B → AND1
    edge(2,6); edge(3,6); // D, E → OR1
    edge(4,7);            // F → NOT_F
    edge(6,8); edge(7,8); // OR1, NOT_F → AND2
    edge(5,9); edge(8,9); // AND1, AND2 → OR2
    edge(9,10);
}

// FASE 05: ((~AB)(~C~D))(EF)
// Nós: 0=A 1=B 2=C 3=D 4=E 5=F | 6=NA 7=NC 8=ND
//      9=AND(NA,B) 10=AND(NC,ND) 11=AND(E,F)
//      12=AND(9,10) 13=AND(12,11) 14=S
void init_fase_5() {
    num_nos = 15; num_inputs = 6; output_id = 14;
    input_ids[0]=0; input_labels[0]='A';
    input_ids[1]=1; input_labels[1]='B';
    input_ids[2]=2; input_labels[2]='C';
    input_ids[3]=3; input_labels[3]='D';
    input_ids[4]=4; input_labels[4]='E';
    input_ids[5]=5; input_labels[5]='F';

    kinds[6]=GATE_NOT; kinds[7]=GATE_NOT; kinds[8]=GATE_NOT;
    kinds[9]=GATE_AND; kinds[10]=GATE_AND; kinds[11]=GATE_AND;
    kinds[12]=GATE_AND; kinds[13]=GATE_AND;

    edge(0,6);              // A → NOT_A
    edge(2,7);              // C → NOT_C
    edge(3,8);              // D → NOT_D
    edge(6,9);  edge(1,9);  // NOT_A, B  → AND1
    edge(7,10); edge(8,10); // NOT_C, ND → AND2
    edge(4,11); edge(5,11); // E, F      → AND3
    edge(9,12); edge(10,12);// AND1, AND2 → AND4
    edge(12,13);edge(11,13);// AND4, AND3 → AND5
    edge(13,14);
}

// FASE 06: (((~AB)((C+D)~(CD)))E)~F
// ~(CD) faz XOR(C,D) junto com OR(C,D)
// Nós: 0=A 1=B 2=C 3=D 4=E 5=F | 6=NA 7=NF
//      8=AND(NA,B)  9=OR(C,D)  10=AND(C,D)  11=NOT(AND_CD)
//      12=AND(OR,NOT_CD) [XOR]  13=AND(8,12)
//      14=AND(13,E)  15=AND(14,NF)  16=S
void init_fase_6() {
    num_nos = 17; num_inputs = 6; output_id = 16;
    input_ids[0]=0; input_labels[0]='A';
    input_ids[1]=1; input_labels[1]='B';
    input_ids[2]=2; input_labels[2]='C';
    input_ids[3]=3; input_labels[3]='D';
    input_ids[4]=4; input_labels[4]='E';
    input_ids[5]=5; input_labels[5]='F';

    kinds[6]=GATE_NOT;  kinds[7]=GATE_NOT;
    kinds[8]=GATE_AND;
    kinds[9]=GATE_OR;   kinds[10]=GATE_AND; kinds[11]=GATE_NOT;
    kinds[12]=GATE_AND;
    kinds[13]=GATE_AND; kinds[14]=GATE_AND; kinds[15]=GATE_AND;

    edge(0,6);              // A → NOT_A
    edge(5,7);              // F → NOT_F
    edge(6,8);  edge(1,8);  // NOT_A, B    → AND(~AB)
    edge(2,9);  edge(3,9);  // C, D        → OR(C,D)
    edge(2,10); edge(3,10); // C, D        → AND(C,D)
    edge(10,11);            // AND(C,D)    → NOT
    edge(9,12); edge(11,12);// OR, NOT_CD  → XOR(C,D)
    edge(8,13); edge(12,13);// AND(~AB), XOR → AND
    edge(13,14);edge(4,14); // AND, E       → AND
    edge(14,15);edge(7,15); // AND, NOT_F   → AND
    edge(15,16);
}

// FASE 07: (~(~AB))(~CD)(~EF)
// Nós: 0=A 1=B 2=C 3=D 4=E 5=F | 6=NA 7=NC 8=NE
//      9=AND(NA,B)  10=NOT(9)  11=AND(NC,D)  12=AND(NE,F)
//      13=AND(10,11)  14=AND(13,12)  15=S
void init_fase_7() {
    num_nos = 16; num_inputs = 6; output_id = 15;
    input_ids[0]=0; input_labels[0]='A';
    input_ids[1]=1; input_labels[1]='B';
    input_ids[2]=2; input_labels[2]='C';
    input_ids[3]=3; input_labels[3]='D';
    input_ids[4]=4; input_labels[4]='E';
    input_ids[5]=5; input_labels[5]='F';

    kinds[6]=GATE_NOT; kinds[7]=GATE_NOT; kinds[8]=GATE_NOT;
    kinds[9]=GATE_AND; kinds[10]=GATE_NOT;
    kinds[11]=GATE_AND; kinds[12]=GATE_AND;
    kinds[13]=GATE_AND; kinds[14]=GATE_AND;

    edge(0,6);              // A → NOT_A
    edge(2,7);              // C → NOT_C
    edge(4,8);              // E → NOT_E
    edge(6,9);  edge(1,9);  // NOT_A, B  → AND(~AB)
    edge(9,10);             // AND(~AB)  → NOT
    edge(7,11); edge(3,11); // NOT_C, D  → AND(~CD)
    edge(8,12); edge(5,12); // NOT_E, F  → AND(~EF)
    edge(10,13);edge(11,13);// NOT(~AB), AND(~CD) → AND
    edge(13,14);edge(12,14);// AND, AND(~EF)       → AND
    edge(14,15);
}

// FASE 08: ((AB)(~(CD)))(E+F)
// Nós: 0=A 1=B 2=C 3=D 4=E 5=F | 6=AND(A,B) 7=AND(C,D) 8=NOT(7)
//      9=OR(E,F)  10=AND(6,8)  11=AND(10,9)  12=S
void init_fase_8() {
    num_nos = 13; num_inputs = 6; output_id = 12;
    input_ids[0]=0; input_labels[0]='A';
    input_ids[1]=1; input_labels[1]='B';
    input_ids[2]=2; input_labels[2]='C';
    input_ids[3]=3; input_labels[3]='D';
    input_ids[4]=4; input_labels[4]='E';
    input_ids[5]=5; input_labels[5]='F';

    kinds[6]=GATE_AND; kinds[7]=GATE_AND; kinds[8]=GATE_NOT;
    kinds[9]=GATE_OR;
    kinds[10]=GATE_AND; kinds[11]=GATE_AND;

    edge(0,6);  edge(1,6);  // A, B → AND(AB)
    edge(2,7);  edge(3,7);  // C, D → AND(CD)
    edge(7,8);              // AND(CD) → NOT
    edge(4,9);  edge(5,9);  // E, F → OR
    edge(6,10); edge(8,10); // AND(AB), NOT(CD) → AND
    edge(10,11);edge(9,11); // AND, OR → AND
    edge(11,12);
}

// FASE 09: (~A+(BC))(~A+(F(DE)))
// Nós: 0=A 1=B 2=C 3=D 4=E 5=F | 6=NA
//      7=AND(B,C)  8=AND(D,E)  9=AND(F,8)
//      10=OR(NA,BC)  11=OR(NA,F_DE)  12=AND(10,11)  13=S
void init_fase_9() {
    num_nos = 14; num_inputs = 6; output_id = 13;
    input_ids[0]=0; input_labels[0]='A';
    input_ids[1]=1; input_labels[1]='B';
    input_ids[2]=2; input_labels[2]='C';
    input_ids[3]=3; input_labels[3]='D';
    input_ids[4]=4; input_labels[4]='E';
    input_ids[5]=5; input_labels[5]='F';

    kinds[6]=GATE_NOT;
    kinds[7]=GATE_AND; kinds[8]=GATE_AND; kinds[9]=GATE_AND;
    kinds[10]=GATE_OR; kinds[11]=GATE_OR;
    kinds[12]=GATE_AND;

    edge(0,6);              // A → NOT_A
    edge(1,7);  edge(2,7);  // B, C   → AND(BC)
    edge(3,8);  edge(4,8);  // D, E   → AND(DE)
    edge(5,9);  edge(8,9);  // F, DE  → AND(F,DE)
    edge(6,10); edge(7,10); // NA, BC → OR1
    edge(6,11); edge(9,11); // NA, F_DE → OR2
    edge(10,12);edge(11,12);// OR1, OR2 → AND
    edge(12,13);
}

// FASE 10: (((A~B)C)+(~C(~AB)))+((DE)F)
// Nós: 0=A 1=B 2=C 3=D 4=E 5=F | 6=NA 7=NB 8=NC
//      9=AND(A,NB)  10=AND(NA,B)
//      11=AND(9,C)  12=AND(NC,10)  13=AND(D,E)  14=AND(13,F)
//      15=OR(11,12)  16=OR(15,14)  17=S
void init_fase_10() {
    num_nos = 18; num_inputs = 6; output_id = 17;
    input_ids[0]=0; input_labels[0]='A';
    input_ids[1]=1; input_labels[1]='B';
    input_ids[2]=2; input_labels[2]='C';
    input_ids[3]=3; input_labels[3]='D';
    input_ids[4]=4; input_labels[4]='E';
    input_ids[5]=5; input_labels[5]='F';

    kinds[6]=GATE_NOT; kinds[7]=GATE_NOT; kinds[8]=GATE_NOT;
    kinds[9]=GATE_AND; kinds[10]=GATE_AND;
    kinds[11]=GATE_AND; kinds[12]=GATE_AND;
    kinds[13]=GATE_AND; kinds[14]=GATE_AND;
    kinds[15]=GATE_OR;  kinds[16]=GATE_OR;

    edge(0,6);              // A → NOT_A
    edge(1,7);              // B → NOT_B
    edge(2,8);              // C → NOT_C
    edge(0,9);  edge(7,9);  // A, NOT_B  → AND(A,~B)
    edge(6,10); edge(1,10); // NOT_A, B  → AND(~A,B)
    edge(9,11); edge(2,11); // AND(A,NB), C   → AND
    edge(8,12); edge(10,12);// NOT_C, AND(NA,B)→ AND
    edge(3,13); edge(4,13); // D, E      → AND(DE)
    edge(13,14);edge(5,14); // AND(DE), F → AND
    edge(11,15);edge(12,15);// → OR1
    edge(15,16);edge(14,16);// → OR2
    edge(16,17);
}

// ── Dispatcher de fases ───────────────────────────────────────────────────────

void load_phase(int phase) {
    clear_circuit();
    switch (phase) {
        case 1:  init_fase_1();  break;
        case 2:  init_fase_2();  break;
        case 3:  init_fase_3();  break;
        case 4:  init_fase_4();  break;
        case 5:  init_fase_5();  break;
        case 6:  init_fase_6();  break;
        case 7:  init_fase_7();  break;
        case 8:  init_fase_8();  break;
        case 9:  init_fase_9();  break;
        case 10: init_fase_10(); break;
    }
    atualizar_rgb();
}

// ── Lógica do circuito ────────────────────────────────────────────────────────

bool is_input(int node) {
    for (int i = 0; i < num_inputs; i++) {
        if (input_ids[i] == node) return true;
    }
    return false;
}

void propagate() {
    for (int node = 0; node < num_nos; node++) {
        if (is_input(node)) continue;

        // Coleta predecessores consultando a coluna `node` da matriz
        bool inputs[MAX_N];
        int  n_in = 0;
        for (int i = 0; i < num_nos; i++) {
            if (adjacency[i][node]) inputs[n_in++] = values[i];
        }
        if (n_in == 0) continue;

        switch (kinds[node]) {
            case GATE_AND: {
                bool r = true;
                for (int k = 0; k < n_in; k++) if (!inputs[k]) { r=false; break; }
                values[node] = r; break;
            }
            case GATE_OR: {
                bool r = false;
                for (int k = 0; k < n_in; k++) if (inputs[k])  { r=true;  break; }
                values[node] = r; break;
            }
            case GATE_NOT:    values[node] = !inputs[0]; break;
            case GATE_BUFFER: values[node] =  inputs[0]; break;
        }
    }
}

// Alterna entrada pelo índice em input_ids[] e repropaga
void toggle_input(int input_index) {
    int node_id = input_ids[input_index];
    values[node_id] = !values[node_id];
    propagate();
    atualizar_rgb();
    display_dirty = true;
}

// Verifica vitória — apenas marca phase_won para habilitar avanço de fase.
// A mensagem de vitória é exibida dentro de print_state() quando S=1.
// O jogador continua podendo alterar entradas normalmente após vencer.
void check_victory() {
    if (values[output_id]) {
        phase_won = true;
    }
}

void advance_phase() {
    Serial.println(">>> advance_phase() chamada");

    if (current_phase < NUM_PHASES) {
        current_phase++;
        load_phase(current_phase);
        propagate();
        Serial.println();
        Serial.print("=== FASE ");
        Serial.print(current_phase);
        Serial.print(" === ");
        Serial.println(phase_formulas[current_phase - 1]);
        print_state();
    } else {
        Serial.println("Ja esta na ultima fase!");
    }

    display_dirty = true;
}

void previous_phase() {
    Serial.println(">>> previous_phase() chamada");

    if (current_phase > 1) {
        current_phase--;
        load_phase(current_phase);
        propagate();
        Serial.println();
        Serial.print("=== FASE ");
        Serial.print(current_phase);
        Serial.print(" === ");
        Serial.println(phase_formulas[current_phase - 1]);
        print_state();
    } else {
        Serial.println("Ja esta na primeira fase!");
    }

    display_dirty = true;
}

void cycle_selected_input(int direction) {
    selected_input = (selected_input + direction + num_inputs) % num_inputs;
    display_dirty = true;
}

// ── Exibição ──────────────────────────────────────────────────────────────────

void print_state() {
    Serial.println("-----------------------------------------");
    Serial.print("FASE "); Serial.print(current_phase);
    Serial.print("/"); Serial.print(NUM_PHASES);
    Serial.print("  f = ");
    Serial.println(phase_formulas[current_phase - 1]);
    Serial.println();

    // Entradas — mostra letra, valor e seleção atual
    for (int i = 0; i < num_inputs; i++) {
        Serial.print("  ");
        Serial.print(input_labels[i]);
        Serial.print(" = ");
        Serial.print(values[input_ids[i]] ? "1" : "0");
        if (i == selected_input) Serial.print("  <-- selecionado");
        Serial.println();
    }

    Serial.println();

    // Nós internos (portas) — exibe índice e valor
    for (int i = num_inputs; i < num_nos - 1; i++) {
        Serial.print("  no["); Serial.print(i); Serial.print("] = ");
        Serial.println(values[i] ? "1" : "0");
    }

    Serial.println();
    Serial.print("  S (saida) = ");
    Serial.println(values[output_id] ? "1" : "0");

    // Banner de vitória inline — aparece sempre que S=1
    if (values[output_id]) {
        Serial.println();
        Serial.println("  >>> CIRCUITO ATIVADO! Voce venceu! <<<");
        if (current_phase < NUM_PHASES) {
            Serial.println("  Use 'n' ou BTN_CYCLE para a proxima fase.");
        } else {
            Serial.println("  Parabens! Todas as 10 fases concluidas!");
        }
    }
    Serial.println("-----------------------------------------");

    // Instrução das teclas disponíveis nesta fase
    Serial.print("[BOTOES] CYCLE=seleciona | TOGGLE=alterna");
    if (phase_won) Serial.print(" | CYCLE(hold)=prox.fase");
    Serial.println();
    Serial.print("[SERIAL] ");
    for (int i = 0; i < num_inputs; i++) {
        Serial.print("'");
        // imprime letra minúscula
        Serial.print((char)(input_labels[i] + 32));
        Serial.print("'=");
        Serial.print(input_labels[i]);
        if (i < num_inputs - 1) Serial.print(" | ");
    }
    if (phase_won && current_phase < NUM_PHASES) Serial.print(" | 'n'=prox.fase");
    Serial.println();
    Serial.println();
}

// ── Leitura serial ────────────────────────────────────────────────────────────

void handle_serial() {
    if (!Serial.available()) return;
    char key = (char)Serial.read();

    // Normaliza para maiúsculo
    if (key >= 'a' && key <= 'z') key -= 32;

    // 'N' avança fase após vitória
    if (key == 'N') {
        if (phase_won) {
            advance_phase();
        } else {
            Serial.println("[SERIAL] Faca S=1 primeiro para avanÃ§ar de fase.");
        }
        return;
    }

    // Ignora quebras de linha
    if (key == '\n' || key == '\r') return;

    // Busca a entrada correspondente à letra digitada
    int idx = -1;
    for (int i = 0; i < num_inputs; i++) {
        if (input_labels[i] == key) { idx = i; break; }
    }

    if (idx >= 0) {
        Serial.print("[SERIAL] Alternando ");
        Serial.print(input_labels[idx]);
        Serial.println("...");
        toggle_input(idx);
        check_victory();
        print_state();
    } else {
        Serial.print("[SERIAL] '");
        Serial.print(key);
        Serial.println("' nao existe nesta fase.");
    }
}

// ── Leitura dos botões ────────────────────────────────────────────────────────

// void handle_buttons(unsigned long now) {
//     bool curr_cycle  = digitalRead(BTN_CYCLE);
//     bool curr_toggle = digitalRead(BTN_TOGGLE);

//     // ── Modo leitura: BTN_CYCLE avança telas ─────────────────────────────────
//     if (game_mode == MODE_READING) {
//         if (prev_cycle == HIGH && curr_cycle == LOW) {
//             if (now - last_cycle_press > DEBOUNCE_MS) {
//                 last_cycle_press = now;
//                 reading_next();   // avança tela ou entra no jogo
//             }
//         }
//         // BTN_TOGGLE ignorado no modo leitura
//         prev_cycle  = curr_cycle;
//         prev_toggle = curr_toggle;
//         return;
//     }

//     // ── Modo jogo: comportamento original ────────────────────────────────────
//     if (prev_cycle == HIGH && curr_cycle == LOW) {
//         if (now - last_cycle_press > DEBOUNCE_MS) {
//             last_cycle_press = now;
//             cycle_selected_input();
//             print_state();
//         }
//     }

//     if (prev_toggle == HIGH && curr_toggle == LOW) {
//         if (now - last_toggle_press > DEBOUNCE_MS) {
//             last_toggle_press = now;
//             toggle_input(selected_input);
//             check_victory();
//             print_state();
//         }
//     }

//     prev_cycle  = curr_cycle;
//     prev_toggle = curr_toggle;

//     desenharFase1();
// }

void handle_joystick(unsigned long now) {
    int x_val = readFilteredX();
    int y_val = readFilteredY();
    bool curr_sw = digitalRead(JOY_SW);

    static unsigned long last_log = 0;


    // ── 1. Interrupção por Polling no Chaveamento do Eixo Z (SW) ─────────
    if (prev_sw == HIGH && curr_sw == LOW) {
        if (now - last_sw_press > 100) { // Filtro elétrico contra repique 
            last_sw_press = now;
            
            if (game_mode == MODE_PLAYING) {
                toggle_input(selected_input);
                check_victory();
                print_state();
            } else if (current_screen == 3) {
                alterar_cor_joystick();
            }
        }
    }
    prev_sw = curr_sw;

    // ── 2. Cálculo dos Limites Histeréticos (Eixos Contínuos) ───────────
    bool x_in_deadzone = (x_val > joy_center_x - JOY_DEADZONE && x_val < joy_center_x + JOY_DEADZONE);
    bool y_in_deadzone = (y_val > joy_center_y - JOY_DEADZONE && y_val < joy_center_y + JOY_DEADZONE);

    // EIXO X: Só destrava a trava lógica se a mola repousar estática no centro por JOY_SETTLE_TIME
    if (x_in_deadzone) {
        if (now - x_center_time > JOY_SETTLE_TIME) {
            axis_x_active = false; 
        }
    } else {
        // Se a haste saiu do centro (seja pelo seu dedo ou pelo overshoot da mola), reseta o cronômetro
        x_center_time = now; 
    }

    // EIXO Y: Mesma lógica de filtro passa-baixa mecânico
    if (y_in_deadzone) {
        if (now - y_center_time > JOY_SETTLE_TIME) {
            axis_y_active = false;
        }
    } else {
        y_center_time = now;
    }

    bool x_in_center = (abs(x_val - joy_center_x) <= JOY_CENTER_ZONE);
    if (x_in_center) {
        if (now - x_center_time > JOY_SETTLE_TIME) {
            axis_x_active = false;
        }
    } else {
        x_center_time = now;
    }

    if (millis() - last_log > 500) {
        last_log = millis();
        Serial.printf("X=%4d  Y=%4d  | deadX=%d  axisX=%d\n", x_val, y_val, x_in_deadzone, axis_x_active);
    }

// ── 3. ESTADO: MODO LEITURA (Varredura de Cenas via Eixo X) ─────────
    if (game_mode == MODE_READING) {
        if (!axis_x_active) {
            if (x_val > joy_center_x + JOY_DEADZONE) {
                reading_next();
                axis_x_active = true;
            } else if (x_val < joy_center_x - JOY_DEADZONE) {
                reading_prev();
                axis_x_active = true;
            }
        }
    }

    if (game_mode == MODE_READING && !axis_x_active && (millis() - last_reading_action > 400)) {
        if (x_val > joy_center_x + JOY_DEADZONE) {
            reading_next();
            axis_x_active = true;
            last_reading_action = millis();
        } else if (x_val < joy_center_x - JOY_DEADZONE) {
            reading_prev();
            axis_x_active = true;
            last_reading_action = millis();
        }
    }

    // ── 4. ESTADO: MODO JOGO (Operação das Portas Lógicas) ──────────────
    if (game_mode == MODE_PLAYING) {
        
        // Eixo Y (Cima / Baixo): Selecionar qual entrada será operada
        if (!axis_y_active) {
            if (y_val > joy_center_y + JOY_DEADZONE) {
                cycle_selected_input(1);
                print_state();
                axis_y_active = true;
            } else if (y_val < joy_center_y - JOY_DEADZONE) {
                cycle_selected_input(-1);
                print_state();
                axis_y_active = true;
            }
        }

        // Eixo X (Direita / Esquerda): Trocar de Fase
        if (!axis_x_active) {
            if (x_val > joy_center_x + JOY_DEADZONE) {
                if (phase_won) {
                    advance_phase();
                } else {
                    Serial.println("[AVISO] Resolva o circuito (S=1) para avancar!");
                }
                axis_x_active = true;
            } else if (x_val < joy_center_x - JOY_DEADZONE) {
                previous_phase();
                axis_x_active = true;
            }
        }
        
        // desenharFase1();
        // desenharFaseAtual();
    }
}

// ── Setup e Loop ──────────────────────────────────────────────────────────────

void calibrate_joystick() {
    long sumX = 0, sumY = 0;
    for (int i = 0; i < 200; i++) {
        sumX += analogRead(JOY_VRX);
        sumY += analogRead(JOY_VRY);
        delay(5);
    }
    joy_center_x = sumX / 200;
    joy_center_y = sumY / 200;
    Serial.printf("Centro calibrado: X=%d Y=%d\n", joy_center_x, joy_center_y);
}

void atualizar_rgb() {
    if (values[output_id]) rgb_verde();
    else                   rgb_vermelho();
}

void setup() {
    Serial.begin(115200);
    pinMode(JOY_SW, INPUT_PULLUP);
    pinMode(RGB_R, OUTPUT);
    pinMode(RGB_G, OUTPUT);
    rgb_vermelho();

    calibrate_joystick();
    int joy_center_x = 2048, joy_center_y = 2048;

    load_phase(current_phase);
    propagate();

    Serial.println("=== LOGIC GATE PUZZLE — 10 FASES ===");
    Serial.println("Objetivo: fazer S = 1 em cada fase");
    Serial.println("~ = NOT  letras juntas = AND  + = OR");
    Serial.println();
    print_state();

    inicializar_display();

    iniciarCoresFase();      // inicializa COR_ATIVO/INATIVO/FUNDO

    // Exibe a primeira tela de leitura
    reading_show_current();
}

void loop() {
    unsigned long now = millis();

    handle_joystick(now);
    handle_serial();

    if (game_mode == MODE_READING) {
        if (now - last_rainbow_update >= RAINBOW_INTERVAL) {
            last_rainbow_update = now;
            updateRainbowColor();               // calcula nova cor
            reading_show_current();             // redesenha a tela atual
        }

        // Movimenta a cobrinha na borda
        advanceSnake();
    }

    // 3. Máquina de Estados: Modo Jogo (Atualização de Lógica/Timers)
    if (game_mode == MODE_PLAYING) {
        // Oscilador não-bloqueante para a entrada selecionada
        if (now - last_blink_time >= BLINK_INTERVAL) {
            last_blink_time = now;
            blink_state = !blink_state; // Alterna o estado do bit (0/1)
            display_dirty = true;       // Aciona o gatilho para atualizar a IHM
        }
    }

    // ── Redesenha o circuito apenas quando necessário ──
    if (game_mode == MODE_PLAYING && display_dirty) {
        display_dirty = false;
        desenharFaseAtual();
    }
}
