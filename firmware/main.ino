// ============================================================
// ESP32 DEVKIT + 2 MPU-6050 + BUZZER PASIVO + THINGSPEAK
// ACUMULADOR DE EVENTOS Y TIEMPO
// ============================================================

#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ================= RED Y NUBE ===============================
const char* ssid     = "Hotspot UAI";           // ⚠️ CORRIGE EL NOMBRE EXACTO
const char* password = "12345678";
String apiKey        = "D1Q1RJZZDHYEGBID";

unsigned long ultimoEnvioTS = 0;
const unsigned long INTERVALO_TS = 20000;

// ================= PINES Y DIRECCIONES ======================
const int SDA_PIN = 21;
const int SCL_PIN = 22;

const byte MPU1_ADDR = 0x68;
const byte MPU2_ADDR = 0x69;

const int BUZZER_PIN = 27;          // ✅ Cambiado de 12 a 27
const int FRECUENCIA_BUZZER = 2000;

// ================= LÓGICA DE POSTURA Y MÉTRICAS =============
const float LIMITE_PELIGRO = 8.0;
const float LIMITE_SEGURO  = 4.0;

float pitch1 = 0.0;
float pitch2 = 0.0;
float diferenciaBase     = 0.0;
float curvaturaFiltrada  = 0.0;

bool posturaMala   = false;
bool alarmaActiva  = false;
unsigned long inicioMalaPostura = 0;
const unsigned long TIEMPO_ALERTA = 5000;

int contadorEventos                  = 0;
unsigned long tiempoTotalMalaPostura = 0;
unsigned long inicioEventoActual     = 0;
bool registrandoEvento       = false;
bool huboAlarmaEnIntervalo   = false;  // ✅ Nueva variable para ThingSpeak

// ================= FUNCIONES MPU ============================
bool despertarMPU(byte direccion) {
  Wire.beginTransmission(direccion);
  Wire.write(0x6B);
  Wire.write(0x00);
  return Wire.endTransmission() == 0;
}

bool configurarMPU(byte direccion) {
  Wire.beginTransmission(direccion);
  Wire.write(0x1C);
  Wire.write(0x00);
  return Wire.endTransmission() == 0;
}

int16_t leer16(byte direccion, byte registro) {
  Wire.beginTransmission(direccion);
  Wire.write(registro);
  Wire.endTransmission(false);
  Wire.requestFrom(direccion, (byte)2);
  if (Wire.available() < 2) return 0;
  return Wire.read() << 8 | Wire.read();
}

float leerPitchMPU(byte direccion) {
  int16_t axRaw = leer16(direccion, 0x3B);
  int16_t ayRaw = leer16(direccion, 0x3D);
  int16_t azRaw = leer16(direccion, 0x3F);

  float ax = axRaw / 16384.0;
  float ay = ayRaw / 16384.0;
  float az = azRaw / 16384.0;

  return atan2(ax, sqrt(ay * ay + az * az)) * 180.0 / PI;
}

bool existeI2C(byte direccion) {
  Wire.beginTransmission(direccion);
  return Wire.endTransmission() == 0;
}

void calibrarPosturaInicial() {
  Serial.println(">> Calibrando postura inicial (mantener quieto)...");
  float sumaDiferencias = 0.0;
  int muestras = 100;

  for (int i = 0; i < muestras; i++) {
    float p1 = leerPitchMPU(MPU1_ADDR);
    float p2 = leerPitchMPU(MPU2_ADDR);
    sumaDiferencias += (p1 - p2);
    delay(30);
  }

  diferenciaBase         = sumaDiferencias / muestras;
  curvaturaFiltrada      = 0.0;
  alarmaActiva           = false;
  posturaMala            = false;
  contadorEventos        = 0;
  tiempoTotalMalaPostura = 0;
  registrandoEvento      = false;
  huboAlarmaEnIntervalo  = false;

  Serial.println(">> Calibracion Lista! Base: " + String(diferenciaBase));

  tone(BUZZER_PIN, FRECUENCIA_BUZZER); delay(150); noTone(BUZZER_PIN); delay(100);
  tone(BUZZER_PIN, FRECUENCIA_BUZZER); delay(150); noTone(BUZZER_PIN);
}

// ================= ENVÍO A THINGSPEAK =======================
void enviarDatosThingSpeak(float curvatura, int estado, int eventos, float tiempoSegundos) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://api.thingspeak.com/update?api_key=" + apiKey +
                 "&field1=" + String(curvatura) +
                 "&field2=" + String(estado) +
                 "&field3=" + String(eventos) +
                 "&field4=" + String(tiempoSegundos);

    http.begin(url);
    int httpCode = http.GET();
    if (httpCode > 0) {
      Serial.println(">> TS OK | Curvatura: " + String(curvatura) +
                     " | Alarma intervalo: " + String(estado) +
                     " | Eventos: " + String(eventos) +
                     " | Tiempo(s): " + String(tiempoSegundos));
    } else {
      Serial.println(">> Error HTTP: " + String(httpCode));
    }
    http.end();
  } else {
    Serial.println(">> Sin WiFi - dato no enviado");
  }
}

