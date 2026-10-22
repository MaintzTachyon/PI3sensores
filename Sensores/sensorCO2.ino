#include <Arduino.h>
#include <BluetoothSerial.h>
#include <esp_bt_device.h>

BluetoothSerial SerialBT;

// ---------- PINES (UART2) ----------
#define RX2_PIN 16   // al TX del sensor
#define TX2_PIN 17   // al RX del sensor

// ---------- UMBRALES ----------
const int  CO2_ALERTA_PPM  = 1000;
const int  CO2_CRITICO_PPM = 1500;
const int  CO2_HISTERESIS  = 100;
const int  CONFIRMACIONES  = 3;
const int  CO2_MIN_VALIDO  = 300;
const int  CO2_MAX_VALIDO  = 10000;
const int  PICO_SUBIDA_PPM = 300;
const int  VENTANA         = 6;
const int  FALLOS_PARA_ALARMA = 5;
const unsigned long PERIODO_MS       = 5000;
const unsigned long CALENTAMIENTO_MS = 180000;

// ---------- ESTADO ----------
byte cmdLeer[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
unsigned long tUltimo = 0;
int nivel = 0;
int candPrev = -1;
int contCand = 0;
int fallosSeguidos = 0;
int hist[6];
int histIdx = 0, histN = 0;

void agregar(char *dst, size_t tam, const char *txt) {
  if (dst[0]) strncat(dst, "|", tam - strlen(dst) - 1);
  strncat(dst, txt, tam - strlen(dst) - 1);
}

int leerCO2() {
  byte r[9];
  while (Serial2.available()) Serial2.read();
  Serial2.write(cmdLeer, 9);
  if (Serial2.readBytes(r, 9) != 9) return -1;
  if (r[0] != 0xFF || r[1] != 0x86) return -2;
  byte sum = 0;
  for (int i = 1; i < 8; i++) sum += r[i];
  if ((byte)(0xFF - sum + 1) != r[8]) return -2;
  return r[2] * 256 + r[3];
}

int calcularNivel(int ppm, int actual) {
  if (ppm >= CO2_CRITICO_PPM) return 2;
  if (ppm >= CO2_ALERTA_PPM)  return (actual == 2 && ppm >= CO2_CRITICO_PPM - CO2_HISTERESIS) ? 2 : 1;
  if (actual >= 1 && ppm >= CO2_ALERTA_PPM - CO2_HISTERESIS) return 1;
  return 0;
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RX2_PIN, TX2_PIN);
  Serial2.setTimeout(1000);

  SerialBT.begin("ESP32_CO2");          // nombre Bluetooth que verá la laptop
  delay(1000);

  const uint8_t* m = esp_bt_dev_get_address();
  Serial.println("# MODULO 1 - CO2 (MH-Z1911A) por Bluetooth");
  Serial.printf("# MAC Bluetooth: %02X:%02X:%02X:%02X:%02X:%02X\n",
                m[0], m[1], m[2], m[3], m[4], m[5]);
  Serial.println("# Durante los primeros 3 min el sensor calienta: no se evaluan anomalias.");
  Serial.println("ms,co2_ppm,delta_30s,calentando,nivel,anomalias");
}

void loop() {
  if (millis() - tUltimo < PERIODO_MS) return;
  tUltimo = millis();

  int  ppm        = leerCO2();
  bool valido     = (ppm >= CO2_MIN_VALIDO && ppm <= CO2_MAX_VALIDO);
  bool calentando = millis() < CALENTAMIENTO_MS;
  char anomalias[80] = "";
  const char *nivelTxt = "SIN_DATO";
  int delta30 = 0;

  if (valido) fallosSeguidos = 0; else fallosSeguidos++;
  if (fallosSeguidos >= FALLOS_PARA_ALARMA) agregar(anomalias, sizeof(anomalias), "FALLO_SENSOR");

  if (calentando) {
    nivelTxt = "CALENTANDO";
  } else if (valido) {
    if (histN == VENTANA) delta30 = ppm - hist[histIdx];
    hist[histIdx] = ppm;
    histIdx = (histIdx + 1) % VENTANA;
    if (histN < VENTANA) histN++;
    if (delta30 >= PICO_SUBIDA_PPM) agregar(anomalias, sizeof(anomalias), "CO2_PICO_RAPIDO");

    int cand = calcularNivel(ppm, nivel);
    if (cand == nivel) {
      candPrev = -1; contCand = 0;
    } else {
      if (cand == candPrev) contCand++; else { candPrev = cand; contCand = 1; }
      if (contCand >= CONFIRMACIONES) { nivel = cand; candPrev = -1; contCand = 0; }
    }
    if (nivel == 1) { nivelTxt = "ALERTA";  agregar(anomalias, sizeof(anomalias), "CO2_ALERTA_VENTILACION"); }
    else if (nivel == 2) { nivelTxt = "CRITICO"; agregar(anomalias, sizeof(anomalias), "CO2_CRITICO"); }
    else nivelTxt = "NORMAL";
  }
  if (!anomalias[0]) strcpy(anomalias, "NINGUNA");

  // Misma línea CSV por USB (monitor serie) y por Bluetooth
  Serial.printf("%lu,%d,%d,%d,%s,%s\n",
                millis(), ppm, delta30, calentando ? 1 : 0, nivelTxt, anomalias);
  SerialBT.printf("%lu,%d,%d,%d,%s,%s\n",
                  millis(), ppm, delta30, calentando ? 1 : 0, nivelTxt, anomalias);
}