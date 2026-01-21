#include <umsg/umsg.h>
#include <umsg/transports/posix/serial_port.hpp>
#include <messages/SetLed.hpp>
#include <iostream>
#include <thread>
#include <chrono>

/*
 * PosixSerialLedController
 *
 * Connects to an Arduino via Serial and toggles its LED.
 * Usage: ./PosixSerialLedController <device> (e.g. /dev/ttyUSB0)
 */

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <serial-device>" << std::endl;
        return 1;
    }

    const char* device = argv[1];
    umsg::posix::SerialPort port;
    
    std::cout << "Opening " << device << "..." << std::endl;
    if (!port.open(device, B115200)) {
        std::cerr << "Failed to open serial port" << std::endl;
        return 1;
    }

    // Create the Node with 64-byte RX buffer, 4 max handlers
    umsg::Node<umsg::posix::SerialPort, 64, 4> node(port);

    if (!node.ok()) {
        std::cerr << "Node initialization failed" << std::endl;
        return 1;
    }

    // Message ID for SetLed (must match Arduino)
    const uint32_t MSG_SET_LED = 4;
    
    bool ledState = true;

    while (true) {
        // Poll for incoming messages (though we don't expect many)
        node.poll();

        // Toggle LED every second
        SetLed msg;
        msg.state = ledState;
        
        std::cout << "Sending SetLed: " << (ledState ? "ON" : "OFF") << std::endl;
        node.publish(MSG_SET_LED, msg);
        
        ledState = !ledState;
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
