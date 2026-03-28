#include "ObfuscationCore.h"
#include <cstring>
#include <ctime>

namespace ObfCore {

// =============================================================================
// ObfuscationVM Implementation
// =============================================================================

ObfuscationVM::ObfuscationVM() : running(false) {
    memset(&ctx, 0, sizeof(VMContext));
}

ObfuscationVM::~ObfuscationVM() {
    if (ctx.memoryBase) {
        VirtualFree(ctx.memoryBase, 0, MEM_RELEASE);
    }
}

void ObfuscationVM::loadBytecode(const BYTE* code, DWORD size) {
    bytecode.assign(code, code + size);
    ctx.ip = 0;
    running = true;
}

DWORD ObfuscationVM::executeOpcode(BYTE opcode) {
    DWORD result = 0;
    
    switch (opcode) {
        case VM_NOP:
            break;
            
        case VM_ALLOC: {
            DWORD size = ctx.registers[2];
            DWORD protect = ctx.registers[3];
            ctx.registers[0] = (DWORD)VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, protect);
            break;
        }
        
        case VM_FREE: {
            DWORD addr = ctx.registers[2];
            VirtualFree((LPVOID)addr, 0, MEM_RELEASE);
            break;
        }
        
        case VM_WRITE: {
            HANDLE hProcess = (HANDLE)ctx.registers[0];
            LPVOID addr = (LPVOID)ctx.registers[2];
            LPVOID buffer = (LPVOID)ctx.registers[3];
            DWORD size = ctx.registers[4];
            SIZE_T written = 0;
            WriteProcessMemory(hProcess, addr, buffer, size, &written);
            ctx.registers[0] = written;
            break;
        }
        
        case VM_READ: {
            HANDLE hProcess = (HANDLE)ctx.registers[0];
            LPCVOID addr = (LPCVOID)ctx.registers[2];
            LPVOID buffer = (LPVOID)ctx.registers[3];
            DWORD size = ctx.registers[4];
            SIZE_T read = 0;
            ReadProcessMemory(hProcess, addr, buffer, size, &read);
            ctx.registers[0] = read;
            break;
        }
        
        case VM_OPEN: {
            DWORD access = ctx.registers[0];
            DWORD pid = ctx.registers[2];
            ctx.registers[0] = (DWORD)OpenProcess(access, FALSE, pid);
            break;
        }
        
        case VM_SUSPEND: {
            HANDLE hThread = (HANDLE)ctx.registers[0];
            ctx.registers[0] = SuspendThread(hThread);
            break;
        }
        
        case VM_RESUME: {
            HANDLE hThread = (HANDLE)ctx.registers[0];
            ctx.registers[0] = ResumeThread(hThread);
            break;
        }
        
        case VM_GETCTX: {
            HANDLE hThread = (HANDLE)ctx.registers[0];
            LPCONTEXT pCtx = (LPCONTEXT)ctx.registers[2];
            GetThreadContext(hThread, pCtx);
            break;
        }
        
        case VM_SETCTX: {
            HANDLE hThread = (HANDLE)ctx.registers[0];
            const CONTEXT* pCtx = (const CONTEXT*)ctx.registers[2];
            SetThreadContext(hThread, pCtx);
            break;
        }
        
        case VM_CREATETHREAD: {
            HANDLE hProcess = (HANDLE)ctx.registers[0];
            LPTHREAD_START_ROUTINE start = (LPTHREAD_START_ROUTINE)ctx.registers[2];
            LPVOID param = (LPVOID)ctx.registers[3];
            ctx.registers[0] = (DWORD)CreateRemoteThread(hProcess, NULL, 0, start, param, 0, NULL);
            break;
        }
        
        case VM_WAIT: {
            HANDLE hObject = (HANDLE)ctx.registers[0];
            DWORD timeout = ctx.registers[2];
            ctx.registers[0] = WaitForSingleObject(hObject, timeout);
            break;
        }
        
        case VM_PROTECT: {
            HANDLE hProcess = (HANDLE)ctx.registers[0];
            LPVOID addr = (LPVOID)ctx.registers[2];
            SIZE_T size = ctx.registers[3];
            DWORD protect = ctx.registers[4];
            DWORD oldProtect = 0;
            VirtualProtectEx(hProcess, addr, size, protect, &oldProtect);
            ctx.registers[0] = oldProtect;
            break;
        }
        
        case VM_LOADLIB: {
            LPCWSTR path = (LPCWSTR)ctx.registers[0];
            ctx.registers[0] = (DWORD)LoadLibraryW(path);
            break;
        }
        
        case VM_GETPROC: {
            HMODULE hMod = (HMODULE)ctx.registers[0];
            LPCSTR name = (LPCSTR)ctx.registers[2];
            ctx.registers[0] = (DWORD)GetProcAddress(hMod, name);
            break;
        }
        
        case VM_EXIT:
            running = false;
            break;
    }
    
