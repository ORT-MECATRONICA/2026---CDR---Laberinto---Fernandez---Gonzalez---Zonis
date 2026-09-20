"""
Empirical Stress Test Harness for Micromouse Median Filter & PID Controller
Simulates bahiaBlanca implementation exactly as written in C++.
"""

def actualizar_filtro_mediana(f, nuevo_valor, n=5):
    f['buffer'][f['index']] = nuevo_valor
    f['index'] = (f['index'] + 1) % n
    if f['count'] < n:
        f['count'] += 1

    temp = [f['buffer'][i] for i in range(f['count'])]

    # In-place insertion sort
    for i in range(1, f['count']):
        key = temp[i]
        j = i - 1
        while j >= 0 and temp[j] > key:
            temp[j + 1] = temp[j]
            j -= 1
        temp[j + 1] = key

    f['valorMediana'] = temp[f['count'] // 2]
    return f['valorMediana']

def prellenar_filtro(f, valor_inicial, n=5):
    f['buffer'] = [valor_inicial] * n
    f['index'] = 0
    f['count'] = n
    f['valorMediana'] = valor_inicial

def test_median_filter():
    print("=== STRESS TEST 1: MEDIAN FILTER ===")

    # Test 1.1: Sequence [0, 2000, 50, 50, 50] with primed filter (50)
    f = {'buffer': [0]*5, 'index': 0, 'count': 0, 'valorMediana': 0}
    prellenar_filtro(f, 50)
    
    seq = [0, 2000, 50, 50, 50]
    medians = []
    for val in seq:
        m = actualizar_filtro_mediana(f, val)
        medians.append(m)
    print(f"Primed (50) + input {seq} -> Output medians: {medians}")
    assert medians == [50, 50, 50, 50, 50], f"Expected all 50, got {medians}"

    # Test 1.1b: Sequence [0, 2000, 50, 50, 50] from empty cold start
    f_cold = {'buffer': [0]*5, 'index': 0, 'count': 0, 'valorMediana': 0}
    medians_cold = []
    for val in seq:
        m = actualizar_filtro_mediana(f_cold, val)
        medians_cold.append(m)
    print(f"Unprimed cold start + input {seq} -> Output medians: {medians_cold}")
    assert medians_cold == [0, 2000, 50, 50, 50], f"Cold start anomaly: {medians_cold}"

    # Test 1.4: Duplicate handling
    f_dup = {'buffer': [0]*5, 'index': 0, 'count': 0, 'valorMediana': 0}
    prellenar_filtro(f_dup, 100)
    for v in [100, 100, 100, 100, 100]:
        actualizar_filtro_mediana(f_dup, v)
    assert f_dup['valorMediana'] == 100

    # Test 1.4b: Sorting bounds & negative index simulation
    # Verify insertion sort stability and index boundaries
    f_bounds = {'buffer': [0]*5, 'index': 0, 'count': 0, 'valorMediana': 0}
    prellenar_filtro(f_bounds, 500)
    for v in [10, 2000, 10, 2000, 10]:
        actualizar_filtro_mediana(f_bounds, v)
    # Sorted: [10, 10, 10, 2000, 2000] -> median index 2 is 10
    assert f_bounds['valorMediana'] == 10
    print("Median filter tests PASSED.")

# PID Constants from config.h
KP = 0.5
KD = 0.3
UMBRAL_PARED_VALIDA_PID = 110
DISTANCIA_OBJETIVO_PARED_IZQ = 40
DISTANCIA_OBJETIVO_PARED_DER = 47
MAX_CORRECCION_PID = 50
VEL_BASE_IZQ = 65
VEL_BASE_DER = 65

class PIDController:
    def __init__(self):
        self.error_anterior = 0

    def resetear_error_anterior(self):
        self.error_anterior = 0

    def calcular_correccion(self, dist_izq, dist_der):
        pared_izq_valida = (dist_izq > 0) and (dist_izq <= UMBRAL_PARED_VALIDA_PID)
        pared_der_valida = (dist_der > 0) and (dist_der <= UMBRAL_PARED_VALIDA_PID)

        error = 0
        if pared_izq_valida and pared_der_valida:
            error = dist_der - dist_izq
        elif pared_izq_valida and not pared_der_valida:
            error = DISTANCIA_OBJETIVO_PARED_IZQ - dist_izq
        elif not pared_izq_valida and pared_der_valida:
            error = dist_der - DISTANCIA_OBJETIVO_PARED_DER
        else:
            error = 0
            self.error_anterior = 0

        correccion = int((KP * error) + (KD * (error - self.error_anterior)))
        self.error_anterior = error

        # Constrain
        if correccion > MAX_CORRECCION_PID:
            correccion = MAX_CORRECCION_PID
        elif correccion < -MAX_CORRECCION_PID:
            correccion = -MAX_CORRECCION_PID

        return correccion, error

def test_pid():
    print("\n=== STRESS TEST 2: PID CONTROLLER ===")
    pid = PIDController()

    # Test 2.1: Distance boundary values [0, 109, 110, 111, 200, 2000]
    test_dists = [0, 109, 110, 111, 200, 2000]
    print("Guard clause boundary evaluations:")
    for d in test_dists:
        valida = (d > 0) and (d <= UMBRAL_PARED_VALIDA_PID)
        print(f"  Distance {d} mm: Valid Wall = {valida}")

    # Test 2.2: Right side opening (> 110 mm) holding left wall reference
    pid.resetear_error_anterior()
    # Steady corridor: left=40, right=47
    corr, err = pid.calcular_correccion(40, 47)
    print(f"Corridor centered (izq=40, der=47): err={err}, corr={corr}")

    # Right wall opens to 200 mm (opening on right)
    corr_gap, err_gap = pid.calcular_correccion(40, 200)
    print(f"Right opening (izq=40, der=200): err={err_gap}, corr={corr_gap}")
    # Without guard, err would be 200 - 40 = 160, corr = 50 (sharp right dive)
    # With guard, err is 40 - 40 = 0. Derivative term is KD * (0 - 7) = -2.
    assert err_gap == 0, f"Expected error 0, got {err_gap}"
    assert corr_gap <= 0, f"Expected non-positive correction (steering left/straight), got {corr_gap}"

    # Test 2.3: Mathematical sign verification
    # Closer to right wall: e.g. izq=60, der=27
    pid.resetear_error_anterior()
    corr_right, err_right = pid.calcular_correccion(60, 27)
    # err = 27 - 60 = -33 < 0
    # corr < 0
    # left_motor = VEL_BASE_IZQ + corr = 65 + (-16) = 49 (slower)
    # right_motor = VEL_BASE_DER - corr = 65 - (-16) = 81 (faster)
    left_motor = VEL_BASE_IZQ + corr_right
    right_motor = VEL_BASE_DER - corr_right
    print(f"Close to right wall: err={err_right}, corr={corr_right}, left_motor={left_motor}, right_motor={right_motor}")
    assert err_right < 0, "Error must be negative when close to right wall"
    assert corr_right < 0, "Correction must be negative when close to right wall"
    assert right_motor > left_motor, "Right motor must be faster than left motor to turn LEFT"

    # Test 2.4: resetear_error_anterior prevents derivative kick
    # Simulate turn exit with stale error
    pid.error_anterior = -45
    # Without reset:
    derivative_no_reset = KD * (0 - pid.error_anterior) # +13.5 -> +13
    # With reset:
    pid.resetear_error_anterior()
    derivative_with_reset = KD * (0 - pid.error_anterior) # 0
    print(f"Derivative kick without reset: {derivative_no_reset} vs with reset: {derivative_with_reset}")
    assert derivative_with_reset == 0, "Derivative term must be 0 after reset"

if __name__ == '__main__':
    test_median_filter()
    test_pid()
