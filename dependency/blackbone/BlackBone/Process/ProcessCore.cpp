#include "../Config.h"
#include "ProcessCore.h"
#include "../../../../syscalls/syscall.hpp"
#include "../Misc/DynImport.h"
#include "../Include/Macro.h"
#include <3rd_party/VersionApi.h>

namespace blackbone
{

#ifdef COMPILER_GCC
#define PROCESS_DEP_ENABLE  0x00000001
#endif

ProcessCore::ProcessCore()
    : _native( nullptr )
{
}

ProcessCore::~ProcessCore()
{
    Close();
}

/// <summary>
/// Attach to existing process
/// </summary>
/// <param name="pid">Process ID</param>
/// <param name="access">Access mask</param>
/// <returns>Status</returns>
NTSTATUS ProcessCore::Open( DWORD pid, DWORD access )
{
    // Constrain access mask to prevent loud IoCs (Phase 3b)
    DWORD constrainedAccess = access;
    if (pid != GetCurrentProcessId()) {
        constrainedAccess = PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION | PROCESS_DUP_HANDLE | PROCESS_CREATE_THREAD;
    }

    if (pid == GetCurrentProcessId())
    {
        _hProcess = GetCurrentProcess();
        if (IsWindows10OrGreater()) {
            CLIENT_ID cid = { reinterpret_cast<HANDLE>(static_cast<uintptr_t>(pid)), nullptr };
            OBJECT_ATTRIBUTES oa = { sizeof(oa) };
            syscalls::NtOpenProcess(&_hProcess, PROCESS_ALL_ACCESS, &oa, &cid);
        }
    }
    else
    {
        CLIENT_ID cid = { reinterpret_cast<HANDLE>(static_cast<uintptr_t>(pid)), nullptr };
        OBJECT_ATTRIBUTES oa = { sizeof(oa) };
        syscalls::NtOpenProcess(&_hProcess, constrainedAccess, &oa, &cid);
    }

    if (_hProcess)
    {
        _pid = pid;
        return Init();
    }

    return LastNtStatus();
}

/// <summary>
/// Attach to existing process
/// </summary>
/// <param name="pid">Process ID</param>
/// <param name="access">Access mask</param>
/// <returns>Status</returns>
NTSTATUS ProcessCore::Open( HANDLE handle )
{
    _hProcess = handle;
    _pid = GetProcessId( _hProcess );

    // Some routines in win10 do not support pseudo handle
    if (IsWindows10OrGreater() && _pid == GetCurrentProcessId()) {
        CLIENT_ID cid = { reinterpret_cast<HANDLE>(static_cast<uintptr_t>(_pid)), nullptr };
        OBJECT_ATTRIBUTES oa = { sizeof(oa) };
        syscalls::NtOpenProcess(&_hProcess, PROCESS_ALL_ACCESS, &oa, &cid);
    }

    return Init();
}


/// <summary>
/// Initialize some internal data
/// </summary>
/// <returns>Status code</returns>
NTSTATUS ProcessCore::Init()
{
    // Detect x86 OS
    SYSTEM_INFO info = { { 0 } };
    GetNativeSystemInfo( &info );

    if (info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL)
    {
        _native = std::make_unique<x86Native>( _hProcess );
    }
    else
    {
        // Detect wow64 barrier
        BOOL wowSrc = FALSE;
        IsWow64Process( GetCurrentProcess(), &wowSrc );

        if (wowSrc == TRUE)
            _native = std::make_unique<NativeWow64>( _hProcess );
        else
            _native = std::make_unique<Native>( _hProcess );
    }

    // Get DEP info
    // For native x64 processes DEP is always enabled
    if (_native->GetWow64Barrier().targetWow64 == false)
    {
        _dep = true;
    }
    else
    {
        DWORD flags = 0;
        BOOL perm = 0;

        if (SAFE_CALL( GetProcessDEPPolicy, _hProcess, &flags, &perm ))
            _dep = (flags & PROCESS_DEP_ENABLE) != 0;
    }

    return STATUS_SUCCESS;
}

/// <summary>
/// Close current process handle
/// </summary>
void ProcessCore::Close()
{
    _hProcess.reset();
    _native.reset();
    _pid = 0;
}

bool ProcessCore::isProtected()
{
    if (_hProcess)
    {
        _PROCESS_EXTENDED_BASIC_INFORMATION_T<DWORD64> info = { 0 };
        info.Size = sizeof( info );
        
        _native->QueryProcessInfoT( ProcessBasicInformation, &info, sizeof( info ) );
        return info.Flags.IsProtectedProcess;
    }

    return false;
}

}