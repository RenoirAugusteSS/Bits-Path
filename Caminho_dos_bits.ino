#include <Adafruit_NeoPixel.h>
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
extern bool exibir_resultado;

// logic_gate_game_v6.ino
// 10 fases de portas lógicas — ESP32
// Base: v3 (dois botões + serial em paralelo, vitória sem travar)
//
// Hardware:
//   BTN_CYCLE  (GPIO 18) INPUT_PULLUP — cicla entradas / avança fase ao vencer
//   BTN_TOGGLE (GPIO 19) INPUT_PULLUP — alterna valor da entrada selecionada

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
#define NUM_PHASES 13

// Animação da borda
#define PERIMETER       188      // número de pixels na borda externa (2*(64+32)-4)
#define SNAKE_LENGTH    16          // comprimento da cobrinha (8 pixels)
#define BORDER_SPEED_MS 60       // intervalo entre cada movimento da cobrinha (ms)

#define RGB_R 22
#define RGB_G 2

#define NUMERO_LEDS 6
#define PIN_DATA_LEDS 18
// Objeto de controle da fita de LEDs
Adafruit_NeoPixel fita_LED(NUMERO_LEDS, PIN_DATA_LEDS);

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
bool phase_won      = false; // true após S=1; BTN_CYCLE avança fase

int vidas = 3;
int movimentos = 0;


// ── Fórmulas para exibição ────────────────────────────────────────────────────

const char* phase_formulas[NUM_PHASES] = {
    "A B (AND)",           // Fase 1 (Tutorial)
    "A + B (OR)",          // Fase 2 (Tutorial)
    "~A (NOT)",            // Fase 3 (Tutorial)
    "(A~E)+(~AE)",         // Fase 4 (Antiga Fase 1)
    "(A~B)+(BF)",          // Fase 5
    "(~AC)+(~A~E)",        // Fase 6
    "(AB)+((D+E)~F)",      // Fase 7
    "((~AB)(~C~D))(EF)",   // Fase 8
    "(((~AB)((C+D)~(CD)))E)~F", // Fase 9
    "(~(~AB))(~CD)(~EF)",  // Fase 10
    "((AB)(~(CD)))(E+F)",  // Fase 11
    "(~A+(BC))(~A+(F(DE)))",// Fase 12
    "(((A~B)C)+(~C(~AB)))+((DE)F)" // Fase 13
};

// ── Debounce ──────────────────────────────────────────────────────────────────

#define DEBOUNCE_MS 50
unsigned long last_cycle_press  = 0;
unsigned long last_toggle_press = 0;
bool prev_cycle  = HIGH;
bool prev_toggle = HIGH;

// ── Leitura de Analógicos ─────────────────────────────────────────────────────

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

void init_fase_tutorial_and() {
    num_nos = 4; 
    num_inputs = 2; 
    output_id = 3;
    
    input_ids[0] = 0; input_labels[0] = 'A';
    input_ids[1] = 1; input_labels[1] = 'B';

    kinds[2] = GATE_AND;
    
    edge(0, 2); 
    edge(1, 2); 
    edge(2, 3); 
}

void init_fase_tutorial_or() {
    num_nos = 4; 
    num_inputs = 2; 
    output_id = 3;
    
    input_ids[0] = 0; input_labels[0] = 'A';
    input_ids[1] = 1; input_labels[1] = 'B';

    kinds[2] = GATE_OR;
    
    edge(0, 2); 
    edge(1, 2); 
    edge(2, 3); 
}

void init_fase_tutorial_not() {
    num_nos = 3; 
    num_inputs = 1; 
    output_id = 2;
    
    input_ids[0] = 0; input_labels[0] = 'A';

    kinds[1] = GATE_NOT;
    
    edge(0, 1); 
    edge(1, 2); 
}

void init_fase_1() {
    num_nos = 8; num_inputs = 2; output_id = 7;
    input_ids[0]=0; input_labels[0]='A';
    input_ids[1]=1; input_labels[1]='E';

    kinds[2]=GATE_NOT; kinds[3]=GATE_NOT;
    kinds[4]=GATE_AND; kinds[5]=GATE_AND;
    kinds[6]=GATE_OR;

    edge(0,2);            
    edge(1,3);            
    edge(0,4); edge(3,4); 
    edge(2,5); edge(1,5); 
    edge(4,6); edge(5,6); 
    edge(6,7);            
}

void init_fase_2() {
    num_nos = 8; num_inputs = 3; output_id = 7;
    input_ids[0]=0; input_labels[0]='A';
    input_ids[1]=1; input_labels[1]='B';
    input_ids[2]=2; input_labels[2]='F';

    kinds[3]=GATE_NOT;
    kinds[4]=GATE_AND; kinds[5]=GATE_AND;
    kinds[6]=GATE_OR;

    edge(1,3);            
    edge(0,4); edge(3,4); 
    edge(1,5); edge(2,5); 
    edge(4,6); edge(5,6); 
    edge(6,7);
}

