#include <iostream>
#include <fstream>
#include <vector>
#include <bitset>
#include <serial/serial.h>
#include <cstdint>
#include <indicators.hpp>
#include <optargParser.hpp>

#define dcout if (debug) std::cout << "[DEBUG] "
#define end "\n"
#define INIT_ARRAY(array, size) for (int i = 0; i < size; i++) { array[i] = 0; }
#define declareProcess(process)     \
uint8_t opcode = process;           \
serialConnection->write(&opcode, 1)
#define readSignal() static_cast<uint8_t>(serialConnection->read(1)[0])

using namespace std;
using namespace indicators;

bool debug;
#define defaults \
    option::BarWidth{50},\
    option::Start{"["},\
    option::Fill{"#"},\
    option::Remainder{"-"},\
    option::End{"]"},\
    option::ShowElapsedTime{true}, \
    option::ShowRemainingTime{true}

class InputBuffer {
public:
    explicit InputBuffer(const int sizeInBytes) : bufferSize(sizeInBytes) {
        buffer = new uint8_t[sizeInBytes];
    }
    ~InputBuffer() {
        delete[] buffer;
    }
    int size () const {
        return bufferSize;
    }
    operator uint8_t*() const {
        return buffer;
    }
    operator uint16_t() const {
        uint16_t num;
        num = buffer[0];
        num |= (buffer[1] << 8);
        return num;
    }
private:
    const int bufferSize;
    uint8_t* buffer;
};

long getSize(std::ifstream &file) {
    long out = file.tellg();
    file.seekg(0);
    return out;
}

template<typename t>
uint8_t* getArrayFrom(t* value) {
    return (uint8_t*)value;
}

enum Process {
    WRITE,
    READ,
    WRITE_BYTE,
    READ_BYTE
};

void checkDiff(unsigned char* x, unsigned char* y, size_t size) {
    for (size_t i = 0; i < size; i++) {
        if (x[i] != y[i]) cout << "error at index " << i << " (" << std::bitset<16>(i) << ") where ROM is " << std::bitset<8>(x[i]) << " and file is " << std::bitset<8>(y[i]) << end;
    }
}

uint8_t readByte(serial::Serial* serialConnection, uint8_t* data, uint16_t address) {
    declareProcess(READ_BYTE);
    serialConnection->write((uint8_t*)&address, 2);
    serialConnection->read(data, 1);
    return readSignal();
}

uint8_t writeByte(serial::Serial* serialConnection, uint8_t data, uint16_t address) {
    declareProcess(WRITE_BYTE);
    serialConnection->write((uint8_t*)&address, 2);
    serialConnection->write(&data, 1);
    return readSignal();
}

uint8_t writeProcess(serial::Serial* serialConnection, const unsigned char* byteFile, unsigned long fileSize) {
    declareProcess(WRITE);

    InputBuffer bufferSize(2);
    serialConnection->read(bufferSize, bufferSize.size());

    dcout << "reported buffer size: " << (uint16_t) bufferSize << end;

    const auto numberOfPackets = (short) (fileSize / bufferSize + (fileSize % bufferSize == 0 ? 0 : 1));
    ProgressBar bar{
        defaults,
        option::PostfixText{"sending file to ROM"}
    };
    for (int i = 0; i < numberOfPackets; i++) { // iterates once for every packet needed to send
        bar.set_progress(100 * i / numberOfPackets);
        dcout << "iteration: " << i << end;
        uint8_t flag = 0;
        serialConnection->read(&flag, 1);
        if (flag != 0xFF) return 1;
        uint16_t remainingBytes = fileSize - (bufferSize * i) + 1;
        if (remainingBytes <= bufferSize) {
            serialConnection->write(getArrayFrom<uint16_t>(&remainingBytes), 2);
        } else {
            serialConnection->write(bufferSize, 2);
        }
        const auto packetSize = (remainingBytes <= bufferSize ? remainingBytes : bufferSize);
        dcout << "\tsize of packet: " << packetSize << end;
        const auto* out = static_cast<const uint8_t*>(static_cast<const void*>(&(byteFile[i * bufferSize])));
        serialConnection->write(out, packetSize);
    }
    bar.set_progress(100);
    return readSignal();
}

