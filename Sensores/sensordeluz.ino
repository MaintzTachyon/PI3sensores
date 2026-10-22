#include <Arduino.h>
#include <Wire.h>
#include <BH1750.h>
#include <BluetoothSerial.h>
#include <esp_bt_device.h>

BluetoothSerial SerialBT;
BH1750 sensorLuz;

const char* NOMBRE_BT = "GY302_ESP32";

const int PIN_SDA = 21;
const int PIN_SCL = 22;

const unsigned long INTERVALO_MS = 1000;
unsigned long ultimaLectura = 0;

void setup() {
  Serial.begin(115200);

  Wire.begin(PIN_SDA, PIN_SCL);

  bool sensorOK = sensorLuz.begin(
    BH1750::CONTINUOUS_HIGH_RES_MODE,
    0x23,
    &Wire
  );

  if (!sensorOK) {
    sensorOK = sensorLuz.begin(
      BH1750::CONTINUOUS_HIGH_RES_MODE,
      0x5C,
      &Wire
    );
  }

  SerialBT.begin(NOMBRE_BT);
  delay(1000);

  const uint8_t* mac = esp_bt_dev_get_address();

  Serial.println("# Sensor GY-302 BH1750");

  if (mac != nullptr) {
    Serial.printf(
      "# MAC Bluetooth: %02X:%02X:%02X:%02X:%02X:%02X\n",
      mac[0], mac[1], mac[2],
      mac[3], mac[4], mac[5]
    );
  }

  if (!sensorOK) {
    Serial.println("# ERROR: no se detecto el BH1750.");
    SerialBT.println("# ERROR: no se detecto el BH1750.");

    while (true) {
      delay(1000);
    }
  }

  Serial.println("ms,iluminancia_lux");
  SerialBT.println("ms,iluminancia_lux");
}

void loop() {
  if (millis() - ultimaLectura < INTERVALO_MS) {
    return;
  }

  ultimaLectura = millis();

  float lux = sensorLuz.readLightLevel();

  if (!isfinite(lux) || lux < 0) {
    Serial.println("# ERROR: lectura de luz invalida.");
    return;
  }

  char linea[64];

  snprintf(
    linea,
    sizeof(linea),
    "%lu,%.2f",
    millis(),
    lux
  );

  Serial.println(linea);
  SerialBT.println(linea);
}