void init_fase_3() {
    num_nos = 9; num_inputs = 3; output_id = 8;
    input_ids[0]=0; input_labels[0]='A';
    input_ids[1]=1; input_labels[1]='C';
    input_ids[2]=2; input_labels[2]='E';

    kinds[3]=GATE_NOT; kinds[4]=GATE_NOT;
    kinds[5]=GATE_AND; kinds[6]=GATE_AND;
    kinds[7]=GATE_OR;

    edge(0,3);            
    edge(2,4);            
    edge(3,5); edge(1,5); 
    edge(3,6); edge(4,6); 
    edge(5,7); edge(6,7); 
    edge(7,8);
}

void init_fase_4() {
    num_nos = 11; num_inputs = 5; output_id = 10;
    input_ids[0]=0; input_labels[0]='A';
    input_ids[1]=1; input_labels[1]='B';
    input_ids[2]=2; input_labels[2]='D';
    input_ids[3]=3; input_labels[3]='E';
    input_ids[4]=4; input_labels[4]='F';

    kinds[5]=GATE_AND; kinds[6]=GATE_OR; kinds[7]=GATE_NOT;
    kinds[8]=GATE_AND; kinds[9]=GATE_OR;

    edge(0,5); edge(1,5); 
    edge(2,6); edge(3,6); 
    edge(4,7);            
    edge(6,8); edge(7,8); 
    edge(5,9); edge(8,9); 
    edge(9,10);
}

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

    edge(0,6);              
    edge(2,7);              
    edge(3,8);              
    edge(6,9);  edge(1,9);  
    edge(7,10); edge(8,10); 
    edge(4,11); edge(5,11); 
    edge(9,12); edge(10,12);
    edge(12,13);edge(11,13);
    edge(13,14);
}

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

    edge(0,6);              
    edge(5,7);              
    edge(6,8);  edge(1,8);  
    edge(2,9);  edge(3,9);  
    edge(2,10); edge(3,10); 
    edge(10,11);            
    edge(9,12); edge(11,12);
    edge(8,13); edge(12,13);
    edge(13,14);edge(4,14); 
    edge(14,15);edge(7,15); 
    edge(15,16);
}

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

    edge(0,6);              
    edge(2,7);              
    edge(4,8);              
    edge(6,9);  edge(1,9);  
    edge(9,10);             
    edge(7,11); edge(3,11); 
    edge(8,12); edge(5,12); 
    edge(10,13);edge(11,13);
    edge(13,14);edge(12,14);
    edge(14,15);
}

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

    edge(0,6);  edge(1,6);  
    edge(2,7);  edge(3,7);  
    edge(7,8);              
    edge(4,9);  edge(5,9);  
    edge(6,10); edge(8,10); 
    edge(10,11);edge(9,11); 
    edge(11,12);
}

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

    edge(0,6);              
    edge(1,7);  edge(2,7);  
    edge(3,8);  edge(4,8);  
    edge(5,9);  edge(8,9);  
    edge(6,10); edge(7,10); 
    edge(6,11); edge(9,11); 
    edge(10,12);edge(11,12);
    edge(12,13);
}

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

    edge(0,6);              
    edge(1,7);              
    edge(2,8);              
    edge(0,9);  edge(7,9);  
    edge(6,10); edge(1,10); 
    edge(9,11); edge(2,11); 
    edge(8,12); edge(10,12);
    edge(3,13); edge(4,13); 
    edge(13,14);edge(5,14); 
    edge(11,15);edge(12,15);
    edge(15,16);edge(14,16);
    edge(16,17);
}

// ── Dispatcher de fases ───────────────────────────────────────────────────────

void load_phase(int phase) {
    clear_circuit();
    exibir_resultado = false;

    switch (phase) {
        case 1:  init_fase_tutorial_and();  break;
        case 2:  init_fase_tutorial_or();  break;
        case 3:  init_fase_tutorial_not();  break;
        case 4:  init_fase_1();  break;
        case 5:  init_fase_2();  break;
        case 6:  init_fase_3();  break;
        case 7:  init_fase_4();  break;
        case 8:  init_fase_5();  break;
        case 9:  init_fase_6();  break;
        case 10:  init_fase_7();  break;
        case 11:  init_fase_8();  break;
        case 12:  init_fase_9();  break;
        case 13: init_fase_10(); break;
    }
    atualizar_rgb();
    atualizar_fita_led();
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
    movimentos++;
    
    exibir_resultado = false; // <-- Esconde as cores ao fazer um novo movimento
    
    propagate();
    atualizar_rgb();
    atualizar_fita_led();
    display_dirty = true;
}

