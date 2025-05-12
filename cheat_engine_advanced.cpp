#include <windows.h>
#include <psapi.h>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <string>
#include <cstdlib>
#include <cstring>

// ฟังก์ชันแปลง std::wstring เป็น std::string โดยใช้ WideCharToMultiByte
std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
    if (size_needed == 0) return std::string();
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

std::mutex print_mutex;

// ฟังก์ชันสแกนหน่วยความจำในช่วงที่กำหนด (สำหรับ string)
void ScanMemoryRangeForString(HANDLE hProcess, SIZE_T startAddr, SIZE_T endAddr, const std::string& valueToFind, std::vector<SIZE_T>& foundAddresses) {
    MEMORY_BASIC_INFORMATION mbi;
    SIZE_T address = startAddr;

    while (address < endAddr) {
        try {
            if (!VirtualQueryEx(hProcess, (LPCVOID)address, &mbi, sizeof(mbi))) {
                address += 0x1000;
                continue;
            }

            if (mbi.State == MEM_COMMIT && (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE)) && !(mbi.Protect & PAGE_GUARD)) {
                std::cout << "Scanning region: 0x" << std::hex << address << " to 0x" << (address + mbi.RegionSize) << std::dec << std::endl;
                BYTE* buffer = new BYTE[mbi.RegionSize];
                SIZE_T bytesRead;
                if (ReadProcessMemory(hProcess, (LPCVOID)address, buffer, mbi.RegionSize, &bytesRead)) {
                    for (SIZE_T i = 0; i <= bytesRead - valueToFind.size(); i++) {
                        bool match = true;
                        for (size_t j = 0; j < valueToFind.size(); j++) {
                            if (buffer[i + j] != static_cast<unsigned char>(valueToFind[j])) {
                                match = false;
                                break;
                            }
                        }
                        if (match) {
                            SIZE_T foundAddress = address + i;
                            {
                                std::lock_guard<std::mutex> lock(print_mutex);
                                foundAddresses.push_back(foundAddress);
                                std::cout << "Found string \"" << valueToFind << "\" at address: 0x" << std::hex << foundAddress << std::dec << std::endl;
                            }
                        }
                    }
                }
                delete[] buffer;
            }
            address += mbi.RegionSize;
        }
        catch (...) {
            address += 0x1000;
        }
    }
}

