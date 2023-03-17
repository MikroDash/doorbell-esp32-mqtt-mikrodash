#include "esp_camera.h"
#include <WiFi.h>

//Librerias para MQTT y lectura de JSON
#include <PubSubClient.h> //https://github.com/knolleary/pubsubclient
#include <Arduino_JSON.h> //https://github.com/arduino-libraries/Arduino_JSON
#include <base64.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h> //https://github.com/adafruit/Adafruit_NeoPixel
#include "hsv.h"

//
// WARNING!!! PSRAM IC required for UXGA resolution and high JPEG quality
//            Ensure ESP32 Wrover Module or other board with PSRAM is selected
//            Partial images will be transmitted if image exceeds buffer size
//
//            You must select partition scheme from the board menu that has at least 3MB APP space.
//            Face Recognition is DISABLED for ESP32 and ESP32-S2, because it takes up from 15 
//            seconds to process single frame. Face Detection is ENABLED if PSRAM is enabled as well

// ===================
// Select camera model
// ===================
//#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
//#define CAMERA_MODEL_ESP_EYE // Has PSRAM
//#define CAMERA_MODEL_ESP32S3_EYE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_PSRAM // Has PSRAM
//#define CAMERA_MODEL_M5STACK_V2_PSRAM // M5Camera version B Has PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_ESP32CAM // No PSRAM
//#define CAMERA_MODEL_M5STACK_UNITCAM // No PSRAM
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
//#define CAMERA_MODEL_TTGO_T_JOURNAL // No PSRAM
// ** Espressif Internal Boards **
//#define CAMERA_MODEL_ESP32_CAM_BOARD
//#define CAMERA_MODEL_ESP32S2_CAM_BOARD
//#define CAMERA_MODEL_ESP32S3_CAM_LCD

#include "camera_pins.h"
#include "soc/soc.h" //disable brownout problems
#include "soc/rtc_cntl_reg.h" //disable brownout problems

#define SCREEN_WIDTH 128 // Ancho del display OLED en píxeles
#define SCREEN_HEIGHT 32 // Alto del display OLED en píxeles
// Inicializamos el objeto del display OLED
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ===========================
// Enter your WiFi credentials
// ===========================
const char* ssid = "WIFI_NAME";
const char* password = "WIFI_PASSWORD";

const int LED_PIN = 13;  // número del pin del LED
const int botonPin  = 2; // número del pin donde se conecta el botón
const int BUZZZER_PIN = 12; // número del pin donde se conecta el botón

#define NUMPIXELS 16
//Se aplica la configuracion que necesita el WS2812 y se le indica el numero de LEDS
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);
// Define constants
const int LOADING_WIFI_HUE = 50;
const int LOADING_INTERNET_HUE = 1130;

const int DELAY_TIME = 30;
const int MAX_HUE = 256*6;
int position = 0;
int currentHue = LOADING_INTERNET_HUE; // Define la variable y le asigna un valor inicial de cero
const int ledBrightness = 255;


int statusAro = 0;
TaskHandle_t Task1;


//Parametros de conexion MQTT con MikroDash
const char* mqtt_server = "mqtt.mikrodash.com"; //Servidor MQTT
const int mqtt_port = 1883;                     //Puerto MQTT
const int qos_level=1;                          //Calidad del servicio MQTT


//Credenciales MQTT MikroDash
const char* usernameMQTT = "MQTT_USER";
const char* passwordMQTT = "MQTT_PASSWORD";

//Topics MQTT
const char* topic_doorbell = "TOPIC_DOORBELL";
const char* topic_messages = "TOPIC_MESSAGES";

String customMessage = "";

//Variables para la conexión y envio de mensajes MQTT
WiFiClient espClient;
PubSubClient client(espClient);
#define MSG_BUFFER_SIZE  (60000)
const int MAX_PAYLOAD = 60000;
char msg[MSG_BUFFER_SIZE];


void showSolidColorAnimation(int hue) {
  for (int i = 0; i < NUMPIXELS; i++)
    pixels.setPixelColor((i + position) % NUMPIXELS, getPixelColorHsv(i, hue, 255, pixels.gamma8(i * (255 / NUMPIXELS))));
  pixels.show();
  position++;
  position %= NUMPIXELS;
  hue += 2;
  delay(30);
}

