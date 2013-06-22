;	VirtualDub - Video processing and capture application
;	Copyright (C) 1998-2001 Avery Lee
;
;	This program is free software; you can redistribute it and/or modify
;	it under the terms of the GNU General Public License as published by
;	the Free Software Foundation; either version 2 of the License, or
;	(at your option) any later version.
;
;	This program is distributed in the hope that it will be useful,
;	but WITHOUT ANY WARRANTY; without even the implied warranty of
;	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;	GNU General Public License for more details.
;
;	You should have received a copy of the GNU General Public License
;	along with this program; if not, write to the Free Software
;	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

	.686
	.mmx
	.xmm
	.model	flat


	.const

	align 16

ISSE_02w		dq	00002000200020002h

predictorsISSE_Y		dd	predict_Y_normal_ISSE
			dd	predict_Y_halfpelX_ISSE
			dd	predict_Y_halfpelY_ISSE
			dd	predict_Y_quadpel_ISSE
addersISSE_Y		dd	predict_add_Y_normal_ISSE
			dd	predict_add_Y_halfpelX_ISSE
			dd	predict_add_Y_halfpelY_ISSE
			dd	predict_add_Y_quadpel_ISSE
predictorsISSE_C		dd	predict_C_normal_ISSE
			dd	predict_C_halfpelX_ISSE
			dd	predict_C_halfpelY_ISSE
			dd	predict_C_quadpel_ISSE
addersISSE_C		dd	predict_add_C_normal_ISSE
			dd	predict_add_C_halfpelX_ISSE
			dd	predict_add_C_halfpelY_ISSE
			dd	predict_add_C_quadpel_ISSE

	align 16
predict_Y_halfpelX_table	dq	 0,64
				dq	 8,56
				dq	16,48
				dq	24,40
				dq	32,32
				dq	40,24
				dq	48,16
				dq	56, 8
				dq	64, 0

	.code

src	equ	[esp+4+16]
dst	equ	[esp+8+16]
vx	equ	[esp+12+16]
vy	equ	[esp+16+16]
pitch	equ	[esp+20+16]

	public	_video_copy_prediction_Y_ISSE

	align	16
_video_copy_prediction_Y_ISSE:
	push	ebp
	push	edi
	push	esi
	push	ebx

	mov	esi,pitch
	mov	ecx,src
	mov	eax,vx
	mov	ebx,vy

	mov	edi,eax			;edi = vx
	mov	ebp,ebx			;ebp = vy
	sar	edi,1			;edi = x
	and	eax,1			;eax = x half-pel
	sar	ebp,1			;ebp = y
	add	ecx,edi			;ecx = src + x
	imul	ebp,esi			;ebp = y*pitch
	add	ecx,ebp			;ecx = src + x + y*pitch
	and	ebx,1			;ebx = y half-pel
	shl	eax,2
	mov	edx,dst
	call	dword ptr [predictorsISSE_Y+eax+ebx*8]

	pop	ebx
	pop	esi
	pop	edi
	pop	ebp
	ret

;*********************************************************
;*
;*	Luminance - quadpel
;*
;*********************************************************

	align 16

predict_Y_quadpel_ISSE:
	movq	mm6,ISSE_02w

	pxor	mm7,mm7
	mov	edi,16
predict_Y_quadpel_ISSE@loop:
	mov	eax,2
