;////////////////////////////////////////////////////////////////////////////////////////////////////////
;// Part of Injectable Generic Camera System
;// Copyright(c) 2020, Frans Bouma
;// All rights reserved.
;// https://github.com/FransBouma/InjectableGenericCameraSystem
;//
;// Redistribution and use in source and binary forms, with or without
;// modification, are permitted provided that the following conditions are met :
;//
;//  * Redistributions of source code must retain the above copyright notice, this
;//	  list of conditions and the following disclaimer.
;//
;//  * Redistributions in binary form must reproduce the above copyright notice,
;//    this list of conditions and the following disclaimer in the documentation
;//    and / or other materials provided with the distribution.
;//
;// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
;// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
;// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
;// DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
;// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
;// DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
;// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
;// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
;// OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
;// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
;////////////////////////////////////////////////////////////////////////////////////////////////////////
;---------------------------------------------------------------
; Game specific asm file to intercept execution flow to obtain addresses, prevent writes etc.
;---------------------------------------------------------------

;---------------------------------------------------------------
; Public definitions so the linker knows which names are present in this file
PUBLIC cameraStructInterceptor
PUBLIC cameraWriteInjection1
PUBLIC cameraWriteInjection2
PUBLIC cameraWriteInjection3
PUBLIC cameraWriteInjection4
PUBLIC cameraWriteInjection5
PUBLIC carPositionInterceptor

;---------------------------------------------------------------

;---------------------------------------------------------------
; Externs which are used and set by the system. Read / write these
; values in asm to communicate with the system
EXTERN g_cameraEnabled: byte
EXTERN g_cameraStructAddress: qword
EXTERN g_cameraQuaternionAddress: qword
EXTERN g_cameraPositionAddress: qword
EXTERN g_carPositionAddress: qword
;---------------------------------------------------------------

;---------------------------------------------------------------
; Own externs, defined in InterceptorHelper.cpp
EXTERN _cameraStructInterceptionContinue: qword
EXTERN _cameraWriteInjection1Continue: qword
EXTERN _cameraWriteInjection2Continue: qword
EXTERN _cameraWriteInjection3Continue: qword
EXTERN _cameraWriteInjection4Continue: qword
EXTERN _cameraWriteInjection5Continue: qword
EXTERN _carPositionInjectionContinue: qword

.data

.code


cameraStructInterceptor PROC
;aob 4C 8B A1 F0 38 04 00
;dirtrally2.exe+AF8513 - 0F85 C6050000         - jne dirtrally2.exe+AF8ADF
;dirtrally2.exe+AF8519 - 48 8B 81 A8370400     - mov rax,[rcx+000437A8]	
;dirtrally2.exe+AF8520 - 4D 89 63 E8           - mov [r11-18],r12
;dirtrally2.exe+AF8524 - 4C 8B A1 F0380400     - mov r12,[rcx+000438F0]		<<<INJECT/R12 is cam base
;dirtrally2.exe+AF852B - 4D 89 73 E0           - mov [r11-20],r14
;dirtrally2.exe+AF852F - 4C 8B B0 60030000     - mov r14,[rax+00000360]
;dirtrally2.exe+AF8536 - 4D 89 7B D8           - mov [r11-28],r15			<<RETURN
;dirtrally2.exe+AF853A - 45 32 FF              - xor r15b,r15b
;dirtrally2.exe+AF853D - 4D 85 F6              - test r14,r14
	mov r12,[rcx+000438F0h]
	mov [g_cameraStructAddress],r12
	push rdi
	lea rdi,[r12+130h]
	mov [g_cameraQuaternionAddress],rdi
	xor rdi,rdi
	lea rdi,[r12+140h]
	mov [g_cameraPositionAddress],rdi
	pop rdi
	mov [r11-20h],r14
	mov r14,[rax+00000360h]
	jmp qword ptr [_cameraStructInterceptionContinue]
