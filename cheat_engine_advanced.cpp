#include <windows.h>
#include <psapi.h>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <string>
#include <cstdlib>

std::mutex print_mutex;

// �ѧ��ѹ�� PID �ҡ��������
DWORD FindProcessId(const std::wstring& processName) {
    DWORD processIds[1024], bytesReturned;
    if (!EnumProcesses(processIds, sizeof(processIds), &bytesReturned)) {
        std::cout << "Error: Failed to enumerate processes. Error code: " << GetLastError() << std::endl;
        return 0;
    }

    DWORD processCount = bytesReturned / sizeof(DWORD);
    for (DWORD i = 0; i < processCount; i++) {
        if (processIds[i] == 0) continue;
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processIds[i]);
        if (hProcess) {
            WCHAR szProcessName[MAX_PATH] = L"";
            if (GetModuleBaseNameW(hProcess, NULL, szProcessName, sizeof(szProcessName) / sizeof(WCHAR))) {
                if (_wcsicmp(szProcessName, processName.c_str()) == 0) {
                    CloseHandle(hProcess);
                    return processIds[i];
                }
            }
            CloseHandle(hProcess);
        }
    }
    return 0;
}

// �ѧ��ѹ�᡹˹��¤�����㹪�ǧ����˹�
void ScanMemoryRange(HANDLE hProcess, SIZE_T startAddr, SIZE_T endAddr, int valueToFind, std::vector<SIZE_T>& foundAddresses) {
    MEMORY_BASIC_INFORMATION mbi;
    SIZE_T address = startAddr;

    while (address < endAddr) {
        try {
            if (!VirtualQueryEx(hProcess, (LPCVOID)address, &mbi, sizeof(mbi))) {
                std::cout << "VirtualQueryEx failed at address: 0x" << std::hex << address << std::dec << ". Error code: " << GetLastError() << std::endl;
                address += 0x1000;
                continue;
            }

            if (mbi.State == MEM_COMMIT && (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE)) && !(mbi.Protect & PAGE_GUARD)) {
                std::cout << "Scanning region: 0x" << std::hex << address << " to 0x" << (address + mbi.RegionSize) << std::dec << std::endl;
                BYTE* buffer = new BYTE[mbi.RegionSize];
                SIZE_T bytesRead;
                if (ReadProcessMemory(hProcess, (LPCVOID)address, buffer, mbi.RegionSize, &bytesRead)) {
                    for (SIZE_T i = 0; i < bytesRead - sizeof(int); i += sizeof(int)) {
                        try {
                            int* value = (int*)(buffer + i);
                            if (*value == valueToFind) {
                                SIZE_T foundAddress = address + i;
                                {
                                    std::lock_guard<std::mutex> lock(print_mutex);
                                    foundAddresses.push_back(foundAddress);
                                    std::cout << "Found value " << valueToFind << " at address: 0x" << std::hex << foundAddress << std::dec << std::endl;
                                }
                            }
                        }
                        catch (...) {
                            continue;
                        }
                    }
                }
                else {
                    std::cout << "ReadProcessMemory failed at address: 0x" << std::hex << address << std::dec << ". Error code: " << GetLastError() << std::endl;
                }
                delete[] buffer;
            }
            address += mbi.RegionSize;
        }
        catch (...) {
            std::cout << "Exception occurred at address: 0x" << std::hex << address << std::dec << std::endl;
            address += 0x1000;
        }
    }
}

