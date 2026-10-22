import csv
import socket
import time
from datetime import datetime
from pathlib import Path

MAC_ESP32 = "AA:BB:CC:DD:EE:FF"
CANAL_BT = 1

CARPETA = Path(__file__).resolve().parent / "datos_gy302"
ARCHIVO_CSV = CARPETA / "registro_gy302.csv"

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
    "iluminancia_lux"
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

    if len(datos) != 2:
        return

    try:
        ms = int(datos[0])
        lux = float(datos[1])

        if not (lux >= 0 and lux < float("inf")):
            return

    except ValueError:
        return

    fecha_hora = datetime.now().astimezone().isoformat(
        timespec="milliseconds"
    )

    escritor.writerow({
        "fecha_hora": fecha_hora,
        "ms": ms,
        "iluminancia_lux": lux
    })

    archivo.flush()

    print(
        f"{fecha_hora} | Iluminancia: {lux:.2f} lux"
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
                print("Conexion Bluetooth establecida.")

                buffer = b""

                while True:
                    bloque = bluetooth.recv(1024)

                    if not bloque:
                        raise ConnectionError(
                            "El ESP32 cerro la conexion."
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

        except (OSError, ConnectionError) as error:
            print(f"Conexion interrumpida: {error}")
            print("Nuevo intento en 5 segundos...")
            time.sleep(5)


try:
    recibir_datos()
finally:
    archivo.close()