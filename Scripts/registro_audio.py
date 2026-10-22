import csv
import os
import socket
import time
from datetime import datetime

MAC = "AA:BB:CC:DD:EE:FF"        # MAC de la ESP32 del micrófono
RUTA = "datos/registro_esp32_audio.csv"
ENCABEZADO = ["fecha_hora", "ms", "audio_rms_dbfs", "audio_pico_dbfs",
              "audio_base_dbfs", "audio_freq_hz", "audio_tonal",
              "bloques_fuertes", "calibrando", "anomalias"]

os.makedirs("datos", exist_ok=True)
nuevo = not os.path.exists(RUTA)
f = open(RUTA, "a", newline="")
w = csv.writer(f)
if nuevo:
    w.writerow(ENCABEZADO)
    f.flush()

print("Registrando audio. Ctrl+C para detener.")
try:
    while True:
        try:
            print(f"Conectando a {MAC}...")
            s = socket.socket(socket.AF_BLUETOOTH, socket.SOCK_STREAM,
                              socket.BTPROTO_RFCOMM)
            s.settimeout(10)
            s.connect((MAC, 1))
            s.settimeout(None)
            print("Conectado")

            buffer = ""
            while True:
                datos = s.recv(1024)
                if not datos:
                    raise ConnectionError("conexión cerrada")
                buffer += datos.decode(errors="ignore")
                while "\n" in buffer:
                    linea, buffer = buffer.split("\n", 1)
                    linea = linea.strip()
                    if not linea or linea.startswith("#") or linea.startswith("ms"):
                        continue
                    campos = linea.split(",")
                    if len(campos) != 9:
                        continue
                    fecha = datetime.now().isoformat(timespec="milliseconds")
                    w.writerow([fecha] + campos)
                    f.flush()
                    print(linea)
        except (OSError, ConnectionError) as e:
            print(f"Error: {e}. Reintentando en 5 s...")
            time.sleep(5)
except KeyboardInterrupt:
    print("Detenido.")
finally:
    f.close()
