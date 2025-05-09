#include <windows.h>
#include <psapi.h>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <string>
#include <cstdlib>

std::mutex print_mutex;

// ฟังก์ชันหา PID จากชื่อโปรเซส
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

// ฟังก์ชันสแกนหน่วยความจำในช่วงที่กำหนด
void ScanMemoryRange(HANDLE hProcess, SIZE_T startAddr, SIZE_T endAddr, int valueToFind, std::vector<SIZE_T>& foundAddresses) {
    MEMORY_BASIC_INFORMATION mbi;
    SIZE_T address = startAddr;

    while (address < endAddr && VirtualQueryEx(hProcess, (LPCVOID)address, &mbi, sizeof(mbi))) {
        if (mbi.State == MEM_COMMIT && (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE)) && !(mbi.Protect & PAGE_GUARD)) {
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
            delete[] buffer;
        }
        address += mbi.RegionSize;
    }
}

int main() {
    std::wstring processName;
    std::wcout << "Enter process name (e.g., notepad.exe): ";
    std::getline(std::wcin, processName);

    // หา PID ของโปรเซส
    DWORD pid = FindProcessId(processName);
    if (pid == 0) {
        std::cout << "Process not found!" << std::endl;
        system("pause");
        return 1;
    }

    // เปิดโปรเซส
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        std::cout << "Failed to open process! Error code: " << GetLastError() << std::endl;
        system("pause");
        return 1;
    }

    std::cout << "Found process with PID: " << pid << std::endl;

    // รับค่าที่ต้องการหา
    int valueToFind;
    std::cout << "Enter the value to find (e.g., 30): ";
    std::cin >> valueToFind;
    std::cin.clear(); // ล้างสถานะ error
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // ล้าง buffer

    // สแกนหน่วยความจำ
    std::vector<SIZE_T> foundAddresses;
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    SIZE_T startAddr = (SIZE_T)sysInfo.lpMinimumApplicationAddress;
    SIZE_T endAddr = (SIZE_T)sysInfo.lpMaximumApplicationAddress;

    // ใช้ multi-threading เพื่อเพิ่มความเร็ว
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

    if (foundAddresses.empty()) {
        std::cout << "No addresses found with that value!" << std::endl;
        system("pause");
        CloseHandle(hProcess);
        return 1;
    }

    // รับค่าใหม่หลังเปลี่ยนแปลง
    int newValue;
    std::cout << "\nChange the value in the program (e.g., from 30 to 35), then press Enter to continue..." << std::endl;
    std::cin.get(); // รอให้ผู้ใช้กด Enter
    std::cout << "Enter the new value in game (e.g., 35): ";
    std::cin >> newValue;
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    // กรองค่า
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

    if (filteredAddresses.empty()) {
        std::cout << "No addresses found with the new value!" << std::endl;
        system("pause");
        CloseHandle(hProcess);
        return 1;
    }

    // แก้ไขค่า
    int targetValue;
    std::cout << "\nEnter the value to set (e.g., 100): ";
    std::cin >> targetValue;
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    for (SIZE_T address : filteredAddresses) {
        SIZE_T bytesWritten;
        try {
            if (WriteProcessMemory(hProcess, (LPVOID)address, &targetValue, sizeof(targetValue), &bytesWritten)) {
                std::cout << "Successfully set value to " << targetValue << " at address: 0x" << std::hex << address << std::dec << std::endl;
            }
            else {
                std::cout << "Failed to set value at address: 0x" << std::hex << address << std::dec << ". Error code: " << GetLastError() << std::endl;
            }
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