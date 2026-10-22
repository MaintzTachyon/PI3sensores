// INMP441 + ESP32: captura y análisis de audio por Bluetooth.
// Mantener silencio durante los primeros 5 segundos.

#include <Arduino.h>
#include "driver/i2s.h"
#include <BluetoothSerial.h>
#include <esp_bt_device.h>

BluetoothSerial SerialBT;
const char* NOMBRE_BT = "ESP32_AUDIO";

// Pines I2S
#define PIN_SCK 26
#define PIN_WS  25
#define PIN_SD  33
#define I2S_PORT I2S_NUM_0

// Parámetros de audio
const int FS = 16000;
const int N = 512;
const float SALTO_DB = 20.0;
const float PISO_FUERTE_DB = -45.0;
const float CREST_IMPACTO_DB = 15.0;
const int MAX_BLOQ_IMPACTO = 8;
const int BLOQ_SOSTENIDO = 16;
const float TONAL_MIN = 0.35;
const float PISO_TONAL_DB = -55.0;
const int TONAL_VENTANAS = 30;
const int BLOQUES_ARRANQUE = 150;

// Variables de análisis
int32_t bufI2S[N];
float bloque[N], bloqueMax[N], re[N], im[N], pw[N / 2];
float baseDb = -60;
bool baseInit = false;
long bloquesTotal = 0;

unsigned long tVentana = 0, tUltValido = 0, tFalloPrint = 0;
int bloquesVent = 0, fuertes = 0, sostenidoVent = 0, tonalCont = 0;
float potAcum = 0, picoVent = -200, maxRms = -200;
float maxCrest = 0, freqPrev = 0;

// Añade una etiqueta a la lista de anomalías.
void agregar(char *dst, size_t tam, const char *txt) {
  if (dst[0]) strncat(dst, "|", tam - strlen(dst) - 1);
  strncat(dst, txt, tam - strlen(dst) - 1);
}

// Envía datos por USB y Bluetooth.
void emitir(const char* linea) {
  Serial.println(linea);
  SerialBT.println(linea);
}

// Calcula la FFT.
void fft(float *r, float *i_, int n) {
  for (int i = 1, j = 0; i < n; i++) {
    int bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;

    if (i < j) {
      float t = r[i];
      r[i] = r[j];
      r[j] = t;

      t = i_[i];
      i_[i] = i_[j];
      i_[j] = t;
    }
  }

  for (int len = 2; len <= n; len <<= 1) {
    float ang = -2.0f * PI / len;
    float wr = cosf(ang), wi = sinf(ang);

    for (int i = 0; i < n; i += len) {
      float cr = 1, ci = 0;

      for (int k = 0; k < len / 2; k++) {
        int a = i + k, b = i + k + len / 2;
        float tr = r[b] * cr - i_[b] * ci;
        float ti = r[b] * ci + i_[b] * cr;

        r[b] = r[a] - tr;
        i_[b] = i_[a] - ti;
        r[a] += tr;
        i_[a] += ti;

        float ncr = cr * wr - ci * wi;
        ci = cr * wi + ci * wr;
        cr = ncr;
      }
    }
  }
}

// Informa si no se reciben muestras de audio.
void avisarFallo() {
  if (millis() - tUltValido > 3000 &&
      millis() - tFalloPrint >= 1000) {
    tFalloPrint = millis();

    char linea[80];
    snprintf(linea, sizeof(linea),
             "%lu,nan,nan,nan,nan,nan,0,0,FALLO_AUDIO",
             millis());

    emitir(linea);
  }
}

void setup() {
  Serial.begin(115200);
  SerialBT.begin(NOMBRE_BT);
  delay(1500);

  const uint8_t* m = esp_bt_dev_get_address();

  Serial.println("# Microfono INMP441");
  Serial.printf("# MAC Bluetooth: %02X:%02X:%02X:%02X:%02X:%02X\n",
                m[0], m[1], m[2], m[3], m[4], m[5]);

  // Configuración de entrada I2S.
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate = FS;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 8;
  cfg.dma_buf_len = 256;
  cfg.use_apll = false;

  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
  pins.bck_io_num = PIN_SCK;
  pins.ws_io_num = PIN_WS;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = PIN_SD;

  if (i2s_driver_install(I2S_PORT, &cfg, 0, NULL) != ESP_OK ||
      i2s_set_pin(I2S_PORT, &pins) != ESP_OK) {
    Serial.println("# ERROR: no se pudo iniciar I2S.");
    while (1) delay(1000);
  }

  tVentana = tUltValido = millis();

  Serial.println("# Calibrando ruido de fondo: mantener silencio 5 s.");
  Serial.println(
    "ms,audio_rms_dbfs,audio_pico_dbfs,audio_base_dbfs,"
    "audio_freq_hz,audio_tonal,bloques_fuertes,calibrando,anomalias"
  );
}

