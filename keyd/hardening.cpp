#include "keyd/hardening.hpp"

#include <windows.h>

#include "keyd/protocol.hpp"

namespace izan::keyd {

namespace {

    bool exclude_from_wer()
    {
        // WerAddExcludedApplication lives in wer.dll; resolve it
        // dynamically so the import table stays kernel-only.
        HMODULE wer = LoadLibraryW(L"wer.dll");
        if (!wer)
            return false;
        using Fn = HRESULT(WINAPI*)(PCWSTR, BOOL);
        auto fn = reinterpret_cast<Fn>(
            GetProcAddress(wer, "WerAddExcludedApplication"));
        if (!fn)
            return false;
        wchar_t self[MAX_PATH];
        if (!GetModuleFileNameW(nullptr, self, MAX_PATH))
            return false;
        return fn(self, FALSE) == S_OK;
    }

}

uint8_t apply_process_hardening()
{
    uint8_t engaged = 0;

    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX
        | SEM_NOOPENFILEERRORBOX);
    if (exclude_from_wer())
        engaged |= kHardenedDumps;

    if (SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32))
        engaged |= kHardenedDllDirs;

    PROCESS_MITIGATION_DYNAMIC_CODE_POLICY dynCode {};
    dynCode.ProhibitDynamicCode = 1;
    if (SetProcessMitigationPolicy(
            ProcessDynamicCodePolicy, &dynCode, sizeof dynCode))
        engaged |= kHardenedDynCode;

    PROCESS_MITIGATION_BINARY_SIGNATURE_POLICY sig {};
    sig.MicrosoftSignedOnly = 1;
    if (SetProcessMitigationPolicy(ProcessSignaturePolicy, &sig, sizeof sig))
        engaged |= kHardenedDllSig;

    return engaged;
}

}
