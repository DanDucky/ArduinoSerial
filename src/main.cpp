#include <iostream>
#include <fstream>
#include <vector>
#include <bitset>
#include <serial/serial.h>
#include <unistd.h>

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
std::vector<uint8_t> getVectorFrom(t value) {
    std::vector<uint8_t> output;
    for (int i = 0; i < sizeof(t); i++) {
        output.push_back((value >> i * 8) & 0xFF);
    }
    return output;
}

std::vector<uint8_t> subarray(char * index, uint16_t size) {
    std::vector<uint8_t> output(index, index + size);
    return output;
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
        while((opt = getopt(argc, argv, "p:f:s")) != -1) {
            switch(opt) {
                case 'p':
                    port = optarg;
                    break;
                case 'f':
                    file = optarg;
                    break;
                case 's':
                    autoSelectPort(port);
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

    ifstream input(file, std::ifstream::binary | std::ifstream::ate);
    if (!input.is_open()) {
        cout << "failed to open file: " << file << "\nexiting...\n";
        return 1;
    }
    const long fileSize = getSize(input);
    char *byteFile = new char[fileSize];
    input.getline(byteFile, fileSize);

    for (int i = 0; i < fileSize; i++) {
        cout << byteFile[i];
    }

    cout << endl;

    cout << "connecting to port " << port << " with file " << file << "\n";

    serial::Serial serialConnection(port, 9600, serial::Timeout::simpleTimeout(1000));
    serialConnection.flushInput();
    serialConnection.write("write");

    serialConnection.waitReadable();

    InputBuffer bufferSize(2);
    serialConnection.read(bufferSize, bufferSize.size());

    std::vector<uint8_t> test[14];

    for (int i = 0; i < fileSize / bufferSize + (fileSize % bufferSize == 0 ? 0 : 1); i++) { // iterates once for every packet needed to send
        serialConnection.waitReadable();
        serialConnection.read(1);
        cout << "iteration " << i << "\n";
        uint16_t remainingBytes = fileSize - (bufferSize * i);
        if (remainingBytes <= bufferSize) {
            serialConnection.write(getVectorFrom<uint16_t>(remainingBytes));
        } else {
            serialConnection.write(bufferSize, 2);
        }
        test[i] = subarray(&(byteFile[i * bufferSize]), remainingBytes < bufferSize ? remainingBytes : bufferSize);
        serialConnection.write(subarray(&(byteFile[i * bufferSize]), remainingBytes < bufferSize ? remainingBytes : bufferSize));
    }

    /*
    bool failed = false;
    for (int i = 0; i < 14; i++) {
        for (int j = 0; j < test[i].size(); j++) {
            failed = failed || ((char) test[i][j] != byteFile[(i * 4096) + j]);
        }
    }
    cout << failed << endl;
     */

    delete[] byteFile;
}
