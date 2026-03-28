#pragma once
#ifndef OBFUSCATION_CORE_H
#define OBFUSCATION_CORE_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#include <vector>
#include <string>

namespace ObfCore {

// =============================================================================
// SECTION 1: Virtual Machine (VM) for Code Virtualization
// =============================================================================

// VM Bytecode Opcodes for Thread Hijacking operations
enum VMOpcodes : BYTE {
    VM_NOP = 0x00,
    VM_ALLOC = 0x01,       // VirtualAllocEx
    VM_FREE = 0x02,        // VirtualFreeEx
    VM_WRITE = 0x03,       // WriteProcessMemory
    VM_READ = 0x04,        // ReadProcessMemory
    VM_OPEN = 0x05,        // OpenProcess
    VM_SUSPEND = 0x06,     // SuspendThread
    VM_RESUME = 0x07,      // ResumeThread
    VM_GETCTX = 0x08,      // GetThreadContext
    VM_SETCTX = 0x09,      // SetThreadContext
    VM_CREATETHREAD = 0x0A, // CreateRemoteThread
    VM_WAIT = 0x0B,        // WaitForSingleObject
    VM_PROTECT = 0x0C,     // VirtualProtectEx
    VM_QUERY = 0x0D,       // VirtualQueryEx
    VM_LOADLIB = 0x0E,     // LoadLibrary
    VM_GETPROC = 0x0F,     // GetProcAddress
    VM_EXIT = 0xFF        // Exit VM
};

// VM Context structure
struct VMContext {
    DWORD registers[8];     // General purpose registers (EAX, EBX, ECX, EDX, ESI, EDI, EBP, ESP)
    DWORD flags;           // EFLAGS
    DWORD ip;              // Instruction pointer
    PVOID memoryBase;       // VM memory base
    DWORD memorySize;       // VM memory size
    PVOID context;         // External context (thread/process handles)
};

// VM Bytecode instruction
struct VMInstruction {
    BYTE opcode;
    DWORD operand1;
    DWORD operand2;
    DWORD operand3;
    DWORD operand4;
};

// Custom VM handler
class ObfuscationVM {
private:
    std::vector<BYTE> bytecode;
    VMContext ctx;
    bool running;
    
    // Execute individual opcode
    DWORD executeOpcode(BYTE opcode);
    
    // VM memory operations
    PVOID vmAlloc(DWORD size, DWORD protect);
    bool vmFree(PVOID addr);
    bool vmRead(PVOID addr, PVOID buffer, DWORD size);
    bool vmWrite(PVOID addr, PVOID buffer, DWORD size);
    
public:
    ObfuscationVM();
    ~ObfuscationVM();
    
    // Load bytecode
    void loadBytecode(const BYTE* code, DWORD size);
    
    // Execute VM
    DWORD execute();
    
    // Control
    void stop();
    VMContext* getContext();
    
    // Set context
    void setContext(PVOID extContext);
};

// =============================================================================
// SECTION 2: Polymorphic Engine
// =============================================================================

// Polymorphic mutation types
enum MutationType : BYTE {
    MUT_NONE = 0,
    MUT_XOR = 1,
    MUT_ADD = 2,
    MUT_SUB = 3,
    MUT_ROTATE = 4,
    MUT_SWAP = 5
};

// Mutation context
struct MutationContext {
    DWORD seed;
    MutationType currentOp;
    DWORD key;
};

// Polymorphic engine class
class PolymorphicEngine {
private:
    MutationContext mutCtx;
    
    // Random number generator (custom LCG for determinism)
    DWORD customRand();
    void seedRand(DWORD seed);
    
    // Mutation functions
    void mutateXOR(BYTE* data, DWORD size, DWORD key);
    void mutateAdd(BYTE* data, DWORD size, DWORD key);
    void mutateSub(BYTE* data, DWORD size, DWORD key);
    void mutateRotate(BYTE* data, DWORD size, DWORD key);
    void mutateSwap(BYTE* data, DWORD size);
    
    // Dead code generators
    void generateDeadCode(std::vector<BYTE>& buffer);
    void generateOpaquePredicate(std::vector<BYTE>& buffer);
    
public:
    PolymorphicEngine();
    
    // Initialize with seed (for reproducibility)
    void init(DWORD seed = GetTickCount());
    
    // Apply mutation to data
    void mutate(BYTE* data, DWORD size, MutationType type = MUT_XOR);
    
    // Demutate (reverse mutation)
    void demutate(BYTE* data, DWORD size, MutationType type);
    
    // Generate polymorphic code
    std::vector<BYTE> generatePolymorphicCode(const BYTE* original, DWORD size);
    
    // Generate random key
    DWORD generateKey();
    
    // Set mutation key
    void setKey(DWORD key);
};

// =============================================================================
// SECTION 3: Control Flow Obfuscation
// =============================================================================

// State machine for control flow flattening
struct FlattenedBlock {
    DWORD state;
    DWORD nextStateTrue;
    DWORD nextStateFalse;
    DWORD codeOffset;
    bool hasOpaquePredicate;
    DWORD opaqueValue;
};

// Control flow flattener
class ControlFlowFlattener {
private:
    std::vector<FlattenedBlock> blocks;
    DWORD currentState;
    
    // Generate opaque predicate
    DWORD generateOpaquePredicate();
    
public:
    ControlFlowFlattener();
    
