void desenharFase7() {
    inicializarMatriz();

    // ── Extração de Estados (Mapeamento exato da init_fase_7) ─────────────
    short int vA       = values[0]; 
    short int vB       = values[1]; 
    short int vC       = values[2]; 
    short int vD       = values[3]; 
    short int vE       = values[4]; 
    short int vF       = values[5]; 
    
    short int vNA      = values[6];  // NOT_A
    short int vNC      = values[7];  // NOT_C
    short int vNE      = values[8];  // NOT_E
    
    short int vAND_AB  = values[9];  // AND(~A, B)
    short int vNAND_AB = values[10]; // NOT(AND(~A, B))
    short int vAND_CD  = values[11]; // AND(~C, D)
    short int vAND_EF  = values[12]; // AND(~E, F)
    
    short int vAND_L3  = values[13]; // AND(NAND_AB, AND_CD)
    short int vAND_L4  = values[14]; // AND(AND_L3, AND_EF)
    short int vS       = values[15]; // Saída Final (S)

    // ── 1. Planejamento Espacial (Eixo Y - Afunilamento) ──────────────────
    // Margens bem distribuídas para acomodar os inversores de entrada
    int rowA = 2;  
    int rowB = 6;  
    int rowC = 12; 
    int rowD = 16; 
    int rowE = 22; 
    int rowF = 26; 

    // ── 2. Renderização de Entradas e Inversores Primários ────────────────
    // Entradas A, C e E passam pelos inversores na Coluna X=5
    MH(0, 4, rowA, vA); mpNOT(5, rowA - 2, vNA);  // Saída NA em X=7, Y=2
    MH(0, 4, rowC, vC); mpNOT(5, rowC - 2, vNC);  // Saída NC em X=7, Y=12
    MH(0, 4, rowE, vE); mpNOT(5, rowE - 2, vNE);  // Saída NE em X=7, Y=22

    // Entradas B, D e F fluem diretamente pelo barramento até a Coluna X=11
    MH(0, 11, rowB, vB);
    MH(0, 11, rowD, vD);
    MH(0, 11, rowF, vF);

    // ── 3. Estágio Lógico 1 (Portas AND de Front-end - Coluna X=12) ───────
    // Ajuste de Y para o NA, NC e NE (descendo para encontrar os pinos superiores)
    MH(7, 10, rowA, vNA); MV(10, rowA, 4, vNA); MH(11, 11, 4, vNA);
    MH(7, 10, rowC, vNC); MV(10, rowC, 14, vNC); MH(11, 11, 14, vNC);
    MH(7, 10, rowE, vNE); MV(10, rowE, 24, vNE); MH(11, 11, 24, vNE);

    mpAND(12, 3, vAND_AB);  // Saída em X=17, Y=5
    mpAND(12, 13, vAND_CD); // Saída em X=17, Y=15
    mpAND(12, 23, vAND_EF); // Saída em X=17, Y=25

    // ── 4. Estágio Lógico 2 (Inversor Intermediário - Coluna X=20) ────────
    // Apenas a linha superior recebe inversão (formando a NAND)
    MH(17, 19, 5, vAND_AB);
    mpNOT(20, 3, vNAND_AB); // Saída NAND_AB em X=22, Y=5

    // ── 5. Estágio Lógico 3 (Conjunção L3 - Coluna X=30) ──────────────────
    // Convergência estrutural na Coluna de Derivação X=28
    
    // Ramo Superior (NAND_AB) descendo para o Pino Topo (Y=10)
    MH(22, 28, 5, vNAND_AB); MV(28, 5, 10, vNAND_AB); MH(29, 29, 10, vNAND_AB);
    
    // Ramo Central (AND_CD) subindo para o Pino Base (Y=12)
    MH(17, 28, 15, vAND_CD); MV(28, 15, 12, vAND_CD); MH(29, 29, 12, vAND_CD);
    
    mpAND(30, 9, vAND_L3);  // Saída L3 em X=35, Y=11

    // ── 6. Estágio Lógico Final (Conjunção L4 - Coluna X=42) ──────────────
    // Convergência final na Coluna de Derivação X=40
    
    // Ramo L3 descendo para o Pino Topo (Y=18)
    MH(35, 40, 11, vAND_L3); MV(40, 11, 18, vAND_L3); MH(41, 41, 18, vAND_L3);
    
    // Ramo Inferior (AND_EF) subindo para o Pino Base (Y=20)
    MH(17, 40, 25, vAND_EF); MV(40, 25, 20, vAND_EF); MH(41, 41, 20, vAND_EF);

    mpAND(42, 17, vAND_L4); // Saída Final em X=47, Y=19

    // ── 7. Roteamento até o Indicador de Estado Lógico (LED S) ────────────
    MH(47, 52, 19, vS);
    
    // Malha do LED indicativo (3x3 pixels)
    for (int r = 18; r <= 20; r++) {
        MH(53, 55, r, vS);
    }

    // ── 8. Flush no Framebuffer ───────────────────────────────────────────
    renderizarComCores();
    desenharNumeroFase(7);
}