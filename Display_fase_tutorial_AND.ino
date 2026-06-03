void desenharFaseTutorialAND() {
    inicializarMatriz();

    // Recupera valores lógicos da engine do jogo (Fase de 2 entradas)
    short int vA   = values[0]; // entrada A (cima)
    short int vB   = values[1]; // entrada B (baixo)
    short int vAND = values[2]; // AND(A, B) -> Saída S
    
    // ── 0. Controle de Seleção (Blink IHM) ────────────────────────────────
    bool show_A = !(selected_input == 0 && !blink_state);
    bool show_B = !(selected_input == 1 && !blink_state);

    // ── 1. Geometria Centralizada (Painel 64x32) ──────────────────────────
    // Porta AND tem 5 pixels de altura (desenha topo=0, base=4).
    // Centralizando em Y=32: (32 - 5) / 2 = 13. Topo = 13, Base = 17.
    int rowAND_Topo = 13; 
    int colAND      = 28; // Centralizado no eixo X ( (64 - 5)/2 )
    
    // Entradas do AND: linha de cima é (topo+1), linha de baixo é (topo+3)
    int rowA = rowAND_Topo + 1; // 14
    int rowB = rowAND_Topo + 3; // 16
    
    // Saída do AND é centralizada verticalmente em (topo+2)
    int rowSaida = rowAND_Topo + 2; // 15

    // ── 2. Roteamento de Fios ─────────────────────────────────────────────
    
    // Fio de Entrada A
    if (show_A) {
        MH(0, colAND - 1, rowA, vA);
    }
    
    // Fio de Entrada B
    if (show_B) {
        MH(0, colAND - 1, rowB, vB);
    }

    // ── 3. Renderização da Porta Lógica ───────────────────────────────────
    mpAND(colAND, rowAND_Topo, 2); // '2' é a constante COR_PORTA branca

    // ── 4. Roteamento de Saída e Atuador (LED) ────────────────────────────
    // Pino de saída da porta AND sai em (colAND + 4)
    MH(colAND + 5, 50, rowSaida, vAND);

    // Bloco LED Atuador Final (3x3 pixels em Y centrado em rowSaida)
    for (int r = rowSaida - 1; r <= rowSaida + 1; r++) {
        MH(51, 53, r, vAND);
    }

    // ── 5. Despacho Gráfico ───────────────────────────────────────────────
    renderizarComCores();
    // Você pode criar uma função desenharNomeFase("T1") se quiser
    // desenharNumeroFase(0); 
}