predict_Y_quadpel_ISSE@loop_2:
	movq	mm0,[ecx]
	prefetcht0 [ecx+esi]

	movd	mm1,[ecx+8]
	movq	mm4,mm0

	movq	mm2,[ecx+esi]
	psrlq	mm4,8

	movd	mm3,[ecx+esi+8]
	psllq	mm1,56

	por	mm1,mm4
	movq	mm5,mm2

	psrlq	mm5,8
	movq	mm4,mm0

	psllq	mm3,56
	punpcklbw mm0,mm7

	por	mm3,mm5
	punpckhbw mm4,mm7

	movq	mm5,mm1
	punpcklbw mm1,mm7

	punpckhbw mm5,mm7
	paddw	mm0,mm1		;mm0: low total
	movq	mm1,mm2

	paddw	mm4,mm5		;mm4: high total
	movq	mm5,mm3

	punpcklbw mm2,mm7
	add	edx,8

	punpcklbw mm3,mm7
	paddw	mm0,mm2

	punpckhbw mm1,mm7
	paddw	mm0,mm3

	punpckhbw mm5,mm7
	paddw	mm4,mm1
	
	paddw	mm4,mm5
	paddw	mm0,mm6

	paddw	mm4,mm6
	psrlw	mm0,2

	psrlw	mm4,2
	add	ecx,8

	packuswb mm0,mm4

	movq	[edx-8],mm0

	dec	eax
	jne	predict_Y_quadpel_ISSE@loop_2

	lea	ecx,[ecx+esi-16]
	lea	edx,[edx+esi-16]

	dec	edi
	jne	predict_Y_quadpel_ISSE@loop

	ret


;*********************************************************
;*
;*	Luminance - half-pel Y
;*
;*********************************************************

	align 16
predict_Y_halfpelY_ISSE:
	mov	edi,8
	mov	ebx,7
	and	ebx,ecx
	jz	predict_Y_halfpelY_ISSE@start_aligned

	shl	ebx,3
	mov	edi,64
	sub	edi,ebx
	and	ecx,0fffffff8h
	movd	mm7,ebx
	movd	mm6,edi
	mov	edi,16

predict_Y_halfpelY_ISSE@loop_unaligned:
	prefetcht0 [ecx+esi]
	movq	mm0,[ecx]
	movq	mm1,[ecx+8]

	psrlq	mm0,mm7
	movq	mm2,mm1
	psllq	mm1,mm6

	movq	mm3,[ecx+16]
	psrlq	mm2,mm7
	psllq	mm3,mm6

	por	mm0,mm1
	movq	mm4,[ecx+esi]
	por	mm2,mm3

	movq	mm1,[ecx+esi+8]
	psrlq	mm4,mm7
	movq	mm5,mm1

	psllq	mm1,mm6
	movq	mm3,[ecx+esi+16]
	psrlq	mm5,mm7

	psllq	mm3,mm6
	por	mm4,mm1
	pavgb	mm0,mm4

	movq	[edx],mm0
	por	mm5,mm3
	pavgb	mm2,mm5

	movq	[edx+8],mm2
	add	ecx,esi
	add	edx,esi

	dec	edi
	jne	predict_Y_halfpelY_ISSE@loop_unaligned
	ret

predict_Y_halfpelY_ISSE@start_aligned:
	movq	mm0,[ecx]
	movq	mm1,[ecx+8]

predict_Y_halfpelY_ISSE@loop_aligned:
	prefetcht0 [ecx+esi*2]
	movq	mm2,[ecx+esi]
	dec	edi

	movq	mm3,[ecx+esi+8]
	pavgb	mm0,mm2

	movq	mm4,[ecx+esi*2]
	pavgb	mm1,mm3

	movq	[edx],mm0
	pavgb	mm2,mm4

	movq	mm5,[ecx+esi*2+8]
	movq	mm0,mm4

	movq	[edx+8],mm1
	pavgb	mm3,mm5

	movq	[edx+esi],mm2
	movq	mm1,mm5

	movq	[edx+esi+8],mm3

	lea	ecx,[ecx+esi*2]
	lea	edx,[edx+esi*2]

	jne	predict_Y_halfpelY_ISSE@loop_aligned
	ret

;*********************************************************
;*
;*	Luminance - half-pel X
;*
;*********************************************************

	align 16
predict_Y_halfpelX_ISSE:
	mov	edi,16
	mov	ebx,ecx
	and	ebx,7
	shl	ebx,4
	and	ecx,0fffffff8h
	sub	edx,esi
	add	ebx,offset predict_Y_halfpelX_table
	movq	mm7,qword ptr [ebx+0]