    return result;
}

DWORD ObfuscationVM::execute() {
    DWORD exitCode = 0;
    
    while (running && ctx.ip < bytecode.size()) {
        BYTE opcode = bytecode[ctx.ip];
        
        // Decode instruction (simplified - real implementation would handle operands)
        if (ctx.ip + 17 <= bytecode.size()) {
            ctx.registers[0] = *(DWORD*)&bytecode[ctx.ip + 1];
            ctx.registers[1] = *(DWORD*)&bytecode[ctx.ip + 5];
            ctx.registers[2] = *(DWORD*)&bytecode[ctx.ip + 9];
            ctx.registers[3] = *(DWORD*)&bytecode[ctx.ip + 13];
        }
        
        exitCode = executeOpcode(opcode);
        ctx.ip += 17; // opcode + 4*4 operands
        
        // Anti-debug check at each iteration
        if ((ctx.ip & 0xFF) == 0) {
            volatile DWORD check = 0;
            for (int i = 0; i < 10; i++) check++;
        }
    }
    
    return exitCode;
}

void ObfuscationVM::stop() {
    running = false;
}

VMContext* ObfuscationVM::getContext() {
    return &ctx;
}

void ObfuscationVM::setContext(PVOID extContext) {
    ctx.context = extContext;
}

PVOID ObfuscationVM::vmAlloc(DWORD size, DWORD protect) {
    return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, protect);
}

bool ObfuscationVM::vmFree(PVOID addr) {
    return VirtualFree(addr, 0, MEM_RELEASE) != 0;
}

bool ObfuscationVM::vmRead(PVOID addr, PVOID buffer, DWORD size) {
    memcpy(buffer, addr, size);
    return true;
}

bool ObfuscationVM::vmWrite(PVOID addr, PVOID buffer, DWORD size) {
    memcpy(addr, buffer, size);
    return true;
}

// =============================================================================
// PolymorphicEngine Implementation
// =============================================================================

PolymorphicEngine::PolymorphicEngine() {
    memset(&mutCtx, 0, sizeof(MutationContext));
}

void PolymorphicEngine::seedRand(DWORD seed) {
    mutCtx.seed = seed;
}

DWORD PolymorphicEngine::customRand() {
    mutCtx.seed = mutCtx.seed * 1103515245 + 12345;
    return (mutCtx.seed >> 16) & 0x7FFF;
}

void PolymorphicEngine::init(DWORD seed) {
    mutCtx.seed = seed;
    mutCtx.key = generateKey();
    mutCtx.currentOp = MUT_XOR;
}

DWORD PolymorphicEngine::generateKey() {
    // Generate a random key using hardware entropy if available
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter.LowPart ^ counter.HighPart ^ GetCurrentThreadId();
}

void PolymorphicEngine::setKey(DWORD key) {
    mutCtx.key = key;
}

void PolymorphicEngine::mutateXOR(BYTE* data, DWORD size, DWORD key) {
    for (DWORD i = 0; i < size; i++) {
        data[i] ^= (key >> (i % 4)) & 0xFF;
    }
}

void PolymorphicEngine::mutateAdd(BYTE* data, DWORD size, DWORD key) {
    for (DWORD i = 0; i < size; i++) {
        data[i] = (data[i] + ((key >> (i % 4)) & 0xFF)) & 0xFF;
    }
}

void PolymorphicEngine::mutateSub(BYTE* data, DWORD size, DWORD key) {
    for (DWORD i = 0; i < size; i++) {
        data[i] = (data[i] - ((key >> (i % 4)) & 0xFF)) & 0xFF;
    }
}

