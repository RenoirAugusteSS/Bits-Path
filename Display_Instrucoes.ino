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
    // Atualiza o background dinâmico do Matrix Rain (O Pintor: Camada de Fundo)
    drawMatrixBackground();
    
    // Ativa TomThumb (glifos de 3x5 px)
    display->setFont(&TomThumb);
    display->setTextSize(1);
    display->setTextWrap(false);
    display->setTextColor(rainbow_text_color);
    
    const char* linhas[] = { "CAMINHO", "DOS", "BITS" };
    const int num_linhas = 3;
    
    // ── Constantes Geométricas da Fonte TomThumb ─────────────────────────
    const int ADVANCE_X = 4;       // Passo horizontal (3px do glifo + 1px de espaço)
    const int CHAR_H_LOCAL = 6;    // Altura da caixa do caractere
    const int BASELINE_OFFSET = 5; // Distância do topo até a linha de base tipográfica
    const int LINE_SPACING = 2;    // Respiro vertical entre as linhas
    
    // Altura total do bloco de texto = n*h + (n-1)*espaço
    int block_h = num_linhas * CHAR_H_LOCAL + (num_linhas - 1) * LINE_SPACING;
    int y_start = (PANEL_HEIGHT - block_h) / 2;
    
    for (int i = 0; i < num_linhas; i++) {
        int len = strlen(linhas[i]);
        int text_width = len * ADVANCE_X;
        
        // Coordenadas absolutas do Bounding Box (canto superior esquerdo)
        int x_box = (PANEL_WIDTH - text_width) / 2;
        int y_box = y_start + i * (CHAR_H_LOCAL + LINE_SPACING);

        // 1. Aplica a máscara de apagamento (Z-buffer via software)
        // Expandimos 1 pixel em todas as direções (+2 na largura/altura) para margem
        display->fillRect(x_box - 1, y_box - 1, text_width + 2, CHAR_H_LOCAL + 2, 0x0000);
        
        // 2. Renderiza o texto compensando a translação da baseline
        display->setCursor(x_box, y_box + BASELINE_OFFSET);
        display->print(linhas[i]);
    }
}

// ── 2. INSTRUÇÕES: EIXO X (Avançar/Voltar) ──────────────────────────────────
void instrucoes_eixo_x() {
    // 1. Renderiza o background estocástico dinâmico (Camada de Fundo)
    drawMatrixBackground();

    uint16_t branco   = display->color565(255, 255, 255);
    uint16_t vermelho = display->color565(220, 0, 0);

    // ── BORDA FIXA DISCRETA (opcional, para destacar o percurso) ──
    // display->drawRect(0, 0, PANEL_WIDTH, PANEL_HEIGHT, display->color565(20, 20, 20));

    // ── Constantes da Fonte TomThumb ──────────────────────────────────────────
    const int ADVANCE_X = 4;
    const int CHAR_H = 6;
    const int BASELINE_OFFSET = 5;

    // ── Máscaras de Oclusão das Setas (Z-Buffer via Software) ─────────────────
    display->fillRect(13, 3, 15, 9, 0x0000); // Máscara Seta Esquerda
    display->fillRect(36, 3, 15, 9, 0x0000); // Máscara Seta Direita

    // 2. Renderização vetorial da Seta Esquerda (Branca e mais fina, Y=7)
    display->fillTriangle(14, 7, 18, 4, 18, 10, branco); // Ponta
    display->fillRect(19, 6, 8, 3, branco);              // Corpo

    // 3. Renderização vetorial da Seta Direita (Branca e mais fina, Y=7)
    display->fillTriangle(50, 7, 46, 4, 46, 10, branco); // Ponta
    display->fillRect(38, 6, 8, 3, branco);              // Corpo

    // ── Textos com Máscara e Correção de Baseline (Subiram para Y=14) ─────────
    display->setFont(&TomThumb);
    display->setTextSize(1);
    display->setTextWrap(false);
    display->setTextColor(branco);

    int y_box_txt = 14; 

    // Texto "VOLTAR"
    int x_voltar = 3;
    display->fillRect(x_voltar - 1, y_box_txt - 1, (6 * ADVANCE_X) + 2, CHAR_H + 2, 0x0000);
    display->setCursor(x_voltar, y_box_txt + BASELINE_OFFSET);
    display->print("VOLTAR");

    // Texto "SEGUIR"
    int x_seguir = 37;
    display->fillRect(x_seguir - 1, y_box_txt - 1, (6 * ADVANCE_X) + 2, CHAR_H + 2, 0x0000);
    display->setCursor(x_seguir, y_box_txt + BASELINE_OFFSET);
    display->print("SEGUIR");

    display->setFont(NULL); // Restaura fonte padrão

    // ── Animação do Joystick (Eixo X) ─────────────────────────────────────────
    // O oscilador harmônico (seno) gera um offset suave de -6 a +6 pixels
    int offset = (int)(sin(millis() * 0.005) * 6.0);
    
    int joy_x = 32;       // Centro geométrico
    int joy_base_y = 30;  // Base inferior
    int knob_y = 25;      // Altura do "pino" do joystick

    // Máscara de oclusão para toda a área de varredura da animação
    display->fillRect(20, 22, 24, 10, 0x0000);

    // Desenho físico do controle
    display->drawLine(joy_x - 5, joy_base_y, joy_x + 5, joy_base_y, branco); // Base
    display->drawLine(joy_x, joy_base_y, joy_x + offset, knob_y, branco);    // Haste inclinada
    display->fillCircle(joy_x + offset, knob_y, 2, vermelho);                // Knob (Vermelho)
}

