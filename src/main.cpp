#include <iostream>
#include <fstream>
#include <vector>
#include <bitset>
#include <serial/serial.h>
#include <unistd.h>
//#include <sys/ioctl.h>

#define dcout if (debug) std::cout << "[DEBUG] "
#define end "\n"
#define INIT_ARRAY(array, size) for (int i = 0; i < size; i++) { array[i] = 0; }

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

uint8_t readByte(serial::Serial* serialConnection, uint8_t* data, uint16_t address) {
    uint8_t process = READ_BYTE;
    serialConnection->write(&process, 1);
    serialConnection->write((uint8_t*)&address, 2);
    serialConnection->read(data, 1);
    uint8_t signal;
    serialConnection->read(&signal, 1);
    return signal;
}

uint8_t writeByte(serial::Serial* serialConnection, uint8_t data, uint16_t address) {
    uint8_t process = WRITE_BYTE;
    serialConnection->write(&process, 1);
    serialConnection->write((uint8_t*)&address, 2);
    serialConnection->write(&data, 1);
    uint8_t signal;
    serialConnection->read(&signal, 1);
    return signal;
}

uint8_t writeProcess(serial::Serial* serialConnection, const char* byteFile, unsigned long fileSize) {
//    for (int i = 0; i< fileSize; i++) {
//        writeByte(serialConnection, byteFile[i], i);
//    }
//    return 1;


    uint8_t opcode = WRITE;
    serialConnection->write(&opcode, 1);

    serialConnection->waitReadable();

    InputBuffer bufferSize(2);
    serialConnection->read(bufferSize, bufferSize.size());

    dcout << "reported buffer size: " << (uint16_t) bufferSize << end;

    const auto numberOfPackets = (short) (fileSize / bufferSize + (fileSize % bufferSize == 0 ? 0 : 1));
    for (int i = 0; i < numberOfPackets; i++) { // iterates once for every packet needed to send
        dcout << "iteration: " << i << end;
//        while(serialConnection.read(&flag, 1) != 1) {
//            dcout << "waiting" << end;
//        }
//        serialConnection.waitReadable();
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
    uint8_t signal = 0x00;
    serialConnection->read(&signal, 1);
    return signal;
}

uint8_t readProcess(serial::Serial* serialConnection, unsigned char* fileFromArduino, uint16_t fileSize) {
    uint8_t opcode = READ;
    serialConnection->write(&opcode, 1);

    serialConnection->write((uint8_t*)&fileSize, 2);

    dcout;
    for (int i = 0; i < fileSize; i++) {
        if (serialConnection->read(&fileFromArduino[i], 1) != 1) return 1;
        if (debug) cout << i<<".";
    }
    if (debug) cout << end;

    uint8_t signal = 0x00;
    serialConnection->read(&signal, 1);
    dcout << "read signal as " << std::bitset<8>(signal) << end;
    return signal;
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
        while((opt = getopt(argc, argv, "p:f:sd")) != -1) {
            switch(opt) {
                case 'p':
                    port = optarg;
                    break;
                case 'f':
                    file = optarg;
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
                    cout << "unknown option: " << optopt << "\nexiting...\n";
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
    char *byteFile = new char[fileSize];
    input.getline(byteFile, fileSize);

//    winsize terminal{};
//    ioctl(0, TIOCGWINSZ, &terminal); // get terminal dimensions
//    const unsigned short columns = terminal.ws_col;
//    dcout << "terminal columns: " << columns << end;

    cout << "connecting to port " << port << " with file " << file << " of size " << fileSize << "\n";

//    for (int i = 0; i < fileSize; i++) {
//        byteFile[i] = 0xFF;
//    }

    serial::Serial serialConnection(port, 9600, serial::Timeout::simpleTimeout(10000));
    serialConnection.flush();
    writeProcess(&serialConnection, byteFile, fileSize);
    serialConnection.flush();
//    for (int i = 0; i < 50000; i++) {
//        writeByte(&serialConnection, 0, i);
//        uint8_t out;
//        readByte(&serialConnection, &out, i);
//        if (out != 0) dcout << std::bitset<8>(out) << " = " << i << end;
//    }

//    process = READ_BYTE;
//    serialConnection.write(&process, 1);
//    serialConnection.write((uint8_t*)&address, 2);
//    serialConnection.read(&data, 1);
//    serialConnection.read(&signal, 1);
//    dcout << std::bitset<8>(data) << end;
//    writeProcess(&serialConnection, byteFile, fileSize);
//    serialConnection.flush();

    auto *fileFromArduino = new unsigned char[fileSize];
    INIT_ARRAY(fileFromArduino, fileSize);

    if (readProcess(&serialConnection, fileFromArduino, fileSize) == 1) return 1;
    serialConnection.close();
    int missed  =0 ;
    for (int i = 0; i < fileSize; i++) {
        if (byteFile[i] != static_cast<char>(fileFromArduino[i])) {
            dcout << "ERROR AT INDEX " << i << " WHERE FILE IS " << std::bitset<8>(byteFile[i]) << " AND ROM IS " << std::bitset<8>(fileFromArduino[i]) << end;
            missed++;
        }
    }
    dcout << "missed " << missed << " words" << end;


    delete[] fileFromArduino;
    delete[] byteFile;
}