//Task1code: para hacer cosas en segundo plano
void Task1code( void * pvParameters ){
  Serial.print("Task1 running on core ");
  Serial.println(xPortGetCoreID());

  for(;;){
    switch (statusAro) {
      case 0: //Deja los leds en blanco
        pixels.clear();
        pixels.show();
        delay(1);
        statusAro = 1000;
        break;
      case 1: //Loading Internet
        if (currentHue > LOADING_INTERNET_HUE + 200 || currentHue < LOADING_INTERNET_HUE - 200) {
          currentHue = LOADING_INTERNET_HUE;
        }
        showSolidColorAnimation(currentHue);
        break;
      case 2: //Loading Wifi
        if (currentHue > LOADING_WIFI_HUE + 250 || currentHue < LOADING_WIFI_HUE - 50) {
          currentHue = LOADING_WIFI_HUE;
        }
        showSolidColorAnimation(currentHue);
        break;
      case 3: //Se activa led verde
        for (int i = 0; i < 50; i++) {
          showSolidColorAnimation(400);
        }
        statusAro = 0;
        break;
      case 4:
        for (int i = 0; i < 50; i++) {
          showSolidColorAnimation(260);
        }
        statusAro = 0;
        break;
      default:
        for (int i = 0; i < NUMPIXELS; i++) {
          pixels.setPixelColor(i, pixels.Color(110, 110, 110));
        }
        pixels.show();
        delay(DELAY_TIME);
        break;
    }
  } 
}

//Callback para la recepcion de datos en los topic suscrito
void callback(char* topic, byte* payload, unsigned int length) {
  String myString = ""; 
  for (int i = 0; i < length; i++) {
    myString = myString + (char)payload[i]; //Se convierte el payload de tipo bytes a una cadena de texto (String)
  }
  JSONVar myObject = JSON.parse(myString); // Se convierte la respuesta a tipo Object para acceder a los valores por el nombre.
  if (JSON.typeof(myObject) == "undefined") { // Si el archivo no tiene información o no es JSON se sale del callback
    Serial.println("[JSON] JSON NO VALIDO"); 
    return;
  }else{                                      //Si el archivo es JSON y contiene info entra aqui
    const char* from = myObject["from"];
    Serial.println(myObject["value"]);
    if(strcmp(topic,topic_messages)==0 && strcmp(from,"app")==0 ){  //Si el topic es topic_led y lo envia app.mikrodash.com
      String temporalStr = JSON.stringify(myObject["value"]);
      temporalStr.replace("\"", "");
      if(temporalStr.length() > 0 && temporalStr != "" && !customMessage.equals(temporalStr)){
        // Generar el tono de ding-dong
        statusAro = 4;
        tone(BUZZZER_PIN, 880, 500);  // Tono de 880Hz durante 300ms
        delay(100);
        tone(BUZZZER_PIN, 880, 500);  // Tono de 880Hz durante 300ms
        delay(100);
        customMessage = temporalStr;
        Serial.println(customMessage); 
        defaultMessage();
      }
    }
  }

}
/*
  * Funcion para reconectar a MQTT
*/
void reconnect() {
  while (!client.connected()) {
    Serial.print("Conectando al broker MQTT...");
    // Se genera un ID aleatorio con
    String clientId = "MikroDasLampClient-";
    clientId += String(random(0xffff), HEX);

    //Cuando se conecta
    if (client.connect(clientId.c_str(), usernameMQTT, passwordMQTT) ) {
      statusAro = 3;
      Serial.println("Conectado");
      client.subscribe(topic_messages,qos_level);
      
    } else {
      statusAro = 1;
      Serial.print("Error al conectar: ");
      Serial.print(client.state());
      Serial.println(" Reintentando en 5 segundos...");
      delay(5000);
    }
  }
}

