-- ============================================================================
-- Lavoe Salsa - arquivo de configuracao
--
-- Gerado automaticamente na primeira execucao em:
--     ~/.config/lavoe-salsa/config.lua
--
-- Homenagem a Hector Lavoe (1946-1993), "El Cantante" da salsa.
-- Todas as cores estao em #rrggbb. Ajuste a vontade.
-- ============================================================================

return {
    -- ---------------------------------------------------------------------
    -- Geometria da janela do overlay
    --   monitor: indice do monitor (0 = o mais a esquerda; contando da
    --            esquerda para a direita). Ignorado se x e y forem dados.
    --   x, y:    posicao absoluta na tela. Se ambos forem informados,
    --            centralizacao automatica e desativada.
    --   w, h:    tamanho da janela (redimensionavel com o mouse).
    --   margem_base: distancia da base da janela ate a borda inferior do
    --            monitor, para ficar acima do painel do Cinnamon.
    -- ---------------------------------------------------------------------
    geometry = {
        monitor      = 0,
        x            = nil,
        y            = nil,
        w            = 1100,
        h            = 280,
        margem_base  = 70,
        margem_lados = 12,
    },

    -- Numero de barras do espectro
    barras = 56,

    -- Largura de cada barra (px) e espaco entre barras (px).
    -- Barras finas e elegantes, estilo medidor minimalista.
    largura_barra = 6,
    espacamento   = 3,

    -- Quadros por segundo (throttle do loop principal)
    fps = 60,

    -- Taxa de amostragem esperada do PulseAudio (o valor real negociado
    -- com o servidor e usado automaticamente)
    taxa_amostragem = 48000,

    -- ---------------------------------------------------------------------
    -- Gradiente vertical das barras, do rodape ao topo.
    -- Cada entrada: { posicao (0.0 a 1.0), "#rrggbb" }.
    -- Padrao: paleta quente estilo Gruvbox.
    -- ---------------------------------------------------------------------
    gradiente = {
        { 0.00, "#a89984" },  -- cinza-areia (base)
        { 0.35, "#b57614" },  -- mostarda
        { 0.70, "#d65d0e" },  -- laranja queimado
        { 1.00, "#cc241d" },  -- vermelho (topo)
    },

    -- ---------------------------------------------------------------------
    -- Suavizacao estilo cava:
    --   ataque:    velocidade de subida das barras (0..1, maior = mais rapido)
    --   queda:     velocidade de descida das barras (0..1, menor = mais lento)
    --   piso_ruido: reducao de ruido; valores abaixo disso viram zero
    --   gama:      curva de resposta perceptual aplicada aos niveis
    --   piso_db:   piso de dB para o mapeamento logaritmico (ex.: -55)
    -- ---------------------------------------------------------------------
    suavizacao = {
        ataque     = 0.65,
        queda      = 0.18,
        piso_ruido = 0.02,
        gama       = 1.0,
        piso_db    = -55.0,
    },

    -- ---------------------------------------------------------------------
    -- Cor por barra via Lua (opcional).
    -- Se definida como funcao, tem PRIORIDADE sobre o gradiente.
    -- Recebe: i (indice da barra, comecando em 1), nivel (0.0 a 1.0) e
    -- t (segundos desde o inicio). Deve retornar {r=, g=, b=} (0..1)
    -- ou uma string "#rrggbb".
    --
    -- Exemplo (arco-iris girando):
    --   bar_color = function(i, nivel, t)
    --       local h = (i / 56 + t * 0.1) % 1
    --       return { r = 0.5 + 0.5 * math.sin(6.2831 * h),
    --                g = 0.5 + 0.5 * math.sin(6.2831 * (h + 0.33)),
    --                b = 0.5 + 0.5 * math.sin(6.2831 * (h + 0.67)) }
    --   end,
    -- ---------------------------------------------------------------------
    bar_color = nil,
}
