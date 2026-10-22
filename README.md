# PI3sensores

Proyecto de adquisición de datos ambientales y detección de eventos usando
ESP32, sensores electrónicos, Bluetooth clásico y scripts de Python.

El proyecto tiene dos partes:

- **`Sensores/`**: programas Arduino (`.ino`) que leen los sensores conectados
  al ESP32. Cada programa muestra los datos por el monitor serial y, en la
  mayoría de los casos, también los envía por Bluetooth.
- **`Scripts/`**: programas Python que se conectan al ESP32 por Bluetooth,
  validan los datos recibidos y los guardan en archivos CSV.

## Estructura del proyecto

```text
PI3/
├── README.md
├── Scripts/
│   ├── registro_audio.py
│   ├── registro_co2.py
│   ├── registro_ky024.py
│   ├── registro_luz.py
│   ├── registro_mlx.py
│   └── registro_temperatura.py
└── Sensores/
    ├── microfono.ino
    ├── sensordeluz.ino
    ├── sensorPTH.ino
    ├── sensorCO2.ino
    ├── sensorHall.ino
    └── sensorInfrarojo.ino
```

## Descripción de los archivos

### Programas para ESP32

| Archivo | Qué hace |
|---|---|
| [`Sensores/microfono.ino`](./Sensores/microfono.ino) | Lee un micrófono INMP441 mediante I2S. Calcula nivel RMS, pico, frecuencia dominante y características tonales. Detecta posibles impactos, ruido fuerte sostenido y ruido mecánico constante. Envía un registro cada segundo por USB y Bluetooth. |
| [`Sensores/sensordeluz.ino`](./Sensores/sensordeluz.ino) | Lee la iluminancia del sensor GY-302/BH1750 por I2C. Envía la medida en lux aproximadamente cada segundo por USB y Bluetooth. |
| [`Sensores/sensorPTH.ino`](./Sensores/sensorPTH.ino) | Lee del BME280 la temperatura, humedad y presión atmosférica. Publica una línea con esos tres valores cada segundo por USB y, si hay un cliente conectado, por Bluetooth. |
| [`Sensores/sensorCO2.ino`](./Sensores/sensorCO2.ino) | Consulta un sensor de CO2 MH-Z19/MH-Z19B por UART. Valida las lecturas, considera el tiempo de calentamiento, detecta aumentos rápidos y clasifica el nivel como normal, alerta o crítico. Envía los datos cada cinco segundos. |
| [`Sensores/sensorHall.ino`](./Sensores/sensorHall.ino) | Lee el sensor magnético KY-024 mediante sus salidas analógica y digital. Envía el valor ADC, el voltaje aproximado y el estado digital cada 100 ms. |
| [`Sensores/sensorInfrarojo.ino`](./Sensores/sensorInfrarojo.ino) | Lee la cámara térmica MLX90640. Calcula temperaturas mínima, máxima, promedio y central, y envía por Bluetooth un JSON con esas estadísticas y los 768 píxeles de la imagen térmica. |

### Registradores Python

Todos los registradores:

- se conectan al ESP32 mediante Bluetooth RFCOMM;
- reciben líneas de texto separadas por saltos de línea;
- agregan la fecha y hora local de recepción;
- crean el archivo de salida si no existe;
- reintentan la conexión después de cinco segundos si se interrumpe;
- permanecen ejecutándose hasta que se presiona `Ctrl+C`.

| Archivo | Qué hace | Archivo de salida |
|---|---|---|
| [`Scripts/registro_audio.py`](./Scripts/registro_audio.py) | Guarda las métricas de audio y las anomalías detectadas por `microfono.ino`. | `datos/registro_esp32_audio.csv` |
| [`Scripts/registro_co2.py`](./Scripts/registro_co2.py) | Guarda las lecturas de CO2, el cambio aproximado de 30 segundos, el estado de calentamiento, el nivel y las anomalías. | `datos/registro_esp32_co2.csv` |
| [`Scripts/registro_ky024.py`](./Scripts/registro_ky024.py) | Valida y guarda el valor analógico, el voltaje y el estado digital del KY-024. | `Scripts/datos_ky024/registro_ky024.csv` |
| [`Scripts/registro_luz.py`](./Scripts/registro_luz.py) | Valida y guarda la iluminancia del GY-302/BH1750 en lux. | `Scripts/datos_gy302/registro_gy302.csv` |
| [`Scripts/registro_mlx.py`](./Scripts/registro_mlx.py) | Interpreta el JSON de la cámara MLX90640 y guarda un resumen y la matriz completa de 768 temperaturas. | `datos_mlx90640/mlx90640_resumen.csv` y `datos_mlx90640/mlx90640_matrices.csv` |
| [`Scripts/registro_temperatura.py`](./Scripts/registro_temperatura.py) | Valida y guarda temperatura, humedad y presión del BME280. | `datos/registro_bme280.csv` |

