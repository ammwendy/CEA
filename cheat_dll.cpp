#include <windows.h>
#include <iostream>

// �ѧ��ѹ����ѹ����� DLL �١��Ŵ
DWORD WINAPI MainThread(LPVOID lpParam) {
    // �ʴ���ͤ�������� DLL �١��Ŵ
    MessageBoxA(NULL, "DLL Injected into ProjectLH.exe!", "Success", MB_OK | MB_ICONINFORMATION);

    // ������ҧ: ���˹��¤����� (��ͧ���������˹��¤����ӷ���ͧ������)
    // ������ҧ������ʴ���ͤ��� ���������ö���������˹��¤�������
    return 0;
}

// �����ҹ DLL
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