    // Flatten a control flow
    void flatten(BYTE* code, DWORD size, DWORD numBlocks);
    
    // Get next state
    DWORD getNextState(DWORD currentBlock, bool condition);
    
    // Execute flattened code
    void execute(DWORD entryState);
};

// =============================================================================
// SECTION 4: String Encryption
// =============================================================================

// Encrypted string wrapper
template<DWORD Key>
class EncryptedString {
private:
    BYTE data[256];
    DWORD length;
    
    // Runtime decryption
    void decrypt() {
        for (DWORD i = 0; i < length; i++) {
            data[i] ^= (Key >> (i % 4)) & 0xFF;
            data[i] = ~data[i];
        }
    }
    
public:
    constexpr EncryptedString(const char* str) : length(0) {
        while (str[length] && length < 255) {
            BYTE c = str[length];
            c ^= (Key >> (length % 4)) & 0xFF;
            c = ~c;
            data[length] = c;
            length++;
        }
        data[length] = 0;
    }
    
    const char* get() {
        decrypt();
        return (const char*)data;
    }
};

// Helper macro for encrypted strings
#define ENC_STR(key, str) (ObfCore::EncryptedString<key>(str).get())

// =============================================================================
// SECTION 5: Code Obfuscation Utilities
// =============================================================================

// Insert dead code at random intervals
void insertDeadCode(BYTE*& code, DWORD& size);

// Obfuscate function calls (indirect calls)
PVOID obfGetProcAddress(HMODULE hModule, LPCSTR funcName);

// Obfuscate indirect jumps
void obfIndirectJump(PVOID target);

// Self-modifying code helper
class SelfModifyingCode {
private:
    BYTE* codeBlock;
    DWORD codeSize;
    DWORD key;
    
public:
    SelfModifyingCode(BYTE* code, DWORD size);
    
    // Encrypt/decrypt code block
    void encrypt();
    void decrypt();
    
    // Execute decrypted code
    void execute();
};

// =============================================================================
// SECTION 6: API Obfuscation
// =============================================================================

// Obfuscated Windows API wrappers
namespace ObfAPI {
    
    // OpenProcess with obfuscation
    HANDLE ObfOpenProcess(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId);
    
    // VirtualAllocEx with obfuscation
    LPVOID ObfVirtualAllocEx(HANDLE hProcess, LPVOID lpAddress, 
                             SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect);
    
    // WriteProcessMemory with obfuscation
    BOOL ObfWriteProcessMemory(HANDLE hProcess, LPVOID lpBaseAddress,
                               LPCVOID lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesWritten);
    
    // GetThreadContext with obfuscation
    BOOL ObfGetThreadContext(HANDLE hThread, LPCONTEXT lpContext);
    
    // SetThreadContext with obfuscation
    BOOL ObfSetThreadContext(HANDLE hThread, const CONTEXT* lpContext);
    
    // CreateRemoteThread with obfuscation
    HANDLE ObfCreateRemoteThread(HANDLE hProcess, LPSECURITY_ATTRIBUTES lpThreadAttributes,
                                  SIZE_T dwStackSize, LPTHREAD_START_ROUTINE lpStartAddress,
                                  LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId);
    
    // ResumeThread with obfuscation
    DWORD ObfResumeThread(HANDLE hThread);
    
    // SuspendThread with obfuscation
    DWORD ObfSuspendThread(HANDLE hThread);
    
    // GetFileAttributes with obfuscation
    BOOL ObfGetFileAttributes(LPCWSTR lpFileName, LPWIN32_FILE_ATTRIBUTE_DATA lpFileInformation);
    
    // LoadLibraryW with obfuscation
    HMODULE ObfLoadLibraryW(LPCWSTR lpLibFileName);
    
    // GetProcAddress with obfuscation
    FARPROC ObfGetProcAddress(HMODULE hModule, LPCSTR lpProcName);
}

// =============================================================================
// SECTION 7: Runtime Integrity Check
// =============================================================================

// Checksum calculator
DWORD calcChecksum(BYTE* data, DWORD size);

// Runtime integrity verification
class IntegrityChecker {
private:
    DWORD originalChecksum;
    BYTE* codeStart;
    DWORD codeSize;
    
public:
    IntegrityChecker(BYTE* code, DWORD size);
    
    // Calculate current checksum
    DWORD calculateCurrentChecksum();
    
    // Verify integrity
    bool verify();
    
    // Protect code section
    void protectSection();
};

// =============================================================================
// SECTION 8: Obfuscation Manager
// =============================================================================

class ObfuscationManager {
private:
    PolymorphicEngine polyEngine;
    ObfuscationVM vm;
    IntegrityChecker* integrityChecker;
    bool initialized;
    
public:
    ObfuscationManager();
    ~ObfuscationManager();
    
    // Initialize obfuscation
    bool initialize(BYTE* protectedCode, DWORD codeSize);
    
    // Run protected code in VM
    DWORD runInVM(const BYTE* bytecode, DWORD size);
    
    // Mutate data
    void mutateData(BYTE* data, DWORD size);
    
    // Check if being debugged/analyzed
    bool isBeingAnalyzed();
    
    // Cleanup
    void cleanup();
};

// Global instance
extern ObfuscationManager* g_ObfManager;

} // namespace ObfCore

#endif // OBFUSCATION_CORE_H