predict_Y_halfpelX_ISSE@loop:
	movq	mm0,[ecx]			;left
	add	edx,esi

	movq	mm1,[ecx+8]			;left
	movq	mm2,mm0				;left

	psrlq	mm0,mm7				;left
	movq	mm3,mm1				;left

	psrlq	mm2,qword ptr [ebx+16]		;left
	movq	mm4,mm3				;right

	psllq	mm3,qword ptr [ebx+24]		;left
	movq	mm5,mm4				;right

	psllq	mm1,qword ptr [ebx+8]		;left
	por	mm2,mm3				;left

	movq	mm3,[ecx+16]			;right
	por	mm0,mm1				;left

	psrlq	mm5,qword ptr [ebx+16]		;right
	movq	mm1,mm3				;right

	psllq	mm1,qword ptr [ebx+24]		;right
	psrlq	mm4,mm7				;right

	psllq	mm3,qword ptr [ebx+8]		;right
	por	mm5,mm1				;right
	por	mm4,mm3				;right

	pavgb	mm0,mm2				;left
	pavgb	mm4,mm5				;right
	add	ecx,esi

	dec	edi
	movq	[edx],mm0			;left
	movq	[edx+8],mm4			;right
	jne	predict_Y_halfpelX_ISSE@loop

	ret

;*********************************************************
;*
;*	Luminance - normal
;*
;*********************************************************

	align 16
predict_Y_normal_ISSE:
	mov	eax,8
	mov	ebx,7
	and	ebx,ecx
	jz	predict_Y_normal_ISSE@loop_aligned

	shl	ebx,3
	mov	edi,64
	sub	edi,ebx
	and	ecx,0fffffff8h
	movd	mm7,ebx
	movd	mm6,edi
	mov	eax,16

predict_Y_normal_ISSE@loop_unaligned:
	movq	mm1,[ecx+8]
	movq	mm0,[ecx]
	movq	mm2,mm1
	psrlq	mm0,mm7
	movq	mm3,[ecx+16]
	psllq	mm1,mm6
	psrlq	mm2,mm7
	por	mm0,mm1
	psllq	mm3,mm6
	por	mm2,mm3
	movq	[edx],mm0
	movq	[edx+8],mm2
	add	ecx,esi
	add	edx,esi
	dec	eax
	jne	predict_Y_normal_ISSE@loop_unaligned
	ret

	align 16
predict_Y_normal_ISSE@loop_aligned:
	movq	mm0,[ecx]
	movq	mm1,[ecx+8]
	movq	mm2,[ecx+esi]
	movq	mm3,[ecx+esi+8]
	movq	[edx],mm0
	movq	[edx+8],mm1
	movq	[edx+esi],mm2
	movq	[edx+esi+8],mm3
	lea	ecx,[ecx+esi*2]
	lea	edx,[edx+esi*2]
	dec	eax
	jne	predict_Y_normal_ISSE@loop_aligned

	ret





;**************************************************************************
;*
;*
;*
;*  Addition predictors
;*
;*
;*
;**************************************************************************

	public	_video_add_prediction_Y_ISSE

	align	16
_video_add_prediction_Y_ISSE:
	push	ebp
	push	edi
	push	esi
	push	ebx

	mov	esi,pitch
	mov	ecx,src
	mov	eax,vx
	mov	ebx,vy

	mov	edi,eax			;edi = vx
	mov	ebp,ebx			;ebp = vy
	sar	edi,1			;edi = x
	and	eax,1			;eax = x half-pel
	sar	ebp,1			;ebp = y
	add	ecx,edi			;ecx = src + x
	imul	ebp,esi			;ebp = y*pitch
	add	ecx,ebp			;ecx = src + x + y*pitch
	and	ebx,1			;ebx = y half-pel
	shl	eax,2
	mov	edx,dst
	call	dword ptr [addersISSE_Y+eax+ebx*8]

	pop	ebx
	pop	esi
	pop	edi
	pop	ebp
	ret

