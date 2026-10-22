#include <Arduino.h>
#include <BluetoothSerial.h>
#include <esp_bt_device.h>

BluetoothSerial SerialBT;

const char* NOMBRE_BT = "KY024_ESP32";

const int PIN_AO = 34;
const int PIN_DO = 27;

const unsigned long INTERVALO_MS = 100;
unsigned long ultimaLectura = 0;

void setup() {
  Serial.begin(115200);

  pinMode(PIN_DO, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_AO, ADC_11db);

  SerialBT.begin(NOMBRE_BT);
  delay(1000);

  const uint8_t* mac = esp_bt_dev_get_address();

  Serial.println("# Sensor KY-024");
  Serial.println("# Registro por Bluetooth Classic");

  if (mac != nullptr) {
    Serial.printf(
      "# MAC Bluetooth: %02X:%02X:%02X:%02X:%02X:%02X\n",
      mac[0], mac[1], mac[2],
      mac[3], mac[4], mac[5]
    );
  }

  Serial.println(
    "ms,valor_analogico,voltaje_mV,estado_digital"
  );

  SerialBT.println(
    "ms,valor_analogico,voltaje_mV,estado_digital"
  );
}

void loop() {
  if (millis() - ultimaLectura < INTERVALO_MS) {
    return;
  }

  ultimaLectura = millis();

  int valorAnalogico = analogRead(PIN_AO);
  uint32_t voltajeMv = analogReadMilliVolts(PIN_AO);
  int estadoDigital = digitalRead(PIN_DO);

  char linea[100];

  snprintf(
    linea,
    sizeof(linea),
    "%lu,%d,%lu,%d",
    millis(),
    valorAnalogico,
    (unsigned long)voltajeMv,
    estadoDigital
  );

  Serial.println(linea);
  SerialBT.println(linea);
}