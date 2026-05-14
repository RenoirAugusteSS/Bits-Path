void desenharFase1() {
    inicializarMatriz();
    entradas_leds();

    // Recupera valores lógicos da engine do jogo
    short int vA    = values[0];  // entrada A
    short int vE    = values[1];  // entrada E
    short int vNA   = values[2];  // NOT_A
    short int vNE   = values[3];  // NOT_E
    short int vAND1 = values[4];  // AND(A,NE)
    short int vAND2 = values[5];  // AND(NA,E)
    short int vOR   = values[6];  // OR — saída final
    // values[7] = S = vOR (buffer)

    // ── 1. Linhas de entrada ──────────────────────────────────────────────
    //    yEntrada(i) com num_inputs=2: A=row3, E=row23
    int rowA = 3;
    int rowE = 23; 

    int rowNOTA = 1;   // row 7
    int colNOTA = 9;

    int rowNOTE = 21;   // row 18
    int colNOTE = 9;

    int rowAND1 = 8;  // topo do AND1 (rows 12..16)
    int colAND1 = 23;

    int rowAND2 = 18;  // topo do AND2 (rows 18..22)
    int colAND2 = 23;

    int rowOR = 13;  // row12
    int colOR = 42;

    int rowSaidaAND1 = rowAND1 + 2;  // row14
    int rowSaidaAND2 = rowAND2 + 2;  // row20

    int rowSaidaOR = rowOR + 2;   // centro do OR — entre row14 e row20

    // ─── A e E lines ───────────────────────
    MH(0, colNOTE-1, rowA, vA);
    MH(0, colNOTE-1, rowE, vE);

    // ── 2. NOT_A (col9, row7) e NOT_E (col9, row18) ───────────────────────
    //    NOT 7 linhas de altura: centralizado em row10 → topo=row7
    //    NOT 7 linhas de altura: centralizado em row21 → topo=row18

    mpNOT(colNOTA, rowNOTA, vNA);  // NOT_A
    mpNOT(colNOTE, rowNOTE, vNE);  // NOT_E

    // ── 3. Roteamento NOT→AND ─────────────────────────────────────────────
    //    Saída do NOT está em col+5,row+3 = col14, row10 (NA) e col14, row21 (NE)
    //    AND1 está em col18, precisa de NE na entrada inferior (row16) e A na entrada superior (row12)
    //    AND2 está em col18, precisa de NA na entrada superior (row17) e E  na entrada inferior (row21)

    // Layout dos ANDs — cada AND(4px alto) centralizado:
    // AND1 entre rowA(10) e meio: centrado em row13 → topo=row13, base=row17
    // AND2 entre meio e rowE(21): centrado em row18 → topo=row18, base=row22


    // entradas do AND: pino superior=topo, pino inferior=base

    // fio NOT_A saída (col14,row10) → horizontal até col17, depois desce até topAND2
    MH(colNOTA+3, colNOTA+5, rowA, vNA);           // NA sai do NOT e vai até col17
    MV(colNOTA+5, rowA+1, rowAND2+3, vNA);    // desce col17 de row11 até row18 (entrada AND2 topo)
    MH(colNOTA+6, colAND2-1, rowAND2+3, vNA);

    // fio NOT_E saída (col14,row21) → horizontal até col17, depois sobe até topAND1+4
    MH(colNOTE+3, colNOTE+8, rowE, vNE);           // NE sai do NOT e vai até col17
    MV(colNOTE+8, rowE-1, rowAND1+3, vNE);  // sobe col17 de row16 até row20 (entrada AND1 base)
    MH(colNOTE+9, colAND1-1, rowAND1+3, vNE);

    // fio A→AND1 (entrada superior AND1=topo=row12): A vem de col8, desce col16
    MV(4, rowA+1, rowAND1+1, vA);   // desce col16 de row11 até row12
    MH(5, colAND1-1, rowAND1+1, vA);            // A horizontal col8..16

    // fio E→AND2 (entrada inferior AND2=base=row22): E vem de col8
    MV(4, rowE-1, rowAND2+1, vE); // sobe col16 de row22 até row20
    MH(5, colAND2-1, rowAND2+1, vE);

    // ── 4. AND1 e AND2 ───────────────────────────────────────────────────────
    mpAND(colAND1, rowAND1, vAND1);   // AND(A, NE)  col18, rows12..16
    mpAND(colAND2, rowAND2, vAND2);   // AND(NA, E)  col18, rows18..22

    MH(colAND1+5, colAND1+8, rowSaidaAND1, vAND1);   // AND1→junção horizontal
    MV(colAND1+8, rowSaidaAND1+1, rowSaidaOR-1, vAND1); // vertical subindo
    MH(colAND1+9, colOR, rowSaidaOR-1, vAND1);   // AND1→junção horizontal

    MH(colAND2+5, colAND2+8, rowSaidaAND2, vAND2);   // AND2→junção horizontal
    MV(colAND2+8, rowSaidaAND2-1, rowSaidaOR+1, vAND2); // vertical descendo
    MH(colAND2+9, colOR, rowSaidaOR+1, vAND2);   // AND1→junção horizontal

    // ── 6. OR (col27, centrado em rowOR=15, topo=row12) ──────────────────
    
    mpOR(colOR, rowOR, vOR);

    // ── 7. Saída OR→S ─────────────────────────────────────────────────────
    // saída do OR: col32, rowOR=15
    MH(colOR+6 , colOR+10, rowSaidaOR, vOR);

    // LED indicador 3×3 em col39..41, rows 14..16
    for (int r = rowSaidaOR-1; r <= rowSaidaOR+1; r++)
        MH(colOR+11, colOR+13, r, vOR);

    renderizarComCores();
}
