// ============================================================
// ESP32 DEVKIT + 2 MPU-6050 + BUZZER PASIVO + THINGSPEAK
// ACUMULADOR DE EVENTOS Y TIEMPO
// ============================================================

#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ================= RED Y NUBE ===============================
const char* ssid = "TU_RED_WIFI";          // REEMPLAZA CON TU WIFI
const char* password = "TU_CLAVE_WIFI";    // REEMPLAZA CON TU CLAVE
String apiKey = "TU_API_KEY_THINGSPEAK";   // REEMPLAZA CON TU WRITE API KEY

unsigned long ultimoEnvioTS = 0;
const unsigned long INTERVALO_TS = 20000; 

// ================= PINES Y DIRECCIONES ======================
const int SDA_PIN = 21;
const int SCL_PIN = 22;

const byte MPU1_ADDR = 0x68;
const byte MPU2_ADDR = 0x69;

const int BUZZER_PIN = 12;
const int FRECUENCIA_BUZZER = 2000;

// ================= LÓGICA DE POSTURA Y MÉTRICAS =============
const float LIMITE_PELIGRO = 18.0;  
const float LIMITE_SEGURO  = 12.0;  

float pitch1 = 0.0;
float pitch2 = 0.0;
float diferenciaBase = 0.0;
float curvaturaFiltrada = 0.0;

bool posturaMala = false;
bool alarmaActiva = false;
unsigned long inicioMalaPostura = 0;
const unsigned long TIEMPO_ALERTA = 5000; 

// --- NUEVAS VARIABLES PARA EL DASHBOARD ---
int contadorEventos = 0;
unsigned long tiempoTotalMalaPostura = 0;
unsigned long inicioEventoActual = 0;
bool registrandoEvento = false;

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
  Serial.println("Calibrando postura inicial (mantener quieto)...");
  float sumaDiferencias = 0.0;
  int muestras = 100;

  for (int i = 0; i < muestras; i++) {
    float p1 = leerPitchMPU(MPU1_ADDR);
    float p2 = leerPitchMPU(MPU2_ADDR);
    sumaDiferencias += (p1 - p2);
    delay(30);
  }

  diferenciaBase = sumaDiferencias / muestras;
  curvaturaFiltrada = 0.0;
  alarmaActiva = false;
  posturaMala = false;
  
  // Reiniciar métricas al calibrar
  contadorEventos = 0;
  tiempoTotalMalaPostura = 0;
  registrandoEvento = false;

  Serial.println("Calibracion Lista!");
  tone(BUZZER_PIN, FRECUENCIA_BUZZER); delay(150); noTone(BUZZER_PIN); delay(100);
  tone(BUZZER_PIN, FRECUENCIA_BUZZER); delay(150); noTone(BUZZER_PIN);
}

// ================= ENVÍO A THINGSPEAK =======================
void enviarDatosThingSpeak(float curvatura, int estado, int eventos, float tiempoSegundos) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    // Field 1: Curvatura actual | Field 2: Estado Alarma (0/1) 
    // Field 3: Total Eventos | Field 4: Tiempo acumulado (segundos)
    String url = "http://api.thingspeak.com/update?api_key=" + apiKey + 
                 "&field1=" + String(curvatura) + 
                 "&field2=" + String(estado) + 
                 "&field3=" + String(eventos) + 
                 "&field4=" + String(tiempoSegundos);
    
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode > 0) {
      Serial.println("Datos enviados a TS | Eventos: " + String(eventos) + " | Tiempo (s): " + String(tiempoSegundos));
    } else {
      Serial.println("Error HTTP");
    }
    http.end();
  }
}

// ================= SETUP ====================================
void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);

  Serial.print("Conectando a WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Conectado!");

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!existeI2C(MPU1_ADDR) || !existeI2C(MPU2_ADDR)) {
    Serial.println("Error MPU.");
    while (1) { tone(BUZZER_PIN, 1000); delay(200); noTone(BUZZER_PIN); delay(700); }
  }

  despertarMPU(MPU1_ADDR); despertarMPU(MPU2_ADDR);
  configurarMPU(MPU1_ADDR); configurarMPU(MPU2_ADDR);

  calibrarPosturaInicial();
}

// ================= LOOP =====================================
void loop() {
  pitch1 = leerPitchMPU(MPU1_ADDR);
  pitch2 = leerPitchMPU(MPU2_ADDR);

  float diferenciaActual = pitch1 - pitch2;
  float curvatura = abs(diferenciaActual - diferenciaBase);
  curvaturaFiltrada = 0.85 * curvaturaFiltrada + 0.15 * curvatura;

  // Lógica de estado
  if (curvaturaFiltrada >= LIMITE_PELIGRO) {
    if (!posturaMala) {
      posturaMala = true;
      inicioMalaPostura = millis();
    } else if (millis() - inicioMalaPostura >= TIEMPO_ALERTA) {
      if (!alarmaActiva) {
        alarmaActiva = true;
        contadorEventos++;                 // SUMA UN EVENTO
        inicioEventoActual = millis();     // INICIA CRONOMETRO DEL EVENTO
        registrandoEvento = true;
      }
    }
  }

  if (curvaturaFiltrada <= LIMITE_SEGURO) {
    posturaMala = false;
    if (alarmaActiva) {
      alarmaActiva = false;
      if (registrandoEvento) {
        // Al volver a postura segura, suma el tiempo del evento al total
        tiempoTotalMalaPostura += (millis() - inicioEventoActual);
        registrandoEvento = false;
      }
    }
  }

  // Control de Buzzer
  if (alarmaActiva) {
    tone(BUZZER_PIN, FRECUENCIA_BUZZER);
  } else {
    noTone(BUZZER_PIN);
  }

  // Envío a TS cada 20 segundos
  if (millis() - ultimoEnvioTS >= INTERVALO_TS) {
    
    // Calcular el tiempo a enviar (histórico + lo que lleve el evento actual si está sonando)
    unsigned long tiempoAEnviar = tiempoTotalMalaPostura;
    if (registrandoEvento) {
      tiempoAEnviar += (millis() - inicioEventoActual);
    }
    float tiempoSegundos = tiempoAEnviar / 1000.0;

    enviarDatosThingSpeak(curvaturaFiltrada, alarmaActiva ? 1 : 0, contadorEventos, tiempoSegundos);
    ultimoEnvioTS = millis();
  }

  delay(100);
}