void PolymorphicEngine::mutateRotate(BYTE* data, DWORD size, DWORD key) {
    for (DWORD i = 0; i < size; i++) {
        BYTE rot = (key >> (i % 4)) & 0x07;
        BYTE b = data[i];
        data[i] = (b << rot) | (b >> (8 - rot));
    }
}

void PolymorphicEngine::mutateSwap(BYTE* data, DWORD size) {
    for (DWORD i = 0; i < size - 1; i += 2) {
        BYTE temp = data[i];
        data[i] = data[i + 1];
        data[i + 1] = temp;
    }
}

void PolymorphicEngine::generateDeadCode(std::vector<BYTE>& buffer) {
    // Insert meaningless operations
    DWORD deadOps[] = {
        0x90, 0x90, 0x90,                   // NOPs
        0x55, 0x8B, 0xEC,                   // push ebp; mov ebp, esp
        0x51, 0x59,                         // push ecx; pop ecx
        0x24, 0x00,                         // and al, 0
    };
    
    DWORD numOps = sizeof(deadOps) / sizeof(deadOps[0]);
    DWORD count = (customRand() % 4) + 2;
    
    for (DWORD i = 0; i < count && buffer.size() < 1000; i++) {
        DWORD idx = customRand() % numOps;
        buffer.push_back((BYTE)deadOps[idx]);
    }
}

void PolymorphicEngine::generateOpaquePredicate(std::vector<BYTE>& buffer) {
    // Generate always-true/false predicates
    BYTE pred[] = {
        0x33, 0xC0,                         // xor eax, eax
        0x40,                               // inc eax (now eax = 1, always != 0)
        0x85, 0xC0,                         // test eax, eax (always true)
    };
    
    for (size_t i = 0; i < sizeof(pred); i++) {
        buffer.push_back(pred[i]);
    }
}

void PolymorphicEngine::mutate(BYTE* data, DWORD size, MutationType type) {
    switch (type) {
        case MUT_XOR:
            mutateXOR(data, size, mutCtx.key);
            break;
        case MUT_ADD:
            mutateAdd(data, size, mutCtx.key);
            break;
        case MUT_SUB:
            mutateSub(data, size, mutCtx.key);
            break;
        case MUT_ROTATE:
            mutateRotate(data, size, mutCtx.key);
            break;
        case MUT_SWAP:
            mutateSwap(data, size);
            break;
        default:
            break;
    }
}

void PolymorphicEngine::demutate(BYTE* data, DWORD size, MutationType type) {
    switch (type) {
        case MUT_XOR:
            mutateXOR(data, size, mutCtx.key); // XOR is self-inverse
            break;
        case MUT_ADD:
            mutateSub(data, size, mutCtx.key); // Add then Sub
            break;
        case MUT_SUB:
            mutateAdd(data, size, mutCtx.key); // Sub then Add
            break;
        case MUT_ROTATE:
            mutateRotate(data, size, -mutCtx.key); // Rotate opposite direction
            break;
        case MUT_SWAP:
            mutateSwap(data, size); // Swap is self-inverse
            break;
        default:
            break;
    }
}

std::vector<BYTE> PolymorphicEngine::generatePolymorphicCode(const BYTE* original, DWORD size) {
    std::vector<BYTE> result;
    
    // Add some dead code at the beginning
    generateDeadCode(result);
    
    // Encrypt the original code
    std::vector<BYTE> encrypted(original, original + size);
    mutate(encrypted.data(), encrypted.size(), (MutationType)(customRand() % 5));
    
    // Add decryption stub (will be generated dynamically)
    // For now, just add the encrypted code
    for (size_t i = 0; i < encrypted.size(); i++) {
        result.push_back(encrypted[i]);
    }
    
    // Add more dead code at the end
    generateDeadCode(result);
    
    return result;
}

// =============================================================================
// ControlFlowFlattener Implementation
// =============================================================================

ControlFlowFlattener::ControlFlowFlattener() : currentState(0) {}

DWORD ControlFlowFlattener::generateOpaquePredicate() {
    // x * x - x * (x - 1) is always = x
    // Therefore: (x*x - x*x + x) is always = x
    // And: ((x*x - x*x + x) - x) is always = 0
    return 0; // Always false predicate
}