cameraStructInterceptor ENDP

cameraWriteInjection1 PROC
;0F C6 D2 27 F3 0F 10 D1 0F C6 D2 27 0F 29 12 48
;dirtrally2.exe+C5252C - 0FC6 D2 C6            - shufps xmm2,xmm2,-3A { 198 }
;dirtrally2.exe+C52530 - 0F28 C7               - movaps xmm0,xmm7
;dirtrally2.exe+C52533 - 0F28 3C 24            - movaps xmm7,[rsp]
;dirtrally2.exe+C52537 - F3 0F10 D1            - movss xmm2,xmm1
;dirtrally2.exe+C5253B - 0FC6 D2 C6            - shufps xmm2,xmm2,-3A { 198 }
;dirtrally2.exe+C5253F - 0F28 C8               - movaps xmm1,xmm0
;dirtrally2.exe+C52542 - 0FC6 D2 27            - shufps xmm2,xmm2,27 { 39 }		<INJECT
;dirtrally2.exe+C52546 - F3 0F10 D1            - movss xmm2,xmm1
;dirtrally2.exe+C5254A - 0FC6 D2 27            - shufps xmm2,xmm2,27 { 39 }
;dirtrally2.exe+C5254E - 0F29 12               - movaps [rdx],xmm2				<<SKIP
;dirtrally2.exe+C52551 - 48 83 C4 28           - add rsp,28 { 40 }				<<RETURN
	shufps xmm2,xmm2,27h
	movss xmm2,xmm1
	shufps xmm2,xmm2,27h
	cmp byte ptr [g_cameraEnabled], 1
	jne originalcode
	cmp [g_cameraQuaternionAddress],rdx
	je skip
originalcode:
	movaps [rdx],xmm2
skip:
   jmp qword ptr [_cameraWriteInjection1Continue]
cameraWriteInjection1 ENDP


cameraWriteInjection2 PROC
;0F 29 03 F3 0F 5C 4B 70
;dirtrally2.exe+A3971E - E8 6D36AFFF           - call dirtrally2.exe+52CD90
;dirtrally2.exe+A39723 - 0F28 45 00            - movaps xmm0,[rbp+00]
;dirtrally2.exe+A39727 - F3 0F10 4D A0         - movss xmm1,[rbp-60]
;dirtrally2.exe+A3972C - 0F29 03               - movaps [rbx],xmm0			<<INJECT/SKIP
;dirtrally2.exe+A3972F - F3 0F5C 4B 70         - subss xmm1,[rbx+70]
;dirtrally2.exe+A39734 - F3 0F59 CF            - mulss xmm1,xmm7
;dirtrally2.exe+A39738 - F3 0F58 4B 70         - addss xmm1,[rbx+70]
;dirtrally2.exe+A3973D - F3 0F11 4B 70         - movss [rbx+70],xmm1		<<skip this for FOV too
;dirtrally2.exe+A39742 - F3 0F10 4D A8         - movss xmm1,[rbp-58]		<<return
;dirtrally2.exe+A39747 - F3 0F5C 4B 78         - subss xmm1,[rbx+78]
;dirtrally2.exe+A3974C - F3 0F59 CF            - mulss xmm1,xmm7			
;dirtrally2.exe+A39750 - F3 0F58 4B 78         - addss xmm1,[rbx+78]
;dirtrally2.exe+A39755 - F3 0F11 4B 78         - movss [rbx+78],xmm1
;dirtrally2.exe+A3975A - F3 0F10 83 AC000000   - movss xmm0,[rbx+000000AC]
	cmp byte ptr [g_cameraEnabled], 1
	jne noskip
	cmp [g_cameraQuaternionAddress],rbx
	je skip
noskip:
	movaps [rbx],xmm0
