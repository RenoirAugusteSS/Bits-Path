void desenharFase9() {
    inicializarMatriz();

    // ── Extração de Estados (Mapeamento do motor do jogo) ─────────────────
    short int vA       = values[0]; 
    short int vB       = values[1]; 
    short int vC       = values[2]; 
    short int vD       = values[3]; 
    short int vE       = values[4]; 
    short int vF       = values[5]; 
    
    short int vNA      = values[6];  // NOT_A
    short int vAND_BC  = values[7];  // AND(B, C)
    short int vAND_DE  = values[8];  // AND(D, E)
    short int vAND_FDE = values[9];  // AND(F, AND(D,E))
    short int vOR1     = values[10]; // OR(NA, AND(B,C))
    short int vOR2     = values[11]; // OR(NA, AND(F,DE))
    
    short int vAND_FINAL = values[12]; // Conjunção Final (AND)
    short int vS         = values[13]; // Saída Final (Buffer S)

    // ── 1. Planejamento Espacial (Eixo Y) ─────────────────────────────────
    int rowA = 2;  
    int rowB = 6;  
    int rowC = 10; 
    int rowD = 14; 
    int rowE = 18; 
    int rowF = 26; 

    // ── 2. Renderização de Entradas e Inversor de Borda ───────────────────
    MH(0, 4, rowA, vA); mpNOT(5, rowA - 2, vNA); // Saída NA emerge em X=7, Y=2
    
    // Entradas fluem para a Coluna X=10 (Preparação L1)
    MH(0, 10, rowB, vB);
    MH(0, 10, rowC, vC);
    MH(0, 10, rowD, vD);
    MH(0, 10, rowE, vE);
    
    // Entrada F sofre um longo bypass, reservada para a Coluna X=20 (L2)
    MH(0, 20, rowF, vF); 

    // ── 3. Estágio Lógico 1 (Front-end - Coluna X=12) ─────────────────────
    // Ajuste em Y para os pinos do AND_BC (Topo=7, Base=9)
    MV(10, 6, 7, vB);  MH(11, 11, 7, vB);
    MV(10, 10, 9, vC); MH(11, 11, 9, vC);
    mpAND(12, 6, vAND_BC); // Saída em X=17, Y=8

    // Ajuste em Y para os pinos do AND_DE (Topo=15, Base=17)
    MV(10, 14, 15, vD); MH(11, 11, 15, vD);
    MV(10, 18, 17, vE); MH(11, 11, 17, vE);
    mpAND(12, 14, vAND_DE); // Saída em X=17, Y=16

    // ── 4. Estágio Lógico 2 (Cascata F_DE - Coluna X=24) ──────────────────
    // AND_DE (Y=16) desce para o Pino Topo do L2 (Y=22)
    MH(17, 20, 16, vAND_DE); MV(20, 16, 22, vAND_DE); MH(21, 23, 22, vAND_DE);
    
    // F (Y=26) sobe para o Pino Base do L2 (Y=24)
    MV(20, 26, 24, vF); MH(21, 23, 24, vF);
    
    mpAND(24, 21, vAND_FDE); // Saída em X=29, Y=23

    // ── 5. Estágio Lógico 3 (Distribuição NA e Portas OR - Coluna X=36) ───
    // JUMPER VIRTUAL: Criamos o barramento descendente para NA em X=34.
    
    MH(7, 34, 2, vNA);        // NA avança até o duto vertical X=34
    MV(34, 2, 19, vNA);       // NA desce até Y=19 (Pino Topo do OR2)
    
    MH(34, 35, 6, vNA);       // Derivação de NA para OR1 (Pino Topo)
    MH(34, 35, 19, vNA);      // Derivação de NA para OR2 (Pino Topo)

    // Jumper: Alimentação Base OR1 (AND_BC em Y=8) saltando sobre X=34
    MH(17, 32, 8, vAND_BC);   // Trilha para antes do cruzamento (deixa X=33, 34 vazios)
    MH(35, 35, 8, vAND_BC);   // Retoma após o cruzamento alimentando o OR1
    
    // Alimentação Base OR2 (AND_FDE em Y=23 subindo para Y=21)
    MH(29, 31, 23, vAND_FDE); MV(31, 23, 21, vAND_FDE); MH(31, 35, 21, vAND_FDE);

    mpOR(36, 5, vOR1);        // Saída em X=41, Y=7
    mpOR(36, 18, vOR2);       // Saída em X=41, Y=20

    // ── 6. Estágio Lógico Final (Convergência de Saída - Coluna X=48) ─────
    // OR1 (Y=7) desce para o Pino Topo (Y=13)
    MH(41, 46, 7, vOR1); MV(46, 7, 13, vOR1); MH(47, 47, 13, vOR1);
    
    // OR2 (Y=20) sobe para o Pino Base (Y=15)
    MH(41, 46, 20, vOR2); MV(46, 20, 15, vOR2); MH(47, 47, 15, vOR2);

    mpAND(48, 12, vAND_FINAL); // Saída em X=53, Y=14

    // ── 7. Roteamento até o Indicador de Estado Lógico (LED S) ────────────
    MH(53, 56, 14, vS);
    
    // Malha do LED indicativo (3x3 pixels)
    for (int r = 13; r <= 15; r++) {
        MH(57, 59, r, vS);
    }

    // ── 8. Flush no Framebuffer ───────────────────────────────────────────
    renderizarComCores();
    desenharNumeroFase(9);
}