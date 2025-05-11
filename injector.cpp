#include <windows.h>
#include <iostream>
#include <string>

int main() {
    DWORD pid;
    std::cout << "Enter the PID of ProjectLH.exe: ";
    std::cin >> pid;

    // เปิดโปรเซส
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        std::cout << "Failed to open process! Error code: " << GetLastError() << std::endl;
        return 1;
    }

    // เส้นทางไปยัง DLL
    std::string dllPath = "D:\\CheatEngineAdvanced\\cheat_dll.dll";
    LPVOID pDllPath = VirtualAllocEx(hProcess, NULL, dllPath.size() + 1, MEM_COMMIT, PAGE_READWRITE);
    if (!pDllPath) {
        std::cout << "Failed to allocate memory in target process! Error code: " << GetLastError() << std::endl;
        CloseHandle(hProcess);
        return 1;
    }

    if (!WriteProcessMemory(hProcess, pDllPath, dllPath.c_str(), dllPath.size() + 1, NULL)) {
        std::cout << "Failed to write DLL path to target process! Error code: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return 1;
    }

    // โหลด DLL
    HMODULE hKernel32 = GetModuleHandleA("Kernel32"); // เปลี่ยนจาก GetModuleHandle เป็น GetModuleHandleA
    LPVOID pLoadLibrary = reinterpret_cast<LPVOID>(GetProcAddress(hKernel32, "LoadLibraryA")); // ใช้ reinterpret_cast
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pLoadLibrary, pDllPath, 0, NULL);
    if (!hThread) {
        std::cout << "Failed to create remote thread! Error code: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return 1;
    }

    WaitForSingleObject(hThread, INFINITE);
    VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
    CloseHandle(hThread);
    CloseHandle(hProcess);

    std::cout << "DLL injected successfully!" << std::endl;
    system("pause");
    return 0;
}