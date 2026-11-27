#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/spi.h"
#include "inc/ssd1306.h"

// =====================================================================
// SERIP 2 — BitDogLab V7 (RP2350 Dual-Core)
//
// Core 1: Captura de áudio ADC/DMA Ping-Pong + FFT 256 pontos (Fs = 16 kHz)
//         Faixa de frequências: 0 Hz a 8 kHz (62,5 Hz/bin | 250 Hz/coluna).
// Core 0: Botões, Sensor de Presença LD2410B (com correção RP2350-E9),
//         Matriz MAX7219 32x8 (SPI0) e Interface OLED SSD1306 (I2C1).
// =====================================================================

// --- CONFIGURAÇÕES DE HARDWARE (BitDogLab V7) ---
#define MIC_PIN            28
#define MIC_ADC_CH         2
#define OLED_SDA_PIN       2
#define OLED_SCL_PIN       3
#define BTN_A              5   // Botão A (Vermelho): Cicla Automático / Reativo / Desligado
#define BTN_B              6   // Botão B (Azul): Alterna Gráfico de Espectro no OLED
#define BTN_C              10  // Botão C (Verde): Entra/Sai do Menu de Configuração
#define JOY_Y_PIN          26
#define JOY_X_PIN          27
#define JOY_Y_ADC_CH       0
#define JOY_X_ADC_CH       1

// --- SENSOR DE PRESENÇA LD2410B ---
#define LD2410_OUT_PIN     4

// --- MATRIZ DE LED MAX7219 32x8 (4 módulos 8x8 em cascata via SPI0) ---
#define MAX7219_SPI        spi0
#define MAX7219_PIN_CS     17
#define MAX7219_PIN_SCK    18
#define MAX7219_PIN_DIN    19
#define NUM_MAX7219_CHIPS  4
#define MATRIX_COLS        32
#define MATRIX_ROWS        8

// Intensidade dos LEDs (Registrador 0x0A do MAX7219 vai de 0x00 a 0x0F):
#define INTENSITY_REACTIVE 0x04  // 9/32 do duty cycle
#define INTENSITY_AMBIENT  0x00  // 1/32 do duty cycle (mínimo consumo em espera)

// --- PARÂMETROS DE ÁUDIO E DMA (Fs = 16 kHz -> Nyquist = 8 kHz) ---
#define SAMPLES            256
#define FFT_STAGES         8   // 2^8 = 256
#define FS                 16000
#define ADC_CLOCK_DIV      ((48000000.0f / (float)FS) - 1.0f) // 2999.0f
#define PI                 3.14159265358979323846f

// ---------------------------------------------------------------------
// Parâmetros visuais configuráveis em tempo de execução pelo menu
// ---------------------------------------------------------------------
volatile float cfg_noise_threshold = 20.0f;   // piso de ruído da FFT
volatile float cfg_max_energy_bin  = 800.0f;  // teto de energia (100% da escala)
volatile float cfg_gain_visual     = 1.50f;   // ganho visual (só afeta a matriz)
volatile float cfg_falloff_speed   = 1.00f;   // velocidade de queda das colunas

typedef struct {
    const char *name;
    volatile float *value;
    float min;
    float max;
    float step;
    float def_val;
    const char *fmt;
} ConfigParam;

ConfigParam config_params[] = {
    { "LIMIAR RUIDO",    &cfg_noise_threshold, 0.0f,   500.0f,  5.0f,  20.0f,  "%.0f" },
    { "TETO ENERGIA",    &cfg_max_energy_bin,  100.0f, 3000.0f, 50.0f, 800.0f, "%.0f" },
    { "GANHO VISUAL",    &cfg_gain_visual,     0.5f,   5.0f,    0.1f,  1.50f,  "%.2f" },
    { "QUEDA (FALLOFF)", &cfg_falloff_speed,   0.1f,   5.0f,    0.1f,  1.00f,  "%.2f" },
};
#define NUM_CONFIG_PARAMS (sizeof(config_params) / sizeof(config_params[0]))

volatile int selected_param = 0;

// ---------------------------------------------------------------------
// Domínio do CORE 1: captura de áudio + FFT
// ---------------------------------------------------------------------
uint dma_chan_a;
uint dma_chan_b;
uint16_t adc_buffer_a[SAMPLES];
uint16_t adc_buffer_b[SAMPLES];
volatile uint16_t *ready_buffer = NULL;
volatile bool data_ready = false;