uint8_t readProcess(serial::Serial* serialConnection, unsigned char* fileFromArduino, uint16_t fileSize) {
    declareProcess(READ);

    serialConnection->write((uint8_t*)&fileSize, 2);

    ProgressBar bar{
            defaults,
            option::PostfixText{"reading back ROM"}
    };
    for (int i = 0; i < fileSize; i++) {
        bar.set_progress(100*i/fileSize);
        if (serialConnection->read(&fileFromArduino[i], 1) != 1) return 1;
    }
    bar.set_progress(100);

    return readSignal();
}

void autoSelectPort(string & port) {
    const vector<serial::PortInfo> devicesFound = serial::list_ports();
    for (const serial::PortInfo & portInfo : devicesFound) {
        if (portInfo.description.contains("Arduino")) {
            port = portInfo.port;
            return;
        }
    }
}

int main(int argc, char **argv) {

    cli::Opt<std::string, CLI_OPTION_NULL> port("Serial Port", "serial port to communicate with arduino", "p", "port");
    cli::Opt<std::string, CLI_OPTION_NULL> file("File", "file to flash to the eeprom", "f", "file", "hex", "bin", "b");
    cli::Opt<bool, CLI_OPTION_NULL> debugInput("Debug", "enter debug mode (with debug prints)", "d", "debug", "dev");
    cli::Opt<bool, CLI_OPTION_NULL> autoSelect("Auto Port", "auto select port if port is not specified", "s", "auto", "select");

    cli::parse(argc, argv, port, file, debugInput, autoSelect);
    if (debugInput) debug = true;
    if (autoSelect) {
        std::string selected;
        autoSelectPort(selected);
        if (selected.empty()) {
            cout << "failed to auto select a port, referring to manual selection\n";
        } else {
            port.set(selected);
        }
    }
    if (!file.hasValue()) {
        cout << "please enter the file you would like to send over serial: ";
        std::string inputFile;
        cin >> inputFile;
        file.set(inputFile);
        cout << "\n";
    }
    if (!port.hasValue()) {
        vector<serial::PortInfo> devicesFound = serial::list_ports();

        int portSelectionNumber = 0;
        vector<serial::PortInfo> validDevices;
        cout << "list of open ports:\n";
        for (const auto & portInfo : devicesFound) {
            if (portInfo.hardware_id == "n/a") continue;
            validDevices.push_back(portInfo);
            cout << "\t" << portSelectionNumber << ": " << portInfo.port << " | " << portInfo.description << "\n";
            portSelectionNumber++;
        }

        if (validDevices.empty()) {
            cout << "\nfailed to find any connected serial devices, checked " << devicesFound.size() << " ports\nexiting...\n";
            return 1;
        }

        cout << "please enter the number corresponding to the port of the arduino: ";
        string selection;
        cin >> selection;
        if (selection.empty() || stoi(selection) >= portSelectionNumber || stoi(selection) < 0) {
            cout << "\nunknown port: " << selection << "\nexiting...\n";
            return 1;
        }
        port = validDevices[stoi(selection)].port;
        cout << "\n" << (std::string)port << endl;
    }

    ifstream input(file, std::ifstream::binary | std::ifstream::ate); // binary file beginning at the last index
    if (!input.is_open()) {
        cout << "failed to open file: " << (std::string)file << "\nexiting...\n";
        return 1;
    }
    const long fileSize = getSize(input);
    auto *byteFile = new unsigned char[fileSize];
    input.getline(reinterpret_cast<char*>(byteFile), fileSize);

    cout << "connecting to port " << (std::string)port << " with file " << (std::string)file << " of size " << fileSize << "\n";

    show_console_cursor(false);
    serial::Serial serialConnection(port, 9600, serial::Timeout::simpleTimeout(10000));
    serialConnection.flush();
    writeProcess(&serialConnection, byteFile, fileSize);
    serialConnection.flush();
    auto *fileFromArduino = new unsigned char[fileSize];
    if (readProcess(&serialConnection, fileFromArduino, fileSize) == 1) return 1;
    serialConnection.close();
    checkDiff(fileFromArduino, byteFile, fileSize);
    show_console_cursor(true);

    delete[] fileFromArduino;
    delete[] byteFile;

    cout << "finished flashing ROM!" << end;
}
