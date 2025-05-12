#include <windows.h>
#include <iostream>
#include <string>
#include <fstream>

// Manual Mapping พื้นฐาน
bool ManualMapDll(HANDLE hProcess, const char* dllPath) {
    // อ่านไฟล์ DLL
    std::ifstream file(dllPath, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cout << "Failed to open DLL file!" << std::endl;
        return false;
    }

    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> dllData(fileSize);
    file.read(dllData.data(), fileSize);
    file.close();

    // Parse PE header
    IMAGE_DOS_HEADER* dosHeader = (IMAGE_DOS_HEADER*)dllData.data();
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        std::cout << "Invalid DOS header!" << std::endl;
        return false;
    }

    IMAGE_NT_HEADERS* ntHeader = (IMAGE_NT_HEADERS*)(dllData.data() + dosHeader->e_lfanew);
    if (ntHeader->Signature != IMAGE_NT_SIGNATURE) {
        std::cout << "Invalid NT header!" << std::endl;
        return false;
    }

    // จัดสรรหน่วยความจำใน target process
    LPVOID imageBase = VirtualAllocEx(hProcess, NULL, ntHeader->OptionalHeader.SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!imageBase) {
        std::cout << "Failed to allocate memory in target process! Error code: " << GetLastError() << std::endl;
        return false;
    }

    // เขียน PE header
    if (!WriteProcessMemory(hProcess, imageBase, dllData.data(), ntHeader->OptionalHeader.SizeOfHeaders, NULL)) {
        std::cout << "Failed to write PE header! Error code: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, imageBase, 0, MEM_RELEASE);
        return false;
    }

    // เขียน section ทั้งหมด
    IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(ntHeader);
    for (int i = 0; i < ntHeader->FileHeader.NumberOfSections; i++) {
        if (section[i].SizeOfRawData) {
            if (!WriteProcessMemory(hProcess, (LPVOID)((size_t)imageBase + section[i].VirtualAddress), dllData.data() + section[i].PointerToRawData, section[i].SizeOfRawData, NULL)) {
                std::cout << "Failed to write section " << i << "! Error code: " << GetLastError() << std::endl;
                VirtualFreeEx(hProcess, imageBase, 0, MEM_RELEASE);
                return false;
            }
        }
    }

    // เตรียมเรียก DllMain (ในตัวอย่างนี้ยังไม่เรียก DllMain เพื่อความง่าย)
    std::cout << "DLL manually mapped at address: 0x" << std::hex << imageBase << std::dec << std::endl;
    return true;
}

int main() {
    DWORD pid;
    std::cout << "Enter the PID of ProjectLH.exe: ";
    std::cin >> pid;

    // เปิดโปรเซส
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        std::cout << "Failed to open process! Error code: " << GetLastError() << std::endl;
        system("pause");
        return 1;
    }

    // เส้นทางไปยัง DLL
    const char* dllPath = "D:\\CheatEngineAdvanced\\cheat_dll.dll";
    if (!ManualMapDll(hProcess, dllPath)) {
        CloseHandle(hProcess);
        system("pause");
        return 1;
    }

    CloseHandle(hProcess);
    std::cout << "DLL injected successfully!" << std::endl;
    system("pause");
    return 0;
}