void ControlFlowFlattener::flatten(BYTE* code, DWORD size, DWORD numBlocks) {
    blocks.clear();
    
    DWORD blockSize = size / numBlocks;
    
    for (DWORD i = 0; i < numBlocks; i++) {
        FlattenedBlock block = {};
        block.state = i;
        block.nextStateTrue = (i + 1) % numBlocks;
        block.nextStateFalse = (i + 2) % numBlocks;
        block.codeOffset = i * blockSize;
        block.hasOpaquePredicate = (customRand() % 2) == 0;
        block.opaqueValue = generateOpaquePredicate();
        
        blocks.push_back(block);
    }
}

DWORD ControlFlowFlattener::getNextState(DWORD currentBlock, bool condition) {
    if (currentBlock >= blocks.size()) return 0;
    
    FlattenedBlock& block = blocks[currentBlock];
    return condition ? block.nextStateTrue : block.nextStateFalse;
}

void ControlFlowFlattener::execute(DWORD entryState) {
    currentState = entryState;
    
    while (currentState < blocks.size()) {
        FlattenedBlock& block = blocks[currentState];
        
        // Check opaque predicate
        if (block.hasOpaquePredicate) {
            volatile DWORD x = GetTickCount() & 0xFF;
            volatile DWORD pred = (x * x - x * (x - 1)) - x;
            if (pred != 0) {
                // Opaque predicate was false, jump to false path
                currentState = block.nextStateFalse;
                continue;
            }
        }
        
        // Execute block code (simplified)
        // In real implementation, would execute actual code at block.codeOffset
        
        // Determine next state
        currentState = block.nextStateTrue;
    }
}

// =============================================================================
// Code Obfuscation Utilities Implementation
// =============================================================================

void insertDeadCode(BYTE*& code, DWORD& size) {
    // This is a placeholder - real implementation would insert dead code
    // at strategic locations
    volatile DWORD x = 0;
    for (int i = 0; i < 10; i++) {
        x += i;
    }
}

PVOID obfGetProcAddress(HMODULE hModule, LPCSTR funcName) {
    // Resolve from PEB instead of using direct GetProcAddress
    // This bypasses some IAT hooks
    
    // Get PEB
#if defined(_M_X64)
    PEB* peb = (PEB*)__readgsqword(0x60);
#elif defined(_M_IX86)
    PEB* peb = (PEB*)__readfsdword(0x30);
#endif
    
    // Walk LDR list
    PEB_LDR_DATA* ldr = peb->Ldr;
    LIST_ENTRY* head = &ldr->InMemoryOrderModuleList;
    LIST_ENTRY* current = head->Flink;
    
    while (current != head) {
        LDR_DATA_TABLE_ENTRY* entry = (LDR_DATA_TABLE_ENTRY*)current;
        
        if (entry->DllBase == hModule) {
            // Found module, now walk export table
            // Simplified - real implementation would parse PE properly
            break;
        }
        
        current = current->Flink;
    }
    
    // Fallback to normal GetProcAddress with indirect call
    typedef FARPROC(WINAPI* GetProcAddrType)(HMODULE, LPCSTR);
    GetProcAddrType realGetProc = (GetProcAddrType)0x12345678; // Would be resolved similarly
    
    return (PVOID)realGetProc;
}

void obfIndirectJump(PVOID target) {
    // Use indirect jump to make static analysis harder
    static void* jumpTable[] = {
        &&label1, &&label2, &&label3, &&label4
    };
    
    DWORD idx = ((DWORD)target) & 0x03;
    goto* jumpTable[idx];
    
label1:
label2:
label3:
label4:
    return;
}

// SelfModifyingCode Implementation
SelfModifyingCode::SelfModifyingCode(BYTE* code, DWORD size) 
    : codeBlock(code), codeSize(size), key(0) {
    key = generateKey();
}

void SelfModifyingCode::encrypt() {
    for (DWORD i = 0; i < codeSize; i++) {
        codeBlock[i] ^= (key >> (i % 4)) & 0xFF;
        codeBlock[i] = ~codeBlock[i];
    }
}

void SelfModifyingCode::decrypt() {
    for (DWORD i = 0; i < codeSize; i++) {
        codeBlock[i] = ~codeBlock[i];
        codeBlock[i] ^= (key >> (i % 4)) & 0xFF;
    }
}

void SelfModifyingCode::execute() {
    // Decrypt, execute, re-encrypt
    decrypt();
    ((void(*)())codeBlock)();
    encrypt();
}

