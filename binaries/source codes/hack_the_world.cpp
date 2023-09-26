#include <iostream>
#include <vector>
#include <Windows.h>
#include <TlHelp32.h>
#include <thread>
#include <cmath>

uintptr_t GetModuleBaseAddress(DWORD processId, const wchar_t* moduleName) {
    uintptr_t baseAddress = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);

    if (snapshot != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W moduleEntry; // Note the use of MODULEENTRY32W for wide character support
        moduleEntry.dwSize = sizeof(MODULEENTRY32W);

        if (Module32FirstW(snapshot, &moduleEntry)) { // Use Module32FirstW for wide character support
            do {
                if (_wcsicmp(moduleEntry.szModule, moduleName) == 0) {
                    baseAddress = reinterpret_cast<uintptr_t>(moduleEntry.modBaseAddr);
                    break;
                }
            } while (Module32NextW(snapshot, &moduleEntry)); // Use Module32NextW for wide character support
        }

        CloseHandle(snapshot);
    }

    return baseAddress;
}

uintptr_t FindDMAAddy(HANDLE hProc, uintptr_t ptr, std::vector<unsigned int> offsets)
{
    uintptr_t addr = ptr;
    for (unsigned int i = 0; i < offsets.size(); ++i)
    {
        ReadProcessMemory(hProc, (BYTE*)addr, &addr, sizeof(addr), 0);
        addr += offsets[i];
    }
    return addr;
}
int main(int argc, char* argv[]){
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <PID> (shift+esc in firefox and find PID of tab)" << std::endl;
        return 1;
    }
    DWORD procId = std::stoi(argv[1]);    uintptr_t moduleBase = GetModuleBaseAddress(procId, L"xul.dll");
    
    HANDLE hProcess = 0;
    hProcess = OpenProcess(PROCESS_VM_READ, FALSE, procId);
    uintptr_t dynamicPtrBaseAddrx = moduleBase + 0x69C0278;
    uintptr_t dynamicPtrBaseAddry = moduleBase + 0x69C6738;
    uintptr_t dynamicPtrBaseAddrclick = moduleBase + 0x69C0310;

    std::vector<unsigned int> yOffsets = {0x1D8, 0xC0, 0x20, 0x90, 0x88, 0x78, 0xC};
    std::vector<unsigned int> xOffsets = {0x38, 0x50, 0x90, 0x88, 0x78, 0x8};
    std::vector<unsigned int> cOffsets = {0x8, 0x68, 0x38, 0xC0, 0x140, 0x50, 0x8};
    uintptr_t yAddr = FindDMAAddy(hProcess, dynamicPtrBaseAddry, yOffsets);
    uintptr_t xAddr = FindDMAAddy(hProcess, dynamicPtrBaseAddrx, xOffsets);
    uintptr_t cAddr = FindDMAAddy(hProcess, dynamicPtrBaseAddrclick, cOffsets);

    DWORD lastDoubleClickTime = 0;
    INPUT input;
    memset(&input, 0, sizeof(INPUT));
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    int yValue = 0;
    int xValue = 0;
    int cValue = 0;
    int max_y = 0, max_x = 0, mid_y = 0, mid_x = 0;
    while(1){
        ReadProcessMemory(hProcess, (BYTE*)yAddr, &yValue, sizeof(yValue), nullptr);
        ReadProcessMemory(hProcess, (BYTE*)xAddr, &xValue, sizeof(xValue), nullptr);
        ReadProcessMemory(hProcess, (BYTE*)cAddr, &cValue, sizeof(cValue), nullptr);

        if(yValue == 0){
            input.mi.dx = 0;
            input.mi.dy = -1;
            SendInput(3, &input, sizeof(INPUT));
        }else if (yValue > max_y-5){
            input.mi.dx = 0;
            input.mi.dy = 1;
            SendInput(3, &input, sizeof(INPUT));
            if (yValue>max_y){max_y=yValue;mid_y=yValue/2;}
        }else if(std::abs(yValue-mid_y)>.1*yValue){max_y=yValue;mid_y=yValue/2;}
        if(xValue == 0){
            input.mi.dx = -1;
            input.mi.dy = 0;
            SendInput(3, &input, sizeof(INPUT));
        }else if (xValue > max_x-5){
            input.mi.dx = 1;
            input.mi.dy = 0;
            SendInput(3, &input, sizeof(INPUT));
            if (xValue>max_x){max_x=xValue;mid_x=xValue/2;}
        }else if(std::abs(xValue-mid_x)>.1*xValue){max_x=xValue;mid_x=xValue/2;}
        if (cValue == 0) {
            DWORD currentTime = GetTickCount();
            if (currentTime - lastDoubleClickTime >= 2000) {
                input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                SendInput(1, &input, sizeof(INPUT));
                input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
                SendInput(1, &input, sizeof(INPUT));
                Sleep(100);
                input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                SendInput(1, &input, sizeof(INPUT));
                input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
                SendInput(1, &input, sizeof(INPUT));
                lastDoubleClickTime = currentTime;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }
    CloseHandle(hProcess);
    return 0;
}