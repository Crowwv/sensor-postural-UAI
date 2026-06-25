# Sistema de Monitoreo Postural IoT

**Descripción:** Dispositivo IoT vestible diseñado para medir y registrar la curvatura de la espalda mediante sensores inerciales, alertando al usuario sobre malas posturas y enviando métricas de tiempo y frecuencia a la nube.

**Integrantes:** * Vicente Rodríguez
* Benjamin Hinojosa
* Diego Escobar
*Bernardita Hinostrosa

## El Problema
Este proyecto busca mitigar los problemas fisioterapéuticos derivados de mantener una mala postura prolongada en entornos de estudio o trabajo. A diferencia de soluciones estáticas, nuestro sistema identifica la desviación postural relativa del usuario, contabiliza los eventos perjudiciales y acumula el tiempo total de exposición al riesgo para generar consciencia y promover la corrección postural.

## Hardware Necesario
* 1x Microcontrolador ESP32 DevKit V2
* 2x Sensores MPU-6050 (Configurados en direcciones I2C 0x68 y 0x69)
* 1x Buzzer Pasivo
* Protoboard y cables jumper
* Fuente de alimentación (conexión USB)

## Instrucciones de Uso
1. **Hardware:** Conectar los MPU-6050 a los pines I2C del ESP32 (SDA: 21, SCL: 22). Conectar el pin AD0 del primer MPU a GND y el del segundo a 3V3. Conectar el pin SIG del buzzer al GPIO 12.
2. **Firmware:** Abrir el archivo `.ino` en Arduino IDE. Reemplazar las credenciales de Wi-Fi y la `apiKey` de ThingSpeak en las líneas iniciales del código. Compilar y subir a la placa.
3. **Calibración:** Al encender, mantener los sensores en la postura ideal durante los primeros segundos hasta escuchar el doble beep del buzzer.
4. **Dashboard:** Los datos en tiempo real y el histórico de eventos se pueden visualizar en nuestro dashboard de ThingSpeak: [Inserta aquí el Link público de tu canal de ThingSpeak]
