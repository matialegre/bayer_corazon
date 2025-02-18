#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>  // Para almacenar datos en NVS
#include <math.h>         // Para sqrt() y pow()

// Configuración de red Wi-Fi
const char *ssid = "Controlador_ESP32";
const char *password = "12345678";

// Configuración MDNS
const char* mdnsName = "bayer";

// Pines GPIO
#define PIN_BOMBA1 4
#define PIN_CIRC1_1 27
#define PIN_CIRC1_2 26

#define PIN_BOMBA2 18
#define PIN_CIRC2_1 13
#define PIN_CIRC2_2 14

// Pines del sensor TCS3200
#define S2 23
#define S3 22
#define sensorOut 21

// Variables del sensor
int redValue = 0;
int greenValue = 0;
int blueValue = 0;
unsigned long lastSensorUpdate = 0;

WebServer server(80);
Preferences prefs;

// Variables de calibración (inicialmente -1 = sin calibrar)
int aguaR = -1, aguaG = -1, aguaB = -1;
// Se mantiene la calibración de contraste en el código, aunque no se usará en la detección simplificada
int contrasteR = -1, contrasteG = -1, contrasteB = -1;

// Variable global para el umbral actual (distancia euclidiana)
float currentTolerance = 20.0;

// Función para detectar si la lectura corresponde a agua o no, usando la distancia euclidiana
String detectType() {
  if (aguaR == -1 || aguaG == -1 || aguaB == -1) {
    return "Sin Calibracion";
  }
  
  // Calcular la distancia euclidiana entre la lectura actual y la calibración de agua
  float dAgua = sqrt( pow((float)redValue - aguaR, 2) +
                      pow((float)greenValue - aguaG, 2) +
                      pow((float)blueValue - aguaB, 2) );
  
  // Si la distancia es menor o igual al umbral, se considera agua
  if (dAgua <= currentTolerance) {
    return "AGUA";
  } else {
    return "CONTRASTE";
  }
}

// Funciones de control de circuitos (sin cambios)
void activateCircuit1() {
  digitalWrite(PIN_BOMBA1, HIGH);
  digitalWrite(PIN_BOMBA2, LOW);
  
  digitalWrite(PIN_CIRC2_1, LOW);
  digitalWrite(PIN_CIRC2_2, LOW);
  digitalWrite(PIN_CIRC1_1, HIGH);
  digitalWrite(PIN_CIRC1_2, HIGH);
  server.send(200, "text/plain", "Circuito 1 activado");
}

void activateCircuit2() {
  digitalWrite(PIN_BOMBA1, LOW);
  digitalWrite(PIN_BOMBA2, HIGH);
  
  digitalWrite(PIN_CIRC2_1, HIGH);
  digitalWrite(PIN_CIRC2_2, HIGH);
  digitalWrite(PIN_CIRC1_1, LOW);
  digitalWrite(PIN_CIRC1_2, LOW);
  server.send(200, "text/plain", "Circuito 2 activado");
}

void turnOffAll() {
  digitalWrite(PIN_BOMBA1, HIGH);
  digitalWrite(PIN_BOMBA2, HIGH);
  digitalWrite(PIN_CIRC1_1, HIGH);
  digitalWrite(PIN_CIRC1_2, HIGH);
  digitalWrite(PIN_CIRC2_1, HIGH);
  digitalWrite(PIN_CIRC2_2, HIGH);
  server.send(200, "text/plain", "Todos los circuitos apagados");
}

// Lectura del sensor TCS3200
void readRGB() {
  // Lectura de rojo
  digitalWrite(S2, LOW);
  digitalWrite(S3, LOW);
  redValue = pulseIn(sensorOut, LOW, 80000);

  // Lectura de verde
  digitalWrite(S2, HIGH);
  digitalWrite(S3, HIGH);
  greenValue = pulseIn(sensorOut, LOW, 80000);

  // Lectura de azul
  digitalWrite(S2, LOW);
  digitalWrite(S3, HIGH);
  blueValue = pulseIn(sensorOut, LOW, 80000);
}

// Función de calibración para AGUA
void calibrateAgua() {
  // Se guarda el valor actual medido como referencia para AGUA
  aguaR = redValue;
  aguaG = greenValue;
  aguaB = blueValue;
  
  // Guardar en memoria no volátil
  prefs.putInt("aguaR", aguaR);
  prefs.putInt("aguaG", aguaG);
  prefs.putInt("aguaB", aguaB);
  
  String msg = "Calibracion AGUA: (" + String(aguaR) + ", " + String(aguaG) + ", " + String(aguaB) + ")";
  server.send(200, "text/plain", msg);
}

/*
// Función de calibración para CONTRASTE (no se usa en este modo simplificado)
void calibrateContraste() {
  contrasteR = redValue;
  contrasteG = greenValue;
  contrasteB = blueValue;
  
  prefs.putInt("contrR", contrasteR);
  prefs.putInt("contrG", contrasteG);
  prefs.putInt("contrB", contrasteB);
  
  String msg = "Calibracion CONTRASTE: (" + String(contrasteR) + ", " + String(contrasteG) + ", " + String(contrasteB) + ")";
  server.send(200, "text/plain", msg);
}
*/

// Endpoint para ajustar el umbral (tolerance) mediante un parámetro "value"
void handleSetTolerance() {
  if (server.hasArg("value")) {
    String valueStr = server.arg("value");
    currentTolerance = valueStr.toFloat();
    String msg = "Tolerance actualizado a: " + String(currentTolerance);
    server.send(200, "text/plain", msg);
  } else {
    server.send(400, "text/plain", "Falta el parametro 'value'");
  }
}

