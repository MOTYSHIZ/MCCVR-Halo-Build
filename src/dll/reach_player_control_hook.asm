; Reach direct-drive: entry detour for main_player_control_update (retail 0x1E0834).
; The native function computes the player's desired_angles from input and writes them to
; player_control (arg4 = r9): yaw @ +0x94, pitch @ +0x98. This detour runs native unchanged, then
; ALWAYS calls ReachPcApplyDesiredAngles(player_control) -- which decides (armed/valid/etc.) whether to
; overwrite those two angle fields with the VR aim, and logs its state. Naked so the native ABI
; (rcx/rdx/r8/r9 + xmm2, integer return in rax) is forwarded byte-exact, no C-signature guess.
; No allocation, logging, locks, file I/O or scans here (those live in the C callee).
option casemap:none

EXTERN g_reachPcOrig:QWORD                ; MinHook trampoline to native main_player_control_update
EXTERN ReachPcApplyDesiredAngles:PROC     ; void ReachPcApplyDesiredAngles(void* player_control /*rcx*/)

.code
ReachPlayerControlHook PROC
    sub  rsp, 38h                         ; 20h shadow + saves + align: entry rsp%16==8 -> here %16==0
    mov  QWORD PTR [rsp+30h], r9          ; save player_control across the native call (r9 is volatile)
    call QWORD PTR [g_reachPcOrig]        ; native runs (rcx/rdx/r8/r9/xmm2 still original); ret in rax
    mov  QWORD PTR [rsp+28h], rax         ; preserve native's return value across the C call
    mov  rcx, QWORD PTR [rsp+30h]         ; player_control -> arg1
    call ReachPcApplyDesiredAngles        ; always: it gates + logs internally (SEH inside)
    mov  rax, QWORD PTR [rsp+28h]         ; restore native's return value
    add  rsp, 38h
    ret
ReachPlayerControlHook ENDP
END
