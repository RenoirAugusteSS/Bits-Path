void desenharFase2() {
    inicializarMatriz();

    short int vA    = values[0];  // entrada A
    short int vB    = values[1];  // entrada B
    short int vF    = values[2];  // entrada F
    short int vNB   = values[3];  // NOT_B
    short int vAND1 = values[4];  // AND(A, NB)
    short int vAND2 = values[5];  // AND(B, F)
    short int vOR   = values[6];  // OR — saída final
    // values[7] = S = vOR (buffer)

    // ── Posicionamento ────────────────────────────────────────────────────
    // 3 entradas distribuídas verticalmente no painel 32px
    bool show_A = !(selected_input == 0 && !blink_state);
    bool show_B = !(selected_input == 1 && !blink_state);
    bool show_F = !(selected_input == 2 && !blink_state);

    int rowA = 4;
    int rowB = 15;
    int rowF = 26;

    // NOT_B centralizado em rowB
    int colNOTB = 9;
    int rowNOTB = 13;  // centro em rowNOTB+2 = row15 = rowB ✓

    // AND1(A, NB) — metade superior
    int colAND1 = 23;
    int rowAND1 = 7;   // spans rows 7-11

    // AND2(B, F) — metade inferior (sem NOT, entradas diretas)
    int colAND2 = 23;
    int rowAND2 = 19;  // spans rows 19-23

    // OR — centro vertical
    int colOR = 42;
    int rowOR = 13;

    int rowSaidaAND1 = rowAND1 + 2;  // 9
    int rowSaidaAND2 = rowAND2 + 2;  // 21
    int rowSaidaOR   = rowOR   + 3;  // 15

// ── 1. Linhas de entrada (col0 até antes do NOT) ──────────────────────
    if (show_A) {
        MH(0, colNOTB-1, rowA, vA);   // A: col0-8 at row4
    }
    if (show_B) {
        MH(0, colNOTB-1, rowB, vB);   // B: col0-8 at row15 → alimenta NOT_B
    }
    if (show_F) {
        MH(0, colNOTB-1, rowF, vF);   // F: col0-8 at row26
    }

    // ── 2. NOT_B (col9, row13) — saída em (col11, row15) ─────────────────
    mpNOT(colNOTB, rowNOTB, /*vNB*/2);

    // ── 3. Roteamento das entradas para os ANDs ───────────────────────────

    // A → AND1 entrada superior (rowAND1+1 = 8)
    MV(colNOTB-1, rowA+1,   rowAND1+1,  vA);           // desce col4: row5→row8
    MH(colNOTB, colAND1-1, rowAND1+1, vA);           // horizontal col5-22 at row8

    // NOT_B saída (col11, row15) → AND1 entrada inferior (rowAND1+3 = 10)
    MH(colNOTB+2, colNOTB+5, rowB,       vNB); // sai do NOT: col11-14 at row15
    MV(colNOTB+5, rowB-1,    rowAND1+3,  vNB); // sobe col14: row14→row10
    MH(colNOTB+6, colAND1-1, rowAND1+3,  vNB); // horizontal col15-22 at row10

    // B → AND2 entrada superior (rowAND2+1 = 20)
    MV(4, rowB+1,   rowAND2+1,  vB);           // desce col4: row16→row20
    MH(5, colAND2-1, rowAND2+1, vB);           // horizontal col5-22 at row20

    // F → AND2 entrada inferior (rowAND2+3 = 22)
    MV(colNOTB-1, rowF-1,   rowAND2+3,  vF);           // sobe col4: row25→row22
    MH(colNOTB, colAND2-1, rowAND2+3, vF);           // horizontal col5-22 at row22

    // ── 4. AND1 e AND2 ────────────────────────────────────────────────────
    mpAND(colAND1, rowAND1, /*vAND1*/2);   // AND(A, NB)  col23, rows 7-11
    mpAND(colAND2, rowAND2, /*vAND2*/2);   // AND(B, F)   col23, rows 19-23

    // ── 5. Roteamento AND→OR ──────────────────────────────────────────────

    // AND1 saída (col27, row9) → OR entrada superior (col42, row14)
    MH(colAND1+5, colAND1+8, rowSaidaAND1,   vAND1);  // col28-31 at row9
    MV(colAND1+8, rowSaidaAND1+1, rowSaidaOR-1, vAND1); // desce col31: row10→row14
    MH(colAND1+9, colOR,     rowSaidaOR-1,   vAND1);  // col32-42 at row14

    // AND2 saída (col27, row21) → OR entrada inferior (col42, row16)
    MH(colAND2+5, colAND2+8, rowSaidaAND2,   vAND2);  // col28-31 at row21
    MV(colAND2+8, rowSaidaAND2-1, rowSaidaOR+1, vAND2); // sobe col31: row20→row16
    MH(colAND2+9, colOR,     rowSaidaOR+1,   vAND2);  // col32-42 at row16

    // ── 6. OR (col42, row13) ─────────────────────────────────────────────
    mpOR(colOR, rowOR, /*vOR*/2);

    // ── 7. Saída OR → LED indicador ───────────────────────────────────────
    MH(colOR+8, colOR+10, rowSaidaOR, vOR);

    for (int r = rowSaidaOR-1; r <= rowSaidaOR+1; r++)
        MH(colOR+11, colOR+13, r, vOR);

    renderizarComCores();
    desenharNumeroFase(2);
}