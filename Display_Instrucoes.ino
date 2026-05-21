bool estado_joystick_painel = false;

void alterar_cor_joystick() {
    estado_joystick_painel = !estado_joystick_painel;
}

// Converte um índice de 0 a PERIMETER-1 em coordenadas (x, y) na borda
void getBorderCoords(int pos, int &x, int &y) {
    if (pos < 64) {               // topo: esquerda → direita
        x = pos;
        y = 0;
    } else if (pos < 64 + 31) {   // direita: topo → baixo (exclui canto superior)
        x = 63;
        y = pos - 64 + 1;
    } else if (pos < 64 + 31 + 63) { // fundo: direita → esquerda
        x = 63 - (pos - (64 + 31));
        y = 31;
    } else {                      // esquerda: fundo → topo (exclui cantos)
        x = 0;
        y = 31 - (pos - (64 + 31 + 63));
    }
}

// Desenha um ponto brilhante na posição atual da borda, com cor arco-íris
void initSnake() {
    for (int i = 0; i < SNAKE_LENGTH; i++) {
        snake_body[i] = (PERIMETER - SNAKE_LENGTH + i) % PERIMETER;
    }
    snake_head = 0;
    snake_initialized = true;
}

// Desenha a cobrinha na borda – cada segmento com uma cor do arco-íris
void drawSnake() {
    for (int i = 0; i < SNAKE_LENGTH; i++) {
        int x, y;
        getBorderCoords(snake_body[i], x, y);
        
        // Cor gradiente: cabeça mais brilhante, cauda mais escura
        int hue = (millis() / 10 + i * 15) % 360;
        uint16_t color = hueToRGB565(hue);
        
        // Pixels maiores (2x2) para mais destaque (opcional)
        // display->fillRect(x, y, 1, 1, color);  // 1x1 normal
        display->drawPixel(x, y, color);          // descomente se quiser 1x1
    }
}

void clearSnake() {
    for (int i = 0; i < SNAKE_LENGTH; i++) {
        int x, y;
        getBorderCoords(snake_body[i], x, y);
        display->drawPixel(x, y, COR_FUNDO);  // apaga com a cor de fundo
    }
}

void advanceSnake() {
    unsigned long now = millis();
    if (now - last_snake_move >= BORDER_SPEED_MS) {
        last_snake_move = now;
        
        // Apaga a cobrinha atual
        clearSnake();
        
        // Move todos os segmentos: corpo vira cauda, cabeça avança
        for (int i = SNAKE_LENGTH - 1; i > 0; i--) {
            snake_body[i] = snake_body[i - 1];
        }
        snake_body[0] = snake_head;  // a cabeça ocupa a posição atual
        
        // Avança a cabeça para a próxima posição
        snake_head = (snake_head + 1) % PERIMETER;
        
        // Redesenha a cobrinha na nova posição
        drawSnake();
    }
}

// Converte HSV (Hue de 0 a 359) para RGB e depois para cor 565
uint16_t hueToRGB565(int hue) {
    hue = hue % 360;
    float h = hue / 60.0;
    int sector = (int)h;
    float f = h - sector;
    uint8_t p = 0;
    uint8_t q = (uint8_t)(255 * (1 - f));
    uint8_t t = (uint8_t)(255 * f);
    uint8_t r, g, b;
    switch(sector) {
        case 0: r = 255; g = t;   b = 0;   break;
        case 1: r = q;   g = 255; b = 0;   break;
        case 2: r = 0;   g = 255; b = t;   break;
        case 3: r = 0;   g = q;   b = 255; break;
        case 4: r = t;   g = 0;   b = 255; break;
        default: r = 255; g = 0;   b = q;   break;
    }
    return display->color565(r, g, b);
}

void updateRainbowColor() {
    unsigned long now = millis();
    int hue = (now / 10) % 360;   // muda completamente a cada 3,6 segundos
    rainbow_text_color = hueToRGB565(hue);
}

// ── 1. BEM-VINDO ────────────────────────────────────────────────────────────────────
void bem_vindo() {
    // Limpa a tela (mantém o fundo preto)
    display->clearScreen();
    
    // ── BORDA FIXA DISCRETA (opcional, para destacar o percurso) ──
    display->drawRect(0, 0, PANEL_WIDTH, PANEL_HEIGHT, display->color565(20, 20, 20));
    
    // ── TÍTULO CENTRALIZADO (com arco-íris nos textos) ─────────────
    display->setTextSize(1);
    display->setTextWrap(false);
    display->setTextColor(rainbow_text_color);
    
    const char* linhas[] = { "CAMINHO", "DOS", "BITS" };
    const int num_linhas = 3;
    const int CHAR_H_LOCAL = 8;
    const int LINE_SPACING = 2;
    int block_h = num_linhas * CHAR_H_LOCAL + (num_linhas - 1) * LINE_SPACING;
    int y_start = (PANEL_HEIGHT - block_h) / 2;
    
    for (int i = 0; i < num_linhas; i++) {
        int len = strlen(linhas[i]);
        int x = (PANEL_WIDTH - len * 6) / 2;
        int y = y_start + i * (CHAR_H_LOCAL + LINE_SPACING);
        display->setCursor(x, y);
        display->print(linhas[i]);
    }
    
    // ── INICIALIZA A COBRINHA (se ainda não foi) ───────────────────
    if (!snake_initialized) {
        initSnake();
    }
    
    // Desenha a cobrinha na posição atual
    drawSnake();
}

