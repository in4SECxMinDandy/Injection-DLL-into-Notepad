#pragma once
#ifndef ANTI_DEBUG_H
#define ANTI_DEBUG_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winternl.h>
#include <intrin.h>

namespace AntiDebug {

// Forward declarations
typedef LONG(NTAPI* NtQueryInformationProcess_t)(
    HANDLE ProcessHandle,
    DWORD ProcessInformationClass,
    PVOID ProcessInformation,
    ULONG ProcessInformationLength,
    PULONG ReturnLength
);

// =============================================================================
// SECTION 1: PEB-Based Detection
// =============================================================================

// Check if process is being debugged via PEB
inline bool IsDebuggerPresentPEB() {
#if defined(_M_X64)
    PEB* peb = (PEB*)__readgsqword(0x60);
    return peb->BeingDebugged != 0;
#elif defined(_M_IX86)
    PEB* peb = (PEB*)__readfsdword(0x30);
    return peb->BeingDebugged != 0;
#endif
}

// Check for remote debugger via NtQueryInformationProcess
inline bool IsRemoteDebuggerPresent() {
    HANDLE hProcess = GetCurrentProcess();
    DWORD debugPort = 0;
    DWORD bytesReturned = 0;

    // Try NtQueryInformationProcess
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (hNtdll) {
        NtQueryInformationProcess_t NtQIP = (NtQueryInformationProcess_t)
            GetProcAddress(hNtdll, "NtQueryInformationProcess");
        
        if (NtQIP) {
            NTSTATUS status = NtQIP(
                hProcess,
                ProcessDebugPort,
                &debugPort,
                sizeof(debugPort),
                &bytesReturned
            );
            
            if (NT_SUCCESS(status)) {
                return debugPort != 0;
            }
        }
    }
    
    // Fallback to CheckRemoteDebuggerPresent
    BOOL isRemote = FALSE;
    CheckRemoteDebuggerPresent(hProcess, &isRemote);
    return isRemote != FALSE;
}

// =============================================================================
// SECTION 2: Timing-Based Detection
// =============================================================================

// RDTSC-based timing check
inline bool CheckRDTSCDetection() {
    int detected = 0;
    
    for (int i = 0; i < 4; i++) {
        unsigned int start1 = __rdtsc();
        unsigned int start2 = __rdtsc();
        
        // Small delay
        for (volatile int j = 0; j < 100; j++) {
            __asm__ __volatile__("nop");
        }
        
        unsigned int end1 = __rdtsc();
        unsigned int end2 = __rdtsc();
        
        // If there's an abnormal gap, debugger might be present
        if ((end2 - start1) > 1000000) {
            detected++;
        }
    }
    
    return detected > 2;
}

// GetTickCount delta check
inline bool CheckTickCountDelta() {
    DWORD start = GetTickCount();
    
    // Execute some operations
    volatile int sum = 0;
    for (int i = 0; i < 100; i++) {
        sum += i;
    }
    
    DWORD elapsed = GetTickCount() - start;
    
    // Normal execution should be very fast
    // If elapsed > 500ms, likely being traced
    return elapsed > 500;
}

// QueryPerformanceCounter check
inline bool CheckPerformanceCounter() {
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    
    // Simple operation
    volatile int x = 0;
    for (int i = 0; i < 1000; i++) x++;
    
    QueryPerformanceCounter(&end);
    
    // If taking too long, debugger might be stepping through
    return ((end.QuadPart - start.QuadPart) > freq.QuadPart);
}

// =============================================================================
// SECTION 3: Exception-Based Detection
// =============================================================================

// INT 2D (kernel debugger check)
inline bool CheckInt2D() {
    __try {
        __asm {
            int 0x2D
            // If we get here, no kernel debugger
            xor eax, eax
            mov eax, 1
        }
        return false; // No debugger
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        return true; // Debugger detected
    }
}

// INT 3 detection
inline bool CheckSoftwareBreakpoints() {
    // Check for INT 3 (0xCC) at specific locations
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)GetModuleHandleW(NULL);
    if (!dosHeader) return false;
    
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((BYTE*)dosHeader + dosHeader->e_lfanew);
    PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(ntHeaders);
    
