# car
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include "esp_camera.h"
#include "DHT.h"
#include <Wire.h>
#include <Adafruit_MotorShield.h>

// ------------------- PIN DEFINITIONS -------------------
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM       5
#define Y1_GPIO_NUM       4
#define Y0_GPIO_NUM       2
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
#define RXD2 3
#define TXD2 1

#define DHTPIN 14   // ✅ FIXED (was 2, conflicted with camera)
#define DHTTYPE DHT11
#define ULTRASONIC_TRIG 13
#define ULTRASONIC_ECHO 12
#define LED_PIN 15

DHT dht(DHTPIN, DHTTYPE);

// ------------------- CAMERA CONFIG -------------------
static camera_config_t camera_config = {
  .pin_pwdn       = PWDN_GPIO_NUM,
  .pin_reset      = RESET_GPIO_NUM,
  .pin_xclk       = XCLK_GPIO_NUM,
  .pin_sccb_sda   = SIOD_GPIO_NUM,
  .pin_sccb_scl   = SIOC_GPIO_NUM,
  .pin_d7         = Y9_GPIO_NUM,
  .pin_d6         = Y8_GPIO_NUM,
  .pin_d5         = Y7_GPIO_NUM,
  .pin_d4         = Y6_GPIO_NUM,
  .pin_d3         = Y5_GPIO_NUM,
  .pin_d2         = Y4_GPIO_NUM,
  .pin_d1         = Y3_GPIO_NUM,
  .pin_d0         = Y2_GPIO_NUM,
  .pin_vsync      = VSYNC_GPIO_NUM,
  .pin_href       = HREF_GPIO_NUM,
  .pin_pclk       = PCLK_GPIO_NUM,
  .xclk_freq_hz   = 20000000,
  .ledc_timer     = LEDC_TIMER_0,
  .ledc_channel   = LEDC_CHANNEL_0,
  .pixel_format   = PIXFORMAT_JPEG,
  .frame_size     = FRAMESIZE_QVGA,
  .jpeg_quality   = 12,
  .fb_count       = 1,  // ✅ IMPROVED
  .grab_mode      = CAMERA_GRAB_WHEN_EMPTY
};

// ------------------- GLOBAL OBJECTS -------------------
WebServer server(80);
WebSocketsServer webSocket(81);

float temperature = 0;
float humidity = 0;
float distance = 0;
bool personDetected = false;
const unsigned long SENSOR_INTERVAL = 2000;

// ------------------- CAMERA -------------------
bool initCamera() {
  if (esp_camera_init(&camera_config) != ESP_OK) {
      Serial.println("Camera init failed");
      return false;
  }
  sensor_t *s = esp_camera_sensor_get();
  s->set_vflip(s, 1);
  return true;
}

// ------------------- ULTRASONIC -------------------
float readUltrasonic() {
  digitalWrite(ULTRASONIC_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIG, LOW);
  
  long duration = pulseIn(ULTRASONIC_ECHO, HIGH, 30000);
  if (duration == 0) return 999;
  return duration * 0.034 / 2.0;
}

// ------------------- SENSORS -------------------
void readSensors() {
  float newHumidity = dht.readHumidity();
  float newTemperature = dht.readTemperature();
  
  if (!isnan(newHumidity) && !isnan(newTemperature)) {
      humidity = newHumidity;
      temperature = newTemperature;
  }
  
  distance = readUltrasonic();
  personDetected = (distance < 50);

    if(distance < 15){
    Serial2.print("%S#");   
    }
}

// ------------------- MOTOR -------------------
void handleCommand(String cmd){

  if(cmd == "arrow-up") {
    Serial2.print("%F#");   // forward
  }
  else if(cmd == "arrow-down") {
    Serial2.print("%B#");   // backward
  }
  else if(cmd == "arrow-tilt-up") {
    Serial2.print("%L#");   // left
  }
  else if(cmd == "arrow-tilt-down") {
    Serial2.print("%R#");   // right
  }
  else if(cmd == "stop") {
    Serial2.print("%S#");   // stop
  }
  else if(cmd.startsWith("speed:")) {
    String val = cmd.substring(6);
    Serial2.print("%V" + val + "#");   // send speed to UNO
  }
  else if(cmd == "ledToggle") {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
}

// ------------------- WEBSOCKET -------------------
void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length){
  if(type == WStype_TEXT){
      String cmd = "";
      for (size_t i = 0; i < length; i++) {
        cmd += (char)payload[i];
      }
      handleCommand(cmd);
  }
}

void sendSensorData(uint8_t clientNum) {
  StaticJsonDocument<256> doc;
  doc["personDetected"] = personDetected;
  String out;
  serializeJson(doc, out);
  webSocket.sendTXT(clientNum, out);
}

