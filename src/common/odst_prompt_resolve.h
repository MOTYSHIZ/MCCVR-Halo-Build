#pragma once
// ODST'S PROMPT AND MAP STATE, WHERE IT IS IN THIS BUILD OF halo3odst.dll (docs/ODST-PROMPT-AND-MAP-EVIDENCE-2026-11-15.md).
//
// VR Mappings' ODST prompt and map rows (vr_mapping::OdstAction) act only while one of ODST's tutorial prompts waits for
// an answer or the VISR map is open. The game keeps both in the controller's action-test record: the confirm prompt's
// byte and the cancel prompt's byte the tutorial scripts set (unit_confirm_message, unit_confirm_cancel_message), and the
// tests word, whose bit 3 is the PDA (the map) up. The mod reads that record at the control builder's read of accept.
//
// NOTHING HERE TRUSTS A RECORDED ADDRESS. Every place is found by a signature that must match exactly once in the
// module's executable sections, wherever it is, and every number the mod then uses is read out of the matched
// instructions, never written into the mod:
//
//   - the builder's read of accept (action 0x12, then cancel 0x13): where the hook reads the record (its call's return
//     address), the builder's output field the two answers go to, and that its call is the abstract-input reader the
//     action bridge hooked;
//   - unit_confirm_message's and unit_confirm_cancel_message's stores: the engine TLS index, the TLS block offset of
//     the action-test records, and the offsets of the two prompt bytes;
//   - the tests' update: the same output field turned into test bits 1 and 2, and the PDA's test bit 3, in one function;
//   - the prompts' clear: the same first prompt byte cleared, with its neighbours, once a test fires.
//
// CROSS-CHECKS. Both script functions load the same TLS index, and it is the module's own (its PE TLS directory) where
// that is known; both name the same block offset; the cancel byte follows the confirm byte; the builder and the tests'
// update name the same output field; the tests' update and the PDA test lie in one function; the clear names the confirm
// byte. Any line that fails is named, and nothing is used: the rows stay off and every control keeps its usual job.
//
// Engine-free: it reads an image through drive_resolve::Image, so the same code resolves the module loaded in the game
// (the DLL opens it with drive_cascade::OpenModuleImage) and a halo3odst.dll file laid out by an offline check
// (tests/odst_prompt_resolve_check.cpp). It allocates nothing and logs nothing.
#include "drive_resolve.h"

#include <cstdio>

