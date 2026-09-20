# Sensor Subsystem & Median Filter (R1) Analysis Report

See complete detailed handoff at:
`c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\.agents\sensor_specialist_1\handoff.md`

## Summary of Findings
1. **Window Size N**: Optimal is `N = 5` (odd, ~66ms group delay, eliminates optical spikes without sluggishness). Defined in `config.h` as `FILTRO_MEDIANA_N 5`.
2. **Buffer & Sorting**: Circular buffer per sensor, inserted ONLY when `(readReg(RESULT_INTERRUPT_STATUS) & 0x07) != 0`. Sorted on stack with insertion sort (deterministic, <0.2 µs on ESP32, zero heap allocations).
3. **Startup Handling**: Pre-fill buffer with initial valid measurement in `inicializacionSensoresDist()` to avoid zero transients.
4. **Consumers**: `right_hand()` (`rightHand.cpp`) and `calcularCorreccion()` (`PID.cpp`) consume `actualizarSensado()`. Encapsulating filter inside `actualizarSensado()` guarantees filtered data with zero API changes.
5. **New Constants for config.h**:
   - `FILTRO_MEDIANA_N` (5)
   - `DISTANCIA_MAX_VALIDA` (2000)
   - `TIMEOUT_SENSOR_MS` (500)
   - `DELAY_BOOT_SENSOR_MS` (10)
6. **Critical Hardware Conflict Found**:
   - `config.h` assigns GPIO 18 to BOTH `ENC_B_2` (encoder B channel 2) and `xshutPinIzq` (Left sensor XSHUT). Must be resolved!