// ------------------- CAMERA STREAM -------------------
void handleMJPGStream() {
  WiFiClient client = server.client();
  String resp = "HTTP/1.1 200 OK\r\nContent-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(resp);

  while(client.connected()) {
    camera_fb_t *fb = esp_camera_fb_get();
    if(!fb) break;

    server.sendContent("--frame\r\n");
    server.sendContent("Content-Type: image/jpeg\r\n\r\n");
    client.write(fb->buf, fb->len);
    server.sendContent("\r\n");

    esp_camera_fb_return(fb);
    delay(80);
  }
}

// ------------------- WEB PAGE -------------------
void handleRoot() {
    server.send(200,"text/html",R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Smart Device Interface</title>
<style>
    body {
        background: radial-gradient(circle at top, #8e8e8e 0%, #4a4a4a 40%, #1a1a1a 100%);
        background-attachment: fixed;
        margin: 0;
        height: 100vh;
        width: 100vw;
        display: flex;
        justify-content: center;
        align-items: center;
        font-family: 'Segoe UI', sans-serif;
        color: white;
    }

    .device-container {
        display: flex;
        align-items: center;
        gap: 40px;
        width: 90%;
        max-width: 1000px;
    }

    .main-screen {
        flex-grow: 1;
        height: 500px;
        background: linear-gradient(145deg, #7a7a7a, #333);
        border: 3px solid #666;
        border-radius: 30px;
        position: relative;
        display: flex;
        justify-content: center;
        align-items: center;
        overflow: hidden;
        box-shadow: 0 20px 50px rgba(0,0,0,0.5);
        flex-direction: column;
    }

    .person-warning, .network-warning {
        background-color: #ff3b30;
        padding: 10px 20px;
        border-radius: 15px;
        font-weight: bold;
        color: white;
        display: none;
        margin-bottom: 15px;
        z-index: 2;
    }

    .dock-menu {
        position: absolute;
        bottom: 20px;
        background: rgba(255, 255, 255, 0.9);
        padding: 10px 30px;
        border-radius: 40px;
        display: flex;
        gap: 20px;
        box-shadow: 0 10px 30px rgba(0,0,0,0.3);
        z-index: 2;
    }

    .dock-item {
        display: flex;
        flex-direction: column;
        align-items: center;
        color: #333;
        font-size: 0.7rem;
        font-weight: bold;
        cursor: pointer;
        transition: transform 0.2s;
    }

    .dock-item:hover { transform: scale(1.1); }

    .icon-bg {
        width: 60px;
        height: 60px;
        border-radius: 15px;
        display: flex;
        justify-content: center;
        align-items: center;
        font-size: 1.5rem;
        margin-bottom: 5px;
        border: 1px solid #999;
    }

    .alert { background: linear-gradient(#ff9d6e, #ff5e62); color: white; }
    .dashboard { background: linear-gradient(#6a82fb, #fc5c7d); color: white; }
    .apps { background: linear-gradient(#f6d365, #fda085); }

    .led-section { margin-bottom: 40px; text-align: center; font-weight: bold; z-index: 2; }
    .led-box {
        width: 80px;
        height: 80px;
        background-color: #ff3b30;
        border-radius: 10px;
        margin-top: 10px;
        box-shadow: 0 0 15px #ff3b30;
        cursor: pointer;
        transition: box-shadow 0.3s, opacity 0.3s;
    }

    .vertical-arrows, .side-controls.right {
        display: flex;
        flex-direction: column;
        gap: 30px;
        z-index: 2;
    }

    button {
        background: none;
        border: none;
        cursor: pointer;
        width: 0; 
        height: 0; 
        transition: transform 0.1s;
        z-index: 2;
    }

    .arrow-up {
        border-left: 40px solid transparent;
        border-right: 40px solid transparent;
        border-bottom: 60px solid #eee;
    }

    .arrow-down {
        border-left: 40px solid transparent;
        border-right: 40px solid transparent;
        border-top: 60px solid #eee;
    }

    .arrow-tilt-up {
        border-left: 40px solid transparent;
        border-right: 40px solid transparent;
        border-bottom: 60px solid #eee;
        transform: rotate(45deg);
    }

    .arrow-tilt-down {
        border-left: 40px solid transparent;
        border-right: 40px solid transparent;
        border-top: 60px solid #eee;
        transform: rotate(45deg);
    }

    .button-pressed {
        transform: scale(0.9) !important;
    }

    .info-box {
        position: absolute;
        top: 20px;
        right: 20px;
        background: rgba(255, 255, 255, 0.95);
        color: #333;
        border-radius: 20px;
        padding: 15px;
        width: 220px;
        max-height: 400px;
        overflow-y: auto;
        display: none;
        box-shadow: 0 10px 30px rgba(0,0,0,0.5);
        z-index: 2;
    }

    .info-box span {
        font-weight: bold;
        display: block;
        margin-bottom: 10px;
        text-align: center;
        font-size: 1.1rem;
    }

    .inner-apps {
        display: flex;
        flex-direction: column;
        gap: 8px;
    }

    .inner-app {
        background: #eee;
        color: #333;
        padding: 8px;
        border-radius: 10px;
        text-align: center;
        cursor: pointer;
        font-weight: bold;
    }

    .inner-app:hover {
        background: #ddd;
        transform: scale(1.05);
        transition: 0.2s;
    }

    .fog-warning {
        background-color: #ff3b30;
        color: white;
        font-weight: bold;
        text-align: center;
        padding: 6px;
        border-radius: 10px;
        display: none;
    }

    .person-warning {
    background-color: rgba(255, 59, 48, 0.85); /* semi-transparent red */
    color: white;
    border-radius: 15px;
    font-weight: bold;
    text-align: center;
    padding: 10px 20px;
    display: none;
    z-index: 5; /* over camera */
    }

    /* ===== ESP32-CAM STREAM STYLE ===== */
    #cameraStream {
        width: 100%;
        height: 100%;
        object-fit: cover;
        position: absolute;
        top: 0;
        left: 0;
        z-index: 1;
    }

    /* ===== SPEED DISPLAY STYLE ===== */
    #speedDisplay {
        color: white;
        font-weight: bold;
        text-align: center;
        font-size: 1.2rem;
        margin-bottom: 10px;
        display: none;
    }

</style>
</head>
<body>

<div class="device-container">
    <div class="side-controls left">
        <div class="led-section">
            <span>LED</span>
            <div id="led-light" class="led-box"></div>
        </div>
        <div class="vertical-arrows">
            <button class="arrow-up"></button>
            <button class="arrow-down"></button>
        </div>
    </div>

    <div class="main-screen">

        <!-- ===== ESP32-CAM LIVE STREAM ===== -->
        <img id="cameraStream" />
        
        <!-- ALERT OVERLAY -->
        <div class="person-warning" id="personWarning" style="
            position: absolute;
            top: 20px;
            left: 50%;
            transform: translateX(-50%);
            z-index: 5;
            display: none;
            font-size: 1.5rem;
            padding: 15px 25px;
            text-align: center;
        ">🚨 Be careful! Something is nearby!</div>

        <div class="dock-menu">
            <div class="dock-item active">
                <div class="icon-bg alert"></div>
                <span>Alert System</span>
            </div>
            <div class="dock-item">
                <div class="icon-bg dashboard">📊</div>
                <span>Dashboard</span>
            </div>
            <div class="dock-item" id="appsDock">
                <div class="icon-bg apps"></div>
                <span>Apps</span>
            </div>
        </div>

        <!-- Alert System -->
        <div class="info-box" id="alertBox">
            <span> Alerts</span>
            <div class="inner-apps" id="innerAlerts">
                <div class="inner-app">High Temperature</div>
                <div class="inner-app">Low Battery</div>
                <div class="inner-app">Person Detected</div>
                <div class="inner-app">Obstacle Ahead</div>
            </div>
        </div>

        <!-- Dashboard -->
        <div class="info-box" id="dashboardBox">
            <span>📊 Dashboard</span>
            <div class="inner-apps" id="innerDashboard">
                <div class="inner-app" id="tempStatus">Temperature: 26&deg;C</div>
                <div class="inner-app" id="humidityStatus">Humidity: 48%</div>
                <div class="inner-app" id="batteryStatus">Battery: 78%</div>
                <div class="inner-app" id="speedStatus">Speed: 0 km/h</div>
                <div class="inner-app fog-warning" id="fogWarning"> Foggy Area Be Careful!</div>
            </div>
        </div>

        <!-- Apps -->
        <div class="info-box" id="appsBox">
            <span>📱 Apps</span>
            <div class="inner-apps" id="innerApps">
                <div class="inner-app">YouTube</div>
                <div class="inner-app">Facebook</div>
                <div class="inner-app">Instagram</div>
                <div class="inner-app">Twitter</div>
                <div class="inner-app">LinkedIn</div>
                <div class="inner-app">TikTok</div>
            </div>
        </div>
    </div>

    <div class="side-controls right">
        <!-- SPEED DISPLAY -->
        <div id="speedDisplay">Speed: 0 km/h</div>
        <button class="arrow-tilt-up"></button>
        <button class="arrow-tilt-down"></button>
    </div>
</div>

<script>
    const led = document.getElementById('led-light');
    const personWarning = document.getElementById('personWarning');
    const networkWarning = document.getElementById("networkWarning");
    const speedStatus = document.getElementById("speedStatus");
    const speedDisplay = document.getElementById("speedDisplay");

    // ===== AUTO CAMERA STREAM =====
    const cameraStream = document.getElementById("cameraStream");
    // ✅ FIXED: MJPG stream should use HTTP port 80, not WebSocket port 81
    cameraStream.src = "http://" + location.hostname + "/stream";

    // LED toggle on click manually
    led.addEventListener('click', () => {
        led.style.opacity = led.style.opacity === '0.3' ? '1' : '0.3';
        led.style.boxShadow = led.style.opacity === '0.3' ? 'none' : '0 0 15px #ff3b30';
    });

    // Dock items toggle
    const items = document.querySelectorAll('.dock-item');
    const appsBox = document.getElementById('appsBox');
    const alertBox = document.getElementById('alertBox');
    const dashboardBox = document.getElementById('dashboardBox');

    items.forEach(item => {
        item.addEventListener('click', () => {
            const label = item.querySelector('span').innerText;
            let boxToToggle = label === "Apps" ? appsBox : label === "Alert System" ? alertBox : label === "Dashboard" ? dashboardBox : null;

            if(boxToToggle) {
                if(boxToToggle.style.display === 'block') {
                    boxToToggle.style.display = 'none';
                    item.style.opacity = '0.6';
                } else {
                    appsBox.style.display = alertBox.style.display = dashboardBox.style.display = 'none';
                    items.forEach(i => i.style.opacity = '0.6');
                    boxToToggle.style.display = 'block';
                    item.style.opacity = '1';
                }
            }
        });
    });

    // Button press effect
    document.querySelectorAll('button').forEach(btn => {
        btn.addEventListener('mousedown', () => btn.classList.add('button-pressed'));
        btn.addEventListener('mouseup', () => btn.classList.remove('button-pressed'));
        btn.addEventListener('mouseleave', () => btn.classList.remove('button-pressed'));
    });

    // ===== WEBSOCKET =====
    const ws = new WebSocket('ws://' + location.hostname + ':81');

    ws.onopen = () => console.log('Connected to ESP32');

    ws.onmessage = (event) => {
    try {
        const data = JSON.parse(event.data);
            if(data.personDetected) {
                personWarning.style.display = 'block'; // SHOW ALERT
            } else {
                personWarning.style.display = 'none';  // HIDE ALERT
            }

            if(data.speed !== undefined) {
                speedStatus.innerHTML = "Speed: " + data.speed + " km/h";
                speedDisplay.innerHTML = "Speed: " + data.speed + " km/h";
            }
        } catch(e) {}
    };

    // ===== MOTOR CONTROL =====
    function bindButton(btn, command){
    btn.addEventListener("mousedown", ()=> ws.send(command));
    btn.addEventListener("touchstart", ()=> ws.send(command));

    btn.addEventListener("mouseup", ()=> ws.send("stop"));
    btn.addEventListener("mouseleave", ()=> ws.send("stop"));
    btn.addEventListener("touchend", ()=> ws.send("stop"));
}

    bindButton(document.querySelector('.arrow-up'), "arrow-up");
    bindButton(document.querySelector('.arrow-down'), "arrow-down");
    bindButton(document.querySelector('.arrow-tilt-up'), "arrow-tilt-up");
    bindButton(document.querySelector('.arrow-tilt-down'), "arrow-tilt-down");

    // STOP when button released
    document.querySelectorAll('button').forEach(btn => {
        btn.addEventListener('mouseup', () => {
            ws.send("stop");
            speedDisplay.style.display = 'none';
        });
        btn.addEventListener('mouseleave', () => {
            ws.send("stop");
            speedDisplay.style.display = 'none';
        });
    });
</script>

</body>
</html>
)rawliteral");
}

// ------------------- SETUP -------------------
void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  WiFiManager wm;
  wm.setConfigPortalTimeout(300);

  bool res = wm.startConfigPortal("ESP32_CAM");

  if(!res){
    Serial.println("Failed to connect");
  } else {
    Serial.println("Connected!");
    Serial.println(WiFi.localIP());
  }

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

// ------------------- LOOP -------------------
void loop() {
  server.handleClient();
  webSocket.loop();

  static unsigned long lastRead=0;
  if(millis()-lastRead>2000){
    readSensors();
    lastRead=millis();
    for(uint8_t i=0;i<webSocket.connectedClients();i++)
      sendSensorData(i);
  }
}