;*********************************************************
;*
;*	Luminance - quadpel
;*
;*********************************************************

	align 16
predict_add_Y_quadpel_ISSE:
	movq	mm6,ISSE_02w

	pxor	mm7,mm7
	mov	edi,16
add_Y_quadpel_ISSE@loop:
	mov	eax,2
add_Y_quadpel_ISSE@loop2:
	movq	mm0,[ecx]
	prefetcht0 [edx]

	movd	mm1,[ecx+8]
	movq	mm4,mm0

	movq	mm2,[ecx+esi]
	psrlq	mm4,8

	movd	mm3,[ecx+esi+8]
	psllq	mm1,56

	por	mm1,mm4
	movq	mm5,mm2

	psrlq	mm5,8
	movq	mm4,mm0

	psllq	mm3,56
	punpcklbw mm0,mm7

	por	mm3,mm5
	punpckhbw mm4,mm7

	movq	mm5,mm1
	punpcklbw mm1,mm7

	punpckhbw mm5,mm7
	paddw	mm0,mm1		;mm0: low total

	paddw	mm4,mm5		;mm4: high total
	movq	mm1,mm2

	movq	mm5,mm3
	punpcklbw mm2,mm7

	punpcklbw mm3,mm7
	add	edx,8

	punpckhbw mm1,mm7
	paddw	mm2,mm3

	punpckhbw mm5,mm7

	paddw	mm1,mm5
	paddw	mm0,mm2

	paddw	mm4,mm1

	add	ecx,8

	paddw	mm0,mm6
	paddw	mm4,mm6

	psrlw	mm0,2
	psrlw	mm4,2

	packuswb mm0,mm4

	pavgb	mm0,[edx-8]
	dec	eax

	movq	[edx-8],mm0
	jne	add_Y_quadpel_ISSE@loop2

	lea	ecx,[ecx+esi-16]
	lea	edx,[edx+esi-16]

	dec	edi
	jne	add_Y_quadpel_ISSE@loop

	ret

;*********************************************************
;*
;*	Luminance - half-pel Y
;*
;*********************************************************

	align 16
predict_add_Y_halfpelY_ISSE:
	movq	mm0,[ecx]
	movq	mm1,[ecx+8]
	add	ecx,esi
	mov	edi,16
predict_add_Y_halfpelY_ISSE@loop:
	movq	mm2,[ecx]
	movq	mm3,[ecx+8]
	pavgb	mm0,mm2
	pavgb	mm1,mm3
	pavgb	mm0,[edx]
	pavgb	mm1,[edx+8]
		
	add	ecx,esi
	dec	edi

	movq	[edx],mm0
	movq	mm0,mm2
	movq	[edx+8],mm1
	movq	mm1,mm3

	lea	edx,[edx+esi]
	jne	predict_add_Y_halfpelY_ISSE@loop

	ret

;*********************************************************
;*
;*	Luminance - half-pel X
;*
;*********************************************************

	align 16
predict_add_Y_halfpelX_ISSE:
	mov	edi,16
predict_add_Y_halfpelX_ISSE@loop:
	movq	mm0,[ecx]
	movq	mm1,[ecx+8]
	pavgb	mm0,[ecx+1]
	pavgb	mm1,[ecx+9]
	pavgb	mm0,[edx]
	pavgb	mm1,[edx+8]

	add	ecx,esi
	dec	edi

	movq	[edx],mm0
	movq	[edx+8],mm1

	lea	edx,[edx+esi]
	jne	predict_add_Y_halfpelX_ISSE@loop

	ret



;*********************************************************
;*
;*	Luminance - normal
;*
;*********************************************************

	align 16
predict_add_Y_normal_ISSE:
	mov	edi,16
	mov	ebx,7
	and	ebx,ecx
	jz	add_Y_normal_ISSE@loop_aligned