void setup() {
  // Inicializamos la comunicación I2C
  Wire.begin(14, 15);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZZER_PIN, OUTPUT); // configurar el pin como entrada con Pull-Down interno
  pinMode(botonPin, INPUT_PULLDOWN); // configurar el pin como entrada con Pull-Down interno
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  digitalWrite(LED_PIN, HIGH);
  digitalWrite(BUZZZER_PIN, LOW);

  delay(200); 
  pixels.begin();
  pixels.show(); // Inicializar todos los LEDS en apagado


     //Se habilita el segundo CORE del procesador
  //create a task that will be executed in the Task1code() function, with priority 1 and executed on core 0
  xTaskCreatePinnedToCore(
                    Task1code,   /* Task function. */
                    "Task1",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    2,           /* priority of the task */
                    &Task1,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 0 */                  
  delay(500);
  pixels.clear();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_VGA;
  config.pixel_format = PIXFORMAT_JPEG; // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 20;
  config.fb_count = 1;
  
  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if(config.pixel_format == PIXFORMAT_JPEG){
    if(psramFound()){
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    statusAro = 1;
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  s->set_vflip(s, 0); // flip it back
  s->set_brightness(s, 1); // up the brightness just a bit
  s->set_saturation(s, 1); // lower the saturation
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, 1); // up the brightness just a bit
    s->set_saturation(s, -2); // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  if(config.pixel_format == PIXFORMAT_JPEG){
    s->set_framesize(s, FRAMESIZE_QVGA);
  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif


  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  
  while (WiFi.status() != WL_CONNECTED) {
    statusAro = 2;
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  client.setServer(mqtt_server, mqtt_port);  //Se indica el servidor y puerto de MQTT para conectarse
  client.setCallback(callback);  //Se establece el callback, donde se reciben los cambios de las suscripciones MQTT
  delay(10);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.display();
  delay(2000);
  defaultMessage();

  statusAro = 3;
}

void loop() {
  if (!client.connected()) { //Si se detecta que no hay conexión con el broker 
    reconnect(); //Reintenta la conexión
  }
  client.loop();            //Crea un bucle para las suscripciones MQTT
  int botonEstado = digitalRead(botonPin); // leer el estado lógico del pin
  if (botonEstado == HIGH) {
    statusAro = 1;
    take_picture();
    delay(1000);
  }

  delay(100); //Un delay de 100ms para darle un respiro al modulo de wifi
}

void take_picture() {
  statusAro = 0;
  camera_fb_t * fb = NULL;
  Serial.println("Taking picture");
  fb = esp_camera_fb_get(); // used to get a single picture.
  if (!fb) {
    statusAro = 4;
    Serial.println("Camera capture failed");
    return;
  }
  Serial.println("Picture taken");
  sendMQTT(fb->buf, fb->len);
  esp_camera_fb_return(fb); // must be used to free the memory allocated by esp_camera_fb_get().
  
}

void sendMQTT(const uint8_t * buf, uint32_t len){
  Serial.println("Sending picture...");
  if(len>MAX_PAYLOAD){
    statusAro = 4;
    Serial.print("Picture too large, increase the MAX_PAYLOAD value");
  }else{
    statusAro = 1;
    dingdongMessage();
    // Generar el tono de ding-dong
    tone(BUZZZER_PIN, 440, 500);  // Tono de 440Hz durante 200ms
    delay(100);
    tone(BUZZZER_PIN, 880, 500);  // Tono de 880Hz durante 300ms

    Serial.print("Picture sent ? : ");
    // Convertir los datos de la imagen en una cadena de caracteres y despues a base64
    String str = "";
    for (int i = 0; i < len; i++) {
      str += char(buf[i]);
    }
    String encoded = base64::encode(str);
    snprintf (msg, MSG_BUFFER_SIZE, "{\"from\":\"device\",\"message\":\"event_doorbell\",\"value\": \"%s\" }", encoded.c_str());   //convierte los parametros en un array de bytes
    //Serial.println(client.publish(topic_doorbell, msg, false));

    client.beginPublish(topic_doorbell, strlen(msg), false);
    client.write((const uint8_t*)msg, strlen(msg));
    client.endPublish();
    Serial.println(msg);
    statusAro = 3;
    delay(5000);
    defaultMessage();
  }
}

void defaultMessage(){
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("¡Bienvenido!");
  display.setCursor(0,24);
  display.println(customMessage);
  display.display();
}

void dingdongMessage(){
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("¡Bienvenido!");
  display.setCursor(0,24);
  display.println("Ding dong...");
  display.display();
}