// ── 3. INSTRUÇÕES: EIXO Y (Selecionar Entrada) ──────────────────────────────
void instrucoes_eixo_y() {
    // 1. Renderiza o background estocástico dinâmico (Camada de Fundo)
    drawMatrixBackground();

    uint16_t branco = display->color565(255, 255, 255);

    // ── BORDA FIXA DISCRETA ──
    display->drawRect(0, 0, PANEL_WIDTH, PANEL_HEIGHT, display->color565(20, 20, 20));

    // ── Constantes da Fonte TomThumb ──────────────────────────────────────────
    const int ADVANCE_X = 4;
    const int CHAR_H = 6;
    const int BASELINE_OFFSET = 5;

    // ── Máscaras de Oclusão das Setas (Z-Buffer via Software) ─────────────────
    display->fillRect(23, 1, 7, 8, 0x0000); // Máscara Seta Cima
    display->fillRect(34, 1, 7, 8, 0x0000); // Máscara Seta Baixo

    // 2. Renderização vetorial da Seta Cima (Branca e fina)
    display->fillTriangle(26, 2, 24, 5, 28, 5, branco); // Ponta
    display->fillRect(25, 5, 3, 3, branco);             // Corpo

    // 3. Renderização vetorial da Seta Baixo (Branca e fina)
    display->fillTriangle(37, 8, 35, 5, 39, 5, branco); // Ponta
    display->fillRect(36, 2, 3, 3, branco);             // Corpo

    // ── Textos com Máscara e Correção de Baseline ─────────────────────────────
    display->setFont(&TomThumb);
    display->setTextSize(1);
    display->setTextWrap(false);
    display->setTextColor(branco);

    // Texto "ESCOLHER" (8 caracteres -> 32px largura)
    int x_linha1 = (PANEL_WIDTH - 8 * ADVANCE_X) / 2;
    int y_linha1 = 11;
    display->fillRect(x_linha1 - 1, y_linha1 - 1, (8 * ADVANCE_X) + 2, CHAR_H + 2, 0x0000);
    display->setCursor(x_linha1, y_linha1 + BASELINE_OFFSET);
    display->print("ESCOLHER");

    // Texto "ENTRADAS" (8 caracteres -> 32px largura)
    int x_linha2 = (PANEL_WIDTH - 8 * ADVANCE_X) / 2;
    int y_linha2 = 18;
    display->fillRect(x_linha2 - 1, y_linha2 - 1, (8 * ADVANCE_X) + 2, CHAR_H + 2, 0x0000);
    display->setCursor(x_linha2, y_linha2 + BASELINE_OFFSET);
    display->print("ENTRADAS");

    display->setFont(NULL); // Restaura ponteiro de fonte

    // ── Animação na Fita de LED Física (Efeito Varredura / Knight Rider) ──────
    // Utiliza NUMERO_LEDS definido globalmente (6)
    const int num_leds = NUMERO_LEDS; 
    
    // Período de transição sincronizado aproximadamente com o RAINBOW_INTERVAL (80ms)
    // para evitar "stuttering" (engasgos visuais) decorrentes de aliasing temporal.
    const int period_ms = 80; 
    
    // Cálculo da onda triangular para efeito ping-pong
    int step = (millis() / period_ms) % ((num_leds - 1) * 2);
    int active_led = (step < num_leds) ? step : ((num_leds - 1) * 2) - step;

    fita_LED.clear();
    for (int i = 0; i < num_leds; i++) {
        if (i == active_led) {
            // LED ativo em potência máxima branca
            fita_LED.setPixelColor(i, fita_LED.Color(255, 255, 255));
        } else {
            // LEDs inativos mantidos desligados
            fita_LED.setPixelColor(i, fita_LED.Color(0, 0, 0));
        }
    }
    
    // Despacha o buffer de memória para o silício da fita WS2812B
    fita_LED.show(); 
}