;*** unaligned loop

	shl	ebx,3
	mov	ebp,64
	sub	ebp,ebx
	and	ecx,0fffffff8h
	movd	mm5,ebx
	movd	mm4,ebp

add_Y_normal_ISSE@loop_unaligned:
	prefetcht0 [edx]
	movq	mm1,[ecx+8]
	movq	mm0,[ecx]

	movq	mm2,mm1
	psrlq	mm0,mm5
	psllq	mm1,mm4

	por	mm0,mm1
	movq	mm3,[ecx+16]
	psrlq	mm2,mm5

	pavgb	mm0,[edx]
	psllq	mm3,mm4
	por	mm2,mm3

	movq	[edx],mm0

	pavgb	mm2,[edx+8]
	add	ecx,esi
	dec	edi

	movq	[edx+8],mm2
	lea	edx,[edx+esi]
	jne	add_Y_normal_ISSE@loop_unaligned

	ret

;*** aligned loop

add_Y_normal_ISSE@loop_aligned:
	prefetcht0 [ecx+esi]
	prefetcht0 [edx+esi]
	movq	mm0,[ecx]
	movq	mm1,[ecx+8]
	pavgb	mm0,[edx]
	pavgb	mm1,[edx+8]
	movq	[edx],mm0
	movq	[edx+8],mm1

	movq	mm2,[ecx+esi]
	movq	mm3,[ecx+esi+8]
	pavgb	mm2,[edx+esi]
	pavgb	mm3,[edx+esi+8]
	movq	[edx+esi],mm2
	movq	[edx+esi+8],mm3

	lea	ecx,[ecx+esi*2]
	lea	edx,[edx+esi*2]

	sub	edi,2
	jne	add_Y_normal_ISSE@loop_aligned

	ret










;**************************************************************************

	public	_video_copy_prediction_C_ISSE

	align	16
_video_copy_prediction_C_ISSE:
	push	ebp
	push	edi
	push	esi
	push	ebx

	mov	esi,pitch
	mov	ecx,src
	mov	eax,vx
	mov	ebx,vy

	mov	edi,eax			;edi = vx
	mov	ebp,ebx			;ebp = vy
	sar	edi,1			;edi = x
	and	eax,1			;eax = x half-pel
	sar	ebp,1			;ebp = y
	add	ecx,edi			;ecx = src + x
	imul	ebp,esi			;ebp = y*pitch
	add	ecx,ebp			;ecx = src + x + y*pitch
	and	ebx,1			;ebx = y half-pel
	shl	eax,2
	mov	edx,dst
	call	dword ptr [predictorsISSE_C+eax+ebx*8]

	pop	ebx
	pop	esi
	pop	edi
	pop	ebp
	ret


;*********************************************************
;*
;*	Luminance - quadpel
;*
;*********************************************************

	align 16
predict_C_quadpel_ISSE:
	movq	mm6,ISSE_02w

	pxor	mm7,mm7
	mov	edi,8
predict_C_quadpel_ISSE@loop:
	movq	mm0,[ecx]
	movd	mm1,[ecx+8]
	movq	mm4,mm0
	psrlq	mm4,8
	psllq	mm1,56
	por	mm1,mm4

	movq	mm2,[ecx+esi]
	movd	mm3,[ecx+esi+8]
	movq	mm5,mm2
	psrlq	mm5,8
	psllq	mm3,56
	por	mm3,mm5

	movq	mm4,mm0
	movq	mm5,mm1

	punpcklbw mm0,mm7
	punpcklbw mm1,mm7
	punpckhbw mm4,mm7
	punpckhbw mm5,mm7

	paddw	mm0,mm1		;mm0: low total
	paddw	mm4,mm5		;mm4: high total

	movq	mm1,mm2
	movq	mm5,mm3

	punpcklbw mm2,mm7
	punpcklbw mm3,mm7
	punpckhbw mm1,mm7
	punpckhbw mm5,mm7

	paddw	mm2,mm3
	paddw	mm1,mm5

	paddw	mm0,mm2
	paddw	mm4,mm1

	paddw	mm0,mm6
	paddw	mm4,mm6

	psrlw	mm0,2
	psrlw	mm4,2

	packuswb mm0,mm4

	movq	[edx],mm0

	add	ecx,esi
	add	edx,esi

	dec	edi
	jne	predict_C_quadpel_ISSE@loop

	ret


