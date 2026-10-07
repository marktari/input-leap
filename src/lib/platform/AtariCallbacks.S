.global _mousexy,_call_ikbdvec,_call_mousevec
.global _asm_set_ipl
#ifdef COLDFIRE
	.global _linea000
#endif
	.text

_mousexy:

#define G_CURX -602
#ifdef COLDFIRE
	lea -16(SP),SP
	movem.l D1-D2/A1-A2,(SP)
	move.l _linea000,D0
	beq.s .mxy2              // no cookie 'A000'
	move.l D0,A0
	jsr (A0)
	bra.s .mxy1
.mxy2:
	dc.w 0xA920              // 0xA000 is MAC instruction on CF
.mxy1:
	movem.l (SP),D1-D2/A1-A2
	lea 16(SP),SP
#else
	movem.l D1-D2/A1-A2,-(SP)
	dc.w 0xA000
	movem.l (SP)+,D1-D2/A1-A2
#endif
	move.l G_CURX(A0),D0
	rts

_call_mousevec:

#ifdef COLDFIRE
	lea -24(SP),SP
	movem.l D0-D2/A0-A2,(SP)
#else
	movem.l D0-D2/A0-A2,-(SP)
#endif
	move.l 28(SP),A0         // data
	move.l 32(SP),A2
	move.l (A2),A2           // mousevec
	jsr (A2)
#ifdef COLDFIRE
	movem.l (SP),D0-D2/A0-A2
	lea 24(SP),SP
#else
	movem.l (SP)+,D0-D2/A0-A2
#endif
	rts

_call_ikbdvec:

#ifdef COLDFIRE
	lea -24(SP),SP
	movem.l D0-D2/A0-A2,(SP)
#else
	movem.l D0-D2/A0-A2,-(SP)
#endif
	move.l 28(SP),D0         // ikbd code
	and.l #0xFF,D0
	move.l 32(SP),A0         // iorec
	move.l 36(SP),A2         // &kbdvecs[-1]
	move.l (A2),A2           // ikbdvec
	jsr (A2)
#ifdef COLDFIRE
	movem.l (SP),D0-D2/A0-A2
	lea 24(SP),SP
#else
	movem.l (SP)+,D0-D2/A0-A2
#endif
	rts

_asm_set_ipl:

	link A6,#-8
	movem.l D6-D7,(SP)
	move.w SR,D7             // current SR
	move.l D7,D0             // prepare return value
	and.l #0x0700,D0         // mask out IPL
	lsr.l #8,D0              // IPL
	move.l 8(A6),D6          // get argument
	and.l #7,D6              // least significant three bits
	lsl.l #8,D6              // move over to make mask
	and.l #0x0000F8FF,D7     // zero out current IPL
	or.l D6,D7               // place new IPL in SR
	move.w D7,SR
	movem.l (SP),D6-D7
	lea 8(SP),SP
	unlk A6
	rts
