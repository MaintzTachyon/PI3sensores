import csv
import json
import os
import socket
import time
from datetime import datetime

MAC = "AA:BB:CC:DD:EE:FF"  # MAC Bluetooth del ESP32
CANAL = 1

CARPETA = "datos_mlx90640"
RUTA_RESUMEN = os.path.join(CARPETA, "mlx90640_resumen.csv")
RUTA_MATRICES = os.path.join(CARPETA, "mlx90640_matrices.csv")

HEADER_RESUMEN = [
    "fecha_hora",
    "temperatura_min_C",
    "temperatura_max_C",
    "temperatura_prom_C",
    "temperatura_centro_C",
]

HEADER_MATRICES = [
    "fecha_hora"
] + [f"pixel_{i:03d}_C" for i in range(768)]

os.makedirs(CARPETA, exist_ok=True)


def abrir_csv(ruta, encabezado):
    nuevo = not os.path.exists(ruta) or os.path.getsize(ruta) == 0
    archivo = open(ruta, "a", newline="", encoding="utf-8")
    escritor = csv.writer(archivo)

    if nuevo:
        escritor.writerow(encabezado)
        archivo.flush()

    return archivo, escritor


f_resumen, w_resumen = abrir_csv(
    RUTA_RESUMEN, HEADER_RESUMEN
)

f_matrices, w_matrices = abrir_csv(
    RUTA_MATRICES, HEADER_MATRICES
)


def procesar_linea(linea):
    try:
        datos = json.loads(linea)

        temperaturas = datos["t"]

        if not isinstance(temperaturas, list) or len(temperaturas) != 768:
            print("Fotograma inválido: se esperaban 768 temperaturas")
            return

        fecha = datetime.now().astimezone().isoformat(
            timespec="milliseconds"
        )

        minimo = float(datos["min"])
        maximo = float(datos["max"])
        promedio = float(datos["avg"])
        centro = float(datos["cen"])
        temperaturas = [float(t) for t in temperaturas]

        w_resumen.writerow([
            fecha,
            minimo,
            maximo,
            promedio,
            centro,
        ])

        w_matrices.writerow([fecha] + temperaturas)

        f_resumen.flush()
        f_matrices.flush()

        print(
            f"[{fecha}] "
            f"Min: {minimo:.1f} °C | "
            f"Max: {maximo:.1f} °C | "
            f"Prom: {promedio:.1f} °C | "
            f"Centro: {centro:.1f} °C"
        )

    except (json.JSONDecodeError, KeyError, TypeError, ValueError) as e:
        print(f"Registro inválido: {e}")


print("=== Registro térmico MLX90640 ===")
print(f"MAC: {MAC}")
print(f"Canal RFCOMM: {CANAL}")
print("Presiona Ctrl+C para detener.")

try:
    while True:
        try:
            print("Conectando al ESP32...")

            with socket.socket(
                socket.AF_BLUETOOTH,
                socket.SOCK_STREAM,
                socket.BTPROTO_RFCOMM,
            ) as s:
                s.settimeout(15)
                s.connect((MAC, CANAL))
                s.settimeout(None)

                print("Conexión Bluetooth establecida.")

                buffer = b""

                while True:
                    bloque = s.recv(4096)

                    if not bloque:
                        raise ConnectionError(
                            "El ESP32 cerró la conexión"
                        )

                    buffer += bloque

                    while b"\n" in buffer:
                        linea, buffer = buffer.split(b"\n", 1)
                        linea = linea.decode(
                            "utf-8", errors="replace"
                        ).strip()

                        if linea:
                            procesar_linea(linea)

        except (OSError, ConnectionError) as e:
            print(f"Conexión interrumpida: {e}")
            print("Reintentando en 5 segundos...")
            time.sleep(5)

except KeyboardInterrupt:
    print("\nRegistro detenido.")

finally:
    f_resumen.close()
    f_matrices.close()
    print(f"Datos guardados en: {CARPETA}/")