;*********************************************************
;*
;*	Luminance - half-pel Y
;*
;*********************************************************

	align 16
predict_C_halfpelY_ISSE:

	movq	mm0,[ecx]
	mov	edi,4

predict_C_halfpelY_ISSE@loop:
	movq	mm2,[ecx+esi]
	movq	mm4,[ecx+esi*2]
	dec	edi

	pavgb	mm0,mm2
	pavgb	mm2,mm4
	lea	ecx,[ecx+esi*2]

	movq	[edx],mm0
	movq	mm0,mm4

	movq	[edx+esi],mm2
	lea	edx,[edx+esi*2]
	jne	predict_C_halfpelY_ISSE@loop

	ret

;*********************************************************
;*
;*	Luminance - half-pel X
;*
;*********************************************************

	align 16
predict_C_halfpelX_ISSE:
	mov	edi,4
predict_C_halfpelX_ISSE@loop:
	movq	mm0,[ecx]
	movq	mm2,[ecx+esi]

	pavgb	mm0,[ecx+1]

	movq	[edx],mm0

	pavgb	mm2,[ecx+esi+1]

	movq	[edx+esi],mm2
	lea	ecx,[ecx+esi*2]
	dec	edi

	lea	edx,[edx+esi*2]
	jne	predict_C_halfpelX_ISSE@loop

	ret

;*********************************************************
;*
;*	Luminance - normal
;*
;*********************************************************

	align 16
predict_C_normal_ISSE:
	movq	mm0,[ecx]
	lea	ebx,[edx+esi*2]

	movq	mm1,[ecx+esi]
	lea	eax,[ecx+esi*2]

	movq	[edx],mm0
	add	ebx,esi

	movq	mm2,[ecx+esi*2]
	add	eax,esi

	movq	[edx+esi],mm1

	movq	mm3,[eax]
	lea	eax,[eax+esi*2]

	movq	[edx+esi*2],mm2

	movq	mm4,[ecx+esi*4]

	movq	[ebx],mm3
	lea	ebx,[ebx+esi*2]

	movq	mm5,[eax]

	movq	[edx+esi*4],mm4

	movq	mm6,[eax+esi]

	movq	[ebx],mm5

	movq	mm7,[eax+esi*2]

	movq	[ebx+esi],mm6

	movq	[ebx+esi*2],mm7

	ret





;**************************************************************************
;*
;*
;*
;*  Addition predictors
;*
;*
;*
;**************************************************************************

	public	_video_add_prediction_C_ISSE

	align	16
_video_add_prediction_C_ISSE:
	push	ebp
	push	edi
	push	esi
	push	ebx

	mov	esi,pitch
	mov	ecx,src
	mov	eax,vx
	mov	ebx,vy

	mov	edi,eax			;edi = vx
	mov	ebp,ebx			;ebp = vy
	sar	edi,1			;edi = x
	and	eax,1			;eax = x half-pel
	sar	ebp,1			;ebp = y
	add	ecx,edi			;ecx = src + x
	imul	ebp,esi			;ebp = y*pitch
	add	ecx,ebp			;ecx = src + x + y*pitch
	and	ebx,1			;ebx = y half-pel
	shl	eax,2
	mov	edx,dst
	call	dword ptr [addersISSE_C+eax+ebx*8]

	pop	ebx
	pop	esi
	pop	edi
	pop	ebp
	ret

;*********************************************************
;*
;*	Luminance - quadpel
;*
;*********************************************************

	align 16
predict_add_C_quadpel_ISSE:
	movq	mm6,ISSE_02w

	pxor	mm7,mm7
	mov	edi,8
