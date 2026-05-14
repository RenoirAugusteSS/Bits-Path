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

#define BTN_CYCLE  18
#define BTN_TOGGLE 21

// ── Dimensões máximas ─────────────────────────────────────────────────────────

// Fase 10 tem 18 nós — MAX_N=20 dá margem para novas fases
#define MAX_N      20
#define MAX_INPUTS  6
#define NUM_PHASES 10

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
}

void cycle_selected_input() {
    selected_input = (selected_input + 1) % num_inputs;
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

void handle_buttons(unsigned long now) {
    bool curr_cycle  = digitalRead(BTN_CYCLE);
    bool curr_toggle = digitalRead(BTN_TOGGLE);

    // BTN_CYCLE: cicla entradas normalmente; avança fase se já venceu
    if (prev_cycle == HIGH && curr_cycle == LOW) {
        if (now - last_cycle_press > DEBOUNCE_MS) {
            last_cycle_press = now;
            // BTN_CYCLE sempre cicla entradas — mesmo após vencer
            // Para avançar de fase use 'n' no serial
            cycle_selected_input();
            print_state();
        }
    }

    // BTN_TOGGLE: alterna valor da entrada selecionada
    if (prev_toggle == HIGH && curr_toggle == LOW) {
        if (now - last_toggle_press > DEBOUNCE_MS) {
            last_toggle_press = now;
            toggle_input(selected_input);
            check_victory();
            print_state();
        }
    }

    prev_cycle  = curr_cycle;
    prev_toggle = curr_toggle;

    desenharFase1();
}

// ── Setup e Loop ──────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    pinMode(BTN_CYCLE,  INPUT_PULLUP);
    pinMode(BTN_TOGGLE, INPUT_PULLUP);

    load_phase(current_phase);
    propagate();

    Serial.println("=== LOGIC GATE PUZZLE — 10 FASES ===");
    Serial.println("Objetivo: fazer S = 1 em cada fase");
    Serial.println("~ = NOT  letras juntas = AND  + = OR");
    Serial.println();
    print_state();

    inicializar_display();
}

void loop() {
    unsigned long now = millis();
    handle_buttons(now);
    handle_serial();
}
