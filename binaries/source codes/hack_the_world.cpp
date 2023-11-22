#include <windows.h>
#include <stdio.h>
#include <tlhelp32.h>
#include <string>
#include <map>
#include <vector>
#include <thread>


unsigned char bytes[] = { 0x7F, 0x00, 0x00, '?', '?', 0x00, 0x00, '?', '?', 0x00, 0x00, '?', '?', 0x00, 0x00, '?', '?', 0x00, 0x00, '?', '?', '?', '?', '?', '?', 0x00, 0x00, '?', '?', '?', '?', '?', '?', 0x00, 0x00, 0x00, '?', '?', '?', '?', '?', 0x00, 0x00, 0xC0, '?', '?', '?', '?', '?', 0x00, 0x00 ,'?', '?', '?', '?', '?', '?', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 ,0x00 ,0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, '?', '?' ,'?' ,'?' ,'?', '?' ,0x00, 0x00 ,'?' ,'?' ,0xD0 ,0x00 ,0x20 ,0x00 ,'?' ,0x00 ,'?' ,'?' ,'?' ,'?' ,'?', '?' ,'?' ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x12};
int xoffset=0x3;
int yoffset=0x7;
int clickoffset=0x59;

bool IsWildcard(unsigned char byte) {
    return byte == '?'; // ASCII code for '?'
}

std::map<uintptr_t, DWORD> ScanMemory(HANDLE process, MEMORY_BASIC_INFORMATION memInfo) {
    std::map<uintptr_t, DWORD> validAddresses;

    unsigned char *buffer = (unsigned char *)calloc(1, memInfo.RegionSize);
    SIZE_T bytesRead;

    // Read memory region into buffer
    if (ReadProcessMemory(process, memInfo.BaseAddress, buffer, memInfo.RegionSize, &bytesRead)) {
        for (unsigned int i = 0; i < memInfo.RegionSize - sizeof(bytes); i++) {
            int j;
            for (j = 0; j < sizeof(bytes); j++) {
                if (!IsWildcard(bytes[j]) && bytes[j] != buffer[i + j]) {
                    break;
                }
            }
            if (j == sizeof(bytes)) {
                validAddresses[(uintptr_t)memInfo.BaseAddress + i] = GetProcessId(process);
                printf("pattern found at: 0x%llX in process %d\n", (uintptr_t)memInfo.BaseAddress + i, GetProcessId(process));
            }
        }
    }

    free(buffer);
    return validAddresses;
}

