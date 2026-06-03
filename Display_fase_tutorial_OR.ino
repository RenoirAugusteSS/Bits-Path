void desenharFaseTutorialOR() {
    inicializarMatriz();

    // Recupera valores lógicos da engine do jogo (Fase de 2 entradas)
    short int vA  = values[0]; // entrada A (cima)
    short int vB  = values[1]; // entrada B (baixo)
    short int vOR = values[2]; // OR(A, B) -> Saída S

    // ── 0. Controle de Seleção (Blink IHM) ────────────────────────────────
    bool show_A = !(selected_input == 0 && !blink_state);
    bool show_B = !(selected_input == 1 && !blink_state);

    // ── 1. Geometria Centralizada (Painel 64x32) ──────────────────────────
    // Porta OR tem 7 pixels de altura (desenha topo=0, base=6).
    // Centralizando em Y=32: (32 - 7) / 2 = 12. Topo = 12, Base = 18.
    int rowOR_Topo = 12; 
    int colOR      = 28; 

    // Entradas do OR: linha de cima é (topo+1), linha de baixo é (topo+5)
    // Isso entra perfeitamente na concavidade traseira desenhada pelo seu algoritmo
    int rowA = rowOR_Topo + 1; // 13
    int rowB = rowOR_Topo + 5; // 17

    // Saída do OR é na ponta central da curva, em (topo+3)
    int rowSaida = rowOR_Topo + 3; // 15

    // ── 2. Roteamento de Fios ─────────────────────────────────────────────
    if (show_A) {
        MH(0, colOR, rowA, vA); // Entra +1 pixel na concavidade
    }
    
    if (show_B) {
        MH(0, colOR, rowB, vB); // Entra +1 pixel na concavidade
    }

    // ── 3. Renderização da Porta Lógica ───────────────────────────────────
    mpOR(colOR, rowOR_Topo, 2); 

    // ── 4. Roteamento de Saída e Atuador (LED) ────────────────────────────
    // Pino central da porta OR termina em (colOR + 7)
    MH(colOR + 8, 50, rowSaida, vOR);

    // Bloco LED Atuador Final (3x3 pixels em Y centrado em rowSaida)
    for (int r = rowSaida - 1; r <= rowSaida + 1; r++) {
        MH(51, 53, r, vOR);
    }

    // ── 5. Despacho Gráfico ───────────────────────────────────────────────
    renderizarComCores();
}