// ── 4. INSTRUÇÕES: BOTÃO SW (Clicar/Pressionar) ─────────────────────────────
void instrucoes_botao_sw() {
    // 1. Renderiza o background estocástico dinâmico (Camada de Fundo)
    drawMatrixBackground();

    uint16_t branco   = display->color565(255, 255, 255);
    uint16_t vermelho = display->color565(220, 0, 0);
    uint16_t verde    = display->color565(0, 220, 0);
    uint16_t cinza    = display->color565(100, 100, 100);

    // ── BORDA FIXA DISCRETA ──
    display->drawRect(0, 0, PANEL_WIDTH, PANEL_HEIGHT, display->color565(20, 20, 20));

    // ── Hardware In-The-Loop: Fita de LEDs Física ─────────────────────────────
    // Responde instantaneamente ao estado real do hardware (acionado via interrupção/polling)
    fita_LED.clear();
    for (int i = 0; i < NUMERO_LEDS; i++) {
        if (estado_joystick_painel) {
            fita_LED.setPixelColor(i, fita_LED.Color(0, 220, 0)); // LIGADO (Verde)
        } else {
            fita_LED.setPixelColor(i, fita_LED.Color(220, 0, 0)); // DESLIGADO (Vermelho)
        }
    }
    fita_LED.show();

    // ── Animação Autônoma Temporizada (IHM Virtual) ───────────────────────────
    int ciclo = millis() % 800;
    bool press_anim = (ciclo > 600); 

    // ── Máscara de Oclusão do Mecanismo (Z-Buffer via Software) ───────────────
    display->fillRect(20, 2, 24, 18, 0x0000);

    // ── Desenho Físico (Vista Lateral) ────────────────────────────────────────
    int base_y = 18;
    int cx = 32;
    int knob_y = press_anim ? 15 : 11; 
    uint16_t cor_knob = press_anim ? verde : vermelho;

    // Chassi estático
    display->drawLine(cx - 6, base_y, cx + 6, base_y, branco); 
    // Haste dinâmica
    display->fillRect(cx - 1, knob_y, 3, base_y - knob_y, cinza); 
    // Knob esférico
    display->fillCircle(cx, knob_y, 3, cor_knob); 

    // Efeitos contextuais baseados no estado da onda (Animação visual)
    if (!press_anim) {
        // Seta instrucional apontando para baixo (Sugere a ação)
        display->fillTriangle(cx - 2, 4, cx + 2, 4, cx, 7, branco);
        display->drawLine(cx, 2, cx, 4, branco);
    } else {
        // Dissipação de energia cinética lateral (O impacto do "clique")
        display->drawLine(cx - 8, knob_y, cx - 5, knob_y, verde);
        display->drawLine(cx + 5, knob_y, cx + 8, knob_y, verde);
    }

    // ── Textos com Máscara e Correção de Baseline ────────────────────────────
    display->setFont(&TomThumb);
    display->setTextSize(1);
    display->setTextWrap(false);
    display->setTextColor(branco);

    const int ADVANCE_X = 4;
    const int CHAR_H = 6;
    const int BASELINE_OFFSET = 5;

    // Redução da carga cognitiva: verbo imperativo direto
    const char* acao = "PRESSIONE"; 
    int len_acao = strlen(acao);
    
    int x_txt = (PANEL_WIDTH - len_acao * ADVANCE_X) / 2;
    int y_txt = 23;

    display->fillRect(x_txt - 1, y_txt - 1, (len_acao * ADVANCE_X) + 2, CHAR_H + 2, 0x0000);
    display->setCursor(x_txt, y_txt + BASELINE_OFFSET);
    display->print(acao);

    display->setFont(NULL); // Restaura fonte padrão
}