## Requisitos

### Hardware

- Placa ESP32 compatible con Bluetooth clásico.
- Uno o más sensores de los que aparecen en la carpeta `Sensores/`.
- Cable USB para cargar el programa y revisar el monitor serial.
- Computadora con Bluetooth para ejecutar los registradores Python.

### Software

- Arduino IDE o PlatformIO con soporte para ESP32.
- Python 3.
- Bluetooth clásico habilitado en la computadora.
- Librerías de Arduino según el programa utilizado:
  - `BH1750` para `sensordeluz.ino`;
  - `Adafruit BME280 Library` para `sensorPTH.ino`;
  - `Adafruit MLX90640` para `sensorInfrarojo.ino`.

Los scripts Python usan únicamente módulos de la biblioteca estándar, como
`socket`, `csv`, `json`, `time` y `datetime`.

## Uso

### 1. Cargar un programa en el ESP32

1. Abrir el archivo `.ino` correspondiente en Arduino IDE.
2. Seleccionar la placa y el puerto del ESP32.
3. Instalar las librerías externas requeridas por ese programa.
4. Compilar y cargar el programa.
5. Abrir el monitor serial a `115200 baudios`.
6. Anotar la dirección MAC Bluetooth que muestra el ESP32.

Cada archivo `.ino` representa un programa independiente. No se deben cargar
varios de ellos al mismo ESP32 al mismo tiempo.

### 2. Configurar la dirección Bluetooth

Los scripts contienen una dirección de ejemplo:

```python
AA:BB:CC:DD:EE:FF
```

Antes de ejecutar un registrador, reemplazarla por la MAC real del ESP32.
También se puede revisar el canal Bluetooth configurado; normalmente es el
canal `1`.

### 3. Ejecutar un registrador

Desde la carpeta raíz del proyecto, ejecutar el script correspondiente. Por
ejemplo:

```bash
python3 Scripts/registro_temperatura.py
```

Otros ejemplos:

```bash
python3 Scripts/registro_audio.py
python3 Scripts/registro_co2.py
python3 Scripts/registro_ky024.py
python3 Scripts/registro_luz.py
python3 Scripts/registro_mlx.py
```

El script mostrará los datos recibidos en la terminal y los guardará
automáticamente en su archivo CSV. Para detenerlo, presionar `Ctrl+C`.

## Formatos de datos

Los sensores envían datos en formatos sencillos:

- La mayoría envía una línea CSV, por ejemplo:
  `milisegundos,temperatura,humedad,presion`.
- `sensorInfrarojo.ino` envía una línea JSON con un resumen térmico y una lista
  de 768 temperaturas.
- Los registradores agregan `fecha_hora` para indicar cuándo recibió los datos
  la computadora.

## Notas importantes

- Las direcciones MAC incluidas en los scripts son valores de ejemplo y deben
  cambiarse antes de conectar el hardware.
- `registro_co2.py` contiene una MAC de ejemplo con un carácter adicional al
  final; debe reemplazarse por la MAC real completa.
- Los nombres de Bluetooth anunciados por los ESP32 son:
  `ESP32_AUDIO`, `GY302_ESP32`, `BME280_ESP32`, `ESP32_CO2`,
  `KY024_ESP32` y `MLX90640_Termica`.
- Los directorios de datos se crean automáticamente cuando se ejecuta cada
  registrador.
- No conviene ejecutar dos registradores contra el mismo ESP32 al mismo
  tiempo, salvo que el programa del ESP32 y el hardware estén preparados para
  aceptar varias conexiones.