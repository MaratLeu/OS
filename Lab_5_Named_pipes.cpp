#include <iostream>
#include <Windows.h>
#include <fstream>
#include <cstring>

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

string filename;

void serializeEmployee(ofstream& fout, const Employee& employee) {
    fout.write((char*)&employee.num, sizeof(employee.num));
    fout.write(employee.name, sizeof(employee.name));
    fout.write((char*)&employee.hours, sizeof(employee.hours));
}

void deserializeEmployee(ifstream& fin, Employee& employee) {
    fin.read((char*)&employee.num, sizeof(employee.num));
    fin.read(employee.name, sizeof(employee.name));
    fin.read((char*)&employee.hours, sizeof(employee.hours));
}

HANDLE* hEvents;

DWORD WINAPI InstanceThread(LPVOID lpvParam) {
    HANDLE hNamedPipe = HANDLE(lpvParam);
    Message message;
    bool clientConnected = true;
    while (clientConnected) {
        DWORD bytesRead;
        BOOL result = ReadFile(hNamedPipe, &message, sizeof(message), &bytesRead, NULL);

        if (strcmp(message.operation, "exit") == 0) {
            cout << "Client has exited\n";
            clientConnected = false;
            DisconnectNamedPipe(hNamedPipe);
            break;
        }
        if (message.type == 0) {
            result = ReadFile(hNamedPipe, &message.client_id, sizeof(message.client_id), &bytesRead, NULL);
            result &= ReadFile(hNamedPipe, message.operation, sizeof(message.operation), &bytesRead, NULL);
            result &= ReadFile(hNamedPipe, &message.num, sizeof(message.num), &bytesRead, NULL);
            result &= ReadFile(hNamedPipe, &message.data, sizeof(message.data), &bytesRead, NULL);
            if (!result) {
                cerr << "Error reading message from client\n";
                clientConnected = false;
                break;
            }
        }

        else if (message.type == 1) {
            if (strcmp(message.operation, "write") == 0) {
                ofstream fout(filename, ios::binary | ios::app);
                if (fout.is_open()) {
                    fout.write((char*)&message.data, sizeof(message.data));
                    fout.close();

                    strcpy_s(message.operation, "write_ok");
                    WriteFile(hNamedPipe, &message, sizeof(message), NULL, NULL);
                }
                else {
                    cerr << "Error opening file for writing.n";
                    strcpy_s(message.operation, "write_error");
                    WriteFile(hNamedPipe, &message, sizeof(message), NULL, NULL);
                }
            }
            else if (strcmp(message.operation, "read") == 0) {
                ifstream fin(filename, ios::binary);
                bool found = false;

                if (fin.is_open()) {
                    Employee emp;
                    while (fin.read((char*)&emp, sizeof(emp))) {
                        if (emp.num == message.num) {
                            message.data = emp;
                            found = true;
                            break;
                        }
                    }
                    fin.close();

                    if (found) {
                        strcpy_s(message.operation, "read_ok");
                        WriteFile(hNamedPipe, &message, sizeof(message), NULL, NULL);
                    }
                    else {
                        strcpy_s(message.operation, "not_found");
                        WriteFile(hNamedPipe, &message, sizeof(message), NULL, NULL);
                    }
                }
                else {
                    cerr << "Error opening file for reading.\n";
                    strcpy_s(message.operation, "read_error");
                    WriteFile(hNamedPipe, &message, sizeof(message), NULL, NULL);
                }
            }
        }
    }
    DisconnectNamedPipe(hNamedPipe);
    CloseHandle(hNamedPipe);
    cout << "Instance thread exiting\n";
    return 1;
}

int main() {
    cout << "Enter name of file: ";
    cin >> filename;

    ofstream fout(filename, ios::binary);
    Employee employee;
    int count;
    cout << "Enter the amount of records: ";
    cin >> count;

    cout << "Enter the data about employees (id, name, hours): \n";
    for (int i = 0; i < count; i++) {
        cin >> employee.num >> employee.name >> employee.hours;
        serializeEmployee(fout, employee);
    }
    fout.close();

    ifstream fin(filename, ios::binary);
    cout << "Output contents to console: \n";
    while (fin.peek() != EOF) {
        deserializeEmployee(fin, employee);
        cout << employee.num << "\t" << employee.name << "\t" << employee.hours << "\n";
    }
    fin.close();

    int numClients;
    cout << "Enter num of clients: ";
    cin >> numClients;
    DWORD dwThreadId = 0;
    HANDLE* hProcesses = new HANDLE[numClients];
    HANDLE hNamedPipe = INVALID_HANDLE_VALUE, hThread = NULL;

    cout << "Waiting for clients...\n";

    for (int i = 0; i < numClients; i++) {
        wchar_t pipeName[50];
        wsprintfW(pipeName, L"\\\\.\\pipe\\Employee_pipe");
        hNamedPipe = CreateNamedPipe(
            pipeName,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            sizeof(Message),
            sizeof(Message),
            0,
            NULL
        );

        if (hNamedPipe == INVALID_HANDLE_VALUE) {
            cerr << "Creation of named pipe failed: " << GetLastError() << "\n";
            return 1;
        }

        wchar_t appName[] = L"C:/Users/Marat Leu/source/repos/Process-server/ARM64/Debug/Client.exe";
        STARTUPINFO si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        if (!CreateProcess(appName, NULL, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
            cerr << "Error launching client " << i + 1 << ": " << GetLastError() << "\n";
            return 1;
        }

        hProcesses[i] = pi.hProcess;
        if (hProcesses[i] == NULL) {
            cerr << "Error for creating Process: " << GetLastError() << "\n";
            return 1;
        }

        if (!ConnectNamedPipe(hNamedPipe, NULL)) {
            cerr << "Error connecting to named pipe: " << GetLastError() << "\n";
            CloseHandle(hNamedPipe);
            return 1;
        }
        else {
            hThread = CreateThread(NULL, 0, InstanceThread, LPVOID(hNamedPipe), 0, &dwThreadId);
            if (hThread == NULL) {
                cerr << "Error creating a thread: " << GetLastError();
                return 1;
            }
            else {
                CloseHandle(hThread);
            }
        }
    }
    WaitForMultipleObjects(numClients, hProcesses, TRUE, INFINITE);

    for (int i = 0; i < numClients; i++) {
        CloseHandle(hProcesses[i]);
    }
    delete[] hProcesses;
    Employee read_employee;
    ifstream read_bin(filename, ios::binary);
    cout << "Output contents to console: \n";
    while (read_bin.peek() != EOF) {
        deserializeEmployee(read_bin, read_employee);
        cout << read_employee.num << "\t" << read_employee.name << "\t" << read_employee.hours << "\n";
    }
    read_bin.close();
    return 0;
}
