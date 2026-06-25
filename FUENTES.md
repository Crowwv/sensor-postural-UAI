# Declaración de Fuentes e Integridad Académica

## 1. Librerías utilizadas
| Librería | Versión | Uso en el proyecto | Fuente |
| :--- | :--- | :--- | :--- |
| `Wire.h` | N/A (Nativa) | Comunicación I2C directa con los sensores MPU-6050, evitando librerías pesadas para optimizar recursos. | Incluida en Arduino Core ESP32 |
| `WiFi.h` | N/A (Nativa) | Conexión del ESP32 a la red local. | Incluida en Arduino Core ESP32 |
| `HTTPClient.h` | N/A (Nativa) | Envío de peticiones HTTP GET a la API REST de ThingSpeak. | Incluida en Arduino Core ESP32 |

## 2. Código externo adaptado
### Lectura directa de registros I2C del MPU-6050
* **Fuente:** Documentación estándar del mapa de registros del MPU-6050 (InvenSense) y ejemplos genéricos de foros de hardware.
* **Adaptación:** Se implementaron funciones personalizadas (`despertarMPU`, `configurarMPU`, `leerPitchMPU`) para leer directamente los registros `0x3B`, `0x3D` y `0x3F` (acelerómetro), calcular el *pitch* y gestionar dos sensores simultáneamente en el mismo bus variando el estado del pin AD0.

## 3. Uso de Inteligencia Artificial
### Lógica no bloqueante y envío a ThingSpeak (millis)
* **Herramienta:** Asistente de IA (Gemini/Claude - Junio 2026).
* **Prompt utilizado:** "Necesito que generes el código fuente completo en C++ para Arduino IDE que [...] envíe los datos de inclinación y el estado de la postura a ThingSpeak usando peticiones HTTP cada 20 segundos sin bloquear el código."
* **Adaptación y Comprensión:** Se integró la lógica generada con nuestra lectura I2C nativa. El equipo comprende que se reemplazaron los `delay()` por el control de tiempo mediante la función `millis()` (`millis() - ultimoEnvioTS >= INTERVALO_TS`) para permitir que la lectura de los sensores y el control del buzzer se mantengan en tiempo real mientras se espera el intervalo de la API de ThingSpeak.

### Lógica acumulativa de eventos en mala postura
* **Herramienta:** Asistente de IA (Gemini/Claude - Junio 2026).
* **Prompt utilizado:** "Específicamente nosotros queremos registrar el tiempo que pasa una persona en mala postura y cuántas veces al día, lo que me estás dando ¿qué información va a guardar?"
* **Adaptación y Comprensión:** Se adaptó el código para no solo enviar un estado binario, sino para que el ESP32 mantenga contadores internos (`contadorEventos` y `tiempoTotalMalaPostura`). El equipo entiende que el código ahora captura el momento exacto (`inicioEventoActual = millis()`) cuando se cruza el umbral de 5 segundos de mala postura y suma la diferencia de tiempo al total cuando el usuario corrige su postura.
