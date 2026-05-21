void desenharFase4() {
    inicializarMatriz();

    // ── Extração de Estados (Baseado na topologia de init_fase_4) ──────────
    short int vA    = values[0]; // Entrada A
    short int vB    = values[1]; // Entrada B
    short int vD    = values[2]; // Entrada D
    short int vE    = values[3]; // Entrada E
    short int vF    = values[4]; // Entrada F
    
    short int vAND1 = values[5]; // AND(A, B)
    short int vOR1  = values[6]; // OR(D, E)
    short int vNF   = values[7]; // NOT_F
    
    short int vAND2 = values[8]; // AND(OR1, NOT_F)
    short int vOR2  = values[9]; // OR2(AND1, AND2)
    short int vS    = values[10]; // Saída Final (S)

    // ── 1. Planejamento Espacial (Eixo Y) ─────────────────────────────────
    // As 5 entradas são distribuídas respeitando uma margem para o fan-in
    int rowA = 4;
    int rowB = 9;
    int rowD = 16;
    int rowE = 21;
    int rowF = 28;

    bool show_A = !(selected_input == 0 && !blink_state);
    bool show_B = !(selected_input == 1 && !blink_state);
    bool show_D = !(selected_input == 2 && !blink_state);
    bool show_E = !(selected_input == 3 && !blink_state);
    bool show_F = !(selected_input == 4 && !blink_state);

    // Coordenadas Centrais das Portas Lógicas
    int colAND1 = 21;  int rowAND1 = 5;   // saída em (col26, row7)
    int colOR1  = 21;  int rowOR1  = 17;  // saída em (col28, row19) — mpOR é mais largo
    int colNOTF = 9;   int rowNOTF = 26;  // saída em (col11, row28)
    
    int colAND2 = 33;  int rowAND2 = 22;  // saída em (col38, row24)
    int colOR2 = 45;  int rowOR2 = 13;    // saída em (col52, row16)

    // Saídas
    int rowSaidaAND1 = rowAND1 + 2;   // 7
    int rowSaidaOR1  = rowOR1  + 3;   // 19
    int rowSaidaAND2 = rowAND2 + 2;   // 24
    int rowSaidaOR2  = rowOR2  + 3;   // 16

    // ── 2. Alimentação de Entradas e Inversor ─────────────────────────────
    
    // ── Condições aplicadas no Roteamento Inicial ────────────────────────────
    if (show_A) { MH(0, 15, rowA, vA); }
    if (show_B) { MH(0, 15, rowB, vB); }
    if (show_D) { MH(0, 15, rowD, vD); }
    if (show_E) { MH(0, 15, rowE, vE); }
    if (show_F) { MH(0, 8, rowF, vF);  }

    // Roteamento até a Coluna de Derivação 1 (X=15) e ajuste em Y
    MV(15, rowA, 6, vA); MH(16, 20, 6, vA);
    MV(15, rowB, 8, vB); MH(16, 20, 8, vB);

    MV(15, rowD, 18, vD); MH(16, 20, 18, vD);
    MV(15, rowE, 20, vE); MH(16, 20, 20, vE);
    
    // ── 3. NOT_F ──────────────────────────────────────────────────────────
    mpNOT(colNOTF, rowNOTF, /*vNF*/2);  // saída em (col11, row28)

    // ── 4. Estágio 1: AND(A,B) e OR(D,E) ─────────────────────────────────
    mpAND(colAND1, rowAND1, /*vAND1*/2);
    mpOR (colOR1,  rowOR1,  /*vOR1*/2);

    // ── 4. Roteamento Intermediário (Sub-árvore Inferior) ─────────────────
    // O barramento converge OR1 e NOT_F na Coluna de Derivação 2 (X=28)
    
    // Ramo OR1 -> Topo do AND2
    MH(26, 28, 19, vOR1); MV(28, 19, 23, vOR1); MH(29, 32, 23, vOR1);
    
    // Ramo NOT_F -> Base do AND2
    MH(12, 28, 28, vNF);  MV(28, 28, 25, vNF);  MH(29, 32, 25, vNF);

    // ── 6. Estágio 2: AND(OR1, NF) ───────────────────────────────────────
    mpAND(colAND2, rowAND2, /*vAND2*/2);

    // ── 5. Estágio Lógico Final (Convergência Global) ─────────────────────
    // O barramento converge AND1 e AND2 na Coluna de Derivação 3 (X=40)
    
    // Ramo AND1 -> Topo do OR2
    MH(26, 40, 7, vAND1); MV(40, 7, 15, vAND1); MH(41, 44, 15, vAND1);
    
    // Ramo AND2 -> Base do OR2
    MH(38, 40, 24, vAND2); MV(40, 24, 17, vAND2); MH(41, 44, 17, vAND2);

    // ── 8. OR2 final ──────────────────────────────────────────────────────
    mpOR(colOR2, rowOR2, /*vOR2*/2);

    // ── 9. Saída → LED indicador ──────────────────────────────────────────
    MH(colOR2+5, colOR2+9, rowSaidaOR2, vS);
    
    for (int r = rowSaidaOR2-1; r <= rowSaidaOR2+1; r++)
        MH(colOR2+10, colOR2+12, r, vS);


    // ── 7. Flush no Framebuffer ───────────────────────────────────────────
    renderizarComCores();
    desenharNumeroFase(4);
}