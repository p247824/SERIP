import matplotlib.pyplot as plt
import matplotlib.patches as patches
import numpy as np

# Configuração da folha técnica (proporção 16:10 em alta definição - 300 DPI)
fig, ax = plt.subplots(figsize=(18, 11.25), dpi=300)
ax.set_xlim(0, 200)
ax.set_ylim(0, 125)
ax.set_aspect('equal')
ax.axis('off')
fig.patch.set_facecolor('#f8fafc')

# ==============================================================================
# 1. MOLDURA TÉCNICA DE ENGENHARIA E GRELHA DE FUNDO
# ==============================================================================
for gx in range(5, 196, 5):
    ax.plot([gx, gx], [5, 120], color='#e2e8f0', linewidth=0.4, zorder=0)
for gy in range(5, 121, 5):
    ax.plot([5, 195], [gy, gy], color='#e2e8f0', linewidth=0.4, zorder=0)

outer_frame = patches.Rectangle((3, 3), 194, 119, facecolor='none', edgecolor='#0f172a', linewidth=2.5, zorder=10)
inner_frame = patches.Rectangle((5, 5), 190, 115, facecolor='none', edgecolor='#475569', linewidth=1.0, zorder=10)
ax.add_patch(outer_frame)
ax.add_patch(inner_frame)

header_box = patches.Rectangle((5, 112), 190, 8, facecolor='#0f172a', edgecolor='#0f172a', zorder=10)
ax.add_patch(header_box)
ax.text(100, 116.8, "DIAGRAMA ESQUEMÁTICO E DE CABLAGEM — SISTEMA SERIP 2",
        color='#ffffff', fontsize=14, fontweight='bold', ha='center', va='center', zorder=11)
ax.text(100, 113.8, "Arquitetura Dual-Core RP2350 (BitDogLab V7)  •  Matriz SPI MAX7219 (32×8)  •  Radar FMCW 24 GHz HLK-LD2410B",
        color='#94a3b8', fontsize=9.5, ha='center', va='center', zorder=11)

# ==============================================================================
# FUNÇÕES AUXILIARES DE DESENHO CAD
# ==============================================================================
def draw_wire(pts, color, lw=2.2, label=None, label_pos=None):
    """Desenha um condutor ortogonal com contorno de isolamento e terminais Dupont."""
    xs, ys = zip(*pts)
    ax.plot(xs, ys, color='#ffffff', linewidth=lw + 1.6, solid_capstyle='round', solid_joinstyle='round', zorder=6)
    ax.plot(xs, ys, color=color, linewidth=lw, solid_capstyle='round', solid_joinstyle='round', zorder=7)

    for px, py in [pts[0], pts[-1]]:
        ax.plot(px, py, marker='o', markersize=5.5, markerfacecolor=color,
                markeredgecolor='#0f172a', markeredgewidth=1.0, zorder=9)

    if label and label_pos:
        lx, ly = label_pos
        bbox_props = dict(boxstyle="round,pad=0.22", fc="#ffffff", ec=color, lw=1.2, alpha=0.96)
        ax.text(lx, ly, label, fontsize=7, fontweight='bold', color='#0f172a',
                ha='center', va='center', bbox=bbox_props, zorder=9)

# ==============================================================================
# 2. PLACA BITDOGLAB V7 (PCB PRETO FIEL À FOTO REAL) - ESQUERDA
# ==============================================================================
# PCB Preto Fosco da BitDogLab V7 com contorno em serigrafia branca
bdl_pcb = patches.FancyBboxPatch((10, 10), 58, 96, boxstyle="round,pad=0.5,rounding_size=2.5",
                                 facecolor='#14171f', edgecolor='#334155', linewidth=2.5, zorder=2)
bdl_silk = patches.FancyBboxPatch((11.2, 11.2), 55.6, 93.6, boxstyle="round,pad=0.3,rounding_size=2.0",
                                  facecolor='none', edgecolor='#e2e8f0', linewidth=0.8, zorder=3)
ax.add_patch(bdl_pcb)
ax.add_patch(bdl_silk)