int main() {
    DWORD pid;
    std::cout << "Enter the PID of the process (e.g., 12345): ";
    std::cin >> pid;
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    // เปิดโปรเซสด้วย PID
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        std::cout << "Failed to open process with PID " << pid << "! Error code: " << GetLastError() << std::endl;
        system("pause");
        return 1;
    }

    std::cout << "Successfully opened process with PID: " << pid << std::endl;

    // รับ string ที่ต้องการหา
    std::string valueToFind;
    std::cout << "Enter the string to find (e.g., 15): ";
    std::getline(std::cin, valueToFind);

    // สแกนหน่วยความจำ
    std::vector<SIZE_T> foundAddresses;
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    SIZE_T startAddr = (SIZE_T)sysInfo.lpMinimumApplicationAddress;
    SIZE_T endAddr = (SIZE_T)sysInfo.lpMaximumApplicationAddress;

    std::cout << "Starting memory scan... This may take a while." << std::endl;

    // ใช้ multi-threading เพื่อเพิ่มความเร็ว (จำกัดจำนวน thread)
    const int numThreads = std::min(4, static_cast<int>(std::thread::hardware_concurrency()));
    std::vector<std::thread> threads;
    SIZE_T rangeSize = (endAddr - startAddr) / numThreads;

    for (int i = 0; i < numThreads; i++) {
        try {
            SIZE_T threadStart = startAddr + (i * rangeSize);
            SIZE_T threadEnd = (i == numThreads - 1) ? endAddr : threadStart + rangeSize;
            if (threadStart >= threadEnd) continue;
            threads.emplace_back(ScanMemoryRangeForString, hProcess, threadStart, threadEnd, valueToFind, std::ref(foundAddresses));
        }
        catch (const std::exception& e) {
            std::cout << "Failed to create thread " << i << ": " << e.what() << std::endl;
        }
    }

    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    std::cout << "Memory scan completed." << std::endl;

    if (foundAddresses.empty()) {
        std::cout << "No addresses found with that string!" << std::endl;
        system("pause");
        CloseHandle(hProcess);
        return 1;
    }

    // รับ string ใหม่หลังเปลี่ยนแปลง
    std::string newValue;
    std::cout << "\nChange the string in the program (e.g., from 15 to 568), then press Enter to continue..." << std::endl;
    std::cin.get();
    std::cout << "Enter the new string in program (e.g., 568): ";
    std::getline(std::cin, newValue);

    // กรองค่า
    std::vector<SIZE_T> filteredAddresses;
    for (SIZE_T address : foundAddresses) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQueryEx(hProcess, (LPCVOID)address, &mbi, sizeof(mbi))) {
            std::cout << "VirtualQueryEx failed at address: 0x" << std::hex << address << std::dec << ". Error code: " << GetLastError() << std::endl;
            continue;
        }

        if (mbi.State != MEM_COMMIT || (mbi.Protect & PAGE_GUARD)) {
            std::cout << "Address 0x" << std::hex << address << std::dec << " is no longer accessible." << std::endl;
            continue;
        }

        char* buffer = new char[newValue.size()];
        SIZE_T bytesRead;
        try {
            if (ReadProcessMemory(hProcess, (LPCVOID)address, buffer, newValue.size(), &bytesRead)) {
                if (bytesRead == newValue.size() && memcmp(buffer, newValue.c_str(), newValue.size()) == 0) {
                    filteredAddresses.push_back(address);
                    std::cout << "Found new string \"" << newValue << "\" at address: 0x" << std::hex << address << std::dec << std::endl;
                }
            }
            else {
                std::cout << "Failed to read memory at address: 0x" << std::hex << address << std::dec << ". Error code: " << GetLastError() << std::endl;
            }
        }
        catch (...) {
            std::cout << "Exception occurred while reading memory at address: 0x" << std::hex << address << std::dec << std::endl;
        }
        delete[] buffer;
    }

    if (filteredAddresses.empty()) {
        std::cout << "No addresses found with the new string! Rescanning memory..." << std::endl;
        foundAddresses.clear();
        threads.clear();
        for (int i = 0; i < numThreads; i++) {
            try {
                SIZE_T threadStart = startAddr + (i * rangeSize);
                SIZE_T threadEnd = (i == numThreads - 1) ? endAddr : threadStart + rangeSize;
                if (threadStart >= threadEnd) continue;
                threads.emplace_back(ScanMemoryRangeForString, hProcess, threadStart, threadEnd, newValue, std::ref(foundAddresses));
            }
            catch (const std::exception& e) {
                std::cout << "Failed to create thread " << i << ": " << e.what() << std::endl;
            }
        }

        for (auto& thread : threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }

        filteredAddresses = foundAddresses;
        if (filteredAddresses.empty()) {
            std::cout << "Still no addresses found with the new string!" << std::endl;
            system("pause");
            CloseHandle(hProcess);
            return 1;
        }
        else {
            for (SIZE_T address : filteredAddresses) {
                std::cout << "Found new string \"" << newValue << "\" at address: 0x" << std::hex << address << std::dec << std::endl;
            }
            std::cout << "Press any key to continue..." << std::endl;
            system("pause");
        }
    }
    else {
        for (SIZE_T address : filteredAddresses) {
            std::cout << "Found new string \"" << newValue << "\" at address: 0x" << std::hex << address << std::dec << std::endl;
        }
        std::cout << "Press any key to continue..." << std::endl;
        system("pause");
    }

    // แก้ไข string
    std::string targetValue;
    std::cout << "\nEnter the new string to set (e.g., 200): ";
    std::getline(std::cin, targetValue);

    for (SIZE_T address : filteredAddresses) {
        SIZE_T bytesWritten;
        DWORD oldProtect;
        try {
            if (!VirtualProtectEx(hProcess, (LPVOID)address, targetValue.size(), PAGE_EXECUTE_READWRITE, &oldProtect)) {
                std::cout << "VirtualProtectEx failed at address: 0x" << std::hex << address << std::dec << ". Error code: " << GetLastError() << std::endl;
                continue;
            }

            if (WriteProcessMemory(hProcess, (LPVOID)address, targetValue.c_str(), targetValue.size(), &bytesWritten)) {
                std::cout << "Successfully set string to \"" << targetValue << "\" at address: 0x" << std::hex << address << std::dec << std::endl;
            }
            else {
                std::cout << "Failed to set string at address: 0x" << std::hex << address << std::dec << ". Error code: " << GetLastError() << std::endl;
            }

            DWORD dummy;
            VirtualProtectEx(hProcess, (LPVOID)address, targetValue.size(), oldProtect, &dummy);
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