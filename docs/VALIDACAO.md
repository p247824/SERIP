# Validação da PCB — Projeto 3

Checklist de bancada para registrar os resultados experimentais da placa adaptadora. Os testes abaixo são propostos; não representam medições realizadas.

## 1. Inspeção antes de energizar

- [ ] Remover USB e bateria.
- [ ] Inspecionar trilhas, pads e soldas, especialmente os pinos adjacentes VCC e GND de J3.
- [ ] Com periféricos desconectados, verificar continuidade das redes ativas e ausência de curtos.
- [ ] Confirmar o espelhamento do footprint IDC e o mapeamento real do cabo flat com a BitDogLab.
- [ ] Verificar a polaridade dos conectores dos módulos.

| Rede | Pontos indicados no relatório (numeração do footprint da PCB) | Resultado |
| --- | --- | --- |
| SPI_CLK | J1.2 ↔ J2.5 | A medir |
| SPI_DIN | J1.4 ↔ J2.3 | A medir |
| SPI_CS | J1.5 ↔ J2.4 | A medir |
| RADAR_OUT | J1.8 ↔ J3.1 | A medir |
| +5 V | J1.14 ↔ J2.1 ↔ J3.5 | A medir |
| GND | J1.13 ↔ J2.2 ↔ J3.4 | A medir |
| Isolamento | +5 V ↔ GND | A medir |

## 2. Energização

- [ ] Com o radar desconectado, conferir 5 V entre VCC e GND nos conectores.
- [ ] Verificar ausência de tensão indevida nas linhas lógicas.
- [ ] Somente após confirmar a polaridade e a ausência de curto, testar o radar isoladamente com fonte de 5 V com limitação de corrente.
- [ ] Se houver aquecimento excessivo, desligar imediatamente.

## 3. Validação funcional

| Teste | Resultado esperado | Estado |
| --- | --- | --- |
| Modo reativo | Matriz responde ao espectro | A validar |
| Modo automático com presença | Linha inferior acesa | A validar |
| Modo automático sem presença | Matriz apagada | A validar |
| Modo OFF | Drivers desligados | A validar |
| Botão B | Alternância de painel e espectro | A validar |
| Botão C e joystick | Menu e ajustes | A validar |
| Radar | OUT alto com presença, baixo sem presença | Falha observada; investigar |

Registro da montagem: houve relato de aquecimento excessivo e suspeita de curto entre VCC e GND durante a soldagem. A causa e a condição do sensor ainda precisam ser comprovadas por medições.