// Generación de la página web con interfaz actualizada
String getHTML() {
  String html = "<!DOCTYPE html><html><head>"
                "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                "<style>"
                "body { font-family: Arial, sans-serif; text-align: center; margin-top: 20px; }"
                "h1 { color: #333; }"
                ".container { display: inline-block; text-align: left; }"
                "button { font-size: 16px; padding: 10px 15px; margin: 5px; color: white; border: none; border-radius: 5px; cursor: pointer; }"
                ".circ1 { background-color: #FF4444; } "
                ".circ2 { background-color: #44FF44; } "
                ".off { background-color: #666666; } "
                ".calib { background-color: #4488FF; }"
                "button:hover { opacity: 0.8; }"
                ".sensor { margin-top: 30px; padding: 20px; background: #f0f0f0; border-radius: 10px; }"
                ".slider { width: 100%; margin: 10px 0; }"
                "</style></head>"
                "<body>"
                "<h1>Sistema de Control Bayer</h1>"
                "<div class='container'>"
                "<button class='circ1' onclick=\"control('circ1')\">Activar Circuito 1</button><br>"
                "<button class='circ2' onclick=\"control('circ2')\">Activar Circuito 2</button><br>"
                "<button class='off' onclick=\"control('off')\">Apagar Todo</button><br>"
                "<button class='calib' onclick=\"control('calibAgua')\">Calibrar AGUA</button><br>"
                // Se quita el botón de calibrar contraste para este modo simplificado
                //"<button class='calib' onclick=\"control('calibContraste')\">Calibrar CONTRASTE</button><br>"
                "<div class='sensor'>"
                "<h3>Lecturas del Sensor</h3>"
                "<p>Rojo: <span id='red'>0</span></p>"
                "<p>Verde: <span id='green'>0</span></p>"
                "<p>Azul: <span id='blue'>0</span></p>"
                "<p>Detección: <span id='detection'>Ninguno</span></p>"
                "<h3>Umbral (Tolerance): <span id='tolValue'>20</span></h3>"
                "<input type='range' min='0' max='100' value='20' class='slider' id='toleranceSlider' "
                "onchange=\"updateTolerance(this.value)\">"
                "</div></div>"
                "<script>"
                "function control(action) {"
                "  fetch('/' + action).then(response => response.text()).then(text => {"
                "    console.log(text);"
                "  });"
                "}"
                "function updateColors() {"
                "  fetch('/rgb').then(r => r.json()).then(data => {"
                "    document.getElementById('red').textContent = data.red;"
                "    document.getElementById('green').textContent = data.green;"
                "    document.getElementById('blue').textContent = data.blue;"
                "    document.getElementById('detection').textContent = data.detection;"
                "  });"
                "}"
                "function updateTolerance(val) {"
                "  document.getElementById('tolValue').textContent = val;"
                "  fetch('/setTolerance?value=' + val).then(response => response.text()).then(text => {"
                "    console.log(text);"
                "  });"
                "}"
                "setInterval(updateColors, 1000);"
                "</script></body></html>";
  return html;
}

// Endpoint principal para la interfaz web
void handleRoot() {
  server.send(200, "text/html", getHTML());
}

// Endpoint para enviar datos del sensor junto con la detección
void handleRGB() {
  String detection = detectType();
  String json = "{\"red\":" + String(redValue) + 
                ",\"green\":" + String(greenValue) + 
                ",\"blue\":" + String(blueValue) +
                ",\"detection\":\"" + detection + "\"}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  // Configurar pines de salida
  pinMode(PIN_BOMBA1, OUTPUT);
  pinMode(PIN_BOMBA2, OUTPUT);
  pinMode(PIN_CIRC1_1, OUTPUT);
  pinMode(PIN_CIRC1_2, OUTPUT);
  pinMode(PIN_CIRC2_1, OUTPUT);
  pinMode(PIN_CIRC2_2, OUTPUT);

  // Configurar sensor TCS3200
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);
  pinMode(sensorOut, INPUT);

  // Inicializar memoria no volátil para calibración
  prefs.begin("calibration", false);
  aguaR = prefs.getInt("aguaR", -1);
  aguaG = prefs.getInt("aguaG", -1);
  aguaB = prefs.getInt("aguaB", -1);
  // Se lee la calibración de contraste en caso de necesitarla luego
  contrasteR = prefs.getInt("contrR", -1);
  contrasteG = prefs.getInt("contrG", -1);
  contrasteB = prefs.getInt("contrB", -1);

  // Iniciar punto de acceso Wi-Fi
  WiFi.softAP(ssid, password);
  
  // Configurar MDNS
  if (!MDNS.begin(mdnsName)) {
    Serial.println("Error iniciando mDNS");
  }
  MDNS.addService("http", "tcp", 80);

  // Configurar rutas del servidor
  server.on("/", handleRoot);
  server.on("/rgb", handleRGB);
  server.on("/circ1", activateCircuit1);
  server.on("/circ2", activateCircuit2);
  server.on("/off", turnOffAll);
  server.on("/calibAgua", calibrateAgua);
  //server.on("/calibContraste", calibrateContraste); // No se usa en este modo
  server.on("/setTolerance", handleSetTolerance);

  // Estado inicial: apaga todos los circuitos
  turnOffAll();
  server.begin();
  Serial.println("Servidor iniciado");
}

void loop() {
  server.handleClient();
  
  // Actualizar sensor cada 500ms sin bloquear
  if (millis() - lastSensorUpdate > 500) {
    readRGB();
    lastSensorUpdate = millis();
  }
}
