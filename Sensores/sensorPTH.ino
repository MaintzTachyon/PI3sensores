#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <BluetoothSerial.h>


#define SDA_PIN 21
#define SCL_PIN 22


const unsigned long PERIODO_MS = 1000;


Adafruit_BME280 bme;
BluetoothSerial SerialBT;


unsigned long ultimaLectura = 0;
bool sensorDisponible = false;


void setup() {
    Serial.begin(115200);
    delay(1000);


    Serial.println("\n=== Sensor GY-BME280 ===");


    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(100000);


    sensorDisponible = bme.begin(0x76, &Wire);


    if (!sensorDisponible) {
        sensorDisponible = bme.begin(0x77, &Wire);
    }


    if (!sensorDisponible) {
        Serial.println("ERROR: BME280 no detectado.");
        Serial.println("Revisa las conexiones I2C.");


        while (true) {
            delay(1000);
        }
    }


    if (!SerialBT.begin("BME280_ESP32")) {
        Serial.println("ERROR al iniciar Bluetooth.");


        while (true) {
            delay(1000);
        }
    }


    Serial.println("BME280 detectado.");
    Serial.println("Bluetooth: BME280_ESP32");
    Serial.println("Esperando conexion Bluetooth...");
}


void loop() {
    unsigned long ahora = millis();


    if (ahora - ultimaLectura >= PERIODO_MS) {
        ultimaLectura = ahora;


        float temperatura = bme.readTemperature();
        float humedad = bme.readHumidity();
        float presion = bme.readPressure() / 100.0F;


        if (isnan(temperatura) ||
            isnan(humedad) ||
            isnan(presion)) {
            Serial.println("Error leyendo el BME280.");
            return;
        }


        // Formato: ms,temperatura_C,humedad_pct,presion_hPa
        char registro[100];


        snprintf(
            registro,
            sizeof(registro),
            "%lu,%.2f,%.2f,%.2f",
            ahora,
            temperatura,
            humedad,
            presion
        );


        Serial.println(registro);


        if (SerialBT.hasClient()) {
            SerialBT.println(registro);
        }
    }
}