float vReal[SAMPLES];
float vImag[SAMPLES];
float dc_offset = 2048.0f;

float hann_window[SAMPLES];
float twiddle_cos[SAMPLES / 2];
float twiddle_sin[SAMPLES / 2];

#define MAG_BUFFERS 3
float magnitudes_buf[MAG_BUFFERS][SAMPLES / 2];

bool joy_moved_x = false;
bool joy_moved_y = false;
uint32_t joy_hold_x_ms = 0;

// ---------------------------------------------------------------------
// Domínio do CORE 0: matriz MAX7219, OLED, botões, sensor de presença
// ---------------------------------------------------------------------
float peak_columns[MATRIX_COLS] = {0};
ssd1306_t display;

typedef enum { MODE_STANDBY, MODE_AMBIENT, MODE_REACTIVE, MODE_OFF } SystemMode;
SystemMode current_mode = MODE_STANDBY;

typedef enum { OVERRIDE_NONE, OVERRIDE_REACTIVE, OVERRIDE_OFF } ManualOverride;
ManualOverride override_mode = OVERRIDE_NONE;

bool presence_detected = false;
bool in_spectrum = false;
bool in_menu = false;

bool btn_a_pressed = false;
bool btn_b_pressed = false;
bool btn_c_pressed = false;

volatile bool core1_in_menu = false;

static inline float energy_to_ratio(float energy) {
    float ratio = (energy / cfg_max_energy_bin) * cfg_gain_visual;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    return ratio;
}

// =====================================================================
// LEITURA DO SENSOR LD2410B COM CORREÇÃO DA ERRATA RP2350-E9
// Desabilita brevemente o buffer de entrada do pad para que o pull-down
// interno descarregue a tensão residual (~2.1V) quando o pino vai a 0V
// ou fica desconectado, evitando travamento em nível lógico alto.
// =====================================================================
static inline bool read_ld2410_presence(void) {
    gpio_set_input_enabled(LD2410_OUT_PIN, false);
    busy_wait_us(10);
    gpio_set_input_enabled(LD2410_OUT_PIN, true);
    busy_wait_us(2);
    return gpio_get(LD2410_OUT_PIN);
}

// --- INTERRUPÇÃO PING-PONG DMA (roda no Core 1) ---
void dma_handler() {
    if (dma_hw->ints0 & (1u << dma_chan_a)) {
        dma_hw->ints0 = 1u << dma_chan_a;
        dma_channel_set_write_addr(dma_chan_a, adc_buffer_a, false);
        ready_buffer = adc_buffer_a;
        data_ready = true;
    }
    if (dma_hw->ints0 & (1u << dma_chan_b)) {
        dma_hw->ints0 = 1u << dma_chan_b;
        dma_channel_set_write_addr(dma_chan_b, adc_buffer_b, false);
        ready_buffer = adc_buffer_b;
        data_ready = true;
    }
}

void setup_dma_adc() {
    adc_gpio_init(MIC_PIN);
    adc_gpio_init(JOY_Y_PIN);
    adc_gpio_init(JOY_X_PIN);
    adc_init();

    adc_select_input(MIC_ADC_CH);
    adc_fifo_setup(true, true, 1, false, false);
    adc_set_clkdiv(ADC_CLOCK_DIV);

    dma_chan_a = dma_claim_unused_channel(true);
    dma_chan_b = dma_claim_unused_channel(true);

    dma_channel_config cfg_a = dma_channel_get_default_config(dma_chan_a);
    channel_config_set_transfer_data_size(&cfg_a, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg_a, false);
    channel_config_set_write_increment(&cfg_a, true);
    channel_config_set_dreq(&cfg_a, DREQ_ADC);
    channel_config_set_chain_to(&cfg_a, dma_chan_b);

    dma_channel_config cfg_b = dma_channel_get_default_config(dma_chan_b);
    channel_config_set_transfer_data_size(&cfg_b, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg_b, false);
    channel_config_set_write_increment(&cfg_b, true);
    channel_config_set_dreq(&cfg_b, DREQ_ADC);
    channel_config_set_chain_to(&cfg_b, dma_chan_a);

    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    dma_channel_set_irq0_enabled(dma_chan_a, true);
    dma_channel_set_irq0_enabled(dma_chan_b, true);
    irq_set_enabled(DMA_IRQ_0, true);

    dma_channel_configure(dma_chan_a, &cfg_a, adc_buffer_a, &adc_hw->fifo, SAMPLES, false);
    dma_channel_configure(dma_chan_b, &cfg_b, adc_buffer_b, &adc_hw->fifo, SAMPLES, false);

    dma_channel_start(dma_chan_a);
    adc_run(true);
}

