/*
 * ArduinoTcpNode.ino
 *
 * Demonstrates umsg over TCP Client transport.
 * - Publishes 'Heartbeat' (ID 1)
 * - Subscribes to 'RobotState' (ID 20)
 *
 * Connects to a TCP server (e.g., a Python script or another umsg Node).
 */

#include <WiFi.h>
#include <WiFiClient.h>

#include <umsg/transports/arduino/tcp_client.hpp>
#include <umsg/umsg.h>
#include "messages/Heartbeat.hpp"
#include "messages/RobotState.hpp"

// Network Config
const char* SSID = "YOUR_SSID";
const char* PASS = "YOUR_PASS";
const char* SERVER_IP = "192.168.1.50";
const uint16_t SERVER_PORT = 9000;

WiFiClient wifiClient;
umsg::arduino::TcpClientTransport<WiFiClient> transport(wifiClient);
umsg::Node<umsg::arduino::TcpClientTransport<WiFiClient>, 128, 4> node(transport);

const uint32_t MSG_HEARTBEAT = 1;
const uint32_t MSG_ROBOT_STATE = 20;

struct Handler {
    umsg::Error onRobotState(const messages::RobotState& msg) {
        Serial.print("Rx RobotState | Mode: ");
        Serial.print(msg.mode);
        Serial.print(" Batt: ");
        Serial.println(msg.battery_voltage);
        return umsg::Error::OK;
    }
} handler;

void setup() {
    Serial.begin(115200);

    // Connect WiFi
    WiFi.begin(SSID, PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected.");

    // Register handlers before connecting loop
    node.registerHandler(MSG_ROBOT_STATE, &handler, &Handler::onRobotState);
}

void loop() {
    // Reconnect logic
    if (!transport.isConnected()) {
        Serial.print("Connecting to server...");
        if (transport.connect(SERVER_IP, SERVER_PORT)) {
            Serial.println("OK");
        } else {
            Serial.println("Fail");
            delay(2000);
            return;
        }
    }

    // Process incoming data
    node.poll();

    // Send Heartbeat periodically
    static unsigned long lastTime = 0;
    if (millis() - lastTime > 1000) {
        lastTime = millis();
        
        messages::Heartbeat hb;
        hb.uptime_ms = millis();
        
        if (node.publish(MSG_HEARTBEAT, hb)) {
             Serial.println("Tx Heartbeat");
        }
    }
}
