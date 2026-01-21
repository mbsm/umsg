/*
 * ArduinoUdpNode.ino
 *
 * Demonstrates umsg over UDP.
 * Subscribes to 'SetLed' to control the built-in LED.
 *
 * Dependencies:
 *  - umsg library
 *  - Generated 'messages' folder (SetLed.hpp)
 *  - WiFi (or Ethernet) library
 */

#include <WiFi.h> // ESP32, replace with <ESP8266WiFi.h> or <WiFiNINA.h> as needed
#include <WiFiUdp.h>

#include <umsg/transports/arduino/udp.hpp>
#include <umsg/umsg.h>
#include "messages/SetLed.hpp"

// Network Settings
const char* SSID = "YOUR_SSID";
const char* PASS = "YOUR_PASS";
const uint16_t LOCAL_PORT = 8888;

// Destination for replies (if we sent any), placeholder
IPAddress destIp(192, 168, 1, 100);
uint16_t destPort = 9000;

WiFiUDP udpParams;
umsg::arduino::UdpTransport<WiFiUDP> transport(udpParams, destIp, destPort);
umsg::Node<umsg::arduino::UdpTransport<WiFiUDP>, 128, 4> node(transport);

const uint32_t MSG_SET_LED = 4;

struct Controller {
    void setup() {
        pinMode(LED_BUILTIN, OUTPUT);
        digitalWrite(LED_BUILTIN, LOW);
    }
    
    umsg::Error onSetLed(const messages::SetLed& msg) {
        digitalWrite(LED_BUILTIN, msg.state ? HIGH : LOW);
        return umsg::Error::OK;
    }
} controller;

void setup() {
    Serial.begin(115200);
    controller.setup();
    
    // Connect access point
    WiFi.begin(SSID, PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected.");
    Serial.print("Listening on UDP port: ");
    Serial.println(LOCAL_PORT);

    udpParams.begin(LOCAL_PORT);
    
    if (node.registerHandler(MSG_SET_LED, &controller, &Controller::onSetLed) != umsg::Error::OK) {
        Serial.println("Handler registration failed");
    }
}

void loop() {
    node.poll();
}