void loop() {
  // Lee las muestras del micrófono.
  size_t bytes = 0;
  i2s_read(I2S_PORT, bufI2S, sizeof(bufI2S), &bytes,
           pdMS_TO_TICKS(200));

  if ((int)(bytes / 4) < N) {
    avisarFallo();
    return;
  }

  tUltValido = millis();

  // Calcula RMS y amplitud máxima.
  double s = 0;

  for (int i = 0; i < N; i++) {
    bloque[i] = (bufI2S[i] >> 8) / 8388608.0f;
    s += bloque[i];
  }

  float media = s / N, pot = 0, pico = 0;

  for (int i = 0; i < N; i++) {
    bloque[i] -= media;
    pot += bloque[i] * bloque[i];

    if (fabsf(bloque[i]) > pico)
      pico = fabsf(bloque[i]);
  }

  float rms = sqrtf(pot / N);
  float rmsDb = rms > 1e-7f ? 20.0f * log10f(rms) : -120.0f;
  float picoDb = pico > 1e-7f ? 20.0f * log10f(pico) : -120.0f;
  float crest = picoDb - rmsDb;

  // Actualiza el ruido de fondo y detecta sonidos fuertes.
  if (!baseInit) {
    baseDb = rmsDb;
    baseInit = true;
  }

  bool fuerte = (rmsDb > baseDb + SALTO_DB) &&
                (rmsDb > PISO_FUERTE_DB);

  if (!fuerte)
    baseDb += 0.01f * (rmsDb - baseDb);

  bloquesTotal++;
  bloquesVent++;
  potAcum += rms * rms;

  if (picoDb > picoVent)
    picoVent = picoDb;

  if (fuerte) {
    fuertes++;
    if (crest > maxCrest) maxCrest = crest;
  }

  if (rmsDb > maxRms) {
    maxRms = rmsDb;
    memcpy(bloqueMax, bloque, sizeof(bloque));
  }

  // Procesa y reporta cada segundo.
  if (millis() - tVentana < 1000) return;
  tVentana = millis();

  float rmsVentDb = 10.0f * log10f(potAcum / bloquesVent + 1e-14f);
  bool listo = bloquesTotal > BLOQUES_ARRANQUE;
  char an[120] = "";

  // Detecta ruidos de impacto y sonidos sostenidos.
  if (listo) {
    if (fuertes >= 1 && fuertes <= MAX_BLOQ_IMPACTO &&
        maxCrest >= CREST_IMPACTO_DB)
      agregar(an, sizeof(an), "RUIDO_IMPACTO");

    if (fuertes >= BLOQ_SOSTENIDO) {
      agregar(an, sizeof(an), "RUIDO_FUERTE_SOSTENIDO");
      sostenidoVent++;
    } else {
      sostenidoVent = 0;
    }

    if (sostenidoVent > 30) {
      baseDb = rmsVentDb;
      sostenidoVent = 0;
    }
  }

  // Prepara la señal para analizar sus frecuencias.
  for (int i = 0; i < N; i++) {
    re[i] = bloqueMax[i] *
            (0.5f - 0.5f * cosf(2.0f * PI * i / (N - 1)));
    im[i] = 0;
  }

  fft(re, im, N);

  float total = 0, mx = 0;
  int kmax = 2;

  for (int k = 2; k < N / 2 - 1; k++) {
    pw[k] = re[k] * re[k] + im[k] * im[k];
    total += pw[k];

    if (pw[k] > mx) {
      mx = pw[k];
      kmax = k;
    }
  }

  float tonal = total > 0
    ? (pw[kmax - 1] + pw[kmax] + pw[kmax + 1]) / total
    : 0;

  float freq = kmax * (float)FS / N;

  // Detecta sonidos tonales persistentes.
  if (listo && tonal >= TONAL_MIN &&
      rmsVentDb > PISO_TONAL_DB &&
      fabsf(freq - freqPrev) <= 62.5f) {
    tonalCont++;
  } else {
    tonalCont = 0;
  }

  freqPrev = freq;

  if (tonalCont >= TONAL_VENTANAS)
    agregar(an, sizeof(an), "RUIDO_MECANICO_CONSTANTE");

  if (!an[0])
    strcpy(an, "NINGUNA");

  // Envía las métricas del segundo.
  char linea[200];

  snprintf(linea, sizeof(linea),
           "%lu,%.1f,%.1f,%.1f,%.0f,%.2f,%d,%d,%s",
           millis(), rmsVentDb, picoVent, baseDb, freq,
           tonal, fuertes, listo ? 0 : 1, an);

  emitir(linea);

  // Reinicia los acumuladores.
  bloquesVent = 0;
  fuertes = 0;
  potAcum = 0;
  picoVent = -200;
  maxRms = -200;
  maxCrest = 0;
}