void desenharFase10() {
    inicializarMatriz();

    // ── Extração de Estados (Mapeamento exato da init_fase_10) ────────────
    short int vA       = values[0]; 
    short int vB       = values[1]; 
    short int vC       = values[2]; 
    short int vD       = values[3]; 
    short int vE       = values[4]; 
    short int vF       = values[5]; 
    
    short int vNA      = values[6];  // NOT_A
    short int vNB      = values[7];  // NOT_B
    short int vNC      = values[8];  // NOT_C
    
    short int vAND1    = values[9];  // AND(A, ~B)
    short int vAND2    = values[10]; // AND(~A, B)
    short int vAND3    = values[11]; // AND(AND1, C)
    short int vAND4    = values[12]; // AND(NC, AND2)
    short int vAND5    = values[13]; // AND(D, E)
    short int vAND6    = values[14]; // AND(AND5, F)
    
    short int vOR1     = values[15]; // OR(AND3, AND4)
    short int vOR2     = values[16]; // OR(OR1, AND6)
    short int vS       = values[17]; // Saída Final (S)

    // ── 1. Planejamento Espacial (Eixo Y - Alta Densidade) ────────────────
    int rowA = 2;  
    int rowB = 6;  
    int rowC = 12; 
    int rowD = 18; 
    int rowE = 22; 
    int rowF = 26; 

    // ── 2. Renderização de Entradas, Inversores e Jumpers Iniciais ────────
    // Trilha A e ramificação para NOT_A
    MH(0, 3, 2, vA);        // Ramo comum
    MV(3, 2, 4, vA);        // A desce para Y=4 (Pino Topo AND1)
    MH(4, 7, 4, vA);        // Trilha pré-jumper
    MH(9, 13, 4, vA);       // Trilha pós-jumper (deixa X=8 livre)
    
    MH(3, 4, 2, vA);        // Ramo para o inversor
    mpNOT(5, 0, vNA);       // Saída NA em X=7, Y=2
    MH(7, 8, 2, vNA);       
    MV(8, 2, 8, vNA);       // NA desce verticalmente pelo JUMPER (X=8) até Y=8
    MH(8, 13, 8, vNA);      // Segue para Pino Topo AND2

    // Trilha B e ramificação para NOT_B
    MH(0, 3, 6, vB);
    MV(3, 6, 10, vB);       // B desce para Y=10 (Pino Base AND2)
    MH(4, 13, 10, vB);
    
    MH(3, 4, 6, vB);
    mpNOT(5, 4, vNB);       // Saída NB em X=7, Y=6
    MH(7, 7, 6, vNB);       // Trilha pré-jumper
    MH(9, 13, 6, vNB);      // Trilha pós-jumper para Pino Base AND1

    // Trilha C e ramificação para NOT_C
    MH(0, 3, 12, vC);
    MV(3, 12, 14, vC);      // C desce para Y=14 
    MH(4, 20, 14, vC);      // Trilha pré-jumper (X=21)
    
    MH(3, 4, 12, vC);
    mpNOT(5, 10, vNC);      // Saída NC em X=7, Y=12
    MH(7, 20, 12, vNC);     // Trilha pré-jumper (X=21)
    MH(22, 25, 12, vNC);    // Trilha pós-jumper para Pino Base AND4

    // Entradas diretas D, E, F
    MH(0, 10, 18, vD); MV(10, 18, 20, vD); MH(10, 13, 20, vD);
    MH(0, 13, 22, vE);
    MH(0, 24, 26, vF); MV(24, 26, 24, vF); MH(24, 25, 24, vF);

    // ── 3. Estágio Lógico 1 (Coluna X=14) ─────────────────────────────────
    mpAND(14, 3, vAND1);    // Saída em X=19, Y=5
    mpAND(14, 7, vAND2);    // Saída em X=19, Y=9
    mpAND(14, 19, vAND5);   // Saída em X=19, Y=21

    // ── 4. Roteamento Intermediário L1 -> L2 (Jumpers de Subida) ──────────
    MH(19, 23, 5, vAND1); MV(23, 5, 6, vAND1); MH(23, 25, 6, vAND1);

    // Jumper para C subir cortando NC e AND2
    MH(19, 20, 9, vAND2);   // Trilha AND2 pré-jumper
    MH(22, 23, 9, vAND2);   // Trilha AND2 pós-jumper
    MV(23, 9, 10, vAND2); MH(23, 25, 10, vAND2); // Ajuste fino ao pino

    MH(21, 21, 14, vC);     // Início do Jumper de C
    MV(21, 14, 8, vC);      // C sobe verticalmente por X=21 rasgando Y=12 e Y=9
    MH(21, 25, 8, vC);      // Entrega no Pino Base AND3

    MH(19, 24, 21, vAND5); MV(24, 21, 22, vAND5); MH(24, 25, 22, vAND5);

    // ── 5. Estágio Lógico 2 (Coluna X=26) ─────────────────────────────────
    mpAND(26, 5, vAND3);    // Saída em X=31, Y=7
    mpAND(26, 9, vAND4);    // Saída em X=31, Y=11
    mpAND(26, 21, vAND6);   // Saída em X=31, Y=23

    // ── 6. Roteamento L2 -> L3 (Convergência do MUX - Coluna X=35) ────────
    MH(31, 35, 7, vAND3); MV(35, 7, 8, vAND3); MH(35, 37, 8, vAND3);
    MH(31, 35, 11, vAND4); MV(35, 11, 10, vAND4); MH(35, 37, 10, vAND4);
    MH(31, 46, 23, vAND6);  // AND6 é um sinal de espera, cruza a tela por baixo

    // ── 7. Estágio Lógico 3 (Coluna X=38) ─────────────────────────────────
    mpOR(38, 7, vOR1);      // Saída em X=43, Y=9

    // ── 8. Roteamento L3 -> L4 (Convergência Final - Coluna X=46) ─────────
    MH(43, 46, 9, vOR1); MV(46, 9, 15, vOR1); MH(46, 47, 15, vOR1);
    MV(46, 23, 17, vAND6); MH(46, 47, 17, vAND6);

    // ── 9. Estágio Lógico Final (Coluna X=48) ─────────────────────────────
    mpOR(48, 14, vOR2);     // Saída final emerge em X=53, Y=16

    // ── 10. Indicador de Estado Lógico Final (LED S) ──────────────────────
    MH(53, 56, 16, vS);
    for (int r = 15; r <= 17; r++) {
        MH(57, 59, r, vS);
    }

    // ── 11. Atualização do Framebuffer ────────────────────────────────────
    renderizarComCores();
    desenharNumeroFase(10);
}