skip:
	subss xmm1,dword ptr [rbx+70h]
	mulss xmm1,xmm7
	addss xmm1,dword ptr [rbx+70h]
	cmp byte ptr [g_cameraEnabled], 1
	jne noskipB
	cmp [g_cameraQuaternionAddress],rbx
	je skipB
noskipB:
	movss dword ptr [rbx+70h],xmm1
skipB:
   jmp qword ptr [_cameraWriteInjection2Continue]
cameraWriteInjection2 ENDP

cameraWriteInjection3 PROC
;0F 29 43 10 0F 5C 73 50
;dirtrally2.exe+A396FE - 0F59 D7               - mulps xmm2,xmm7
;dirtrally2.exe+A39701 - 0F55 CA               - andnps xmm1,xmm2
;dirtrally2.exe+A39704 - 0F58 4B 10            - addps xmm1,[rbx+10]
;dirtrally2.exe+A39708 - 0F55 C1               - andnps xmm0,xmm1
;dirtrally2.exe+A3970B - 0F29 43 10            - movaps [rbx+10],xmm0		<<INJECT/SKIP
;dirtrally2.exe+A3970F - 0F5C 73 50            - subps xmm6,[rbx+50]
;dirtrally2.exe+A39713 - 0F59 F7               - mulps xmm6,xmm7
;dirtrally2.exe+A39716 - 0F58 73 50            - addps xmm6,[rbx+50]
;dirtrally2.exe+A3971A - 0F29 73 50            - movaps [rbx+50],xmm6		<<RETURN
;dirtrally2.exe+A3971E - E8 6D36AFFF           - call dirtrally2.exe+52CD90
	cmp byte ptr [g_cameraEnabled], 1
	jne noskip
	cmp [g_cameraQuaternionAddress],rbx
	je skip
noskip:
	movaps [rbx+10h],xmm0
skip:
	subps xmm6,[rbx+50h]
	mulps xmm6,xmm7
	addps xmm6,[rbx+50h]
   jmp qword ptr [_cameraWriteInjection3Continue]
cameraWriteInjection3 ENDP


cameraWriteInjection4 PROC
;0F C6 D2 27 0F 29 56 50 0F
;dirtrally2.exe+A39CCB - F3 0F58 C8            - addss xmm1,xmm0
;dirtrally2.exe+A39CCF - 0F57 C0               - xorps xmm0,xmm0
;dirtrally2.exe+A39CD2 - F3 0F11 8E A4010000   - movss [rsi+000001A4],xmm1
;dirtrally2.exe+A39CDA - 0F28 C8               - movaps xmm1,xmm0
;dirtrally2.exe+A39CDD - 0F28 56 50            - movaps xmm2,[rsi+50]
;dirtrally2.exe+A39CE1 - 0FC6 D2 27            - shufps xmm2,xmm2,27 { 39 }
;dirtrally2.exe+A39CE5 - F3 0F10 D1            - movss xmm2,xmm1
;dirtrally2.exe+A39CE9 - 0FC6 D2 27            - shufps xmm2,xmm2,27 { 39 }				<<INJECT
;dirtrally2.exe+A39CED - 0F29 56 50            - movaps [rsi+50],xmm2
;dirtrally2.exe+A39CF1 - 0F28 86 20010000      - movaps xmm0,[rsi+00000120]
;dirtrally2.exe+A39CF8 - 66 0F7F 06            - movdqa [rsi],xmm0						<<SKIP
;dirtrally2.exe+A39CFC - E8 6F61C8FF           - call dirtrally2.AK::MusicEngine::Term	<<RETURN
;dirtrally2.exe+A39D01 - 4C 8D 9C 24 20020000  - lea r11,[rsp+00000220]
;dirtrally2.exe+A39D09 - 49 8B 5B 10           - mov rbx,[r11+10]
	shufps xmm2,xmm2,27h
	movaps [rsi+50h],xmm2
	movaps xmm0,[rsi+00000120h]
	cmp byte ptr [g_cameraEnabled], 1
	jne noskip
	cmp [g_cameraQuaternionAddress],rsi
	je skip