int main() {
    std::wstring processName;
    std::wcout << "Enter process name (e.g., test.exe): ";
    std::getline(std::wcin, processName);

    // �� PID �ͧ����
    DWORD pid = FindProcessId(processName);
    if (pid == 0) {
        std::cout << "Process not found!" << std::endl;
        system("pause");
        return 1;
    }

    // �Դ����
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        std::cout << "Failed to open process! Error code: " << GetLastError() << std::endl;
        system("pause");
        return 1;
    }

    std::cout << "Found process with PID: " << pid << std::endl;

    // �Ѻ��ҷ���ͧ�����
    int valueToFind;
    std::cout << "Enter the value to find (e.g., 95): ";
    std::cin >> valueToFind;
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    // �᡹˹��¤�����
    std::vector<SIZE_T> foundAddresses;
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    // �ӡѴ��ǧ����᡹���᤺ŧ����������Ǣ��
    SIZE_T startAddr = 0x1000000; // �������� 16MB
    SIZE_T endAddr = 0x7FFFFFFF;  // ����ش��� 2GB

    std::cout << "Starting memory scan... This may take a while." << std::endl;

    // �� multi-threading ����������������
    const int numThreads = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;
    SIZE_T rangeSize = (endAddr - startAddr) / numThreads;

    for (int i = 0; i < numThreads; i++) {
        SIZE_T threadStart = startAddr + (i * rangeSize);
        SIZE_T threadEnd = (i == numThreads - 1) ? endAddr : threadStart + rangeSize;
        threads.emplace_back(ScanMemoryRange, hProcess, threadStart, threadEnd, valueToFind, std::ref(foundAddresses));
    }

    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << "Memory scan completed." << std::endl;

    if (foundAddresses.empty()) {
        std::cout << "No addresses found with that value!" << std::endl;
        system("pause");
        CloseHandle(hProcess);
        return 1;
    }

    // �Ѻ���������ѧ����¹�ŧ
    int newValue;
    std::cout << "\nChange the value in the program (e.g., from 95 to 100), then press Enter to continue..." << std::endl;
    std::cin.get();
    std::cout << "Enter the new value in game (e.g., 100): ";
    std::cin >> newValue;
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    // ��ͧ���
    std::vector<SIZE_T> filteredAddresses;
    for (SIZE_T address : foundAddresses) {
        int value;
        SIZE_T bytesRead;
        try {
            if (ReadProcessMemory(hProcess, (LPCVOID)address, &value, sizeof(value), &bytesRead)) {
                if (value == newValue) {
                    filteredAddresses.push_back(address);
                    std::cout << "Found new value " << newValue << " at address: 0x" << std::hex << address << std::dec << std::endl;
                }
            }
            else {
                std::cout << "Failed to read memory at address: 0x" << std::hex << address << std::dec << ". Error code: " << GetLastError() << std::endl;
            }
        }
        catch (...) {
            std::cout << "Exception occurred while reading memory at address: 0x" << std::hex << address << std::dec << std::endl;
            continue;
        }
    }

    // �������ͤ������ ����᡹���������
    if (filteredAddresses.empty()) {
        std::cout << "No addresses found with the new value! Rescanning memory..." << std::endl;
        foundAddresses.clear();
        for (int i = 0; i < numThreads; i++) {
            SIZE_T threadStart = startAddr + (i * rangeSize);
            SIZE_T threadEnd = (i == numThreads - 1) ? endAddr : threadStart + rangeSize;
            threads.emplace_back(ScanMemoryRange, hProcess, threadStart, threadEnd, newValue, std::ref(foundAddresses));
        }

        for (auto& thread : threads) {
            thread.join();
        }

        filteredAddresses = foundAddresses;
        if (filteredAddresses.empty()) {
            std::cout << "Still no addresses found with the new value!" << std::endl;
            system("pause");
            CloseHandle(hProcess);
            return 1;
        }
        else {
            for (SIZE_T address : filteredAddresses) {
                std::cout << "Found new value " << newValue << " at address: 0x" << std::hex << address << std::dec << std::endl;
            }
        }
    }

    // ��䢤��
    int targetValue;
    std::cout << "\nEnter the value to set (e.g., 200): ";
    std::cin >> targetValue;
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    for (SIZE_T address : filteredAddresses) {
        SIZE_T bytesWritten;
        DWORD oldProtect;
        try {
            // ����¹��û�ͧ�ѹ˹��¤�����
            if (!VirtualProtectEx(hProcess, (LPVOID)address, sizeof(targetValue), PAGE_EXECUTE_READWRITE, &oldProtect)) {
                std::cout << "VirtualProtectEx failed at address: 0x" << std::hex << address << std::dec << ". Error code: " << GetLastError() << std::endl;
                continue;
            }

            if (WriteProcessMemory(hProcess, (LPVOID)address, &targetValue, sizeof(targetValue), &bytesWritten)) {
                std::cout << "Successfully set value to " << targetValue << " at address: 0x" << std::hex << address << std::dec << std::endl;
            }
            else {
                std::cout << "Failed to set value at address: 0x" << std::hex << address << std::dec << ". Error code: " << GetLastError() << std::endl;
            }

            // �׹��ҡ�û�ͧ�ѹ˹��¤�����
            DWORD dummy;
            VirtualProtectEx(hProcess, (LPVOID)address, sizeof(targetValue), oldProtect, &dummy);
        }
        catch (...) {
            std::cout << "Exception occurred while writing memory at address: 0x" << std::hex << address << std::dec << std::endl;
            continue;
        }
    }

    std::cout << "Press any key to exit..." << std::endl;
    system("pause");
    CloseHandle(hProcess);
    return 0;
}