void init_fft_tables(void) {
    for (int i = 0; i < SAMPLES; i++) {
        hann_window[i] = 0.5f * (1.0f - cosf(2.0f * PI * i / (SAMPLES - 1)));
    }
    for (int k = 0; k < SAMPLES / 2; k++) {
        twiddle_cos[k] = cosf(-2.0f * PI * k / SAMPLES);
        twiddle_sin[k] = sinf(-2.0f * PI * k / SAMPLES);
    }
}

void compute_fft() {
    uint16_t i, j, k, n1, n2, a;
    float c, s, t1, t2;
    j = 0;
    for (i = 0; i < SAMPLES - 1; i++) {
        if (i < j) {
            t1 = vReal[i]; vReal[i] = vReal[j]; vReal[j] = t1;
            t1 = vImag[i]; vImag[i] = vImag[j]; vImag[j] = t1;
        }
        k = SAMPLES / 2;
        while (k <= j) { j -= k; k /= 2; }
        j += k;
    }
    n1 = 0; n2 = 1;
    for (i = 0; i < FFT_STAGES; i++) {
        n1 = n2; n2 = n2 + n2; a = 0;
        for (j = 0; j < n1; j++) {
            c = twiddle_cos[a];
            s = twiddle_sin[a];
            a += 1 << (FFT_STAGES - i - 1);
            for (k = j; k < SAMPLES; k = k + n2) {
                t1 = c * vReal[k + n1] - s * vImag[k + n1];
                t2 = s * vReal[k + n1] + c * vImag[k + n1];
                vReal[k + n1] = vReal[k] - t1;
                vImag[k + n1] = vImag[k] - t2;
                vReal[k] = vReal[k] + t1;
                vImag[k] = vImag[k] + t2;
            }
        }
    }
}

// =====================================================================
// MATRIZ MAX7219 32x8 (4 chips em cascata via SPI0)
// =====================================================================
#define MAX7219_REG_NOOP        0x00
#define MAX7219_REG_DIGIT0      0x01
#define MAX7219_REG_DECODE      0x09
#define MAX7219_REG_INTENSITY   0x0A
#define MAX7219_REG_SCANLIMIT   0x0B
#define MAX7219_REG_SHUTDOWN    0x0C
#define MAX7219_REG_DISPLAYTEST 0x0F

// matrix_fb[linha][chip] — chip 0 = módulo ligado aos cabos DIN (direita),
// chip 3 = módulo na extremidade DOUT (esquerda).
uint8_t matrix_fb[MATRIX_ROWS][NUM_MAX7219_CHIPS];

static inline void max7219_write_raw(uint8_t *buf, size_t len) {
    gpio_put(MAX7219_PIN_CS, 0);
    spi_write_blocking(MAX7219_SPI, buf, len);
    gpio_put(MAX7219_PIN_CS, 1);
}

void max7219_write_all(uint8_t reg, uint8_t value) {
    uint8_t buf[NUM_MAX7219_CHIPS * 2];
    for (int i = 0; i < NUM_MAX7219_CHIPS; i++) {
        buf[i * 2] = reg;
        buf[i * 2 + 1] = value;
    }
    max7219_write_raw(buf, sizeof(buf));
}

void max7219_write_row(uint8_t reg, uint8_t *values_per_chip) {
    uint8_t buf[NUM_MAX7219_CHIPS * 2];
    for (int i = 0; i < NUM_MAX7219_CHIPS; i++) {
        int chip = NUM_MAX7219_CHIPS - 1 - i;
        buf[i * 2] = reg;
        buf[i * 2 + 1] = values_per_chip[chip];
    }
    max7219_write_raw(buf, sizeof(buf));
}

