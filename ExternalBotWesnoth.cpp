#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>
#include <string.h>
#include <vector>

using namespace std;



uintptr_t GetModuleBaseAddress(const wchar_t* modName, DWORD procId) {
    uintptr_t modBaseAddr = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, procId);

    if (hSnap != INVALID_HANDLE_VALUE) {
        MODULEENTRY32 moduleEntry;
        moduleEntry.dwSize = sizeof(moduleEntry);

        if (Module32First(hSnap, &moduleEntry)) {
            do
            {
                if (!_wcsicmp(moduleEntry.szModule, modName)) {
                    modBaseAddr = (uintptr_t)moduleEntry.modBaseAddr;
                    break;
                }

            } while (Module32Next(hSnap, &moduleEntry));
        }

    }

    CloseHandle(hSnap);
    return modBaseAddr;
}

DWORD GetProcId(const wchar_t* procName) {
    DWORD procId = 0;
    // The first argument is a flag:Includes all processes in the system in the snapshot
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 procEntry;
        // The docs say that i need to set the dwSize (DWORD/32int) manually
        // sizeof will return the size in bytes
        procEntry.dwSize = sizeof(procEntry);

        // Process32First: Retrieves information about the first process encountered in a system snapshot.
        if (Process32First(hSnap, &procEntry)) {
            //loop trhu all process
            do {
                // wchar chars string comparrision... case insensitive
                if (!_wcsicmp(procEntry.szExeFile, procName)) {
                    procId = procEntry.th32ProcessID;
                    break;
                }

            } while (Process32Next(hSnap, &procEntry));

        }

    }

    CloseHandle(hSnap);
    return procId;
}


uintptr_t FindDMAAddress(HANDLE hProc, uintptr_t ptr, vector<unsigned int> offsets) {
    uintptr_t addr = ptr;
    cout << "Base Address: 0x" << hex << addr << endl;

    for (unsigned int i = 0; i < offsets.size(); ++i) {
        if (!ReadProcessMemory(hProc, (BYTE*)addr, &addr, sizeof(addr), nullptr)) {
            cerr << "Falha ao ler a memória no offset " << i << "!" << endl;
            return 0;
        }
        cout << "Reading Offset[" << i << "]: 0x" << hex << addr << " + 0x" << offsets[i] << " = ";
        addr += offsets[i];
        cout << "0x" << hex << addr << endl;
    }
    return addr;
}


int main()
{
    const wchar_t* mainModule = L"wesnoth.exe";

    // Get ProcId of the target process
    DWORD procId = GetProcId(mainModule); 

    // Get module base address
    uintptr_t moduleBaseAddress = GetModuleBaseAddress(mainModule, procId);
    if (!moduleBaseAddress) {
        cerr << "Falha ao obter o endereço do módulo!" << endl;
        return 1;
    }
    cout << "MODULE: " << moduleBaseAddress << endl;


    // Get handle to process
    HANDLE wesnoth_process = OpenProcess(PROCESS_ALL_ACCESS, true, procId);
    if (!wesnoth_process) {
        cerr << "Nao foi possivel abrir o processo do jogo!" << endl;
        return 1;
    }

    // Resolve base address of the pointer chain
    uintptr_t dynamicPtrBaseAddr = moduleBaseAddress + 0x01157320;
    cout << "DynamicPointerBaseAddress: 0x" << hex << dynamicPtrBaseAddr << endl;


    // Resolve pointer chain to gold address
    vector<unsigned int> goldOffsets = { 0xc, 0x4, 0x4 };
    uintptr_t goldAddress = FindDMAAddress(wesnoth_process, dynamicPtrBaseAddr, goldOffsets);
    cout << "Gold address: 0x" << hex << goldAddress << endl;

    // Real gold value
    int goldValue = 0;
    ReadProcessMemory(wesnoth_process, (BYTE*)goldAddress, &goldValue, sizeof(goldValue), nullptr);
    cout << "Gold value: " << dec << goldValue << endl;

    while (true) {
        if (!ReadProcessMemory(wesnoth_process, (BYTE*)goldAddress, &goldValue, sizeof(goldValue), nullptr)) {
            cerr << "Falha ao ler a quantidade de Gold da memoria!" << endl;
            break;
        }

        system("cls");
        cout << "Quantidade de Gold: " << dec << goldValue << endl;
        Sleep(1000);

        // Pega o estado da tecla "G" e Verifica se o bit mais significativo está definido
        if (GetKeyState(0x47) & 0x8000) {
            int newGoldValue = goldValue + 50;
            WriteProcessMemory(wesnoth_process, (BYTE*)goldAddress, &newGoldValue, sizeof(goldValue), 0);
        }

    }

    system("pause");
    return 0;
}