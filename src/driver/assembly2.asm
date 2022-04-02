;++
;
; Copyright (c) Alex Ionescu.  All rights reserved.
;
; Module:
;
;    shvvmxhvx64.asm
;
; Abstract:
;
;    This module implements the AMD64-specific SimpleVisor VMENTRY routine.
;
; Author:
;
;    Alex Ionescu (@aionescu) 16-Mar-2016 - Initial version
;
; Environment:
;
;    Kernel mode only.
;
;--

include ksamd64.inc

    .code

    extern ShvVmxEntryHandler:proc
    extern ShvOsCaptureContext:proc

    ShvVmxEntry PROC
    push    rcx                 ; save the RCX register, which we spill below
    lea     rcx, [rsp+8h]       ; store the context in the stack, bias for
                                ; the return address and the push we just did.
    sub rsp, 30h
    call    ShvOsCaptureContext
    add rsp, 30h
    mov rcx, [rsp+CxRsp+8h]
    add rcx, 8h
    mov [rsp+CxRsp+8h], rcx

    pop rcx
    mov [rsp+CxRcx], rcx

    jmp     ShvVmxEntryHandler  ; jump to the C code handler. we assume that it
                                ; compiled with optimizations and does not use
                                ; home space, which is true of release builds.
    ShvVmxEntry ENDP

    end