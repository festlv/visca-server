#include <iostream>
#include <CppLinuxSerial/SerialPort.hpp>
#include <args.hxx>
#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <utility>
#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>


using namespace mn::CppLinuxSerial;

typedef struct {
    std::string serial_port;
    int         tcp_server_port;
    bool        args_valid;
} args_t;

args_t parseArguments(int argc, char** argv) {
    args_t ret;
    ret.args_valid = false;
    args::ArgumentParser parser("VISCA control server");
    args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});
    args::ValueFlag<int> port(parser, "tcp-port", "TCP server port", {'p'}, 558);
    args::Positional<std::string> serial_port(parser, "serial_port", "Serial port");
    try
    {
        parser.ParseCLI(argc, argv);
        if (serial_port) {
            ret.args_valid = true;
            ret.serial_port = args::get(serial_port);
            ret.tcp_server_port = args::get(port);
        } else {
            std::cout << parser;
        }
    }
    catch (args::Help)
    {
        std::cout << parser;
    }
    catch (args::ParseError e)
    {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
    }
    catch (args::ValidationError e)
    {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
    }

    return ret;
}


using asio::ip::tcp;

const int max_length = 1024;

void session(tcp::socket sock, SerialPort* serial)
{

    std::vector<uint8_t> serialRxData;
    char data[max_length];

    try
    {
        for (;;)
        {

            std::error_code error;
            size_t length = sock.read_some(asio::buffer(data), error);
            if (error == asio::stream_errc::eof)
                break; // Connection closed cleanly by peer.
            else if (error)
                throw std::system_error(error); // Some other error.
            if (length > 0) {
                serial->WriteBinary(std::vector<uint8_t>(data, data+length));
                serialRxData.clear();
                serial->ReadBinary(serialRxData);
                if (!serialRxData.empty())
                    asio::write(sock, asio::buffer(serialRxData.data(), serialRxData.size()));
            }
        }
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception in thread: " << e.what() << "\n";
    }
}

void server(asio::io_context& io_context, unsigned short port, SerialPort* serial)
{
    tcp::acceptor a(io_context, tcp::endpoint(tcp::v4(), port));
    for (;;)
    {
        tcp::socket sock(io_context);
        a.accept(sock);
        std::thread(session, std::move(sock), serial).detach();
    }
}

int main(int argc, char** argv) {

    auto args = parseArguments(argc, argv);
    if (!args.args_valid) {
        return 1;
    }
    std::cout << "starting VISCA server, serial: " << args.serial_port << ", TCP control port: " << args.tcp_server_port << "..." << std::endl;
    //first, initialize serial port at 115200 and send Pelco AF data
    SerialPort serialPort(args.serial_port, BaudRate::B_115200);
    serialPort.SetTimeout(100); // Block for up to 100ms to receive data
    serialPort.Open();

    std::vector<uint8_t> pelco_af_data({0xff, 0x01, 0x80, 0x20, 0x0A, 0x5F, 0x03, 0x53, 0x60});
    for (int i=0; i < 3; i++) {
        serialPort.WriteBinary(pelco_af_data);
        usleep(100 * 1000);
    }
    //reconfigure to VISCA baud rate
    serialPort.SetBaudRate(BaudRate::B_9600);
    usleep(1000 * 1000);

    std::vector<uint8_t> visca_manual_focus({0x81, 0x01, 0x04, 0x38, 0x03, 0xFF});
    std::vector<uint8_t> visca_zoom_pos({0x81, 0x01, 0x04, 0x47, 0x00, 0x00, 0x00, 0x00, 0xFF});
    std::vector<uint8_t> visca_focus_pos({0x81, 0x01, 0x04, 0x48, 0x01, 0x00, 0x00, 0x00, 0xFF});
    for (int i=0; i < 3; i++) {
        serialPort.WriteBinary(visca_manual_focus);
        usleep(100 * 1000);
        serialPort.WriteBinary(visca_zoom_pos);
        usleep(100 * 1000);
        serialPort.WriteBinary(visca_focus_pos);
        usleep(100 * 1000);
    }

    try
    {
        asio::io_context io_context;

        server(io_context, args.tcp_server_port, &serialPort);
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    serialPort.Close();
    return 0;
}