int main(int argc, char* argv[]) {
    DWORD pid = 0;
    uintptr_t xAddr=0;
    uintptr_t yAddr=0;
    uintptr_t cAddr=0;
    std::map<uintptr_t, DWORD> allValidAddresses;
    printf("searching for AOB pattern\n");
    if (argc > 1) {
        HANDLE hProcess = 0;
        pid = atoi(argv[1]);
        hProcess = OpenProcess(PROCESS_ALL_ACCESS, false, pid);

        if (hProcess == NULL) {
            printf("Failed to open process. Error code: %d\n", GetLastError());
            return 1;
        }

        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);

        MEMORY_BASIC_INFORMATION memInfo;
        unsigned char *addr = 0;

        while (addr < sysInfo.lpMaximumApplicationAddress) {
            if (VirtualQueryEx(hProcess, addr, &memInfo, sizeof(memInfo)) == sizeof(memInfo)) {
                if (memInfo.State == MEM_COMMIT && (memInfo.Type == MEM_MAPPED || memInfo.Type == MEM_PRIVATE)) {
                    std::map<uintptr_t, DWORD> addresses = ScanMemory(hProcess, memInfo);
                    allValidAddresses.insert(addresses.begin(), addresses.end());
                }
                addr += memInfo.RegionSize;
            } else {
                addr += sysInfo.dwPageSize;
            }
        }
        if (allValidAddresses.size()!=1){printf("%d AOB pattern matches found, report with firefox version on https://github.com/SubhranshuSharma/SubhranshuSharma.github.io/issues\n", static_cast<int>(allValidAddresses.size()));return 0;}
        CloseHandle(hProcess);
    }else {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        PROCESSENTRY32 processEntry;
        processEntry.dwSize = sizeof(PROCESSENTRY32);

        if (!Process32First(snapshot, &processEntry)) {
            printf("Failed to get the first process.\n");
            return 1;
        }

        do {
            std::wstring processName(processEntry.szExeFile, processEntry.szExeFile + strlen(processEntry.szExeFile));
            if (processName == L"firefox.exe") {
                HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, false, processEntry.th32ProcessID);
                if (process != NULL) {
                    SYSTEM_INFO sysInfo;
                    GetSystemInfo(&sysInfo);

                    MEMORY_BASIC_INFORMATION memInfo;
                    unsigned char *addr = 0;

                    while (addr < sysInfo.lpMaximumApplicationAddress) {
                        if (VirtualQueryEx(process, addr, &memInfo, sizeof(memInfo)) == sizeof(memInfo)) {
                            if (memInfo.State == MEM_COMMIT && (memInfo.Type == MEM_MAPPED || memInfo.Type == MEM_PRIVATE)) {
                                std::map<uintptr_t, DWORD> addresses = ScanMemory(process, memInfo);
                                allValidAddresses.insert(addresses.begin(), addresses.end());
                            }
                            addr += memInfo.RegionSize;
                        } else {
                            addr += sysInfo.dwPageSize;
                        }
                    }

                    CloseHandle(process);
                } else {
                    printf("Failed to open process %s. Error code: %d\n", processEntry.szExeFile, GetLastError());
                }
            }
        } while (Process32Next(snapshot, &processEntry));
        if (allValidAddresses.size()==1){pid=(allValidAddresses.begin()->second);
        }else if (allValidAddresses.size()>1){
            printf("multiple matches found, see \"about:processes\"(shift+esc) or \"about:memory\" in firefox to find pid of tab (use 'hack_the_world.exe <pid>' for faster start)\nenter one pid: ");
            scanf("%u", &pid);
            for (auto it = allValidAddresses.begin(); it != allValidAddresses.end();) {
                if (it->second != pid) {
                    it = allValidAddresses.erase(it);
                } else {
                    it++;
                }
            }
        }else if (allValidAddresses.size()==0){printf("no AOB pattern match found, report with firefox version on https://github.com/SubhranshuSharma/SubhranshuSharma.github.io/issues\n");return 0;}
        CloseHandle(snapshot);
    }
    xAddr=(allValidAddresses.begin()->first)+xoffset;
    yAddr=(allValidAddresses.begin()->first)+yoffset;
    cAddr=(allValidAddresses.begin()->first)+clickoffset;
    printf("using xAddr: 0x%llX, yAddr: 0x%llX, cAddr: 0x%llX, PID: %d\nlook down then right to calibrate axes\n", xAddr, yAddr, cAddr, pid);
    HANDLE hProcess = 0;
    hProcess = OpenProcess(PROCESS_VM_READ, FALSE, pid);
    DWORD lastDoubleClickTime = 0;
    INPUT input;
    memset(&input, 0, sizeof(INPUT));
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    int xValue = 0, yValue = 0, cValue = 0;
    int max_x = 0, max_y = 0, mid_x = 0, mid_y = 0, min_c=2147483647;
    while(1){
        ReadProcessMemory(hProcess, (BYTE*)xAddr, &xValue, sizeof(xValue), nullptr);
        ReadProcessMemory(hProcess, (BYTE*)yAddr, &yValue, sizeof(yValue), nullptr);
        ReadProcessMemory(hProcess, (BYTE*)cAddr, &cValue, sizeof(cValue), nullptr);
        if(yValue == 0){
            input.mi.dx = 0;
            input.mi.dy = -1;
            SendInput(3, &input, sizeof(INPUT));
        }else if (yValue > .95*max_y){
            input.mi.dx = 0;
            input.mi.dy = 1;
            SendInput(3, &input, sizeof(INPUT));
            if (yValue>max_y){max_y=yValue;mid_y=yValue/2;printf("y axis (re)calibrated\n");}
        }else if(std::abs(yValue-mid_y)>.2*yValue){mid_y=yValue;max_y=yValue*2;printf("y axis (re)calibrated\n");}
        if(xValue == 0){
            input.mi.dx = -1;
            input.mi.dy = 0;
            SendInput(3, &input, sizeof(INPUT));
        }else if (xValue > .95*max_x){
            input.mi.dx = 1;
            input.mi.dy = 0;
            SendInput(3, &input, sizeof(INPUT));
            if (xValue>max_x){max_x=xValue;mid_x=xValue/2;printf("x axis (re)calibrated\n");}
        }else if(std::abs(xValue-mid_x)>.2*xValue){mid_x=xValue;max_x=xValue*2;printf("x axis (re)calibrated\n");}
        if (cValue == min_c+64) {
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
                input.mi.dwFlags = MOUSEEVENTF_MOVE;
            }
        }else if (cValue<min_c){min_c=cValue;}
        Sleep(40);
    }
    CloseHandle(hProcess);
    return 0;
}