void advance_phase() {
    if (current_phase < NUM_PHASES) {
        current_phase++;
        load_phase(current_phase);
        propagate();
    }
    display_dirty = true;
}

void check_victory() {
    exibir_resultado = true; // <-- Dispara o gatilho para revelar o circuito em verde/vermelho
    display_dirty = true;    // <-- Força uma renderização imediata do novo estado
    
    if (values[output_id]) {
        phase_won = true;
        // Opcional: Adicionar um delay() aqui para o jogador ver a luz verde chegar no final
        delay(1000); 
        advance_phase();
    } else {
        vidas--;
        if (vidas <= 0) {
            vidas = 3;
            movimentos = 0;
            current_phase = 1;
            // Opcional: Um delay para ele ver onde errou antes de resetar
            delay(1500);
            load_phase(current_phase);
            propagate();
        }
    }
}

void previous_phase() {
    if (current_phase > 1) {
        current_phase--;
        load_phase(current_phase);
        propagate();
    }
    display_dirty = true;
}

void cycle_selected_input(int direction) {
    selected_input = (selected_input + direction + num_inputs) % num_inputs;
    atualizar_fita_led();
    display_dirty = true;
}

// ── Leitura dos botões / Joystick ─────────────────────────────────────────────

void handle_joystick(unsigned long now) {
    int x_val = readFilteredX();
    int y_val = readFilteredY();
    bool curr_sw = digitalRead(JOY_SW);

    // ── 1. Interrupção por Polling no Chaveamento do Eixo Z (SW) ─────────
    if (prev_sw == HIGH && curr_sw == LOW) {
        if (now - last_sw_press > 100) { // Filtro elétrico contra repique 
            last_sw_press = now;
            
            if (game_mode == MODE_PLAYING) {
                toggle_input(selected_input);
            } else if (current_screen == 3 || current_screen == 4) {
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
                axis_y_active = true;
            } else if (y_val < joy_center_y - JOY_DEADZONE) {
                cycle_selected_input(-1);
                axis_y_active = true;
            }
        }

        // Eixo X (Direita / Esquerda): Trocar de Fase e Checar Vitória
        if (!axis_x_active) {
            if (x_val < joy_center_x - JOY_DEADZONE) { // Movimento para a esquerda (prosseguir/verificar)
                check_victory();
                axis_x_active = true;
            } else if (x_val > joy_center_x + JOY_DEADZONE) { // Movimento para a direita (voltar)
                previous_phase();
                axis_x_active = true;
            }
        }
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
}

void atualizar_rgb() {
    if (values[output_id]) rgb_verde();
    else                   rgb_vermelho();
}

// Sincroniza a fita de LEDs com o estado lógico e a interface do usuário
void atualizar_fita_led() {
    fita_LED.clear(); // Apaga o buffer anterior
    
    // Iteramos apenas até o número de entradas ativas na fase atual
    for (int i = 0; i < num_inputs; i++) {
        if (i == selected_input) {
            // A entrada sob o cursor acende em branco (R:255, G:255, B:255)
            fita_LED.setPixelColor(i, fita_LED.Color(255, 255, 255));
        } else {
            // As outras entradas mostram o estado lógico atual da planta
            // 0 = Vermelho tênue, 1 = Verde tênue
            int node_id = input_ids[i];
            if (values[node_id]) {
                fita_LED.setPixelColor(i, fita_LED.Color(0, 30, 0)); // Estado HIGH
            } else {
                fita_LED.setPixelColor(i, fita_LED.Color(30, 0, 0)); // Estado LOW
            }
        }
    }
    fita_LED.show(); // Dispara o sinal via periférico RMT do ESP32
}

void setup() {
    pinMode(JOY_SW, INPUT_PULLUP);
    pinMode(RGB_R, OUTPUT);
    pinMode(RGB_G, OUTPUT);
    rgb_vermelho();

    fita_LED.begin();
    fita_LED.show();

    calibrate_joystick();

    load_phase(current_phase);
    propagate();

    inicializar_display();

    iniciarCoresFase();      // inicializa COR_ATIVO/INATIVO/FUNDO

    // Exibe a primeira tela de leitura
    reading_show_current();
}

void loop() {
    unsigned long now = millis();

    handle_joystick(now);

    if (game_mode == MODE_READING) {
        if (now - last_rainbow_update >= RAINBOW_INTERVAL) {
            last_rainbow_update = now;
            updateRainbowColor();               // calcula nova cor
            reading_show_current();             // redesenha a tela atual
        }
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