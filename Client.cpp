#include <iostream>
#include <Windows.h>
#include <fstream>
#include <cstring>
#include <limits>

#ifdef max
#undef max
#endif

using namespace std;

struct Employee {
    int num;
    char name[20]; 
    double hours;
};

struct Message {
    DWORD type;
    DWORD client_id;
    char operation[20]; 
    int num;
    Employee data;
};

void serializeEmployee(const Employee& employee, ofstream& fout) {
    fout.write((char*)&employee.num, sizeof(employee.num));
    fout.write(employee.name, sizeof(employee.name)); 
    fout.write((char*)&employee.hours, sizeof(employee.hours));
}

void deserializeEmployee(Employee& employee, ifstream& fin) {
    fin.read((char*)&employee.num, sizeof(employee.num));
    fin.read(employee.name, sizeof(employee.name)); 
    fin.read((char*)&employee.hours, sizeof(employee.hours));
}

int main(int argc, char* argv[]) {
    DWORD bytesRead;
    Message message;
    message.client_id = GetCurrentProcessId();

    wchar_t pipeName[50];
    wsprintfW(pipeName, L"\\\\.\\pipe\\Employee_pipe");

    HANDLE hNamedPipe = CreateFile(pipeName, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

    if (hNamedPipe == INVALID_HANDLE_VALUE) {
        cerr << "Connection with the named pipe failed: " << GetLastError() << "\n";
        return 1;
    }

    string operation;
    while (true) {
        cout << "Enter operation (read, write, exit): ";
        cin >> operation;

        if (operation == "exit") {
            message.type = 0;
            strcpy_s(message.operation, "exit");
            DWORD bytesWritten;
            if (!WriteFile(hNamedPipe, &message, sizeof(message), &bytesWritten, NULL)) {
                cerr << "Error writing to pipe: " << GetLastError() << std::endl;
                break;
            }
            break;
        }

        message.type = 1; 
        strcpy_s(message.operation, operation.c_str());

        if (operation == "write") {
            cout << "Enter employee data (num, name, hours): ";
            cin >> message.data.num >> message.data.name >> message.data.hours;

            if (cin.fail()) {
                cerr << "Invalid input for employee data.\n";
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                continue;
            }

            if (!WriteFile(hNamedPipe, &message, sizeof(message), NULL, NULL)) {
                cerr << "Error sending write request: " << GetLastError() << "\n";
                CloseHandle(hNamedPipe);
                return 1;
            }

            Message response;
            if (!ReadFile(hNamedPipe, &response, sizeof(response), &bytesRead, NULL) || bytesRead == 0) {
                cerr << "Error receiving confirmation for write operation: " << GetLastError() << "\n";
            }
            else if (strcmp(response.operation, "write_ok") != 0) {
                cerr << "Server returned an error for write operation: " << response.operation << "\n";
            }
            else {
                cout << "Employee written successfully.\n";
            }
        }
        else if (operation == "read") {
            cout << "Enter employee number to read: ";
            cin >> message.num;

            if (cin.fail()) {
                cerr << "Invalid input for employee number.\n";
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                continue;
            }

            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            strcpy_s(message.operation, "read"); 

            if (!WriteFile(hNamedPipe, &message, sizeof(message), NULL, NULL)) {
                cerr << "Error sending read request: " << GetLastError() << "\n";
                CloseHandle(hNamedPipe);
                return 1;
            }

            if (!ReadFile(hNamedPipe, &message, sizeof(message), &bytesRead, NULL) || bytesRead == 0) {
                cerr << "Error reading response from server: " << GetLastError() << "\n";
                CloseHandle(hNamedPipe);
                return 1;
            }
            else if (strcmp(message.operation, "read_ok") == 0) {
                cout << "Employee Data:\n";
                cout << "ID: " << message.data.num << ", Name: " << message.data.name
                    << ", Hours: " << message.data.hours << "\n";
            }
            else {
                cout << "Incorrect ID, please enter correct ID\n";
            }
        }
    }
    CloseHandle(hNamedPipe);
    return 0;
}
