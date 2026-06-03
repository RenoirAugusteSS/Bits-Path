void desenharFaseTutorialNOT() {
    inicializarMatriz();

    // Recupera valores lógicos da engine do jogo (Fase de 1 entrada)
    short int vA   = values[0]; // única entrada
    short int vNOT = values[1]; // NOT(A) -> Saída S

    // ── 0. Controle de Seleção (Blink IHM) ────────────────────────────────
    bool show_A = !(selected_input == 0 && !blink_state);

    // ── 1. Geometria Centralizada (Painel 64x32) ──────────────────────────
    // Porta NOT tem 5 pixels de altura no triângulo.
    // O seu desenho de `mpNOT` foca a ponta no meio (row+2).
    // Centralizando em Y=32: (32 - 5) / 2 = 13.
    int rowNOT_Topo = 13;
    int colNOT      = 29; 

    // A entrada única do triângulo fica exatamente no topo (row=13 no seu desenho)
    int rowA = rowNOT_Topo; 

    // A saída (o círculo inversor) na sua geometria original `mpNOT` sai de (row+2)
    int rowSaida = rowNOT_Topo + 2; // 15

    // ── 2. Roteamento de Fios ─────────────────────────────────────────────
    if (show_A) {
        // Para uma entrada apenas, o fio pode ser uma linha reta centralizada.
        // O triângulo `mpNOT` não tem uma "entrada" em Y médio, a entrada fica no y=topo
        // Portanto, aplicamos um "degrau" visual para que o fio principal corra pelo centro e suba.
        MH(0, colNOT - 3, rowSaida, vA);        // Fio central pelo painel
        MV(colNOT - 2, rowA, rowSaida, vA);     // Sobe até a quina do triângulo
        MH(colNOT - 2, colNOT - 1, rowA, vA);   // Entra no NOT
    }

    // ── 3. Renderização da Porta Lógica ───────────────────────────────────
    mpNOT(colNOT, rowNOT_Topo, 2); 

    // ── 4. Roteamento de Saída e Atuador (LED) ────────────────────────────
    // A ponta do NOT está em (colNOT + 2) e o seu design não possui o "bolinha" final, 
    // então a saída natural do seu triângulo é colNOT + 3
    MH(colNOT + 3, 50, rowSaida, vNOT);

    // Bloco LED Atuador Final (3x3 pixels em Y centrado em rowSaida)
    for (int r = rowSaida - 1; r <= rowSaida + 1; r++) {
        MH(51, 53, r, vNOT);
    }

    // ── 5. Despacho Gráfico ───────────────────────────────────────────────
    renderizarComCores();
}