// ================= SETUP ====================================
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== SISTEMA DE POSTURA INICIANDO ===");

  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);

  // --- Conexión WiFi ---
  Serial.print(">> Conectando a WiFi: ");
  Serial.println(ssid);
  WiFi.disconnect(true);
  delay(1000);
  WiFi.begin(ssid, password);

  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 20) {
    delay(500);
    Serial.print(".");
    intentos++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n>> WiFi Conectado! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n>> WiFi FALLIDO - continuando sin red.");
    Serial.print(">> Codigo de error: ");
    Serial.println(WiFi.status());
  }

  // --- Iniciar I2C y verificar MPUs ---
  Wire.begin(SDA_PIN, SCL_PIN);
  delay(100);

  Serial.println(">> Buscando MPUs...");
  if (!existeI2C(MPU1_ADDR)) {
    Serial.println(">> ERROR: MPU1 (0x68) no encontrado!");
  } else {
    Serial.println(">> MPU1 OK");
  }
  if (!existeI2C(MPU2_ADDR)) {
    Serial.println(">> ERROR: MPU2 (0x69) no encontrado!");
  } else {
    Serial.println(">> MPU2 OK");
  }

  if (!existeI2C(MPU1_ADDR) || !existeI2C(MPU2_ADDR)) {
    Serial.println(">> Sistema detenido por error en MPUs.");
    while (1) {
      tone(BUZZER_PIN, 1000); delay(200);
      noTone(BUZZER_PIN);     delay(700);
    }
  }

  despertarMPU(MPU1_ADDR);  despertarMPU(MPU2_ADDR);
  configurarMPU(MPU1_ADDR); configurarMPU(MPU2_ADDR);

  calibrarPosturaInicial();
  Serial.println("=== SISTEMA LISTO ===\n");
}

// ================= LOOP =====================================
void loop() {
  pitch1 = leerPitchMPU(MPU1_ADDR);
  pitch2 = leerPitchMPU(MPU2_ADDR);

  float diferenciaActual = pitch1 - pitch2;
  float curvatura        = abs(diferenciaActual - diferenciaBase);
  curvaturaFiltrada      = 0.85 * curvaturaFiltrada + 0.15 * curvatura;

  Serial.print("Curvatura: "); Serial.print(curvaturaFiltrada);
  Serial.print(" | posturaMala: "); Serial.print(posturaMala);
  Serial.print(" | alarmaActiva: "); Serial.println(alarmaActiva);

  // --- Lógica de postura ---
  if (curvaturaFiltrada >= LIMITE_PELIGRO) {
    if (!posturaMala) {
      posturaMala = true;
      inicioMalaPostura = millis();
    } else if (millis() - inicioMalaPostura >= TIEMPO_ALERTA) {
      if (!alarmaActiva) {
        alarmaActiva          = true;
        huboAlarmaEnIntervalo = true;  // ✅ Registra que hubo alarma
        contadorEventos++;
        inicioEventoActual = millis();
        registrandoEvento  = true;
        Serial.println(">> ALARMA ACTIVADA - Evento #" + String(contadorEventos));
      }
    }
  }

  if (curvaturaFiltrada <= LIMITE_SEGURO) {
    posturaMala = false;
    if (alarmaActiva) {
      alarmaActiva = false;
      if (registrandoEvento) {
        tiempoTotalMalaPostura += (millis() - inicioEventoActual);
        registrandoEvento = false;
        Serial.println(">> Postura corregida. Tiempo acumulado: " +
                       String(tiempoTotalMalaPostura / 1000.0) + "s");
      }
    }
  }

  // --- Control Buzzer ---
  if (alarmaActiva) {
    tone(BUZZER_PIN, FRECUENCIA_BUZZER);
  } else {
    noTone(BUZZER_PIN);
  }

  // --- Envío a ThingSpeak cada 20 segundos ---
  if (millis() - ultimoEnvioTS >= INTERVALO_TS) {
    unsigned long tiempoAEnviar = tiempoTotalMalaPostura;
    if (registrandoEvento) {
      tiempoAEnviar += (millis() - inicioEventoActual);
    }
    float tiempoSegundos = tiempoAEnviar / 1000.0;

    // ✅ Envía si hubo alarma en el intervalo, no solo si está activa ahora
    enviarDatosThingSpeak(curvaturaFiltrada, huboAlarmaEnIntervalo ? 1 : 0,
                          contadorEventos, tiempoSegundos);

    huboAlarmaEnIntervalo = false;  // ✅ Reset para el siguiente intervalo
    ultimoEnvioTS = millis();
  }

  delay(100);
}
