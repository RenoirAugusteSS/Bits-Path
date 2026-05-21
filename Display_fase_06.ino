void desenharFase6() {
    inicializarMatriz();

    // ── Extração de Estados (Mapeamento exato de init_fase_6) ─────────────
    short int vA       = values[0]; 
    short int vB       = values[1]; 
    short int vC       = values[2]; 
    short int vD       = values[3]; 
    short int vE       = values[4]; 
    short int vF       = values[5]; 
    
    short int vNA      = values[6];  // NOT_A
    short int vNF      = values[7];  // NOT_F
    
    short int vAND_AB  = values[8];  // AND(~A, B)
    short int vOR_CD   = values[9];  // OR(C, D)
    short int vAND_CD  = values[10]; // AND(C, D)
    short int vNAND_CD = values[11]; // NOT(AND(C, D))
    short int vXOR     = values[12]; // AND(OR, NAND) -> XOR(C, D)
    
    short int vAND_L3  = values[13]; // AND(AND_AB, XOR)
    short int vAND_L4  = values[14]; // AND(AND_L3, E)
    short int vAND_L5  = values[15]; // AND(AND_L4, NF)
    short int vS       = values[16]; // Saída Final (S)

    // ── 1. Planejamento Espacial (Eixo Y) ─────────────────────────────────
    int rowA = 2;  
    int rowB = 6;  
    int rowC = 12; 
    int rowD = 18; 
    int rowE = 22; 
    int rowF = 28; 

    // ── 2. Entradas Ligeiras e Inversores de Borda ────────────────────────
    MH(0, 4, rowA, vA); mpNOT(5, rowA - 2, vNA);  // NA emerge em X=7, Y=2
    MH(0, 11, rowB, vB);                          // B cruza direto até X=11, Y=6
    MH(0, 4, rowF, vF); mpNOT(5, rowF - 2, vNF);  // NF emerge em X=7, Y=28

    // O barramento NF (Y=28) e E (Y=22) são estendidos até os estágios finais
    MH(7, 48, rowF, vNF);
    MH(0, 40, rowE, vE);

    // ── 3. Subcircuito: Fan-out de C e D para formação do XOR ─────────────
    MH(0, 7, rowC, vC);
    MH(8, 11, 12, vC);                     // Derivação C (Y=12) -> Pino Topo OR_CD
    MV(8, 12, 18, vC); MH(9, 11, 18, vC);  // Derivação C descendo -> Pino Topo AND_CD

    MH(0, 7, rowD, vD);
    MV(9, 18, 14, vD); MH(10, 11, 14, vD); // Derivação D subindo -> Pino Base OR_CD
    MV(8, 18, 20, vD); MH(9, 11, 20, vD);  // Derivação D (Y=20) -> Pino Base AND_CD

    // ── 4. Estágio Lógico 1 (Coluna X=12) ─────────────────────────────────
    // Roteamento para o AND_AB
    MH(7, 11, 2, vNA); MV(11, 2, 4, vNA);  // NA desce para coincidir com o pino 4

    mpAND(12, 3, vAND_AB);                 // Saída em X=17, Y=5
    mpOR(12, 11, vOR_CD);                  // Saída em X=17, Y=13
    mpAND(12, 17, vAND_CD);                // Saída em X=17, Y=19

    // ── 5. Estágio Lógico 2: Conclusão do XOR (NAND) ──────────────────────
    MH(17, 19, 19, vAND_CD);
    mpNOT(20, 17, vNAND_CD);               // Inversor do AND_CD. Saída em X=22, Y=19

    // ── 6. Estágio Lógico 3: Convergência do XOR (Coluna X=26) ────────────
    MH(17, 24, 13, vOR_CD);   MV(24, 13, 15, vOR_CD);   MH(25, 25, 15, vOR_CD);
    MH(22, 24, 19, vNAND_CD); MV(24, 19, 17, vNAND_CD); MH(25, 25, 17, vNAND_CD);
    
    mpAND(26, 14, vXOR);                   // Saída em X=31, Y=16

    // ── 7. Estágio Lógico 4: Cascata com AND_AB (Coluna X=34) ─────────────
    MH(17, 32, 5, vAND_AB); MV(32, 5, 10, vAND_AB); MH(33, 33, 10, vAND_AB);
    MH(31, 32, 16, vXOR);   MV(32, 16, 12, vXOR);   MH(33, 33, 12, vXOR);
    
    mpAND(34, 9, vAND_L3);                 // Saída em X=39, Y=11

    // ── 8. Estágio Lógico 5: Cascata com Entrada E (Coluna X=42) ──────────
    MH(39, 40, 11, vAND_L3); MV(40, 11, 16, vAND_L3); MH(41, 41, 16, vAND_L3);
    /* E vem do Y=22 */      MV(40, 22, 18, vE);      MH(41, 41, 18, vE);
    
    mpAND(42, 15, vAND_L4);                // Saída em X=47, Y=17

    // ── 9. Estágio Lógico Final: Cascata com NOT_F (Coluna X=50) ──────────
    MH(47, 48, 17, vAND_L4); MV(48, 17, 22, vAND_L4); MH(49, 49, 22, vAND_L4);
    /* NF vem do Y=28 */     MV(48, 28, 24, vNF);     MH(49, 49, 24, vNF);
    
    mpAND(50, 21, vAND_L5);                // Saída em X=55, Y=23

    // ── 10. Indicador de Estado Lógico (LED S) ────────────────────────────
    MH(55, 57, 23, vS);
    for (int r = 22; r <= 24; r++) {
        MH(58, 60, r, vS);
    }

    // ── 11. Flush no Framebuffer ──────────────────────────────────────────
    renderizarComCores();
    desenharNumeroFase(6);
}