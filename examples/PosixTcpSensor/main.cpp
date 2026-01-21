#include <chrono>
#include <cmath>
#include <iostream>
#include <messages/SensorReading.hpp>
#include <thread>
#include <umsg/transports/posix/tcp_client.hpp>
#include <umsg/umsg.h>

/*
 * PosixTcpSensor
 *
 * Simulates a sensor node sending readings over TCP.
 * Usage: ./PosixTcpSensor <server-ip> <server-port>
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

    umsg::posix::TcpClient client;

    std::cout << "Connecting to " << ip << ":" << port << "..." << std::endl;
    if (!client.connect(ip, port))
    {
        std::cerr << "Connection failed" << std::endl;
        return 1;
    }

    umsg::Node<umsg::posix::TcpClient, 128, 4> node(client);

    // ID 10 for SensorReading
    const uint32_t MSG_SENSOR_ID = 10;

    // Simple simulated wave
    float t = 0;

    while (true)
    {
        // Poll usually handles incoming data, but also drives internal state if needed
        node.poll();

        if (!client.isOpen())
        {
            std::cerr << "Connection lost" << std::endl;
            break;
        }

        SensorReading msg;
        msg.sensor_id = 101;
        msg.value = std::sin(t) * 10.0f + 25.0f; // 25 +/- 10
        t += 0.1f;

        // If connection is lost, write might fail
        if (node.publish(MSG_SENSOR_ID, msg) != umsg::Error::OK)
        {
            std::cout << "Failed to send message" << std::endl;
        }
        else
        {
            std::cout << "Sent SensorReading: " << msg.value << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    return 0;
}