    // Check .text section for 0xCC
    for (DWORD i = 0; i < section->SizeOfRawData; i++) {
        if (((BYTE*)dosHeader + section->VirtualAddress)[i] == 0xCC) {
            // Possible breakpoint
            return true;
        }
    }
    
    return false;
}

// CloseHandle detection
inline bool CheckCloseHandleException() {
    __try {
        HANDLE hFake = (HANDLE)0xDEADBEEF;
        CloseHandle(hFake);
        return false; // No exception = debugger
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        return true; // Exception = debugger present
    }
}

// =============================================================================
// SECTION 4: Memory Inspection
// =============================================================================

// Check for hardware breakpoints via CONTEXT
inline bool CheckHardwareBreakpoints() {
    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    
    HANDLE hThread = GetCurrentThread();
    if (!GetThreadContext(hThread, &ctx)) {
        return false;
    }
    
    // Check if any DR registers are set
    if (ctx.Dr0 != 0 || ctx.Dr1 != 0 || 
        ctx.Dr2 != 0 || ctx.Dr3 != 0 ||
        (ctx.Dr7 & 0xFF) != 0) {
        return true; // Hardware breakpoints detected
    }
    
    return false;
}

// Check for memory breakpoints (PAGE_GUARD)
inline bool CheckMemoryBreakpoints() {
    MEMORY_BASIC_INFORMATION mbi = {};
    VirtualQuery(&mbi, &mbi, sizeof(mbi));
    
    // PAGE_GUARD is often used by debuggers
    return (mbi.Protect & PAGE_GUARD) != 0;
}

// =============================================================================
// SECTION 5: Parent Process Verification
// =============================================================================

// Verify parent process is not a debugger
inline bool IsParentProcessDebugger() {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    PROCESSENTRY32W pe = {};
    pe.dwSize = sizeof(PROCESSENTRY32W);
    
    DWORD currentPid = GetCurrentProcessId();
    DWORD parentPid = 0;
    
    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            if (pe.th32ProcessID == currentPid) {
                parentPid = pe.th32ParentProcessID;
                break;
            }
        } while (Process32NextW(hSnapshot, &pe));
    }
    
    CloseHandle(hSnapshot);
    
    if (parentPid == 0) {
        return false;
    }
    
    // Check parent process name
    HANDLE hParent = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 
                                  FALSE, parentPid);
    if (!hParent) {
        return false;
    }
    
    WCHAR parentName[MAX_PATH] = {};
    HMODULE hMod = NULL;
    DWORD cbNeeded = 0;
    
    if (EnumProcessModules(hParent, &hMod, sizeof(hMod), &cbNeeded)) {
        GetModuleBaseNameW(hParent, hMod, parentName, MAX_PATH);
    }
    
    CloseHandle(hParent);
    
    // Check for common debuggers
    WCHAR* debuggers[] = {
        L"ollydbg.exe", L"x64dbg.exe", L"x32dbg.exe",
        L"windbg.exe", L"ida.exe", L"idal.exe",
        L"ida64.exe", L"devenv.exe", L"processhacker.exe",
        L"x96dbg.exe", L"dnSpy.exe", L"ilspy.exe"
    };
    
    for (int i = 0; i < sizeof(debuggers) / sizeof(debuggers[0]); i++) {
        if (wcsstr(_wcslwr(parentName), debuggers[i])) {
            return true;
        }
    }
    
    return false;
}

// =============================================================================
// SECTION 6: CPUID-based Detection
// =============================================================================

// Check for hypervisor presence (VM detection)
inline bool IsVirtualized() {
    int info[4] = {};
    
    // CPUID with EAX=1
    __cpuid(info, 1);
    
    // Check hypervisor bit (bit 31 of ECX)
    return (info[2] & (1 << 31)) != 0;
}

