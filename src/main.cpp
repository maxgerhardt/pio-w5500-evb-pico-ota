#include <Arduino.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>

// pinout valid for W5500-EVB-Pico and W5100S. Other boards may differ.
// See https://docs.wiznet.io/Product/iEthernet/W5100S/w5100s-evb-pico
// See https://docs.wiznet.io/Product/iEthernet/W5500/w5500-evb-pico
#define WIZNET_MISO 16
#define WIZNET_CS   17
#define WIZNET_SCLK 18
#define WIZNET_MOSI 19
#define WIZNET_RSTn 20
#define WIZNET_INT  21

// Instantiate the right object. They have the same interface.
#if defined(ARDUINO_WIZNET_5500_EVB_PICO)
#include <W5500lwIP.h>
Wiznet5500lwIP eth(WIZNET_CS /* chip select */, SPI, WIZNET_INT /* interrupt */);
#elif defined (ARDUINO_WIZNET_5100S_EVB_PICO)
#include <W5100lwIP.h>
Wiznet5100lwIP eth(WIZNET_CS /* chip select */, SPI, WIZNET_INT /* interrupt */);
#else
#error "No idea what board you have."
#endif

WiFiClient ethClient;
PubSubClient client(ethClient);
#define MQTT_SERVER_HOSTNAME "public.cloud.shiftr.io"
#define MQTT_SERVER_PORT 1883
#define MQTT_CLIENTID "arduino"
#define MQTT_USERNAME "public"
#define MQTT_PASSWORD "public"
#define MQTT_SUBSCRIBE_TOPIC "/hello"

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(MQTT_CLIENTID, MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      bool ok = client.publish(MQTT_SUBSCRIBE_TOPIC, "hello world");
      if(ok) {
        Serial.println("Published 'hello world' to topic \"" + String(MQTT_SUBSCRIBE_TOPIC) + "\" OK!");
      } else {
        Serial.println("Publishing topic \"" + String(MQTT_SUBSCRIBE_TOPIC) + "\" FAILED!");
      }
      // ... and resubscribe
      ok = client.subscribe(MQTT_SUBSCRIBE_TOPIC);
      if(ok) {
        Serial.println("Subscribing to topic \"" + String(MQTT_SUBSCRIBE_TOPIC) + "\" OK!");
      } else {
        Serial.println("Subscribing to topic \"" + String(MQTT_SUBSCRIBE_TOPIC) + "\" FAILED!");
      }
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  delay(2000);
  Serial.println("Booting");

  // Set up SPI pinout to match your HW
  SPI.setRX(WIZNET_MISO);
  SPI.setCS(WIZNET_CS);
  SPI.setSCK(WIZNET_SCLK);
  SPI.setTX(WIZNET_MOSI);

  // get the W5500 chip out of reset
  pinMode(WIZNET_RSTn, OUTPUT);
  digitalWrite(WIZNET_RSTn, LOW);
  delayMicroseconds(500);
  digitalWrite(WIZNET_RSTn, HIGH);
  delay(200);

  // Start the Ethernet port
  if (!eth.begin()) {
    Serial.println("No wired Ethernet hardware detected. Check pinouts, wiring.");
    while(1) { delay(100); }
  }
  // Wait until IP was acquired
  while(eth.isLinked() == false) {
    Serial.println("Waiting for Ethernet link..");
    delay(1000);
  }
  while(eth.connected() == false) {
    Serial.println("Waiting for DHCP address..");
    delay(1000);
  }
  Serial.println("Ethernet Ready!");
  Serial.print("IP address: ");
  Serial.println(eth.localIP());


  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();

  // startup MQTT client
  client.setServer(MQTT_SERVER_HOSTNAME, MQTT_SERVER_PORT);
  client.setCallback(callback);
}

static unsigned long lastTime = 0;
static unsigned long interval = 2000; 

void loop() {
  // handle OTA stuff
  ArduinoOTA.handle();
  // handle MQTT stuff
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  // toggle built in LED
  if (millis() - lastTime >= interval) {
      lastTime = millis();
      digitalWrite(LED_BUILTIN, digitalRead(LED_BUILTIN) ^ 1);
  }
}