# Conectores JST brancos no topo (J3: I2C1 e J4: I2C0/UART) e botão Reset
ax.add_patch(patches.Rectangle((24, 103.5), 9, 3.2, facecolor='#f8fafc', edgecolor='#64748b', lw=1, zorder=4))
ax.text(28.5, 102.0, "J3 (I2C1)", fontsize=5.5, color='#e2e8f0', ha='center', zorder=4)
ax.add_patch(patches.Rectangle((47, 103.5), 9, 3.2, facecolor='#f8fafc', edgecolor='#64748b', lw=1, zorder=4))
ax.text(51.5, 102.0, "J4 (I2C0/UART)", fontsize=5.5, color='#e2e8f0', ha='center', zorder=4)

# Botão Reset no topo
ax.add_patch(patches.Rectangle((38.2, 99.5), 3.6, 3.6, facecolor='#cbd5e1', edgecolor='#475569', lw=0.8, zorder=4))
ax.add_patch(patches.Circle((40.0, 101.3), 1.2, facecolor='#0f172a', zorder=5))
ax.text(40.0, 97.6, "Reset", fontsize=6.0, fontweight='bold', color='#ffffff', ha='center', zorder=4)

# Buzzer MLT-8530 no canto superior direito (BZ1)
ax.add_patch(patches.FancyBboxPatch((58.5, 95.5), 6.0, 6.0, boxstyle="round,pad=0.1,rounding_size=0.8",
                                    facecolor='#1e293b', edgecolor='#94a3b8', lw=1.0, zorder=4))
ax.text(61.5, 98.5, "MLT\n8530", fontsize=4.8, color='#cbd5e1', ha='center', va='center', zorder=5)
ax.text(61.5, 93.8, "BZ1 (GP21)", fontsize=5.2, color='#94a3b8', ha='center', zorder=4)

# Microfone Analógico MAX4466 (U9 - GP28) no canto superior esquerdo
ax.add_patch(patches.Rectangle((13.5, 88.5), 13, 11.5, facecolor='none', edgecolor='#e2e8f0', lw=0.8, zorder=3))
mic_outer = patches.Circle((19.0, 94.2), 4.2, facecolor='#94a3b8', edgecolor='#e2e8f0', lw=1.2, zorder=4)
mic_inner = patches.Circle((19.0, 94.2), 2.9, facecolor='#0f172a', edgecolor='#475569', lw=0.8, zorder=5)
ax.add_patch(mic_outer)
ax.add_patch(mic_inner)
ax.text(25.2, 94.2, "MAX4466", fontsize=5.2, fontweight='bold', color='#e2e8f0', rotation=270, ha='center', va='center', zorder=4)
ax.text(20.0, 86.8, "Mic U9 (GP28/ADC2)", fontsize=6.2, fontweight='bold', color='#38bdf8', ha='center', zorder=4)

# Identificação central BitDogLab V7 / Escola 4.0 / UNICAMP
ax.text(35.5, 89.5, "Escola 4.0", fontsize=6.5, fontweight='bold', color='#cbd5e1', ha='center', zorder=4)
ax.text(35.5, 86.5, "BitDogLab V7", fontsize=9.5, fontweight='bold', color='#ffffff', ha='center', zorder=4)
ax.text(35.5, 83.8, "UNICAMP • RP2350", fontsize=6.0, fontweight='bold', color='#38bdf8', ha='center', zorder=4)

# Matriz 5x5 WS2812 Onboard (GP7)
for r in range(5):
    for c in range(5):
        ax.add_patch(patches.Rectangle((14.2 + c*2.5, 72.5 + r*2.5), 1.6, 1.6,
                                       facecolor='#cbd5e1', edgecolor='#475569', lw=0.4, zorder=4))
ax.text(19.5, 70.3, "Matriz 5×5 (GP7)", fontsize=6.0, color='#94a3b8', ha='center', zorder=4)

# Display OLED SSD1306 128x64 (I2C1: GP2/GP3) com PCB Azul
oled_pcb = patches.FancyBboxPatch((42, 68), 23.5, 22, boxstyle="round,pad=0.2,rounding_size=0.8",
                                  facecolor='#1d4ed8', edgecolor='#93c5fd', lw=1.2, zorder=4)