namespace odst_prompt_resolve
{
// THE SIGNATURES. "??" bytes are read, not matched: a rel32, or a field this resolution decodes and cross-checks.
// The builder: mov edx,0x12 ... call reader; mov rcx,rax; call pressed; mov ecx,[rdi+FIELD]; test al,al; je;
// or ecx,0x20; jmp; and ecx,~0x20; mov [rdi+FIELD],ecx; mov edx,0x13; mov rcx,rsi; call reader.
inline constexpr char kAcceptRead[] =
    "BA 12 00 00 00 48 8B 85 ?? ?? ?? ?? 48 03 F1 48 8B CE 48 89 74 24 ?? 48 89 30 E8 ?? ?? ?? ?? 48 8B C8 "
    "E8 ?? ?? ?? ?? 8B 4F ?? 84 C0 74 05 83 C9 20 EB 03 83 E1 DF 89 4F ?? BA 13 00 00 00 48 8B CE E8";
inline constexpr uint32_t kAcceptReadCall = 0x1A, kAcceptReadReturn = 0x1F, kAcceptReadField = 0x29,
                          kAcceptReadFieldStore = 0x38;
// unit_confirm_message: mov ecx,[rip+TLS index]; mov rax,gs:[0x58]; ... (the unit's player, its controller) ...
// mov edx,BLOCK; lea rcx,[rax+rax*2]; shl rcx,4; add rcx,[rdx+r10]; mov byte [rcx+FLAG],1; and qword [rcx],~2.
inline constexpr char kConfirmStore[] =
    "8B 0D ?? ?? ?? ?? 65 48 8B 04 25 58 00 00 00 BA 20 00 00 00 4C 8B 14 C8 41 8B 00 0F B7 C8 4A 8B 14 12 "
    "48 8D 0C 49 48 8B 42 48 48 8B 4C C8 10 8B 89 70 01 00 00 83 F9 FF 74 25 E8 ?? ?? ?? ?? 83 F8 03 77 1B 48 98 "
    "BA ?? ?? ?? ?? 48 8D 0C 40 48 C1 E1 04 4A 03 0C 12 C6 41 ?? 01 48 83 21 FD";
// unit_confirm_cancel_message: the same, clearing test bit 2.
inline constexpr char kCancelStore[] =
    "8B 0D ?? ?? ?? ?? 65 48 8B 04 25 58 00 00 00 BA 20 00 00 00 4C 8B 14 C8 41 8B 00 0F B7 C8 4A 8B 14 12 "
    "48 8D 0C 49 48 8B 42 48 48 8B 4C C8 10 8B 89 70 01 00 00 83 F9 FF 74 25 E8 ?? ?? ?? ?? 83 F8 03 77 1B 48 98 "
    "BA ?? ?? ?? ?? 48 8D 0C 40 48 C1 E1 04 4A 03 0C 12 C6 41 ?? 01 48 83 21 FB";
inline constexpr uint32_t kStoreTlsDisp = 2, kStoreTlsNext = 6, kStoreBlock = 0x47, kStoreFlag = 0x59;
// The tests' update: mov eax,[r8+FIELD]; test al,0x20; je; or qword [r11+r10*8],2; mov eax,[r8+FIELD]; test al,0x40;
// je; or qword [r11+r10*8],4.
inline constexpr char kTests[] = "41 8B 40 ?? A8 20 74 09 4B 83 0C D3 02 41 8B 40 ?? A8 40 74 09 4B 83 0C D3 04";
inline constexpr uint32_t kTestsField = 3, kTestsFieldAgain = 0x10;
// The PDA's test: rax = tests & ~8; rcx = tests | 8; cmp [rbx+PDA],r14b; cmove rcx,rax; store.
inline constexpr char kPdaTest[] = "48 8B C1 48 83 E0 F7 48 83 C9 08 44 38 73 ?? 48 0F 44 C8 4B 89 0C D3";
// The prompts' clear: cmp [r15+FLAG],r10b; je; (test bit 1 fired) and dword [r15+FLAG],0; then the cancel byte's.
inline constexpr char kClear[] =
    "45 38 57 ?? 74 0F 41 8A 07 D0 E8 41 84 C3 74 05 41 83 67 ?? 00 45 38 57 ?? 74 10 41 8A 07 C0 E8 02 41 84 C3 "
    "74 05 41 83 67 ?? 00";
inline constexpr uint32_t kClearFlag = 3, kClearFlagAnd = 0x13, kClearCancel = 0x18, kClearCancelAnd = 0x29;

// The PDA's test bit in the tests word, as kPdaTest's masks say (F7 = ~8, 08).
inline constexpr uint32_t kPdaTestBit = 3;

struct Result
{
    bool ok = false;
    uint32_t acceptReadReturn = 0;   // the reader's return address at the builder's read of accept (RVA)
    uint32_t builderStart = 0;       // the control builder's start (.pdata)
    uint32_t readerRva = 0;          // what the builder's read calls
    uint32_t tlsIndexRva = 0;        // the engine TLS index the script functions load
    uint32_t block = 0;              // the TLS block offset of the action-test records
    uint32_t confirmFlag = 0, cancelFlag = 0;   // the prompt bytes in a record
    uint32_t stride = 0x30;          // a record per controller: lea rcx,[rax+rax*2]; shl rcx,4 (in the signature)
    uint32_t pdaBit = kPdaTestBit;
    // What was not found or did not agree, one name each, joined by "; " (empty when ok).
    char missing[384]{};
};

namespace detail
{
inline void Note(Result& r, const char* what)
{
    const size_t used = std::strlen(r.missing);
    std::snprintf(r.missing + used, sizeof(r.missing) - used, "%s%s", used ? "; " : "", what);
}
inline bool Read32(const drive_resolve::Image& im, uint32_t rva, uint32_t& out)
{
    uint8_t b[4]{};
    if (!drive_resolve::ReadOriginal(im, rva, b, 4))
        return false;
    std::memcpy(&out, b, 4);
    return true;
}
inline bool Read8(const drive_resolve::Image& im, uint32_t rva, uint32_t& out)
{
    uint8_t b = 0;
    if (!drive_resolve::ReadOriginal(im, rva, &b, 1))
        return false;
    out = b;
    return true;
}
// The single match of `text` in the module's code, or 0 (and a note naming it: none, or several).
inline uint32_t Unique(const drive_resolve::Image& im, const char* text, const char* name, Result& r)
{
    drive_resolve::Pattern p{};
    if (!drive_resolve::Compile(text, p))
    {
        char line[96];
        std::snprintf(line, sizeof(line), "%s (bad signature)", name);
        Note(r, line);
        return 0;
    }
    const auto hits = drive_resolve::CountExecutable(im, p);
    if (hits.count == 1)
        return hits.first;
    char line[96];
    std::snprintf(line, sizeof(line), "%s (%s)", name, hits.count ? "several matches" : "not found");
    Note(r, line);
    return 0;
}
// A rel32 operand: the address it names.
inline bool RelTarget(const drive_resolve::Image& im, uint32_t dispRva, uint32_t nextRva, uint32_t& target)
{
    uint32_t disp = 0;
    if (!Read32(im, dispRva, disp))
        return false;
    target = nextRva + disp;   // wraps as the CPU's addition does
    return true;
}
} // namespace detail

// `readerRva`: the abstract-input reader the action bridge hooked (0: not checked, an offline check that resolves the
// reader on its own). The image's tlsIndexRva, when set, must be the index both script functions load.
inline Result Resolve(const drive_resolve::Image& im, uint32_t readerRva)
{
    using namespace detail;
    Result r{};
    const uint32_t read = Unique(im, kAcceptRead, "the control builder's read of accept", r);
    const uint32_t confirm = Unique(im, kConfirmStore, "unit_confirm_message's store", r);
    const uint32_t cancel = Unique(im, kCancelStore, "unit_confirm_cancel_message's store", r);
    const uint32_t tests = Unique(im, kTests, "the accept and cancel tests", r);
    const uint32_t pda = Unique(im, kPdaTest, "the PDA's test", r);
    const uint32_t clear = Unique(im, kClear, "the prompts' clear", r);

    if (read)
    {
        uint32_t target = 0, field = 0, store = 0;
        r.acceptReadReturn = read + kAcceptReadReturn;
        r.builderStart = drive_resolve::FunctionStartOf(im, read);
        if (!r.builderStart)
            Note(r, "the control builder's start (.pdata)");
        if (!RelTarget(im, read + kAcceptReadCall + 1, read + kAcceptReadReturn, target) ||
            !drive_resolve::InExecutable(im, target, 1))
            Note(r, "the builder's reader call");
        else if (readerRva && target != readerRva)
            Note(r, "the builder's read does not call the hooked reader");
        else
            r.readerRva = target;
        if (!Read8(im, read + kAcceptReadField, field) || !Read8(im, read + kAcceptReadFieldStore, store) ||
            field != store)
            Note(r, "the builder's answer field");
        else if (tests)
        {
            uint32_t testsField = 0, again = 0;
            if (!Read8(im, tests + kTestsField, testsField) || !Read8(im, tests + kTestsFieldAgain, again) ||
                testsField != field || again != field)
                Note(r, "the tests read another field than the builder writes");
        }
    }
    if (confirm && cancel)
    {
        uint32_t confirmIndex = 0, cancelIndex = 0, confirmBlock = 0, cancelBlock = 0, confirmFlag = 0, cancelFlag = 0;
        const bool decoded =
            RelTarget(im, confirm + kStoreTlsDisp, confirm + kStoreTlsNext, confirmIndex) &&
            RelTarget(im, cancel + kStoreTlsDisp, cancel + kStoreTlsNext, cancelIndex) &&
            Read32(im, confirm + kStoreBlock, confirmBlock) && Read32(im, cancel + kStoreBlock, cancelBlock) &&
            Read8(im, confirm + kStoreFlag, confirmFlag) && Read8(im, cancel + kStoreFlag, cancelFlag);
        if (!decoded)
            Note(r, "the script functions' operands");
        else if (confirmIndex != cancelIndex || !drive_resolve::InImage(im, confirmIndex, 4))
            Note(r, "the two script functions load different TLS indexes");
        else if (im.tlsIndexRva && confirmIndex != im.tlsIndexRva)
            Note(r, "the script functions' TLS index is not the module's own");
        else if (confirmBlock != cancelBlock || !confirmBlock || confirmBlock > 0x10000 || (confirmBlock & 7u))
            Note(r, "the script functions' record block");
        else if (cancelFlag != confirmFlag + 1 || confirmFlag + 4 > r.stride)
            Note(r, "the prompt bytes");
        else
        {
            r.tlsIndexRva = confirmIndex;
            r.block = confirmBlock;
            r.confirmFlag = confirmFlag;
            r.cancelFlag = cancelFlag;
        }
    }
    if (tests && pda)
    {
        const uint32_t testsStart = drive_resolve::FunctionStartOf(im, tests);
        if (!testsStart || testsStart != drive_resolve::FunctionStartOf(im, pda))
            Note(r, "the PDA's test is not in the tests' update");
    }
    if (clear && r.confirmFlag)
    {
        uint32_t a = 0, b = 0, c = 0, d = 0;
        if (!Read8(im, clear + kClearFlag, a) || !Read8(im, clear + kClearFlagAnd, b) ||
            !Read8(im, clear + kClearCancel, c) || !Read8(im, clear + kClearCancelAnd, d) ||
            a != r.confirmFlag || b != r.confirmFlag || c != r.cancelFlag || d != r.confirmFlag)
            Note(r, "the prompts' clear names other bytes");
    }
    r.ok = !r.missing[0] && read && confirm && cancel && tests && pda && clear && r.readerRva && r.tlsIndexRva;
    if (!r.ok && !r.missing[0])
        Note(r, "an operand");
    return r;
}
} // namespace odst_prompt_resolve
