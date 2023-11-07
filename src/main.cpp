#include <iostream>
#include <fstream>
#include <vector>
#include <bitset>
#include <serial/serial.h>
#include <unistd.h>
#include <sys/ioctl.h>

using namespace std;

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
    bool debug;

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
                    cout << "debug enabled\n";
                    debug = true;
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

    winsize terminal{};
    ioctl(0, TIOCGWINSZ, &terminal); // get terminal dimensions
    const unsigned short columns = terminal.ws_col;

    cout << "connecting to port " << port << " with file " << file << "\n";

    serial::Serial serialConnection(port, 9600, serial::Timeout::simpleTimeout(1000));
    serialConnection.flush();
    serialConnection.write("write");

    serialConnection.waitReadable();

    InputBuffer bufferSize(2);
    serialConnection.read(bufferSize, bufferSize.size());

    auto numberOfPackets = (short) (fileSize / bufferSize + (fileSize % bufferSize == 0 ? 0 : 1));
    for (int i = 0; i < numberOfPackets; i++) { // iterates once for every packet needed to send
        uint8_t flag = 0;
        serialConnection.read(&flag, 1);
        uint16_t remainingBytes = fileSize - (bufferSize * i);
        if (remainingBytes <= bufferSize) {
            serialConnection.write(getArrayFrom<uint16_t>(&remainingBytes), 2);
        } else {
            serialConnection.write(bufferSize, 2);
        }
        serialConnection.write((uint8_t*)&(byteFile[i * bufferSize]), remainingBytes < bufferSize ? remainingBytes : bufferSize);
    }

    serialConnection.close();
    delete[] byteFile;
}