// Get CPU brand string to detect VM
inline void GetCPUBrand(BYTE* brand) {
    int info[4] = {};
    
    // Function 0x80000002, 0x80000003, 0x80000004
    __cpuid(info, 0x80000002);
    memcpy(brand, info, 16);
    
    __cpuid(info, 0x80000003);
    memcpy(brand + 16, info, 16);
    
    __cpuid(info, 0x80000004);
    memcpy(brand + 32, info, 16);
}

// Check for VM artifacts
inline bool HasVMArtifacts() {
    BYTE brand[49] = {};
    GetCPUBrand(brand);
    
    char* vm_signatures[] = {
        "VMware", "VirtualBox", "QEMU", "Hyper-V",
        "Parallels", "Xen", "KVM"
    };
    
    for (int i = 0; i < sizeof(vm_signatures) / sizeof(vm_signatures[0]); i++) {
        if (strstr((char*)brand, vm_signatures[i])) {
            return true;
        }
    }
    
    return IsVirtualized();
}

// =============================================================================
// SECTION 7: Comprehensive Detection
// =============================================================================

// Structure to hold all detection results
struct DetectionResult {
    bool debuggerPEB;
    bool remoteDebugger;
    bool timingAnomaly;
    bool int2DTriggered;
    bool softwareBreakpoints;
    bool hardwareBreakpoints;
    bool parentIsDebugger;
    bool isVirtualized;
    bool vmArtifacts;
    
    int totalScore;
    bool highConfidence;
};

// Run all anti-debug checks
inline DetectionResult RunAllChecks() {
    DetectionResult result = {};
    
    result.debuggerPEB = IsDebuggerPresentPEB();
    result.remoteDebugger = IsRemoteDebuggerPresent();
    result.timingAnomaly = CheckRDTSCDetection() || CheckTickCountDelta();
    result.int2DTriggered = CheckInt2D();
    result.softwareBreakpoints = CheckSoftwareBreakpoints();
    result.hardwareBreakpoints = CheckHardwareBreakpoints();
    result.parentIsDebugger = IsParentProcessDebugger();
    result.isVirtualized = IsVirtualized();
    result.vmArtifacts = HasVMArtifacts();
    
    // Calculate detection score
    result.totalScore = 
        (result.debuggerPEB ? 10 : 0) +
        (result.remoteDebugger ? 15 : 0) +
        (result.timingAnomaly ? 5 : 0) +
        (result.int2DTriggered ? 20 : 0) +
        (result.softwareBreakpoints ? 15 : 0) +
        (result.hardwareBreakpoints ? 25 : 0) +
        (result.parentIsDebugger ? 20 : 0) +
        (result.isVirtualized ? 5 : 0) +
        (result.vmArtifacts ? 15 : 0);
    
    // High confidence if score > 30 or parent is debugger
    result.highConfidence = (result.totalScore > 30) || result.parentIsDebugger;
    
    return result;
}

// Quick check (less suspicious)
inline bool QuickDebugCheck() {
    return IsDebuggerPresentPEB() || IsRemoteDebuggerPresent();
}

// =============================================================================
// SECTION 8: Countermeasures
// =============================================================================

// Obfuscate the check results
inline int ObfuscatedCheck() {
    int result = 0;
    
    if (IsDebuggerPresentPEB()) result |= 0x01;
    if (IsRemoteDebuggerPresent()) result |= 0x02;
    if (CheckInt2D()) result |= 0x04;
    if (HasVMArtifacts()) result |= 0x08;
    
    // XOR with random value to hide results
    return result ^ 0x5A;
}

// Conditional exit on debug detection
inline void ExitOnDebug() {
    if (QuickDebugCheck()) {
        // ExitProcess with obfuscated code
        ExitProcess(0xFFFFFFFF);
    }
}

// Anti-debug wrapper macro
#define ANTI_DEBUG_CHECK() \
    do { \
        if (AntiDebug::QuickDebugCheck()) { \
            ExitProcess(AntiDebug::ObfuscatedCheck()); \
        } \
    } while(0)

} // namespace AntiDebug

#endif // ANTI_DEBUG_H