oled_scr = patches.Rectangle((43.2, 71.0), 21.1, 15.5, facecolor='#090d16', edgecolor='#38bdf8', lw=0.9, zorder=5)
ax.add_patch(oled_pcb)
ax.add_patch(oled_scr)
ax.text(53.75, 88.2, "GND VDD SCK(GP3) SDA(GP2)", fontsize=4.6, fontweight='bold', color='#ffffff', ha='center', zorder=5)
ax.text(53.75, 83.5, "BITDOGLAB V7", fontsize=5.8, fontweight='bold', color='#38bdf8', ha='center', zorder=6)
ax.text(53.75, 80.0, "CTRL: AUTO | AMBIENTE", fontsize=4.8, color='#22d3ee', ha='center', zorder=6)
ax.text(53.75, 76.5, "RADAR: PRESENTE", fontsize=4.8, color='#4ade80', ha='center', zorder=6)
ax.text(53.75, 72.5, "A:MODO B:GRAF C:CONF", fontsize=4.5, color='#f8fafc', ha='center', zorder=6)

# Joystick Analógico (GP26 / GP27)
joy_base = patches.Rectangle((13.5, 51.5), 15, 15, facecolor='#334155', edgecolor='#94a3b8', lw=1.0, zorder=4)
joy_cap1 = patches.Circle((21.0, 59.0), 6.8, facecolor='#0f172a', edgecolor='#475569', lw=1.5, zorder=5)
joy_cap2 = patches.Circle((21.0, 59.0), 4.6, facecolor='#1e293b', edgecolor='#64748b', lw=1.0, zorder=6)
ax.add_patch(joy_base)
ax.add_patch(joy_cap1)
ax.add_patch(joy_cap2)
ax.text(21.0, 49.0, "Joystick (VRy:GP26 | VRx:GP27)", fontsize=6.0, fontweight='bold', color='#e2e8f0', ha='center', zorder=4)

# Triângulo de Botões Físicos Fiel à Placa (C Verde em cima, A Vermelho esq, B Azul dir)
# Botão C (Verde - B5 - GP10)
ax.add_patch(patches.Rectangle((48.8, 55.8), 7.4, 7.4, facecolor='#334155', edgecolor='#cbd5e1', lw=1, zorder=4))
ax.add_patch(patches.Circle((52.5, 59.5), 3.1, facecolor='#16a34a', edgecolor='#052e16', lw=1.2, zorder=5))
ax.text(52.5, 59.5, "C", fontsize=9, fontweight='bold', color='#ffffff', ha='center', va='center', zorder=6)
ax.text(52.5, 64.3, "BTN_C: GP10 (Menu)", fontsize=6.0, fontweight='bold', color='#4ade80', ha='center', zorder=5)

# Botão A (Vermelho - B3 - GP5)
ax.add_patch(patches.Rectangle((41.8, 43.8), 7.4, 7.4, facecolor='#334155', edgecolor='#cbd5e1', lw=1, zorder=4))
ax.add_patch(patches.Circle((45.5, 47.5), 3.1, facecolor='#dc2626', edgecolor='#450a0a', lw=1.2, zorder=5))
ax.text(45.5, 47.5, "A", fontsize=9, fontweight='bold', color='#ffffff', ha='center', va='center', zorder=6)
ax.text(44.0, 41.8, "BTN_A: GP5 (Modo)", fontsize=6.0, fontweight='bold', color='#f87171', ha='center', zorder=5)

# Botão B (Azul - B4 - GP6)
ax.add_patch(patches.Rectangle((55.8, 43.8), 7.4, 7.4, facecolor='#334155', edgecolor='#cbd5e1', lw=1, zorder=4))
ax.add_patch(patches.Circle((59.5, 47.5), 3.1, facecolor='#2563eb', edgecolor='#172554', lw=1.2, zorder=5))
ax.text(59.5, 47.5, "B", fontsize=9, fontweight='bold', color='#ffffff', ha='center', va='center', zorder=6)
ax.text(60.0, 41.8, "BTN_B: GP6 (Gráfico)", fontsize=6.0, fontweight='bold', color='#60a5fa', ha='center', zorder=5)

