#include <chrono>
#include <iostream>
#include <messages/SetLed.hpp>
#include <thread>
#include <umsg/transports/posix/udp_socket.hpp>
#include <umsg/umsg.h>

/*
 * PosixUdpLedController
 *
 * Sends SetLed commands via UDP to a target.
 * Usage: ./PosixUdpLedController <ip> <port>
 */

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::cerr << "Usage: " << argv[0] << " <ip> <port>" << std::endl;
        return 1;
    }

    const char *ip = argv[1];
    uint16_t port = (uint16_t)std::atoi(argv[2]);

    umsg::posix::UdpSocket udp;

    // Bind to any local port (0) to send
    if (!udp.bind(0))
    {
        std::cerr << "Failed to create/bind socket" << std::endl;
        return 1;
    }

    udp.setDestination(ip, port);

    std::cout << "Targeting " << ip << ":" << port << std::endl;

    umsg::Node<umsg::posix::UdpSocket, 256, 4> node(udp);

    // Message ID for SetLed
    const uint32_t MSG_SET_LED = 4;

    bool ledState = true;

    while (true)
    {
        node.poll();

        SetLed msg;
        msg.state = ledState;

        std::cout << "Sending SetLed over UDP: " << (ledState ? "ON" : "OFF") << std::endl;
        node.publish(MSG_SET_LED, msg);

        ledState = !ledState;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