// ── 2. INSTRUÇÕES: EIXO X (Avançar/Voltar) ──────────────────────────────────
void instrucoes_eixo_x() {
    uint16_t branco   = display->color565(255, 255, 255);
    uint16_t vermelho = display->color565(220, 0, 0);
    uint16_t verde    = display->color565(0, 220, 0);

    // ── BORDA FIXA DISCRETA (opcional, para destacar o percurso) ──
    display->drawRect(0, 0, PANEL_WIDTH, PANEL_HEIGHT, display->color565(20, 20, 20));

    // Ativa TomThumb (3×5 px por glifo)
    display->setFont(&TomThumb);

    display->setTextSize(1);
    display->setTextWrap(false);
    display->setTextColor(branco);

    // Renderização vetorial da Seta Esquerda (Vermelha)
    // Coordenadas calculadas geometricamente para o quarto esquerdo da tela
    display->fillTriangle(14, 12, 18, 8, 18, 16, vermelho); // Ponta
    display->fillRect(19, 10, 8, 5, vermelho);               // Corpo

    // Renderização vetorial da Seta Direita (Verde)
    // Coordenadas calculadas para o quarto direito da tela
    display->fillTriangle(50, 12, 46, 8, 46, 16, verde);    // Ponta
    display->fillRect(38, 10, 8, 5, verde);                  // Corpo

    // Textos de ação alinhados sob as setas
    display->setCursor(7, 25);
    display->print("VOLTAR");
    
    display->setCursor(33, 25);
    display->print("SEGUIR");

    // Restaura fonte padrão para o resto do sistema
    display->setFont(NULL);
}

// ── 3. INSTRUÇÕES: EIXO Y (Selecionar Entrada) ──────────────────────────────
void instrucoes_eixo_y() {
    uint16_t branco = display->color565(255, 255, 255);
    uint16_t verde  = display->color565(0, 220, 0);

    // ── BORDA FIXA DISCRETA (opcional, para destacar o percurso) ──
    display->drawRect(0, 0, PANEL_WIDTH, PANEL_HEIGHT, display->color565(20, 20, 20));

    // Ativa TomThumb (3×5 px por glifo)
    display->setFont(&TomThumb);
    display->setTextSize(1);
    display->setTextColor(branco);

    // Renderização vetorial das setas verticais no centro-topo
    // Seta Cima
    display->fillTriangle(23, 2, 19, 6, 27, 6, verde);
    display->fillRect(22, 6, 3, 6, verde);

    // Seta Baixo
    display->fillTriangle(39, 12, 35, 8, 43, 8, verde);
    display->fillRect(38, 2, 3, 6, verde);

    const char* linha1 = "ESCOLHER";
    display->setCursor((PANEL_WIDTH - strlen(linha1) * 4) / 2, 19);
    display->print(linha1);

    const char* linha2 = "VARIAVEIS";
    display->setCursor((PANEL_WIDTH - strlen(linha2) * 4) / 2, 29);
    display->print(linha2);

    // Restaura fonte padrão para o resto do sistema
    display->setFont(NULL);
}

// ── 4. INSTRUÇÕES: BOTÃO SW (Alterar Valor) ─────────────────────────────────
void instrucoes_botao_sw(bool estado) {
    uint16_t branco    = display->color565(255, 255, 255);
    uint16_t vermelho  = display->color565(220, 0, 0);
    uint16_t verde  = display->color565(0, 220, 0);

    // ── BORDA FIXA DISCRETA (opcional, para destacar o percurso) ──
    display->drawRect(0, 0, PANEL_WIDTH, PANEL_HEIGHT, display->color565(20, 20, 20));

    // Ativa TomThumb (3×5 px por glifo)
    display->setFont(&TomThumb);

    display->setTextSize(1);
    display->setTextColor(branco);

    // Desenho paramétrico do Joystick sendo pressionado (Círculo central com raios)
    int cx = 32;
    int cy = 15;
    
    if (estado) {
        // Corpo do botão
        display->drawCircle(cx, cy, 6, branco);
        display->fillCircle(cx, cy, 3, verde); // Núcleo desativado
        
        // Linhas radiantes simulando a emissão/clique (equação paramétrica simplificada)
        display->drawLine(cx, cy - 8, cx, cy - 11, verde);          // Raio Topo
        display->drawLine(cx - 7, cy - 5, cx - 10, cy - 8, verde);  // Raio Esq
        display->drawLine(cx + 7, cy - 5, cx + 10, cy - 8, verde);  // Raio Dir
    } else {
        // Corpo do botão
        display->drawCircle(cx, cy, 6, branco);
        display->fillCircle(cx, cy, 3, vermelho); // Núcleo desativado
        
        // Linhas radiantes simulando a emissão/clique (equação paramétrica simplificada)
        display->drawLine(cx, cy - 8, cx, cy - 11, vermelho);          // Raio Topo
        display->drawLine(cx - 7, cy - 5, cx - 10, cy - 8, vermelho);  // Raio Esq
        display->drawLine(cx + 7, cy - 5, cx + 10, cy - 8, vermelho);  // Raio Dir
    }


    const char* acao = "ALTERAR VALOR";
    display->setCursor(((PANEL_WIDTH - strlen(acao) * 4) / 2) + 1, 29);
    display->print(acao);

    // Restaura fonte padrão para o resto do sistema
    display->setFont(NULL);    
}

void wrapper_instrucoes_sw() {
    instrucoes_botao_sw(estado_joystick_painel);
}