void max7219_init(void) {
    spi_init(MAX7219_SPI, 4 * 1000 * 1000);
    gpio_set_function(MAX7219_PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(MAX7219_PIN_DIN, GPIO_FUNC_SPI);
    gpio_init(MAX7219_PIN_CS);
    gpio_set_dir(MAX7219_PIN_CS, GPIO_OUT);
    gpio_put(MAX7219_PIN_CS, 1);

    max7219_write_all(MAX7219_REG_DISPLAYTEST, 0x00);
    max7219_write_all(MAX7219_REG_DECODE,      0x00);
    max7219_write_all(MAX7219_REG_SCANLIMIT,   0x07);
    max7219_write_all(MAX7219_REG_SHUTDOWN,    0x01);
    max7219_write_all(MAX7219_REG_INTENSITY,   INTENSITY_AMBIENT);

    memset(matrix_fb, 0, sizeof(matrix_fb));
    for (int row = 0; row < MATRIX_ROWS; row++) {
        max7219_write_all(MAX7219_REG_DIGIT0 + row, 0x00);
    }
}

void max7219_set_intensity(uint8_t level) {
    if (level > 0x0F) level = 0x0F;
    max7219_write_all(MAX7219_REG_INTENSITY, level);
}

void max7219_shutdown(bool shutdown) {
    max7219_write_all(MAX7219_REG_SHUTDOWN, shutdown ? 0x00 : 0x01);
}

void max7219_render(void) {
    for (int row = 0; row < MATRIX_ROWS; row++) {
        max7219_write_row(MAX7219_REG_DIGIT0 + row, matrix_fb[row]);
    }
}

void matrix_clear(void) {
    memset(matrix_fb, 0, sizeof(matrix_fb));
}

// Mapeamento físico validado:
// - col 0..7 (graves) -> Chip 3 (módulo da esquerda, ponta DOUT)
// - col 24..31 (agudos, até 8 kHz) -> Chip 0 (módulo da direita, ponta DIN)
// - Dentro de cada módulo, Bit 7 = esquerda e Bit 0 = direita.
void matrix_set_pixel(int col, int row, bool on) {
    if (col < 0 || col >= MATRIX_COLS || row < 0 || row >= MATRIX_ROWS) return;
    int chip = (NUM_MAX7219_CHIPS - 1) - (col / 8);
    int bit  = 7 - (col % 8);
    if (on) matrix_fb[row][chip] |= (1u << bit);
    else    matrix_fb[row][chip] &= ~(uint8_t)(1u << bit);
}

// =====================================================================
// INTERFACE VISUAL DO OLED (roda no Core 0)
// =====================================================================
void ssd1306_draw_line(ssd1306_t *ssd, int x0, int y0, int x1, int y1, bool value) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    while (1) {
        ssd1306_draw_pixel(ssd, x0, y0, value);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void ssd1306_draw_rect(ssd1306_t *ssd, int x, int y, int w, int h, bool value) {
    ssd1306_draw_line(ssd, x, y, x + w - 1, y, value);
    ssd1306_draw_line(ssd, x, y + h - 1, x + w - 1, y + h - 1, value);
    ssd1306_draw_line(ssd, x, y, x, y + h - 1, value);
    ssd1306_draw_line(ssd, x + w - 1, y, x + w - 1, y + h - 1, value);
}

void ssd1306_fill_rect(ssd1306_t *ssd, int x, int y, int w, int h, bool value) {
    for (int py = y; py < y + h; py++) {
        for (int px = x; px < x + w; px++) {
            ssd1306_draw_pixel(ssd, px, py, value);
        }
    }
}

// Desenha barra de progresso com marcador do valor padrão de fábrica
void draw_ui_bar_ex(ssd1306_t *ssd, int x, int y, int w, int h, float value, float min_value, float max_value, float default_marker) {
    if (max_value <= min_value) return;
    float ratio = (value - min_value) / (max_value - min_value);
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    ssd1306_draw_rect(ssd, x, y, w, h, true);
    int fill_w = (int)((w - 4) * ratio);
    if (fill_w > 0) {
        ssd1306_fill_rect(ssd, x + 2, y + 2, fill_w, h - 4, true);
    }

    if (default_marker >= min_value && default_marker <= max_value) {
        float def_ratio = (default_marker - min_value) / (max_value - min_value);
        int def_x = x + 2 + (int)((w - 4) * def_ratio);
        ssd1306_draw_pixel(ssd, def_x, y - 1, true);
        ssd1306_draw_pixel(ssd, def_x, y + h, true);
    }
}

void draw_centered_text(ssd1306_t *ssd, int y, const char *text) {
    int text_width = (int)strlen(text) * 6;
    int x = (128 - text_width) / 2;
    if (x < 0) x = 0;
    ssd1306_draw_string(ssd, x, y, text);
}

void draw_radar_icon(ssd1306_t *ssd, int cx, int cy, bool active) {
    ssd1306_draw_rect(ssd, cx - 1, cy - 1, 3, 3, true);
    if (active) {
        ssd1306_draw_line(ssd, cx + 3, cy - 2, cx + 3, cy + 2, true);
        ssd1306_draw_line(ssd, cx + 5, cy - 3, cx + 5, cy + 3, true);
    }
}

// --- TELA 1: PAINEL PRINCIPAL (DASHBOARD LIMPO) ---
void render_normal_ui(void) {
    ssd1306_fill(&display, false);

    // 1. Cabeçalho
    draw_centered_text(&display, 0, "SERIP 2 (8KHZ)");
    ssd1306_draw_line(&display, 0, 9, 127, 9, true);
    ssd1306_draw_line(&display, 2, 3, 2, 6, true);
    ssd1306_draw_line(&display, 125, 3, 125, 6, true);

    // 2. Quadro de Estado Principal
    ssd1306_draw_string(&display, 4, 13, "ESTADO");
    const char *mode_str =
        (current_mode == MODE_REACTIVE) ? "MODO REATIVO" :
        (current_mode == MODE_AMBIENT)  ? "LUZ AMBIENTE" :
        (current_mode == MODE_OFF)      ? "DESLIGADO"    : "EM STANDBY";

    ssd1306_draw_rect(&display, 3, 22, 122, 17, true);
    ssd1306_fill_rect(&display, 5, 24, 3, 13, true);
    ssd1306_fill_rect(&display, 120, 24, 3, 13, true);
    draw_centered_text(&display, 27, mode_str);

    // 3. Linha de Presença (Radar) e Controle (Botão A)
    draw_radar_icon(&display, 6, 45, presence_detected);
    ssd1306_draw_string(&display, 15, 42, presence_detected ? "PRESENCA:SIM" : "PRESENCA:NAO");

    const char *ctrl_str =
        (override_mode == OVERRIDE_REACTIVE) ? "[FIXO]" :
        (override_mode == OVERRIDE_OFF)      ? "[OFF]"  : "[AUTO]";
    ssd1306_draw_string(&display, 128 - ((int)strlen(ctrl_str) * 6) - 2, 42, ctrl_str);

    // 4. Rodapé de Botões
    ssd1306_draw_line(&display, 0, 52, 127, 52, true);
    ssd1306_draw_string(&display, 2,  55, "A:MODO");
    ssd1306_draw_line(&display, 41, 53, 41, 63, true);
    ssd1306_draw_string(&display, 45, 55, "B:GRAF");
    ssd1306_draw_line(&display, 84, 53, 84, 63, true);
    ssd1306_draw_string(&display, 88, 55, "C:MENU");

    ssd1306_send_data(&display);
}

// --- TELA 2: MENU DE CALIBRAÇÃO (4 PARÂMETROS) ---
void render_menu_ui(void) {
    ssd1306_fill(&display, false);

    // Snapshot local para evitar leitura concorrente durante o desenho
    int sel = selected_param;
    if (sel < 0 || sel >= (int)NUM_CONFIG_PARAMS) sel = 0;
    ConfigParam *p = &config_params[sel];
    float cur_val = *(p->value);

    // 1. Cabeçalho + Indicador de Paginação [1..4]
    ssd1306_draw_string(&display, 2, 0, "CONFIGURACAO");
    for (int i = 0; i < (int)NUM_CONFIG_PARAMS; i++) {
        int bx = 88 + (i * 10);
        if (i == sel) {
            ssd1306_fill_rect(&display, bx, 1, 7, 6, true);
        } else {
            ssd1306_draw_rect(&display, bx, 1, 7, 6, true);
        }
    }
    ssd1306_draw_line(&display, 0, 9, 127, 9, true);

    // 2. Nome do Parâmetro
    draw_centered_text(&display, 13, p->name);

    // 3. Valor Formatado com Setas Laterais
    char buf_val[16];
    snprintf(buf_val, sizeof(buf_val), p->fmt, cur_val);
    ssd1306_draw_string(&display, 16, 25, "<<");
    draw_centered_text(&display, 25, buf_val);
    ssd1306_draw_string(&display, 100, 25, ">>");

    // 4. Barra de Progresso com Marcador de Valor Padrão
    draw_ui_bar_ex(&display, 8, 36, 112, 8, cur_val, p->min, p->max, p->def_val);

    char buf_min[16], buf_max[16];
    snprintf(buf_min, sizeof(buf_min), p->fmt, p->min);
    snprintf(buf_max, sizeof(buf_max), p->fmt, p->max);
    ssd1306_draw_string(&display, 8, 46, buf_min);
    ssd1306_draw_string(&display, 120 - ((int)strlen(buf_max) * 6), 46, buf_max);

    // 5. Rodapé de Navegação
    ssd1306_draw_line(&display, 0, 54, 127, 54, true);
    ssd1306_draw_string(&display, 2, 56, "X:AJUSTA Y:ITEM C:OK");

    ssd1306_send_data(&display);
}

// --- TELA 3: ANALISADOR DE ESPECTRO 128 COLUNAS ---
void render_spectrum_ui(const float *magnitudes) {
    ssd1306_fill(&display, false);

    // Área do Gráfico: y = 0 (topo) até y = 53 (base), altura útil = 54 px
    const int graph_bottom = 53;
    const int max_bar_h = 53;

    if (magnitudes != NULL) {
        for (int i = 1; i < (SAMPLES / 2); i++) { // ignora bin 0 (DC)
            int height = (int)((magnitudes[i] / cfg_max_energy_bin) * (float)max_bar_h);
            if (height > max_bar_h) height = max_bar_h;
            if (height < 0) height = 0;

            if (height > 0) {
                ssd1306_draw_line(&display, i, graph_bottom, i, graph_bottom - height, true);
            }
        }
    }

    // Eixo Horizontal de Frequências (0, 2K, 4K, 6K, 8K) na base (y = 54..63)
    ssd1306_draw_line(&display, 0, 54, 127, 54, true);
    const int ticks_x[5] = { 0, 32, 64, 96, 127 };
    for (int t = 0; t < 5; t++) {
        ssd1306_draw_pixel(&display, ticks_x[t], 55, true);
    }
    ssd1306_draw_string(&display, 0,   56, "0");
    ssd1306_draw_string(&display, 27,  56, "2K");
    ssd1306_draw_string(&display, 59,  56, "4K");
    ssd1306_draw_string(&display, 91,  56, "6K");
    ssd1306_draw_string(&display, 116, 56, "8K");

    ssd1306_send_data(&display);
}

// =====================================================================
// CORE 1: captura de áudio + FFT + leitura do joystick no menu.
// =====================================================================
void core1_entry(void) {
    setup_dma_adc();
    init_fft_tables();

    bool was_in_menu = false;
    int mag_idx = 0;

    while (true) {
        if (core1_in_menu) {
            if (!was_in_menu) {
                // Pausa limpa do ADC/DMA sem dma_channel_abort (evita loop de chain_to)
                adc_run(false);
                while (!(adc_hw->cs & ADC_CS_READY_BITS)) {
                    tight_loop_contents();
                }
                adc_fifo_setup(false, false, 0, false, false);
                adc_fifo_drain();
                adc_set_clkdiv(0);
                joy_moved_x = false;
                joy_moved_y = false;
                was_in_menu = true;
            }

            int sel = selected_param;
            ConfigParam *p = &config_params[sel];
            float val = *(p->value);
            uint32_t now_ms = to_ms_since_boot(get_absolute_time());

            // Eixo X com suporte a toque único e Auto-Repeat ao segurar (>320ms)
            adc_select_input(JOY_X_ADC_CH);
            uint16_t joy_x = adc_read();
            if (joy_x < 1000) {
                if (!joy_moved_x) {
                    val += p->step;
                    joy_moved_x = true;
                    joy_hold_x_ms = now_ms + 320;
                } else if (now_ms >= joy_hold_x_ms) {
                    val += p->step;
                    joy_hold_x_ms = now_ms + 70;
                }
            } else if (joy_x > 3000) {
                if (!joy_moved_x) {
                    val -= p->step;
                    joy_moved_x = true;
                    joy_hold_x_ms = now_ms + 320;
                } else if (now_ms >= joy_hold_x_ms) {
                    val -= p->step;
                    joy_hold_x_ms = now_ms + 70;
                }
            } else {
                joy_moved_x = false;
            }
            if (val < p->min) val = p->min;
            if (val > p->max) val = p->max;
            *(p->value) = val;

            // Eixo Y: seleciona qual parâmetro ajustar
            adc_select_input(JOY_Y_ADC_CH);
            uint16_t joy_y = adc_read();
            if (joy_y < 1000) {
                if (!joy_moved_y) {
                    selected_param = (sel + 1) % (int)NUM_CONFIG_PARAMS;
                    joy_moved_y = true;
                    joy_moved_x = false;
                }
            } else if (joy_y > 3000) {
                if (!joy_moved_y) {
                    selected_param = (sel - 1 + (int)NUM_CONFIG_PARAMS) % (int)NUM_CONFIG_PARAMS;
                    joy_moved_y = true;
                    joy_moved_x = false;
                }
            } else {
                joy_moved_y = false;
            }

            sleep_ms(20);
            continue;
        }

        if (was_in_menu) {
            // Reativa a FIFO do ADC e retoma o DMA de onde parou
            adc_select_input(MIC_ADC_CH);
            adc_set_clkdiv(ADC_CLOCK_DIV);
            adc_fifo_drain();
            adc_fifo_setup(true, true, 1, false, false);
            adc_run(true);
            was_in_menu = false;
        }

        if (!data_ready) {
            tight_loop_contents();
            continue;
        }
        data_ready = false;

        const volatile uint16_t *buf = ready_buffer;
        for (int i = 0; i < SAMPLES; i++) {
            float raw = (float)buf[i];
            dc_offset = (dc_offset * 0.995f) + (raw * 0.005f);
            float ac_value = raw - dc_offset;
            vReal[i] = ac_value * hann_window[i];
            vImag[i] = 0.0f;
        }

        compute_fft();

        float *mags = magnitudes_buf[mag_idx];
        float thr = cfg_noise_threshold;

        for (int i = 0; i < SAMPLES / 2; i++) {
            float mag = sqrtf(vReal[i] * vReal[i] + vImag[i] * vImag[i]);
            mag *= 0.25f;

            if (mag < thr) mag = 0.0f;
            else mag -= thr;

            float freq_weight = 1.0f + ((float)i / (SAMPLES / 2.0f));
            mag *= freq_weight;

            mags[i] = mag;
        }

        if (multicore_fifo_wready()) {
            multicore_fifo_push_blocking((uintptr_t)mags);
            mag_idx = (mag_idx + 1) % MAG_BUFFERS;
        }
    }
}

// =====================================================================
// CORE 0: botões, sensor de presença, matriz MAX7219, display OLED.
// =====================================================================
int main() {
    stdio_init_all();

    gpio_init(BTN_A); gpio_set_dir(BTN_A, GPIO_IN); gpio_pull_up(BTN_A);
    gpio_init(BTN_B); gpio_set_dir(BTN_B, GPIO_IN); gpio_pull_up(BTN_B);
    gpio_init(BTN_C); gpio_set_dir(BTN_C, GPIO_IN); gpio_pull_up(BTN_C);

    gpio_init(LD2410_OUT_PIN);
    gpio_set_dir(LD2410_OUT_PIN, GPIO_IN);
    gpio_pull_down(LD2410_OUT_PIN);

    max7219_init();

    i2c_init(i2c1, 400 * 1000);
    gpio_set_function(OLED_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(OLED_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(OLED_SDA_PIN); gpio_pull_up(OLED_SCL_PIN);
    ssd1306_init(&display, 128, 64, 0x3C, i2c1);

    multicore_launch_core1(core1_entry);

    SystemMode previous_mode = (SystemMode)(-1);
    float *last_mags = magnitudes_buf[0];

    while (true) {
        bool btn_a_now = !gpio_get(BTN_A);
        bool btn_b_now = !gpio_get(BTN_B);
        bool btn_c_now = !gpio_get(BTN_C);
        bool ui_dirty = false;

        // Leitura com correção da Errata RP2350-E9 para não reter 3.3V
        bool new_presence = read_ld2410_presence();
        if (new_presence != presence_detected) {
            presence_detected = new_presence;
            ui_dirty = true;
        }

        // --- BOTÃO C (Configurações) ---
        if (btn_c_now) {
            if (!btn_c_pressed) {
                in_menu = !in_menu;
                core1_in_menu = in_menu;
                if (in_menu) in_spectrum = false;
                btn_c_pressed = true;
                ui_dirty = true;
            }
        } else {
            btn_c_pressed = false;
        }

        // --- BOTÃO B (Gráfico de Espectro OLED) ---
        if (btn_b_now) {
            if (!btn_b_pressed) {
                in_spectrum = !in_spectrum;
                if (in_spectrum) { in_menu = false; core1_in_menu = false; }
                btn_b_pressed = true;
                ui_dirty = true;
            }
        } else {
            btn_b_pressed = false;
        }

        // --- FLUXO DO MENU ---
        if (in_menu) {
            while (multicore_fifo_rvalid()) {
                last_mags = (float *)multicore_fifo_pop_blocking();
            }
            render_menu_ui();
            sleep_ms(20);
            continue;
        }

        // --- BOTÃO A (cicla Automático / Reativo / Desligado) ---
        if (btn_a_now) {
            if (!btn_a_pressed) {
                override_mode = (override_mode + 1) % 3;
                btn_a_pressed = true;
                ui_dirty = true;
            }
        } else {
            btn_a_pressed = false;
        }

        // --- Decide o modo atual ---
        if (override_mode == OVERRIDE_REACTIVE) {
            current_mode = MODE_REACTIVE;
        } else if (override_mode == OVERRIDE_OFF) {
            current_mode = MODE_OFF;
        } else {
            current_mode = presence_detected ? MODE_AMBIENT : MODE_STANDBY;
        }

        if (current_mode != previous_mode) {
            if (current_mode == MODE_OFF) {
                matrix_clear();
                max7219_render();
                max7219_shutdown(true);
            } else {
                if (previous_mode == MODE_OFF) max7219_shutdown(false);
                uint8_t intensity =
                    (current_mode == MODE_REACTIVE) ? INTENSITY_REACTIVE :
                    (current_mode == MODE_AMBIENT)  ? INTENSITY_AMBIENT  : 0x00;
                max7219_set_intensity(intensity);
            }
            previous_mode = current_mode;
            ui_dirty = true;
        }

        // --- Pega o quadro de magnitudes mais recente do Core 1 ---
        float *mags = NULL;
        while (multicore_fifo_rvalid()) {
            mags = (float *)multicore_fifo_pop_blocking();
        }

        if (mags != NULL) {
            last_mags = mags;
            ui_dirty = true;
        }

        if (!ui_dirty) {
            sleep_ms(1);
            continue;
        }

        if (current_mode != MODE_OFF) {
            // Agrupa os 128 bins (0 a 8 kHz) em 32 colunas (4 bins = 250 Hz por coluna).
            // Ignora o bin 0 (componente contínua DC) na coluna 0.
            const int bins_per_col = (SAMPLES / 2) / MATRIX_COLS;
            float cols[MATRIX_COLS] = {0};
            for (int c = 0; c < MATRIX_COLS; c++) {
                float sum = 0.0f;
                int start_b = (c == 0) ? 1 : 0;
                for (int b = start_b; b < bins_per_col; b++) {
                    sum += last_mags[c * bins_per_col + b];
                }
                cols[c] = sum / (float)(bins_per_col - start_b);
            }

            matrix_clear();

            if (current_mode == MODE_AMBIENT) {
                for (int c = 0; c < MATRIX_COLS; c++) {
                    matrix_set_pixel(c, MATRIX_ROWS - 1, true);
                }
            } else if (current_mode == MODE_REACTIVE) {
                for (int c = 0; c < MATRIX_COLS; c++) {
                    float col_height_float = energy_to_ratio(cols[c]) * (float)MATRIX_ROWS;

                    if (col_height_float > peak_columns[c]) peak_columns[c] = col_height_float;
                    else peak_columns[c] -= cfg_falloff_speed;

                    if (peak_columns[c] < 0) peak_columns[c] = 0;

                    int col_height = (int)roundf(peak_columns[c]);
                    if (col_height < 1) col_height = 1; // base sempre acesa
                    if (col_height > MATRIX_ROWS) col_height = MATRIX_ROWS;

                    for (int y = 0; y < col_height; y++) {
                        int row = MATRIX_ROWS - 1 - y; // row 7 = base inferior, row 0 = topo
                        matrix_set_pixel(c, row, true);
                    }
                }
            }

            max7219_render();
        }

        // --- Atualiza a Interface do Display OLED ---
        if (in_spectrum) {
            render_spectrum_ui(last_mags);
        } else {
            render_normal_ui();
        }
    }
    return 0;
}