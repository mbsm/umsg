# Updated README.md

## Ready-to-Use Transports

In this section, you will find comprehensive details about the ready-to-use transports provided by our library. Each transport is designed to facilitate message handling in different use-cases and environments. 

### Transport Types
- **HTTP Transport**: Ideal for web-based applications, allowing messages to be sent over HTTP.
- **WebSocket Transport**: A real-time alternative that enables two-way communication between client and server.
- **MQTT Transport**: A lightweight messaging protocol suited for IoT devices.
- **TCP Transport**: Suitable for scenarios requiring direct socket communication.

Each type supports various configurations to tailor its functionality to meet the needs of your application.

## Message Handler Argument Lifetime

Understanding message handler argument lifetime is crucial when developing applications using our library. The arguments provided to the message handlers are valid for a specific duration:

### Lifetime Details
- **Transient**: For short-lived messages that do not need to persist beyond the current operation. Once the operation is complete, the message and its associated arguments are discarded. 
- **Scoped**: This lifetime is suitable for values that exist while the handler is processing a single message.
- **Singleton**: Arguments marked as singleton persist throughout the life of the application, making them ideal for configurations and shared resources.

Proper management of these lifetimes ensures optimal performance and resource utilization. Use the appropriate lifetime for your needs to avoid potential memory leaks or unintended behavior.

--- 

**Note**: This README is continually updated to reflect the latest changes and improvements in the library. Make sure to check regularly for updates.