// ── 5. INSTRUÇÕES: LEGENDA DE CORES (Estados Lógicos) ───────────────────────
void instrucoes_cores() {
    // 1. Renderiza o background estocástico dinâmico (Camada de Fundo)
    drawMatrixBackground();

    uint16_t branco   = display->color565(255, 255, 255);
    uint16_t vermelho = display->color565(220, 0, 0);
    uint16_t verde    = display->color565(0, 220, 0);

    // ── BORDA FIXA DISCRETA ──
    display->drawRect(0, 0, PANEL_WIDTH, PANEL_HEIGHT, display->color565(20, 20, 20));

    // ── Hardware In-The-Loop: Fita de LEDs Física ─────────────────────────────
    // Responde instantaneamente ao estado real do hardware (acionado via interrupção/polling)
    fita_LED.clear();
    for (int i = 0; i < NUMERO_LEDS; i++) {
        if (estado_joystick_painel) {
            fita_LED.setPixelColor(i, fita_LED.Color(0, 220, 0)); // LIGADO (Verde)
        } else {
            fita_LED.setPixelColor(i, fita_LED.Color(220, 0, 0)); // DESLIGADO (Vermelho)
        }
    }
    fita_LED.show();

    // ── Configuração Tipográfica ──────────────────────────────────────────────
    display->setFont(&TomThumb);
    display->setTextSize(1);
    display->setTextWrap(false);

    const int ADVANCE_X = 4;
    const int CHAR_H = 6;
    const int BASELINE_OFFSET = 5;

    // Cálculo estrutural para design tabular (Alinhamento em bloco)
    const char* txt_off = "DESLIGADO"; // Maior string (9 chars)
    int len_off = strlen(txt_off);
    int w_max = 4 + 2 + (len_off * ADVANCE_X); // Ícone(4px) + Espaço(2px) + Texto(36px) = 42px
    int x_base = (PANEL_WIDTH - w_max) / 2;    // Centraliza o bloco inteiro na tela

    // ── Linha 1: Estado LOW (Vermelho) ──
    int y1 = 8;
    // Aplica a máscara negativa no tamanho máximo do bloco para apagar a matriz de fundo
    display->fillRect(x_base - 1, y1 - 1, w_max + 2, CHAR_H + 2, 0x0000);
    
    // Renderiza a legenda (Ícone + Texto)
    display->fillRect(x_base, y1 + 1, 4, 4, vermelho);
    display->setTextColor(branco);
    display->setCursor(x_base + 6, y1 + BASELINE_OFFSET);
    display->print(txt_off);

    // ── Linha 2: Estado HIGH (Verde) ──
    int y2 = 18;
    // Oculta o espaço necessário reutilizando a constante w_max para manter simetria
    display->fillRect(x_base - 1, y2 - 1, w_max + 2, CHAR_H + 2, 0x0000);
    
    // Renderiza a legenda (Ícone + Texto)
    display->fillRect(x_base, y2 + 1, 4, 4, verde);
    display->setCursor(x_base + 6, y2 + BASELINE_OFFSET);
    display->print("LIGADO");

    display->setFont(NULL); // Libera o ponteiro de fonte
}