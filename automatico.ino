#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <math.h>

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

// Variables de calibración
int aguaR = -1, aguaG = -1, aguaB = -1;
float currentTolerance = 20.0;

// Variables de control
bool autoMode = false;

String detectType() {
  if (aguaR == -1 || aguaG == -1 || aguaB == -1) {
    return "Sin Calibracion";
  }
  
  float dAgua = sqrt(pow((float)redValue - aguaR, 2) +
                     pow((float)greenValue - aguaG, 2) +
                     pow((float)blueValue - aguaB, 2));
  
  return (dAgua <= currentTolerance) ? "AGUA" : "CONTRASTE";
}

void activateCircuit1() {
  digitalWrite(PIN_BOMBA1, HIGH);
  digitalWrite(PIN_BOMBA2, LOW);
  
  digitalWrite(PIN_CIRC2_1, LOW);
  digitalWrite(PIN_CIRC2_2, LOW);
  digitalWrite(PIN_CIRC1_1, HIGH);
  digitalWrite(PIN_CIRC1_2, HIGH);
  server.send(200, "text/plain", "Circuito 1 activado");
}

void activateContraste() {
  digitalWrite(PIN_BOMBA1, HIGH);
  digitalWrite(PIN_BOMBA2, LOW);
  
  // Circuito 1 activado pero con modificaciones
  digitalWrite(PIN_CIRC1_1, HIGH);  // Ya estaba HIGH
  digitalWrite(PIN_CIRC1_2, HIGH);  // Mantenemos HIGH (o cambiar según necesidades)
  
  // Modificación específica para contraste
  digitalWrite(PIN_CIRC2_2, LOW);    // Aseguramos que está LOW
  // Agregar aquí otras modificaciones necesarias
  
  server.send(200, "text/plain", "Modo Contraste activado");
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

void readRGB() {
  digitalWrite(S2, LOW);
  digitalWrite(S3, LOW);
  redValue = pulseIn(sensorOut, LOW, 80000);

  digitalWrite(S2, HIGH);
  digitalWrite(S3, HIGH);
  greenValue = pulseIn(sensorOut, LOW, 80000);

  digitalWrite(S2, LOW);
  digitalWrite(S3, HIGH);
  blueValue = pulseIn(sensorOut, LOW, 80000);
}

void calibrateAgua() {
  aguaR = redValue;
  aguaG = greenValue;
  aguaB = blueValue;
  
  prefs.putInt("aguaR", aguaR);
  prefs.putInt("aguaG", aguaG);
  prefs.putInt("aguaB", aguaB);
  
  String msg = "Calibracion AGUA: (" + String(aguaR) + ", " + String(aguaG) + ", " + String(aguaB) + ")";
  server.send(200, "text/plain", msg);
}

void handleSetTolerance() {
  if (server.hasArg("value")) {
    currentTolerance = server.arg("value").toFloat();
    server.send(200, "text/plain", "Tolerance actualizado: " + String(currentTolerance));
  } else {
    server.send(400, "text/plain", "Falta parametro 'value'");
  }
}

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
                ".auto { background-color: #FF8844; }"
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
                "<button class='auto' onclick=\"control('auto')\">Modo Automático</button><br>"
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

void handleRoot() {
  server.send(200, "text/html", getHTML());
}

void handleRGB() {
  String detection = detectType();
  String json = "{\"red\":" + String(redValue) + 
                ",\"green\":" + String(greenValue) + 
                ",\"blue\":" + String(blueValue) +
                ",\"detection\":\"" + detection + "\"}";
  server.send(200, "application/json", json);
}

void handleAuto() {
  autoMode = !autoMode;
  if (autoMode) {
    server.send(200, "text/plain", "Modo automatico ACTIVADO");
  } else {
    turnOffAll();
    server.send(200, "text/plain", "Modo automatico DESACTIVADO");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BOMBA1, OUTPUT);
  pinMode(PIN_BOMBA2, OUTPUT);
  pinMode(PIN_CIRC1_1, OUTPUT);
  pinMode(PIN_CIRC1_2, OUTPUT);
  pinMode(PIN_CIRC2_1, OUTPUT);
  pinMode(PIN_CIRC2_2, OUTPUT);

  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);
  pinMode(sensorOut, INPUT);

  prefs.begin("calibration", false);
  aguaR = prefs.getInt("aguaR", -1);
  aguaG = prefs.getInt("aguaG", -1);
  aguaB = prefs.getInt("aguaB", -1);

  WiFi.softAP(ssid, password);
  MDNS.begin(mdnsName);
  MDNS.addService("http", "tcp", 80);

  server.on("/", handleRoot);
  server.on("/rgb", handleRGB);
  server.on("/circ1", activateCircuit1);
  server.on("/circ2", activateCircuit2);
  server.on("/off", turnOffAll);
  server.on("/calibAgua", calibrateAgua);
  server.on("/setTolerance", handleSetTolerance);
  server.on("/auto", handleAuto);

  turnOffAll();
  server.begin();
  Serial.println("Servidor iniciado");
}

void loop() {
  server.handleClient();
  
  if (millis() - lastSensorUpdate > 500) {
    readRGB();
    
    if (autoMode) {
      String detection = detectType();
      if (detection == "AGUA") {
        activateCircuit1();
      } else {
        activateContraste();
      }
    }
    
    lastSensorUpdate = millis();
  }
}