add_C_quadpel_ISSE@loop:
	movq	mm0,[ecx]
	movd	mm1,[ecx+8]
	movq	mm4,mm0
	psrlq	mm4,8
	psllq	mm1,56
	por	mm1,mm4

	movq	mm2,[ecx+esi]
	movd	mm3,[ecx+esi+8]
	movq	mm5,mm2
	psrlq	mm5,8
	psllq	mm3,56
	por	mm3,mm5

	movq	mm4,mm0
	movq	mm5,mm1

	punpcklbw mm0,mm7
	punpcklbw mm1,mm7
	punpckhbw mm4,mm7
	punpckhbw mm5,mm7

	paddw	mm0,mm1		;mm0: low total
	paddw	mm4,mm5		;mm4: high total

	movq	mm1,mm2
	movq	mm5,mm3

	punpcklbw mm2,mm7
	punpcklbw mm3,mm7
	punpckhbw mm1,mm7
	punpckhbw mm5,mm7

	paddw	mm2,mm3
	paddw	mm1,mm5

	paddw	mm0,mm2

	paddw	mm4,mm1

	paddw	mm0,mm6
	paddw	mm4,mm6

	psrlw	mm0,2
	psrlw	mm4,2

	packuswb mm0,mm4

	pavgb	mm0,[edx]

	movq	[edx],mm0

	add	ecx,esi
	add	edx,esi

	dec	edi
	jne	add_C_quadpel_ISSE@loop

	ret

;*********************************************************
;*
;*	Luminance - half-pel Y
;*
;*********************************************************

	align 16
predict_add_C_halfpelY_ISSE:
	mov	edi,8
predict_add_C_halfpelY_ISSE@loop:
	movq	mm0,[ecx]
	pavgb	mm0,[ecx+esi]
	pavgb	mm0,[edx]
	add	ecx,esi
	dec	edi

	movq	[edx],mm0

	lea	edx,[edx+esi]
	jne	predict_add_C_halfpelY_ISSE@loop

	ret

;*********************************************************
;*
;*	Luminance - half-pel X
;*
;*********************************************************

	align 16
predict_add_C_halfpelX_ISSE:
	mov	edi,8
predict_add_C_halfpelX_ISSE@loop:
	movq	mm0,[ecx]
	pavgb	mm0,[ecx+1]
	pavgb	mm0,[edx]
	
	add	ecx,esi
	dec	edi

	movq	[edx],mm0

	lea	edx,[edx+esi]
	jne	predict_add_C_halfpelX_ISSE@loop

	ret



;*********************************************************
;*
;*	Luminance - normal
;*
;*********************************************************

	align 16
predict_add_C_normal_ISSE:
	movq	mm0,[ecx]
	movq	mm1,[ecx+esi]
	movq	mm2,[ecx+esi*2]
	pavgb	mm0,[edx]
	movq	mm4,[ecx+esi*4]
	pavgb	mm1,[edx+esi]
	pavgb	mm2,[edx+esi*2]
	pavgb	mm4,[edx+esi*4]
	movq	[edx],mm0
	movq	[edx+esi],mm1
	movq	[edx+esi*2],mm2
	movq	[edx+esi*4],mm4

	lea	ecx,[ecx+esi*2]
	lea	edx,[edx+esi*2]

	movq	mm3,[ecx+esi]
	movq	mm6,[ecx+esi*4]
	pavgb	mm3,[edx+esi]
	pavgb	mm6,[edx+esi*4]
	movq	[edx+esi],mm3
	movq	[edx+esi*4],mm6

	lea	ecx,[ecx+esi]
	lea	edx,[edx+esi]

	movq	mm5,[ecx+esi*2]
	movq	mm7,[ecx+esi*4]
	pavgb	mm5,[edx+esi*2]
	pavgb	mm7,[edx+esi*4]
	movq	[edx+esi*2],mm5
	movq	[edx+esi*4],mm7

	ret

	end
