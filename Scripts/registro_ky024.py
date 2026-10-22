import csv
import socket
import time
from datetime import datetime
from pathlib import Path

# Configuración Bluetooth
MAC_ESP32 = "AA:BB:CC:DD:EE:FF"
CANAL_BT = 1

# Archivo de registro
CARPETA = Path(__file__).resolve().parent / "datos_ky024"
ARCHIVO_CSV = CARPETA / "registro_ky024.csv"

CARPETA.mkdir(parents=True, exist_ok=True)

nuevo_archivo = (
    not ARCHIVO_CSV.exists()
    or ARCHIVO_CSV.stat().st_size == 0
)

archivo = open(
    ARCHIVO_CSV,
    "a",
    newline="",
    encoding="utf-8"
)

columnas = [
    "fecha_hora",
    "ms",
    "valor_analogico",
    "voltaje_mV",
    "estado_digital"
]

escritor = csv.DictWriter(
    archivo,
    fieldnames=columnas
)

if nuevo_archivo:
    escritor.writeheader()
    archivo.flush()


def registrar_linea(linea):
    datos = linea.strip().split(",")

    if len(datos) != 4:
        return

    try:
        ms = int(datos[0])
        valor_analogico = int(datos[1])
        voltaje_mv = int(datos[2])
        estado_digital = int(datos[3])

        if estado_digital not in (0, 1):
            return

    except ValueError:
        # Ignora encabezados y líneas no válidas.
        return

    fecha_hora = datetime.now().astimezone().isoformat(
        timespec="milliseconds"
    )

    escritor.writerow({
        "fecha_hora": fecha_hora,
        "ms": ms,
        "valor_analogico": valor_analogico,
        "voltaje_mV": voltaje_mv,
        "estado_digital": estado_digital
    })

    archivo.flush()

    print(
        f"{fecha_hora} | ADC: {valor_analogico} | "
        f"mV: {voltaje_mv} | Digital: {estado_digital}"
    )


def recibir_datos():
    while True:
        try:
            with socket.socket(
                socket.AF_BLUETOOTH,
                socket.SOCK_STREAM,
                socket.BTPROTO_RFCOMM
            ) as bluetooth:

                print(f"Conectando con {MAC_ESP32}...")
                bluetooth.connect((MAC_ESP32, CANAL_BT))
                print("Conexión Bluetooth establecida.")

                buffer = b""

                while True:
                    bloque = bluetooth.recv(1024)

                    if not bloque:
                        raise ConnectionError(
                            "El ESP32 cerró la conexión."
                        )

                    buffer += bloque

                    while b"\n" in buffer:
                        linea, buffer = buffer.split(b"\n", 1)

                        texto = linea.decode(
                            "utf-8",
                            errors="ignore"
                        ).strip()

                        if texto:
                            registrar_linea(texto)

        except KeyboardInterrupt:
            print("\nRegistro detenido.")
            break

        except OSError as error:
            print(f"Error Bluetooth: {error}")
            print("Nuevo intento en 5 segundos...")
            time.sleep(5)

        except ConnectionError as error:
            print(error)
            print("Nuevo intento en 5 segundos...")
            time.sleep(5)


try:
    recibir_datos()
finally:
    archivo.close()