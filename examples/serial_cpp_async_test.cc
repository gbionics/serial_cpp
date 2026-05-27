#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <string>
#include <vector>

#include "serial_cpp/serial.h"

// Test program to demonstrate concurrent read/write with the async Windows implementation.
// Build with: cmake -B build -DSERIAL_CPP_WIN_ASYNC=ON && cmake --build build
//
// Usage: serial_cpp_async_test <port> [baudrate]
// Example: serial_cpp_async_test COM3 115200
//
// This test spawns a reader thread and a writer thread that operate simultaneously.
// With the blocking implementation, the writer would stall while the reader is
// waiting for data. With the async implementation, both proceed independently.

std::atomic<bool> running{true};

void reader_thread(serial_cpp::Serial &serial)
{
    std::cout << "[Reader] Started. Waiting for incoming data...\n";
    while (running) {
        try {
            std::string data = serial.read(256);
            if (!data.empty()) {
                std::cout << "[Reader] Received " << data.size() << " bytes: ";
                // Print as hex for binary data, or as string if printable
                bool printable = true;
                for (char c : data) {
                    if (c < 0x20 && c != '\n' && c != '\r' && c != '\t') {
                        printable = false;
                        break;
                    }
                }
                if (printable) {
                    std::cout << "\"" << data << "\"";
                } else {
                    for (unsigned char c : data) {
                        printf("%02X ", c);
                    }
                }
                std::cout << "\n";
            }
        } catch (const serial_cpp::IOException &e) {
            std::cerr << "[Reader] IOException: " << e.what() << "\n";
        } catch (const serial_cpp::PortNotOpenedException &e) {
            std::cerr << "[Reader] Port not open: " << e.what() << "\n";
            break;
        }
    }
    std::cout << "[Reader] Stopped.\n";
}

void writer_thread(serial_cpp::Serial &serial)
{
    std::cout << "[Writer] Started. Sending periodic messages...\n";
    int counter = 0;
    while (running) {
        try {
            std::string msg = "Hello #" + std::to_string(counter++) + "\r\n";
            size_t written = serial.write(msg);
            std::cout << "[Writer] Sent " << written << " bytes: \"" << msg.substr(0, msg.size() - 2) << "\"\n";
        } catch (const serial_cpp::IOException &e) {
            std::cerr << "[Writer] IOException: " << e.what() << "\n";
        } catch (const serial_cpp::PortNotOpenedException &e) {
            std::cerr << "[Writer] Port not open: " << e.what() << "\n";
            break;
        }

        // Send every 1 second
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << "[Writer] Stopped.\n";
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <port> [baudrate]\n";
        std::cerr << "Example: " << argv[0] << " COM3 115200\n";
        std::cerr << "\nAvailable ports:\n";
        std::vector<serial_cpp::PortInfo> ports = serial_cpp::list_ports();
        for (const auto &port : ports) {
            std::cout << "  " << port.port << " - " << port.description << "\n";
        }
        return 1;
    }

    std::string port_name = argv[1];
    unsigned long baudrate = 115200;
    if (argc >= 3) {
        baudrate = std::stoul(argv[2]);
    }

    std::cout << "=== serial_cpp Async I/O Test ===\n";
    std::cout << "Port: " << port_name << "\n";
    std::cout << "Baudrate: " << baudrate << "\n";
    std::cout << "Press Enter to stop...\n\n";

    try {
        // Set a short read timeout so the reader doesn't block forever
        serial_cpp::Timeout timeout = serial_cpp::Timeout::simpleTimeout(100);

        serial_cpp::Serial serial(port_name, baudrate, timeout);

        if (!serial.isOpen()) {
            std::cerr << "Error: Failed to open port " << port_name << "\n";
            return 1;
        }

        std::cout << "Port opened successfully.\n\n";

        // Launch reader and writer threads concurrently
        // With the blocking implementation, the writer would be stuck waiting
        // for the reader's ReadFile to complete/timeout before it can WriteFile.
        // With SERIAL_CPP_WIN_ASYNC=ON, both operate independently.
        std::thread reader(reader_thread, std::ref(serial));
        std::thread writer(writer_thread, std::ref(serial));

        // Wait for Enter key to stop
        std::cin.get();
        running = false;

        std::cout << "\nStopping...\n";

        reader.join();
        writer.join();

        serial.close();
        std::cout << "Port closed. Done.\n";

    } catch (const serial_cpp::IOException &e) {
        std::cerr << "IOException: " << e.what() << "\n";
        return 1;
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
