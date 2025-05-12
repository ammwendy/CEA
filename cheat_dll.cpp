#include <windows.h>
#include <iostream>

// ฟังก์ชันที่รันเมื่อ DLL ถูกโหลด
DWORD WINAPI MainThread(LPVOID lpParam) {
    // แสดงข้อความเมื่อ DLL ถูกโหลด
    MessageBoxA(NULL, "DLL Injected into ProjectLH.exe!", "Success", MB_OK | MB_ICONINFORMATION);

    // ตัวอย่าง: แก้ไขหน่วยความจำ (ต้องรู้ที่อยู่หน่วยความจำที่ต้องการแก้ไข)
    // ตัวอย่างนี้แค่แสดงข้อความ แต่แอมสามารถเพิ่มโค้ดแก้ไขหน่วยความจำได้
    return 0;
}

// เข้าใช้งาน DLL
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        CreateThread(NULL, 0, MainThread, NULL, 0, NULL);
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}