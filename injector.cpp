#include <windows.h>
#include <iostream>
#include <string>
#include <fstream>
#include <vector> // เพิ่ม header สำหรับ std::vector
#include <tlhelp32.h>

// หา thread แรกของโปรเซส
DWORD GetProcessThreadId(DWORD pid) {
    HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hThreadSnap == INVALID_HANDLE_VALUE) {
        return 0;
    }

    THREADENTRY32 te32;
    te32.dwSize = sizeof(THREADENTRY32);

    if (!Thread32First(hThreadSnap, &te32)) {
        CloseHandle(hThreadSnap);
        return 0;
    }

    do {
        if (te32.th32OwnerProcessID == pid) {
            CloseHandle(hThreadSnap);
            return te32.th32ThreadID;
        }
    } while (Thread32Next(hThreadSnap, &te32));

    CloseHandle(hThreadSnap);
    return 0;
}

LPVOID ManualMapDll(HANDLE hProcess, const char* dllPath, LPVOID& entryPoint) {
    std::ifstream file(dllPath, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cout << "Failed to open DLL file!" << std::endl;
        return nullptr;
    }

    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> dllData(fileSize);
    file.read(dllData.data(), fileSize);
    file.close();

    IMAGE_DOS_HEADER* dosHeader = (IMAGE_DOS_HEADER*)dllData.data();
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        std::cout << "Invalid DOS header!" << std::endl;
        return nullptr;
    }

    IMAGE_NT_HEADERS* ntHeader = (IMAGE_NT_HEADERS*)(dllData.data() + dosHeader->e_lfanew);
    if (ntHeader->Signature != IMAGE_NT_SIGNATURE) {
        std::cout << "Invalid NT header!" << std::endl;
        return nullptr;
    }

    LPVOID imageBase = VirtualAllocEx(hProcess, NULL, ntHeader->OptionalHeader.SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!imageBase) {
        std::cout << "Failed to allocate memory in target process! Error code: " << GetLastError() << std::endl;
        return nullptr;
    }

    if (!WriteProcessMemory(hProcess, imageBase, dllData.data(), ntHeader->OptionalHeader.SizeOfHeaders, NULL)) {
        std::cout << "Failed to write PE header! Error code: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, imageBase, 0, MEM_RELEASE);
        return nullptr;
    }

    IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(ntHeader);
    for (int i = 0; i < ntHeader->FileHeader.NumberOfSections; i++) {
        if (section[i].SizeOfRawData) {
            if (!WriteProcessMemory(hProcess, (LPVOID)((size_t)imageBase + section[i].VirtualAddress), dllData.data() + section[i].PointerToRawData, section[i].SizeOfRawData, NULL)) {
                std::cout << "Failed to write section " << i << "! Error code: " << GetLastError() << std::endl;
                VirtualFreeEx(hProcess, imageBase, 0, MEM_RELEASE);
                return nullptr;
            }
        }
    }

    entryPoint = (LPVOID)((size_t)imageBase + ntHeader->OptionalHeader.AddressOfEntryPoint);
    std::cout << "DLL manually mapped at address: 0x" << std::hex << imageBase << std::dec << std::endl;
    return imageBase; // ส่ง imageBase กลับ
}

int main() {
    DWORD pid;
    std::cout << "Enter the PID of ProjectLH.exe: ";
    std::cin >> pid;

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        std::cout << "Failed to open process! Error code: " << GetLastError() << std::endl;
        system("pause");
        return 1;
    }

    const char* dllPath = "D:\\CheatEngineAdvanced\\cheat_dll.dll";
    LPVOID entryPoint = nullptr;
    LPVOID imageBase = ManualMapDll(hProcess, dllPath, entryPoint);
    if (!imageBase) {
        CloseHandle(hProcess);
        system("pause");
        return 1;
    }

    // Thread Hijacking
    DWORD threadId = GetProcessThreadId(pid);
    if (!threadId) {
        std::cout << "Failed to find thread in target process!" << std::endl;
        VirtualFreeEx(hProcess, imageBase, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        system("pause");
        return 1;
    }

    HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, threadId);
    if (!hThread) {
        std::cout << "Failed to open thread! Error code: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, imageBase, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        system("pause");
        return 1;
    }

    SuspendThread(hThread);
    CONTEXT ctx = { 0 };
    ctx.ContextFlags = CONTEXT_FULL;
    GetThreadContext(hThread, &ctx);

    ctx.Rip = (DWORD64)entryPoint; // เปลี่ยน RIP เพื่อเรียก DllMain
    SetThreadContext(hThread, &ctx);
    ResumeThread(hThread);

    CloseHandle(hThread);
    CloseHandle(hProcess);

    std::cout << "DLL injected successfully using Thread Hijacking!" << std::endl;
    system("pause");
    return 0;
}