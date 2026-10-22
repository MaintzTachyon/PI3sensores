import csv
import os
import socket
import time
from datetime import datetime

MAC = "AA:BB:CC:DD:EE:FF"  # MAC Bluetooth del ESP32
CANAL = 1

CARPETA = "datos"
RUTA_CSV = os.path.join(CARPETA, "registro_bme280.csv")

ENCABEZADO = [
    "fecha_hora",
    "ms",
    "temperatura_C",
    "humedad_pct",
    "presion_hPa",
]

os.makedirs(CARPETA, exist_ok=True)

nuevo = (
    not os.path.exists(RUTA_CSV)
    or os.path.getsize(RUTA_CSV) == 0
)

archivo = open(
    RUTA_CSV,
    "a",
    newline="",
    encoding="utf-8",
)

escritor = csv.writer(archivo)

if nuevo:
    escritor.writerow(ENCABEZADO)
    archivo.flush()


def procesar_linea(linea):
    campos = linea.strip().split(",")

    if len(campos) != 4:
        print(f"Registro inválido: {linea}")
        return

    try:
        ms = int(campos[0])
        temperatura = float(campos[1])
        humedad = float(campos[2])
        presion = float(campos[3])

        fecha = datetime.now().astimezone().isoformat(
            timespec="milliseconds"
        )

        escritor.writerow([
            fecha,
            ms,
            temperatura,
            humedad,
            presion,
        ])

        archivo.flush()

        print(
            f"{fecha} | "
            f"Temp: {temperatura:.2f} °C | "
            f"Humedad: {humedad:.2f} % | "
            f"Presión: {presion:.2f} hPa"
        )

    except ValueError:
        print(f"Datos no numéricos: {linea}")


print("=== Registro BME280 por Bluetooth ===")
print(f"MAC: {MAC}")
print(f"Archivo: {RUTA_CSV}")
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

                print("Bluetooth conectado.")

                buffer = b""

                while True:
                    bloque = s.recv(1024)

                    if not bloque:
                        raise ConnectionError(
                            "Conexión Bluetooth cerrada"
                        )

                    buffer += bloque

                    while b"\n" in buffer:
                        linea, buffer = buffer.split(b"\n", 1)

                        texto = linea.decode(
                            "utf-8",
                            errors="replace",
                        ).strip()

                        if texto:
                            procesar_linea(texto)

        except (OSError, ConnectionError) as error:
            print(f"Conexión interrumpida: {error}")
            print("Reintentando en 5 segundos...")
            time.sleep(5)

except KeyboardInterrupt:
    print("\nRegistro detenido.")

finally:
    archivo.close()
    print(f"Datos guardados en {RUTA_CSV}")