# LED RGB SMD (D12) entre os botões A e B
ax.add_patch(patches.Rectangle((51.3, 45.3), 2.4, 2.4, facecolor='#f8fafc', edgecolor='#94a3b8', lw=0.8, zorder=4))
ax.text(52.5, 44.0, "D12", fontsize=4.8, color='#cbd5e1', ha='center', zorder=4)

# Conector IDC 14 Pinos + Tabela de Serigrafia na borda inferior (Fiel à foto)
idc_shroud = patches.FancyBboxPatch((13.5, 16.5), 51.5, 22.0, boxstyle="round,pad=0.2,rounding_size=0.6",
                                    facecolor='#090d16', edgecolor='#94a3b8', linewidth=1.8, zorder=4)
ax.add_patch(idc_shroud)
ax.text(36.0, 36.2, "CONECTOR IDC 14 PINOS (EXPANSÃO)", fontsize=7.2, fontweight='bold', color='#f8fafc', ha='center', zorder=5)

# Serigrafia na base da placa preta (como na foto)
silk_table = patches.Rectangle((13.5, 12.0), 40.0, 3.6, facecolor='#14171f', edgecolor='#ffffff', lw=0.8, zorder=4)
ax.add_patch(silk_table)
ax.text(33.5, 14.3, "GP21 GP16 GP17 GP4  GP13 3V3 GND", fontsize=5.0, fontweight='bold', color='#ffffff', ha='center', zorder=5)
ax.text(33.5, 12.6, "GP18 GP19 GP28 GP9  GP9  GP8 5V ", fontsize=5.0, fontweight='bold', color='#ffffff', ha='center', zorder=5)

idc_active_pins = [
    ("5V (Alimentação DC)",   33.5, '#ef4444'),
    ("GND (Referência/Massa)",30.8, '#94a3b8'),
    ("GP4  (Entrada Sensor)", 28.1, '#10b981'),
    ("GP17 (SPI0 CSn)",       25.4, '#f59e0b'),
    ("GP18 (SPI0 SCK)",       22.7, '#a855f7'),
    ("GP19 (SPI0 MOSI/TX)",   20.0, '#3b82f6')
]

for pname, py, pcol in idc_active_pins:
    ax.add_patch(patches.Rectangle((61.8, py-0.6), 2.4, 1.2, facecolor='#fbbf24', edgecolor='#b45309', lw=0.6, zorder=5))
    ax.text(60.5, py, pname, fontsize=6.8, fontweight='bold', color=pcol, ha='right', va='center', zorder=5)

# ==============================================================================
# 3. PROTOBOARD AUXILIAR (CENTRO)
# ==============================================================================
pb_body = patches.FancyBboxPatch((78, 36), 38, 36, boxstyle="round,pad=0.4,rounding_size=1.5",
                                 facecolor='#ffffff', edgecolor='#94a3b8', linewidth=2.0, zorder=2)
ax.add_patch(pb_body)
ax.text(97, 69.5, "Protoboard Auxiliar", fontsize=10, fontweight='bold', color='#1e293b', ha='center', zorder=4)
ax.text(97, 67.0, "(Barramentos de Distribuição 5V / GND)", fontsize=7.2, color='#64748b', ha='center', zorder=4)

ax.add_patch(patches.Rectangle((80, 56.5), 34, 2.0, facecolor='#e2e8f0', edgecolor='#cbd5e1', lw=0.8, zorder=3))
for px in np.linspace(82, 112, 13):
    for py in [53, 55, 60, 62]:
        ax.add_patch(patches.Rectangle((px-0.4, py-0.4), 0.8, 0.8, facecolor='#94a3b8', zorder=3))

ax.plot([81, 113], [46.5, 46.5], color='#ef4444', lw=1.2, zorder=3)
ax.text(81, 48.0, "+5V DC", fontsize=7.5, fontweight='bold', color='#dc2626', zorder=4)
ax.add_patch(patches.Rectangle((81, 43.8), 32, 2.0, facecolor='#fef2f2', edgecolor='#fca5a5', lw=0.8, zorder=3))

