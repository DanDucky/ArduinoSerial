#include <iostream>
#include <fstream>
#include <vector>
#include <bitset>
#include <serial/serial.h>
#include <cstdint>
#include "../include/progressbar.hpp"
#include "../include/universalGetopt.hpp"

#define dcout if (debug) std::cout << "[DEBUG] "
#define end "\n"
#define INIT_ARRAY(array, size) for (int i = 0; i < size; i++) { array[i] = 0; }
#define declareProcess(process)     \
uint8_t opcode = process;           \
serialConnection->write(&opcode, 1)
#define readSignal() static_cast<uint8_t>(serialConnection->read(1)[0])

using namespace std;

bool debug;

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

    serialConnection->waitReadable();

    InputBuffer bufferSize(2);
    serialConnection->read(bufferSize, bufferSize.size());

    dcout << "reported buffer size: " << (uint16_t) bufferSize << end;

    const auto numberOfPackets = (short) (fileSize / bufferSize + (fileSize % bufferSize == 0 ? 0 : 1));
    progressbar bar(numberOfPackets);
    bar.set_opening_bracket_char("[");
    bar.set_closing_bracket_char("]");
    bar.set_todo_char(" ");
    bar.set_done_char("#");
    for (int i = 0; i < numberOfPackets; i++) { // iterates once for every packet needed to send
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
        bar.update();
    }
    cout << end;
    return readSignal();
}

uint8_t readProcess(serial::Serial* serialConnection, unsigned char* fileFromArduino, uint16_t fileSize) {
    declareProcess(READ);

    serialConnection->write((uint8_t*)&fileSize, 2);

    progressbar bar(fileSize);
    bar.set_opening_bracket_char("[");
    bar.set_closing_bracket_char("]");
    bar.set_todo_char(" ");
    bar.set_done_char("#");
    for (int i = 0; i < fileSize; i++) {
        if (serialConnection->read(&fileFromArduino[i], 1) != 1) return 1;
        bar.update();
    }
    cout << end;

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

    string port;
    string file;

    if (argc > 1) { // args == true
        int opt;
        while((opt = uni::getopt(argc, argv, "p:f:sd")) != -1) {
            switch(opt) {
                case 'p':
                    port = uni::optarg;
                    break;
                case 'f':
                    file = uni::optarg;
                    break;
                case 's':
                    autoSelectPort(port);
                    if (port.empty()) {
                        cout << "failed to auto select a port, referring to manual selection\n";
                    }
                    break;
                case 'd':
                    debug = true;
                    dcout << "debug enabled" << end;
                    break;
                case '?':
                    cout << "unknown option: " << uni::optopt << "\nexiting...\n";
                    return 1;
                default:
                    cout << "huh?\nexiting...\n";
                    return 1;
            }
        }
    }
    if (file.empty()) {
        cout << "please enter the file you would like to send over serial: ";
        cin >> file;
        cout << "\n";
    }
    if (port.empty()) {
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
        cout << "\n" << port << endl;
    }

    ifstream input(file, std::ifstream::binary | std::ifstream::ate); // binary file beginning at the last index
    if (!input.is_open()) {
        cout << "failed to open file: " << file << "\nexiting...\n";
        return 1;
    }
    const long fileSize = getSize(input);
    auto *byteFile = new unsigned char[fileSize];
    input.getline(reinterpret_cast<char*>(byteFile), fileSize);

    cout << "connecting to port " << port << " with file " << file << " of size " << fileSize << "\n";

    serial::Serial serialConnection(port, 9600, serial::Timeout::simpleTimeout(10000));
    serialConnection.flush();
    writeProcess(&serialConnection, byteFile, fileSize);
    serialConnection.flush();
    auto *fileFromArduino = new unsigned char[fileSize];
    INIT_ARRAY(fileFromArduino, fileSize);
    if (readProcess(&serialConnection, fileFromArduino, fileSize) == 1) return 1;
    serialConnection.close();
    checkDiff(fileFromArduino, byteFile, fileSize);

    delete[] fileFromArduino;
    delete[] byteFile;
}
