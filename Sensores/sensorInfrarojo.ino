#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MLX90640.h>
#include <BluetoothSerial.h>
#include <math.h>


#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error "Bluetooth clasico no esta habilitado en esta placa"
#endif


#if !defined(CONFIG_BT_SPP_ENABLED)
#error "Bluetooth SPP no es compatible con esta placa"
#endif


#define SDA_PIN 21
#define SCL_PIN 22


Adafruit_MLX90640 mlx;
BluetoothSerial SerialBT;


float frame[32 * 24];


float tmin, tmax, tprom, tcentro;


bool leerCamara() {
    if (mlx.getFrame(frame) != 0) {
        Serial.println("Error al leer el MLX90640");
        return false;
    }


    tmin = 1000.0;
    tmax = -1000.0;
    float suma = 0.0;
    int n = 0;


    for (int i = 0; i < 768; i++) {
        if (!isfinite(frame[i])) {
            continue;
        }


        if (frame[i] < tmin) tmin = frame[i];
        if (frame[i] > tmax) tmax = frame[i];


        suma += frame[i];
        n++;
    }


    if (n == 0) {
        Serial.println("No hay temperaturas validas");
        return false;
    }


    tprom = suma / n;


    // Sustituir valores invalidos para evitar JSON no valido.
    for (int i = 0; i < 768; i++) {
        if (!isfinite(frame[i])) {
            frame[i] = tprom;
        }
    }


    tcentro = frame[12 * 32 + 16];


    return true;
}


void enviarDatos() {
    String json;
    json.reserve(6500);


    json = "{\"min\":";
    json += String(tmin, 1);
    json += ",\"max\":";
    json += String(tmax, 1);
    json += ",\"avg\":";
    json += String(tprom, 1);
    json += ",\"cen\":";
    json += String(tcentro, 1);
    json += ",\"t\":[";


    for (int i = 0; i < 768; i++) {
        json += String(frame[i], 1);


        if (i < 767) {
            json += ',';
        }
    }


    json += "]}";


    SerialBT.println(json);
}


void setup() {
    Serial.begin(115200);
    delay(1000);


    Serial.println("=== Camara termica MLX90640 ===");


    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(100000);


    if (!mlx.begin(MLX90640_I2CADDR_DEFAULT, &Wire)) {
        Serial.println("MLX90640 NO detectado");
        while (true) {
            delay(1000);
        }
    }


    mlx.setMode(MLX90640_CHESS);
    mlx.setResolution(MLX90640_ADC_16BIT);
    mlx.setRefreshRate(MLX90640_2_HZ);


    if (!SerialBT.begin("MLX90640_Termica")) {
        Serial.println("Error al iniciar Bluetooth");
        while (true) {
            delay(1000);
        }
    }


    Serial.println("Bluetooth iniciado");
    Serial.println("Nombre: MLX90640_Termica");
    Serial.println("Esperando conexion Bluetooth...");
}


void loop() {
    if (!leerCamara()) {
        delay(100);
        return;
    }


    Serial.printf(
        "Min: %.1f C | Max: %.1f C | Prom: %.1f C | Centro: %.1f C\n",
        tmin, tmax, tprom, tcentro
    );


    if (SerialBT.hasClient()) {
        enviarDatos();
    }
}