ax.plot([81, 113], [38.0, 38.0], color='#2563eb', lw=1.2, zorder=3)
ax.text(81, 41.3, "GND (Massa Comum)", fontsize=7.5, fontweight='bold', color='#1e293b', zorder=4)
ax.add_patch(patches.Rectangle((81, 38.8), 32, 2.0, facecolor='#f1f5f9', edgecolor='#94a3b8', lw=0.8, zorder=3))

rail_x_nodes = [84, 90, 96, 102, 108, 111]
for rx in rail_x_nodes:
    ax.add_patch(patches.Rectangle((rx-0.5, 44.3), 1.0, 1.0, facecolor='#475569', zorder=4))
    ax.add_patch(patches.Rectangle((rx-0.5, 39.3), 1.0, 1.0, facecolor='#475569', zorder=4))

# ==============================================================================
# 4. SENSOR RADAR HLK-LD2410B (SUPERIOR DIREITA)
# ==============================================================================
ld_pcb = patches.FancyBboxPatch((138, 84), 50, 22, boxstyle="round,pad=0.3,rounding_size=1.0",
                                facecolor='#0c4a6e', edgecolor='#082f49', linewidth=2.0, zorder=2)
ax.add_patch(ld_pcb)
ax.text(163, 107.8, "Módulo Radar 24 GHz — HLK-LD2410B", fontsize=10.5, fontweight='bold', color='#0f172a', ha='center', zorder=4)

ant_left = patches.Rectangle((150, 91.5), 7.5, 7.5, facecolor='#eab308', edgecolor='#fef08a', lw=1.0, zorder=3)
ant_right = patches.Rectangle((176, 91.5), 7.5, 7.5, facecolor='#eab308', edgecolor='#fef08a', lw=1.0, zorder=3)
radar_ic = patches.Rectangle((163.5, 91.5), 7.0, 7.0, facecolor='#0f172a', edgecolor='#64748b', lw=1.2, zorder=4)
ax.add_patch(ant_left)
ax.add_patch(ant_right)
ax.add_patch(radar_ic)
ax.plot([157.5, 163.5], [95.2, 95.2], color='#eab308', lw=2.2, zorder=3)
ax.plot([170.5, 176.0], [95.2, 95.2], color='#eab308', lw=2.2, zorder=3)
ax.text(167, 95, "24GHz\nFMCW", fontsize=5.2, fontweight='bold', color='#38bdf8', ha='center', va='center', zorder=5)
ax.text(167, 86.2, "Alimentação: 5V (>200mA)  |  Lógica OUT: 3.3V", fontsize=6.8, color='#bae6fd', ha='center', zorder=4)

ax.add_patch(patches.Rectangle((138.8, 86.2), 3.0, 17.6, facecolor='#1e293b', edgecolor='#0f172a', lw=1, zorder=3))

ld_pins = [
    ("Pino 1: OUT (3.3V Digital)", 102.0, '#10b981', True),
    ("Pino 2: UART Tx (NC)",        98.5, '#94a3b8', False),
    ("Pino 3: UART Rx (NC)",        95.0, '#94a3b8', False),
    ("Pino 4: GND (Massa)",         91.5, '#64748b', True),
    ("Pino 5: VCC (+5V DC)",        88.0, '#ef4444', True)
]
for ptext, py, pcol, active in ld_pins:
    ax.add_patch(patches.Circle((140.3, py), 0.7, facecolor='#fbbf24' if active else '#64748b', zorder=5))
    ax.text(142.8, py, ptext, fontsize=6.5, fontweight='bold' if active else 'normal',
            color='#ffffff' if active else '#94a3b8', va='center', zorder=5)

# ==============================================================================
# 5. MATRIZ DE LED 32x8 MAX7219 (CENTRO-DIREITA)
# ==============================================================================
max_pcb = patches.FancyBboxPatch((138, 48), 52, 28, boxstyle="round,pad=0.3,rounding_size=1.0",
                                 facecolor='#1d4ed8', edgecolor='#1e3a8a', linewidth=2.0, zorder=2)
