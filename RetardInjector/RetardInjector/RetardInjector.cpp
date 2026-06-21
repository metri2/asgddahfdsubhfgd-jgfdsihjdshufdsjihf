#include <iostream>
#include <windows.h>
#include <tlhelp32.h>

// Function to retrieve the Process ID (PID) from a process name
DWORD GetProcessIdByName(const wchar_t* processName) {
    DWORD processId = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 processEntry;
        processEntry.dwSize = sizeof(processEntry);

        if (Process32First(snapshot, &processEntry)) {
            do {
                if (!_wcsicmp(processEntry.szExeFile, processName)) {
                    processId = processEntry.th32ProcessID;
                    break;
                }
            } while (Process32Next(snapshot, &processEntry));
        }
    }
    CloseHandle(snapshot);
    return processId;
}

int main() {
    // 1. Define the target process name and the full path to your DLL
    const wchar_t* targetProcess = L"notepad.exe";
    const char* dllPath = "C:\\path\\to\\your\\library.dll";

    std::wcout << L"Searching for " << targetProcess << L"..." << std::endl;
    DWORD processId = GetProcessIdByName(targetProcess);

    if (processId == 0) {
        std::cerr << "Target process not found!" << std::endl;
        return 1;
    }
    std::cout << "Found target process. PID: " << processId << std::endl;

    // 2. Open the target process with required memory access privileges
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (!hProcess) {
        std::cerr << "Failed to open target process." << std::endl;
        return 1;
    }

    // 3. Allocate memory space inside the target process for the DLL path string
    LPVOID remoteMemory = VirtualAllocEx(hProcess, NULL, strlen(dllPath) + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteMemory) {
        std::cerr << "Failed to allocate memory in target process." << std::endl;
        CloseHandle(hProcess);
        return 1;
    }

    // 4. Write the DLL path string into the newly allocated memory
    if (!WriteProcessMemory(hProcess, remoteMemory, dllPath, strlen(dllPath) + 1, NULL)) {
        std::cerr << "Failed to write memory in target process." << std::endl;
        VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return 1;
    }

    // 5. Get the real memory address of LoadLibraryA inside kernel32.dll
    LPVOID loadLibraryAddress = (LPVOID)GetProcAddress(GetModuleHandle(L"kernel32.dll"), "LoadLibraryA");
    if (!loadLibraryAddress) {
        std::cerr << "Failed to find LoadLibraryA address." << std::endl;
        VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return 1;
    }

    // 6. Create a remote thread that calls LoadLibraryA, pointing to the written path
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)loadLibraryAddress, remoteMemory, 0, NULL);
    if (!hThread) {
        std::cerr << "Failed to create remote thread." << std::endl;
        VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return 1;
    }

    std::cout << "DLL successfully injected!" << std::endl;

    // 7. Cleanup the handles
    WaitForSingleObject(hThread, INFINITE);
    VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
    CloseHandle(hThread);
    CloseHandle(hProcess);

    return 0;
}