noskip:
	movdqa [rsi],xmm0
skip:
	jmp qword ptr [_cameraWriteInjection4Continue]
cameraWriteInjection4 ENDP

cameraWriteInjection5 PROC
;F3 0F 10 5C 24 58 0F 14 D8 0F
;dirtrally2.exe+ADD080 - FF 50 10              - call qword ptr [rax+10]
;dirtrally2.exe+ADD083 - F3 0F10 44 24 40      - movss xmm0,[rsp+40]
;dirtrally2.exe+ADD089 - 0F57 C9               - xorps xmm1,xmm1
;dirtrally2.exe+ADD08C - F3 0F10 54 24 48      - movss xmm2,[rsp+48]
;dirtrally2.exe+ADD092 - F3 0F10 5C 24 58      - movss xmm3,[rsp+58]		<<INJECT
;dirtrally2.exe+ADD098 - 0F14 D8               - unpcklps xmm3,xmm0
;dirtrally2.exe+ADD09B - 0F14 D1               - unpcklps xmm2,xmm1
;dirtrally2.exe+ADD09E - 0F14 DA               - unpcklps xmm3,xmm2
;dirtrally2.exe+ADD0A1 - 0F29 1E               - movaps [rsi],xmm3			<<SKIP ROTATION AND POSITION
;dirtrally2.exe+ADD0A4 - 48 83 C4 20           - add rsp,20 { 32 }			<<RETURN
;dirtrally2.exe+ADD0A8 - 5F                    - pop rdi
	movss xmm3, dword ptr [rsp+58h]
	unpcklps xmm3,xmm0
	unpcklps xmm2,xmm1
	unpcklps xmm3,xmm2
	cmp byte ptr [g_cameraEnabled], 1
	jne noskip
	cmp [g_cameraPositionAddress],rsi
	je skip
noskip:
	movaps [rsi],xmm3
skip:
	jmp qword ptr [_cameraWriteInjection5Continue]
cameraWriteInjection5 ENDP

carPositionInterceptor PROC
;dirtrally2.exe+7490B0 - 48 83 EC 18           - sub rsp,18 { 24 }
;dirtrally2.exe+7490B4 - F3 0F10 81 C0020000   - movss xmm0,[rcx+000002C0]
;dirtrally2.exe+7490BC - F3 0F10 A9 C4020000   - movss xmm5,[rcx+000002C4]
;dirtrally2.exe+7490C4 - F3 0F10 89 C8020000   - movss xmm1,[rcx+000002C8]
;dirtrally2.exe+7490CC - F3 0F10 99 B0020000   - movss xmm3,[rcx+000002B0] <<INJECT/take rcx
;dirtrally2.exe+7490D4 - F3 0F10 A1 B8020000   - movss xmm4,[rcx+000002B8]
;dirtrally2.exe+7490DC - F3 0F59 ED            - mulss xmm5,xmm5			<<return
;dirtrally2.exe+7490E0 - 0F29 34 24            - movaps [rsp],xmm6
;dirtrally2.exe+7490E4 - F3 0F10 B1 B4020000   - movss xmm6,[rcx+000002B4]
;dirtrally2.exe+7490EC - F3 0F59 A9 64030000   - mulss xmm5,[rcx+00000364]
;dirtrally2.exe+7490F4 - F3 0F59 F6            - mulss xmm6,xmm6
;dirtrally2.exe+7490F8 - F3 0F59 C9            - mulss xmm1,xmm1
;dirtrally2.exe+7490FC - F3 0F59 C0            - mulss xmm0,xmm0
	movss xmm3,dword ptr [rcx+000002B0h]
	movss xmm4,dword ptr [rcx+000002B8h]
	mov [g_carPositionAddress],rcx
	jmp qword ptr [_carPositionInjectionContinue]
carPositionInterceptor ENDP

END