ax.add_patch(max_pcb)
ax.text(164, 77.8, "Matriz de LED 32×8 — 4× Drivers MAX7219 em Cascata",
        fontsize=10.5, fontweight='bold', color='#0f172a', ha='center', zorder=4)

spectrum_heights = [
    2, 4, 6, 7, 8, 6, 5, 4,
    3, 5, 7, 6, 4, 3, 5, 6,
    4, 3, 5, 7, 5, 4, 3, 2,
    3, 4, 6, 5, 3, 2, 1, 1
]

for mod in range(4):
    mx = 144.5 + mod * 11.0
    my = 53.5
    ax.add_patch(patches.Rectangle((mx, my), 10.6, 18.0, facecolor='#111827', edgecolor='#374151', lw=0.8, zorder=3))
    ax.text(mx + 5.3, 50.2, f"Módulo {mod+1}", fontsize=6.2, fontweight='bold', color='#dbeafe', ha='center', zorder=4)
    for col in range(8):
        global_col = mod * 8 + col
        bar_h = spectrum_heights[global_col]
        for row in range(8):
            lx = mx + 0.85 + col * 1.25
            ly = my + 1.3 + row * 2.15
            is_lit = (row < bar_h)
            led_color = '#ef4444' if is_lit else '#374151'
            ax.add_patch(patches.Circle((lx, ly), 0.45, facecolor=led_color, edgecolor='none', zorder=4))

ax.add_patch(patches.Rectangle((138.8, 52.0), 3.0, 20.5, facecolor='#1e293b', edgecolor='#0f172a', lw=1, zorder=3))
max_pins = [
    ("VCC (+5V)",  70.5, '#ef4444'),
    ("GND",        66.5, '#94a3b8'),
    ("DIN (MOSI)", 62.5, '#3b82f6'),
    ("CS  (Latch)",58.5, '#f59e0b'),
    ("CLK (SCK)",  54.5, '#a855f7')
]
for mtext, my_pin, mcol in max_pins:
    ax.add_patch(patches.Circle((140.3, my_pin), 0.7, facecolor='#fbbf24', zorder=5))
    ax.text(142.3, my_pin, mtext, fontsize=5.8, fontweight='bold', color='#ffffff', va='center', zorder=5)

# ==============================================================================
# 6. CABLAGEM ORTOGONAL SEM CRUZAMENTOS (ROTEAMENTO CAD)
# ==============================================================================
# A) Alimentação da BitDogLab (IDC) para a Protoboard
draw_wire([(63, 33.5), (72, 33.5), (72, 44.8), (84, 44.8)], '#ef4444', lw=2.5,
          label="5V DC", label_pos=(72, 39.0))
draw_wire([(63, 30.8), (75, 30.8), (75, 39.8), (84, 39.8)], '#1e293b', lw=2.5,
          label="GND", label_pos=(75, 35.0))

# B) Sinal Digital OUT do Sensor LD2410B (Pino 1) -> GP4 da BitDogLab
draw_wire([(140.3, 102.0), (70, 102.0), (70, 28.1), (63, 28.1)], '#10b981', lw=2.2,
          label="PRESENÇA OUT (3.3V -> GP4)", label_pos=(105, 102.0))

# C) Alimentação da Protoboard para o Sensor LD2410B (Pinos 4 e 5)
draw_wire([(96, 44.8), (96, 88.0), (140.3, 88.0)], '#ef4444', lw=2.0,
          label="+5V Radar", label_pos=(96, 78.0))
draw_wire([(90, 39.8), (90, 91.5), (140.3, 91.5)], '#1e293b', lw=2.0,
          label="GND Radar", label_pos=(90, 82.0))

# D) Alimentação da Protoboard para a Matriz MAX7219 (VCC e GND)
draw_wire([(108, 44.8), (121, 44.8), (121, 70.5), (140.3, 70.5)], '#ef4444', lw=2.0,
          label="+5V Matriz", label_pos=(121, 65.5))