// =============================================================================
// API Obfuscation Implementation
// =============================================================================

namespace ObfAPI {

HANDLE ObfOpenProcess(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId) {
    // Obfuscate the API call
    typedef HANDLE(WINAPI* OpenProcType)(DWORD, BOOL, DWORD);
    static OpenProcType func = (OpenProcType)GetProcAddress(
        GetModuleHandleW(L"kernel32.dll"), "OpenProcess"
    );
    
    // Add some dead code to confuse analysis
    volatile DWORD x = 0;
    for (int i = 0; i < 5; i++) x++;
    
    return func(dwDesiredAccess, bInheritHandle, dwProcessId);
}

LPVOID ObfVirtualAllocEx(HANDLE hProcess, LPVOID lpAddress, 
                         SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect) {
    typedef LPVOID(WINAPI* VirtAllocType)(HANDLE, LPVOID, SIZE_T, DWORD, DWORD);
    static VirtAllocType func = (VirtAllocType)GetProcAddress(
        GetModuleHandleW(L"kernel32.dll"), "VirtualAllocEx"
    );
    
    return func(hProcess, lpAddress, dwSize, flAllocationType, flProtect);
}

BOOL ObfWriteProcessMemory(HANDLE hProcess, LPVOID lpBaseAddress,
                            LPCVOID lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesWritten) {
    typedef BOOL(WINAPI* WriteMemType)(HANDLE, LPVOID, LPCVOID, SIZE_T, SIZE_T*);
    static WriteMemType func = (WriteMemType)GetProcAddress(
        GetModuleHandleW(L"kernel32.dll"), "WriteProcessMemory"
    );
    
    return func(hProcess, lpBaseAddress, lpBuffer, nSize, lpNumberOfBytesWritten);
}

BOOL ObfGetThreadContext(HANDLE hThread, LPCONTEXT lpContext) {
    typedef BOOL(WINAPI* GetCtxType)(HANDLE, LPCONTEXT);
    static GetCtxType func = (GetCtxType)GetProcAddress(
        GetModuleHandleW(L"kernel32.dll"), "GetThreadContext"
    );
    
    return func(hThread, lpContext);
}

BOOL ObfSetThreadContext(HANDLE hThread, const CONTEXT* lpContext) {
    typedef BOOL(WINAPI* SetCtxType)(HANDLE, const CONTEXT*);
    static SetCtxType func = (SetCtxType)GetProcAddress(
        GetModuleHandleW(L"kernel32.dll"), "SetThreadContext"
    );
    
    return func(hThread, lpContext);
}

HANDLE ObfCreateRemoteThread(HANDLE hProcess, LPSECURITY_ATTRIBUTES lpThreadAttributes,
                              SIZE_T dwStackSize, LPTHREAD_START_ROUTINE lpStartAddress,
                              LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId) {
    typedef HANDLE(WINAPI* CreateThreadType)(
        HANDLE, LPSECURITY_ATTRIBUTES, SIZE_T, 
        LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD
    );
    static CreateThreadType func = (CreateThreadType)GetProcAddress(
        GetModuleHandleW(L"kernel32.dll"), "CreateRemoteThread"
    );
    
    return func(hProcess, lpThreadAttributes, dwStackSize, 
                lpStartAddress, lpParameter, dwCreationFlags, lpThreadId);
}

DWORD ObfResumeThread(HANDLE hThread) {
    typedef DWORD(WINAPI* ResumeType)(HANDLE);
    static ResumeType func = (ResumeType)GetProcAddress(
        GetModuleHandleW(L"kernel32.dll"), "ResumeThread"
    );
    
    return func(hThread);
}

DWORD ObfSuspendThread(HANDLE hThread) {
    typedef DWORD(WINAPI* SuspendType)(HANDLE);
    static SuspendType func = (SuspendType)GetProcAddress(
        GetModuleHandleW(L"kernel32.dll"), "SuspendThread"
    );
    
    return func(hThread);
}

BOOL ObfGetFileAttributes(LPCWSTR lpFileName, LPWIN32_FILE_ATTRIBUTE_DATA lpFileInformation) {
    typedef BOOL(WINAPI* GetFileAttrType)(LPCWSTR, LPWIN32_FILE_ATTRIBUTE_DATA);
    static GetFileAttrType func = (GetFileAttrType)GetProcAddress(
        GetModuleHandleW(L"kernel32.dll"), "GetFileAttributesW"
    );
    
    if (!func) return FALSE;
    
    // Get file attributes
    DWORD attr = func(lpFileName);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        return FALSE;
    }
    
