void desenharFase8() {
    inicializarMatriz();

    // ── Extração de Estados (Mapeamento exato da init_fase_8) ─────────────
    short int vA       = values[0]; 
    short int vB       = values[1]; 
    short int vC       = values[2]; 
    short int vD       = values[3]; 
    short int vE       = values[4]; 
    short int vF       = values[5]; 
    
    short int vAND_AB  = values[6];  // AND(A, B)
    short int vAND_CD  = values[7];  // AND(C, D)
    short int vNAND_CD = values[8];  // NOT(AND(C, D))
    short int vOR_EF   = values[9];  // OR(E, F)
    
    short int vAND_L3  = values[10]; // AND(AND_AB, NAND_CD)
    short int vAND_L4  = values[11]; // AND(AND_L3, OR_EF)
    short int vS       = values[12]; // Saída Final (S)

    // ── 1. Planejamento Espacial (Eixo Y - Paralelismo Inicial) ───────────
    // As 6 entradas distribuídas uniformemente formando três blocos
    int rowA = 2;  
    int rowB = 6;  
    int rowC = 12; 
    int rowD = 16; 
    int rowE = 22; 
    int rowF = 26; 

    // ── 2. Alimentação do Estágio 1 (Coluna de Derivação X=10) ────────────
    // As entradas ímpares (A, C, E) sofrem deslocamento (Y+2) para o pino Topo
    // As entradas pares (B, D, F) seguem reto para o pino Base
    
    // Bloco Superior -> AND_AB (Centro Y=4)
    MH(0, 10, rowA, vA); MV(10, rowA, 4, vA); MH(11, 11, 4, vA);
    MH(0, 11, rowB, vB);

    // Bloco Central -> AND_CD (Centro Y=14)
    MH(0, 10, rowC, vC); MV(10, rowC, 14, vC); MH(11, 11, 14, vC);
    MH(0, 11, rowD, vD);

    // Bloco Inferior -> OR_EF (Centro Y=24)
    MH(0, 10, rowE, vE); MV(10, rowE, 24, vE); MH(11, 11, 24, vE);
    MH(0, 11, rowF, vF);

    // ── 3. Estágio Lógico 1 (Portas Front-end - Coluna X=12) ──────────────
    mpAND(12, 3, vAND_AB);  // Saída em X=17, Y=5
    mpAND(12, 13, vAND_CD); // Saída em X=17, Y=15
    mpOR(12, 23, vOR_EF);   // Saída em X=17, Y=25

    // ── 4. Estágio Lógico 2 (Inversor do Bloco Central - Coluna X=20) ─────
    // Transformação do AND(C,D) em NAND(C,D)
    MH(17, 19, 15, vAND_CD);
    mpNOT(20, 13, vNAND_CD); // Saída NAND_CD em X=22, Y=15

    // ── 5. Estágio Lógico 3 (Conjunção L3 - Coluna X=30) ──────────────────
    // Junção estrutural na Coluna de Derivação X=28
    
    // AND_AB descendo para o Pino Topo (Y=10)
    MH(17, 28, 5, vAND_AB); MV(28, 5, 10, vAND_AB); MH(29, 29, 10, vAND_AB);
    
    // NAND_CD subindo para o Pino Base (Y=12)
    MH(22, 28, 15, vNAND_CD); MV(28, 15, 12, vNAND_CD); MH(29, 29, 12, vNAND_CD);
    
    mpAND(30, 9, vAND_L3);  // Saída L3 em X=35, Y=11

    // ── 6. Estágio Lógico Final (Conjunção L4 - Coluna X=42) ──────────────
    // Convergência final na Coluna de Derivação X=40
    
    // AND_L3 descendo fortemente para o Pino Topo (Y=18)
    MH(35, 40, 11, vAND_L3); MV(40, 11, 18, vAND_L3); MH(41, 41, 18, vAND_L3);
    
    // OR_EF subindo fortemente para o Pino Base (Y=20)
    MH(17, 40, 25, vOR_EF); MV(40, 25, 20, vOR_EF); MH(41, 41, 20, vOR_EF);

    mpAND(42, 17, vAND_L4); // Saída Final em X=47, Y=19

    // ── 7. Roteamento até o Indicador de Estado Lógico (LED S) ────────────
    MH(47, 52, 19, vS);
    
    // Malha do LED indicativo (3x3 pixels)
    for (int r = 18; r <= 20; r++) {
        MH(53, 55, r, vS);
    }

    // ── 8. Atualização do Framebuffer ─────────────────────────────────────
    renderizarComCores();
    desenharNumeroFase(8);
}