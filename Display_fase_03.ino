void desenharFase3() {
    inicializarMatriz();

    // Recupera os valores lógicos do estado atual do motor do jogo
    short int vA    = values[0]; // Entrada A
    short int vC    = values[1]; // Entrada C
    short int vE    = values[2]; // Entrada E
    short int vNA   = values[3]; // NOT_A
    short int vNE   = values[4]; // NOT_E
    short int vAND1 = values[5]; // AND(NA, C)
    short int vAND2 = values[6]; // AND(NA, NE)
    short int vOR   = values[7]; // OR (Saída S em values[8] = vOR)

    // ── 1. Posicionamento Vertical (Eixo Y) ───────────────────────────────
    int rowA = 4;
    int rowC = 15;
    int rowE = 26;

    bool show_A = !(selected_input == 0 && !blink_state);
    bool show_C = !(selected_input == 1 && !blink_state);
    bool show_E = !(selected_input == 2 && !blink_state);

    // Inversores (offset de -2 em relação à linha para centralizar)
    int colNOTA = 9;
    int rowNOTA = rowA - 2; // Centro em rowA (4)

    int colNOTE = 9;
    int rowNOTE = rowE - 2; // Centro em rowE (26)

    // Portas AND
    int colAND1 = 23;
    int rowAND1 = 7;        // Ocupa linhas 7-11. Entradas em 8 (topo) e 10 (base).

    int colAND2 = 23;
    int rowAND2 = 19;       // Ocupa linhas 19-23. Entradas em 20 (topo) e 22 (base).

    // Porta OR (Convergência)
    int colOR = 42;
    int rowOR = 13;         // Ocupa linhas 13-17. Entradas em 14 (topo) e 16 (base).

    // Linhas de saída das portas lógicas
    int rowSaidaAND1 = rowAND1 + 2; // 9
    int rowSaidaAND2 = rowAND2 + 2; // 21
    int rowSaidaOR   = rowOR + 3;   // 15

// ── 2. Renderização e Roteamento de Entradas ──────────────────────────
    
    // A e E seguem direto para suas respectivas portas NOT (Condicionados ao pisca)
    if (show_A) {
        MH(0, colNOTA - 1, rowA, vA); 
    }
    if (show_E) {
        MH(0, colNOTE - 1, rowE, vE); 
    }

    // C não possui inversor. Roteia até a coluna 15 e sobe para a base do AND1 (row 10)
    if (show_C) {
        MH(0, 15, rowC, vC);
    }
    
    // O restante do roteamento de C permanece visível para não quebrar a linha no meio do painel
    MV(15, 10, 14, vC);       // Ramo vertical subindo de row 14 até 10
    MH(16, colAND1 - 1, 10, vC);

    // ── 3. Renderização das Portas NOT ────────────────────────────────────
    mpNOT(colNOTA, rowNOTA, /*vNA*/2);
    mpNOT(colNOTE, rowNOTE, /*vNE*/2);

    // ── 4. Roteamento Intermediário (Fan-out) ─────────────────────────────
    
    // Sinal NA (sai na coluna 11, linha 4).
    // Precisa alimentar o topo do AND1 (linha 8) e o topo do AND2 (linha 20).
    MH(colNOTA + 2, colNOTA + 4, rowA, vNA); // Avança até a coluna 13
    MV(colNOTA + 4, 5, 20, vNA);             // Distribuição vertical descendo até a linha 20
    MH(colNOTA + 5, colAND1 - 1, 8, vNA);    // Derivação horizontal para AND1
    MH(colNOTA + 5, colAND2 - 1, 20, vNA);   // Derivação horizontal para AND2

    // Sinal NE (sai na coluna 11, linha 26).
    // Alimenta a base do AND2 (linha 22).
    MH(colNOTE + 2, colNOTE + 7, rowE, vNE); // Avança até a coluna 16
    MV(colNOTE + 7, 22, 25, vNE);            // Elevação vertical até a linha 22
    MH(colNOTE + 8, colAND2 - 1, 22, vNE);   // Segue horizontal para AND2

    // ── 5. Renderização das Portas AND ────────────────────────────────────
    mpAND(colAND1, rowAND1, /*vAND1*/2);
    mpAND(colAND2, rowAND2, /*vAND2*/2);

    // ── 6. Roteamento de Convergência (AND -> OR) ─────────────────────────
    
    // AND1 -> Topo do OR (linha 9 para linha 14)
    MH(colAND1 + 5, colAND1 + 8, rowSaidaAND1, vAND1);       // col 28..31
    MV(colAND1 + 8, rowSaidaAND1 + 1, rowSaidaOR - 1, vAND1);// Desce col 31 (10..14)
    MH(colAND1 + 9, colOR, rowSaidaOR - 1, vAND1);           // Segue para OR

    // AND2 -> Base do OR (linha 21 para linha 16)
    MH(colAND2 + 5, colAND2 + 8, rowSaidaAND2, vAND2);       // col 28..31
    MV(colAND2 + 8, rowSaidaOR + 1, rowSaidaAND2 - 1, vAND2);// Sobe col 31 (16..20)
    MH(colAND2 + 9, colOR, rowSaidaOR + 1, vAND2);           // Segue para OR

    // ── 7. Renderização da Porta OR e Terminal de Saída ───────────────────
    mpOR(colOR, rowOR, /*vOR*/2);

    // Terminal central e malha do LED de verificação (S)
    MH(colOR + 8, colOR + 10, rowSaidaOR, vOR);
    for (int r = rowSaidaOR - 1; r <= rowSaidaOR + 1; r++) {
        MH(colOR + 11, colOR + 13, r, vOR);
    }

    // ── 8. Atualização de Matriz de Pixels ────────────────────────────────
    renderizarComCores();
    desenharNumeroFase(3);
}