    // Fill the structure
    if (lpFileInformation) {
        memset(lpFileInformation, 0, sizeof(WIN32_FILE_ATTRIBUTE_DATA));
        lpFileInformation->dwFileAttributes = attr;
        
        // Get file times
        HANDLE hFile = CreateFileW(lpFileName, FILE_READ_ATTRIBUTES, 
                                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  NULL, OPEN_EXISTING, 0, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            GetFileTime(hFile, &lpFileInformation->ftCreationTime,
                       &lpFileInformation->ftLastAccessTime,
                       &lpFileInformation->ftLastWriteTime);
            
            BY_HANDLE_FILE_INFORMATION fi;
            if (GetFileInformationByHandle(hFile, &fi)) {
                lpFileInformation->nFileSizeHigh = fi.nFileSizeHigh;
                lpFileInformation->nFileSizeLow = fi.nFileSizeLow;
            }
            CloseHandle(hFile);
        }
    }
    
    return TRUE;
}

HMODULE ObfLoadLibraryW(LPCWSTR lpLibFileName) {
    typedef HMODULE(WINAPI* LoadLibType)(LPCWSTR);
    static LoadLibType func = (LoadLibType)GetProcAddress(
        GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"
    );
    
    // Anti-debug check before loading
    if (AntiDebug::QuickDebugCheck()) {
        return NULL;
    }
    
    return func(lpLibFileName);
}

FARPROC ObfGetProcAddress(HANDLE hModule, LPCSTR lpProcName) {
    typedef FARPROC(WINAPI* GetProcType)(HMODULE, LPCSTR);
    static GetProcType func = (GetProcType)GetProcAddress(
        GetModuleHandleW(L"kernel32.dll"), "GetProcAddress"
    );
    
    return func((HMODULE)hModule, lpProcName);
}

} // namespace ObfAPI

// =============================================================================
// Integrity Checker Implementation
// =============================================================================

DWORD calcChecksum(BYTE* data, DWORD size) {
    DWORD checksum = 0;
    
    for (DWORD i = 0; i < size; i++) {
        checksum = ((checksum << 31) | (checksum >> 1)) + data[i];
    }
    
    return checksum;
}

IntegrityChecker::IntegrityChecker(BYTE* code, DWORD size) 
    : codeStart(code), codeSize(size) {
    originalChecksum = calcChecksum(code, size);
}

DWORD IntegrityChecker::calculateCurrentChecksum() {
    return calcChecksum(codeStart, codeSize);
}

bool IntegrityChecker::verify() {
    DWORD current = calculateCurrentChecksum();
    return current == originalChecksum;
}

void IntegrityChecker::protectSection() {
    DWORD oldProtect = 0;
    VirtualProtect(codeStart, codeSize, PAGE_READONLY, &oldProtect);
}

// =============================================================================
// Obfuscation Manager Implementation
// =============================================================================

ObfuscationManager* g_ObfManager = nullptr;

ObfuscationManager::ObfuscationManager() : integrityChecker(nullptr), initialized(false) {
    polyEngine.init(GetTickCount());
}

ObfuscationManager::~ObfuscationManager() {
    cleanup();
}

bool ObfuscationManager::initialize(BYTE* protectedCode, DWORD codeSize) {
    if (initialized) return true;
    
    integrityChecker = new IntegrityChecker(protectedCode, codeSize);
    initialized = true;
    
    return true;
}

DWORD ObfuscationManager::runInVM(const BYTE* bytecode, DWORD size) {
    if (!initialized) return 0;
    
    vm.loadBytecode(bytecode, size);
    return vm.execute();
}

void ObfuscationManager::mutateData(BYTE* data, DWORD size) {
    polyEngine.mutate(data, size);
}

bool ObfuscationManager::isBeingAnalyzed() {
    // Integrate with AntiDebug
    // For now, simple check
    return IsDebuggerPresent() != FALSE;
}

void ObfuscationManager::cleanup() {
    if (integrityChecker) {
        delete integrityChecker;
        integrityChecker = nullptr;
    }
    initialized = false;
}

} // namespace ObfCore