draw_wire([(111, 39.8), (124, 39.8), (124, 66.5), (140.3, 66.5)], '#1e293b', lw=2.0,
          label="GND Matriz", label_pos=(124, 61.5))

# E) Barramento SPI0 da BitDogLab (IDC) para a Matriz MAX7219
draw_wire([(63, 20.0), (127, 20.0), (127, 62.5), (140.3, 62.5)], '#3b82f6', lw=2.2,
          label="SPI0 MOSI (GP19 -> DIN)", label_pos=(98, 20.0))
draw_wire([(63, 25.4), (130, 25.4), (130, 58.5), (140.3, 58.5)], '#f59e0b', lw=2.2,
          label="SPI0 CSn (GP17 -> CS)", label_pos=(98, 25.4))
draw_wire([(63, 22.7), (133, 22.7), (133, 54.5), (140.3, 54.5)], '#a855f7', lw=2.2,
          label="SPI0 SCK (GP18 -> CLK)", label_pos=(98, 22.7))

# ==============================================================================
# 7. CARIMBO TÉCNICO E LEGENDA DE PROJETO (CANTO INFERIOR DIREITO)
# ==============================================================================
title_block = patches.Rectangle((138, 7), 55, 36, facecolor='#ffffff', edgecolor='#1e293b', linewidth=1.8, zorder=4)
ax.add_patch(title_block)

ax.add_patch(patches.Rectangle((138, 37.5), 55, 5.5, facecolor='#1e293b', edgecolor='#1e293b', zorder=5))
ax.text(165.5, 40.2, "ESPECIFICAÇÕES E LEGENDA DE CABLAGEM", fontsize=8.5, fontweight='bold', color='#ffffff', ha='center', va='center', zorder=6)

legend_entries = [
    ('#ef4444', "Alimentação +5V DC (Pino 5V IDC -> VCC Módulos)"),
    ('#1e293b', "Referência Comum GND (Terra Unificado)"),
    ('#10b981', "Sinal Digital Presença (LD2410B Pino 1 OUT -> GP4)"),
    ('#3b82f6', "Dados SPI0 MOSI (BitDogLab GP19 -> MAX7219 DIN)"),
    ('#f59e0b', "Seleção SPI0 CSn (BitDogLab GP17 -> MAX7219 CS)"),
    ('#a855f7', "Clock SPI0 SCK (BitDogLab GP18 -> MAX7219 CLK)")
]

for idx, (lcol, ltext) in enumerate(legend_entries):
    ly = 34.5 - idx * 3.6
    ax.plot([140.5, 145.5], [ly, ly], color=lcol, lw=3.0, solid_capstyle='round', zorder=6)
    ax.plot(143.0, ly, marker='o', markersize=4.5, color=lcol, markeredgecolor='#0f172a', zorder=7)
    ax.text(147.0, ly, ltext, fontsize=6.7, fontweight='bold', color='#1e293b', va='center', zorder=6)

ax.plot([138, 193], [14.5, 14.5], color='#cbd5e1', lw=1.2, zorder=5)
ax.text(140, 12.2, "• Botões Onboard (V7): BTN_A (Vermelho) = GP5 | BTN_B (Azul) = GP6 | BTN_C (Verde) = GP10.", fontsize=6.1, fontweight='bold', color='#1e293b', zorder=6)
ax.text(140, 9.8,  "• Nota: Pinos UART Tx/Rx do HLK-LD2410B desconectados (NC). Matriz ligada pela entrada DIN.", fontsize=6.1, color='#475569', zorder=6)
ax.text(140, 7.6,  "• Plataforma: RP2350 Dual-Core (Core 0: SPI/I2C/FSM | Core 1: ADC/DMA Ping-Pong + FFT 256 pt).", fontsize=6.1, fontweight='bold', color='#0f172a', zorder=6)

plt.tight_layout()
plt.savefig("esquematico_montagem.png", dpi=300, bbox_inches='tight')
plt.savefig("esquematico_montagem.pdf", bbox_inches='tight')
plt.savefig("esquematico_montagem.svg", bbox_inches='tight')
print("Diagrama atualizado gerado com sucesso!")