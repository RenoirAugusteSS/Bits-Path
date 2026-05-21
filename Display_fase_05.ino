void desenharFase5() {
    inicializarMatriz();

    // ── Extração de Estados (Baseado na topologia de init_fase_5) ──────────
    short int vA  = values[0];  short int vB  = values[1];
    short int vC  = values[2];  short int vD  = values[3];
    short int vE  = values[4];  short int vF  = values[5];

    // ── Variáveis Booleanas ──────────────────────────────────────────────────
    bool show_A = !(selected_input == 0 && !blink_state);
    bool show_B = !(selected_input == 1 && !blink_state);
    bool show_C = !(selected_input == 2 && !blink_state);
    bool show_D = !(selected_input == 3 && !blink_state);
    bool show_E = !(selected_input == 4 && !blink_state);
    bool show_F = !(selected_input == 5 && !blink_state);
    
    short int vNA = values[6];  short int vNC = values[7];  short int vND = values[8];
    
    short int vAND1 = values[9];  // AND(NA, B)
    short int vAND2 = values[10]; // AND(NC, ND)
    short int vAND3 = values[11]; // AND(E, F)
    
    short int vAND4 = values[12]; // AND(AND1, AND2)
    short int vAND5 = values[13]; // AND(AND4, AND3)
    short int vS    = values[14]; // Saída Final (S)

    // ── 1. Planejamento Espacial (Y-Spacing) ──────────────────────────────
    // 6 Entradas uniformemente distribuídas pelo eixo Y para suportar o fan-out
    int rowA = 3;  int rowB = 8;
    int rowC = 13; int rowD = 18;
    int rowE = 23; int rowF = 28;

    // Colunas de posicionamento para arquitetura em cascata de 4 Níveis
    int colNOT = 7;
    int colL1  = 19; // 1º Estágio AND
    int colL2  = 33; // 2º Estágio AND
    int colL3  = 47; // 3º Estágio AND

    // Linhas centrais operacionais das portas (Top Input index)
    int rowAND1 = 4;  // Consome as linhas 4 (topo) a 6 (base)
    int rowAND2 = 14; // Consome as linhas 14 (topo) a 16 (base)
    int rowAND3 = 24; // Consome as linhas 24 (topo) a 26 (base)
    int rowAND4 = 8;  // Consome as linhas 8 (topo) a 10 (base)
    int rowAND5 = 16; // Consome as linhas 16 (topo) a 18 (base)

    // ── Condições aplicadas no Roteamento Inicial ────────────────────────────
    if (show_A) { MH(0, colNOT - 1, rowA, vA);  }
    if (show_B) { MH(0, 16, rowB, vB);          }
    if (show_C) { MH(0, colNOT - 1, rowC, vC);  }
    if (show_D) { MH(0, colNOT - 1, rowD, vD); }
    if (show_E) { MH(0, 16, rowE, vE);          }
    if (show_F) { MH(0, 16, rowF, vF);          }

    // ── 2. Renderização das Entradas e Inversores ─────────────────────────
    mpNOT(colNOT, rowA - 2, /*vNA*/2);
    mpNOT(colNOT, rowC - 2, /*vNC*/2);
    mpNOT(colNOT, rowD - 2, /*vND*/2);
                
    // Condução das saídas invertidas até a Coluna de Derivação 1 (X=16)
    MH(colNOT + 3, 16, rowA, vNA);
    MH(colNOT + 3, 16, rowC, vNC);
    MH(colNOT + 3, 16, rowD, vND);

    // ── 3. Estágio Lógico 1 (Portas de Front-end) ─────────────────────────
    // Roteamento Ortogonal -> AND1 (NA, B)
    MV(16, rowA, rowAND1, vNA);     MH(16, colL1 - 1, rowAND1, vNA);
    MV(16, rowB, rowAND1 + 2, vB);  MH(16, colL1 - 1, rowAND1 + 2, vB);
    mpAND(colL1, rowAND1, /*vAND1*/2);
    
    // Roteamento Ortogonal -> AND2 (NC, ND)
    MV(16, rowC, rowAND2, vNC);     MH(16, colL1 - 1, rowAND2, vNC);
    MV(16, rowD, rowAND2 + 2, vND); MH(16, colL1 - 1, rowAND2 + 2, vND);
    mpAND(colL1, rowAND2, /*vAND2*/2);
    
    // Roteamento Ortogonal -> AND3 (E, F)
    MV(16, rowE, rowAND3, vE);      MH(16, colL1 - 1, rowAND3, vE);
    MV(16, rowF, rowAND3 + 2, vF);  MH(16, colL1 - 1, rowAND3 + 2, vF);
    mpAND(colL1, rowAND3, /*vAND3*/2);

    // ── 4. Estágio Lógico 2 (Conjunção Intermediária) ─────────────────────
    // Saídas do Estágio 1 projetadas na Coluna de Derivação 2 (X=30)
    int rowOutAND1 = rowAND1 + 1; // 5
    int rowOutAND2 = rowAND2 + 1; // 15
    
    // Alimentando AND4 com AND1 e AND2
    MH(colL1 + 5, 30, rowOutAND1, vAND1);
    MV(30, rowOutAND1, rowAND4, vAND1);        // Desce de 5 para 8
    MH(30, colL2 - 1, rowAND4, vAND1);
    
    MH(colL1 + 5, 30, rowOutAND2, vAND2);
    MV(30, rowOutAND2, rowAND4 + 2, vAND2);    // Sobe de 15 para 10
    MH(30, colL2 - 1, rowAND4 + 2, vAND2);
    
    mpAND(colL2, rowAND4, /*vAND4*/2);

    // ── 5. Estágio Lógico 3 (Estágio de Potência/Saída Final) ─────────────
    // Saídas de L2 e do restante de L1 projetadas na Coluna de Derivação 3 (X=44)
    int rowOutAND4 = rowAND4 + 1; // 9
    int rowOutAND3 = rowAND3 + 1; // 25
    
    // Alimentando AND5 com AND4 e AND3 (Repare que as trilhas verticais 
    // compartilham a coluna 44, mas operam em segmentos Y independentes)
    MH(colL2 + 5, 44, rowOutAND4, vAND4);
    MV(44, rowOutAND4, rowAND5, vAND4);        // Desce de 9 para 16
    MH(44, colL3 - 1, rowAND5, vAND4);
    
    MH(colL1 + 5, 44, rowOutAND3, vAND3); 
    MV(44, rowOutAND3, rowAND5 + 2, vAND3);    // Sobe de 25 para 18
    MH(44, colL3 - 1, rowAND5 + 2, vAND3);
    
    mpAND(colL3, rowAND5, /*vAND5*/2);

    // ── 6. Roteamento até o Indicador de Estado Lógico (LED S) ────────────
    int rowOutAND5 = rowAND5 + 1; // 17
    MH(colL3 + 5, 54, rowOutAND5, vS);
    
    // Malha do LED S de verificação (matriz de 3x3 no fim da trilha)
    for (int r = rowOutAND5 - 1; r <= rowOutAND5 + 1; r++) {
        MH(55, 57, r, vS);
    }

    // ── 7. Flush no Framebuffer ───────────────────────────────────────────
    renderizarComCores();
    desenharNumeroFase(5);
}