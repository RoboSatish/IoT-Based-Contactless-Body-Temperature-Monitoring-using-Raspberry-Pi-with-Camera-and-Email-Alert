#include <stdint.h>
#include <Wire.h>
#include <WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include "MAX30100_PulseOximeter.h"  
#define WLAN_SSID          "satish"
#define WLAN_PASS        "satish123"
#define AIO_UPDATE_RATE_SEC 5
#define AIO_SERVER        "io.adafruit.com"
#define AIO_SERVERPORT  1883                 
#define AIO_USERNAME "satish8209"
#define AIO_KEY                ""
#define I2C_SDA   21
#define I2C_SCL   22
TaskHandle_t poxReadTaskHld = NULL;
TaskHandle_t mqttPubTaskHld = NULL;
// PulseOximeter is the higher-level interface to the sensor
PulseOximeter pox;
uint32_t tsLastReport = 0;
float bpm_dt=0;
float spo2_dt = 0;
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);        
Adafruit_MQTT_Subscribe sw_sub = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/switch");
// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>
Adafruit_MQTT_Publish bpm_pub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/bpm");
Adafruit_MQTT_Publish spo2_pub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/SpO2");
// Callback (registered below) fired when a pulse is detected
void onBeatDetected()
{
                Serial.println("Beat!")
}

void stopReadPOX(){
  pox.shutdown();
}

void startReadPOX(){
  pox.resume();
}

void poxReadTask( void * param )
{
  while(1){
               
                pox.update();
                vTaskDelay( 1 / portTICK_PERIOD_MS );
  }
  poxReadTaskHld = NULL;
  vTaskDelete(NULL); 
}

void mqttPubTask( void * param )
{
  uint8_t sec_count=0;
  while(1){
                Serial.print("Heart rate:");
                float bpm_dt = pox.getHeartRate();
                Serial.print(bpm_dt);
                Serial.print("bpm / SpO2:");
                float spo2_dt = pox.getSpO2();
                Serial.print(spo2_dt);
                Serial.println("%");
                if(sec_count >= AIO_UPDATE_RATE_SEC){
                if (! bpm_pub.publish(bpm_dt)) {
                Serial.println(F("Failed to publish bmp.."));
                } else {
                Serial.println(F("bmp publish OK!"));
                }
                if (! spo2_pub.publish(spo2_dt)) {
                Serial.println(F("Failed to publish SpO2.."));
                } else {
                Serial.println(F("SpO2 publish OK!"));
                }
                sec_count=0;
                }
                vTaskDelay( 1000 / portTICK_PERIOD_MS );
                sec_count++;
  }
  mqttPubTaskHld = NULL;
  vTaskDelete(NULL); // kill itself
}


void MQTT_connect() {
  int8_t ret;
  if (mqtt.connected()) {
                return;
  }
  Serial.print("Connecting to MQTT... ");
  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) {
                Serial.println(mqtt.connectErrorString(ret));
                Serial.println("Retrying MQTT connection in 5 seconds...");
                mqtt.disconnect();
                delay(5000);
                retries--;
                if (retries == 0) {
                while (1);
                }
  }
  Serial.println("MQTT Connected!");
}

void setup()
{
                Serial.begin(115200);
                Wire.begin(I2C_SDA, I2C_SCL);
                WiFi.begin(WLAN_SSID, WLAN_PASS);
                while (WiFi.status() != WL_CONNECTED) {
                delay(500);
                Serial.print(".");
                }
                Serial.println();
                Serial.println("WiFi connected");
                Serial.println("IP address: "); Serial.println(WiFi.localIP());
                mqtt.subscribe(&sw_sub);
                Serial.print("Initializing pulse oximeter..");
                
                if (!pox.begin()) {
                Serial.println("FAILED");
                for(;;);
                } else {
                Serial.println("SUCCESS");
                }
               
                pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);
              
                pox.setOnBeatDetectedCallback(onBeatDetected);
                stopReadPOX();
}
void loop() {
  MQTT_connect();
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(5000)))
  {
                if (subscription == &sw_sub)
                {
                Serial.print(F("Got: "));
                Serial.println((char *)sw_sub.lastread);
                if (!strcmp((char*) sw_sub.lastread, "ON"))
                {
                Serial.print(("Starting POX... "));
                startReadPOX();
                BaseType_t xReturned;
                if(poxReadTaskHld == NULL){
                xReturned = xTaskCreate(
                                poxReadTask,      
                                "pox_read",          
                                1024*3,                 
                                NULL,    
                                2,
                                &poxReadTaskHld );         
                }
                delay(100);
                if(mqttPubTaskHld == NULL){
                xReturned = xTaskCreate(
                                mqttPubTask,       
                                "mqttPub",            
                                1024*3,                
                                NULL,   
                                2,
                                &mqttPubTaskHld );          
                }
                }
                else
                {
                Serial.print(("Stoping POX... "));
                // Detele POX read task
                if(poxReadTaskHld != NULL){
                vTaskDelete(poxReadTaskHld);
                poxReadTaskHld = NULL;
                }
                // Delete the MQTT Pub Task
                if(mqttPubTaskHld != NULL){
                vTaskDelete(mqttPubTaskHld);
                mqttPubTaskHld = NULL;
                }
                stopReadPOX();
                }
                }
  }
}