; Startup code for cc65/ca65
.import _main
.export __STARTUP__:absolute=1, __Hide_Sprites
.importzp _NMI_flag, _Frame_Count
.importzp _Horiz_scroll, _Nametable

; Linker generated symbols
.import __STACK_START__, __STACK_SIZE__
.import NES_PRG_BANKS, NES_CHR_BANKS
.include "zeropage.inc"
.import initlib, copydata

.segment "ZEROPAGE"

NTSC_MODE: 			.res 1
;FT_TEMP: 			.res 3
TEMP: 				.res 11


.segment "HEADER"
  .byte $4e,$45,$53,$1a
	.byte 01 ;<NES_PRG_BANKS
	.byte 01 ;<NES_CHR_BANKS
	.byte 01 ;vertical mirroring = horizontal scrolling
	.byte 00
	.res 8,0

.segment "STARTUP"

start:
	sei
	cld
	ldx #$40
	stx $4017
	ldx #$ff
	txs
	inx
	stx $2000
	stx $2001
	stx $4010
:
	lda $2002
	bpl :-
	lda #$00
Blankram:
	sta $00, x
	sta $0100, x
	sta $0200, x
	sta $0300, x
	sta $0400, x
	sta $0500, x
	sta $0600, x
	sta $0700, x
	inx
	bne Blankram

:
	lda $2002
	bpl :-

Isprites:
	jsr _Hide_Sprites
	jsr ClearNT

MusicInit:			;turns music channels off
	lda #0
	sta $4015

	lda #<(__STACK_START__+__STACK_SIZE__)
  sta	sp
  lda	#>(__STACK_START__+__STACK_SIZE__)
  sta	sp+1            ; Set the c stack pointer

	lda #1
	sta NTSC_MODE


	;init sfx
	ldx #<sounds			;set sound effects data location
	ldy #>sounds
	jsr FamiToneSfxInit

	jsr	copydata
	jsr	initlib

	lda $2002		;reset the 'latch'
	jmp _main		;jumps to main in c code

__Hide_Sprites:
	_Hide_Sprites:
		ldy #$40
		ldx #$00
		lda #$f8
	_Hide_Sprites2:		;puts all sprites off screen
		sta $0200, x
		inx
		inx
		inx
		inx
		dey
		bne _Hide_Sprites2
		rts

_ClearNT:
ClearNT:
	lda $2002
	lda #$20
	sta $2006
	lda #$00
	sta $2006
	lda #$00	;tile 00 is blank
	ldy #$10
	ldx #$00
BlankName:		;blanks screen
	sta $2007
	dex
	bne BlankName
	dey
	bne BlankName
	rts

nmi:
	pha
	tya
	pha
	txa
	pha

	inc _NMI_flag
	inc _Frame_Count
	lda #0
	sta $2003
	lda #2
	sta $4014 ;push sprite data to OAM from $200-2ff
	lda #$88 ; This gets set while "All On"
	clc
	adc _Nametable
	sta $2000 ;nmi on
	lda #$1e
	sta $2001 ;screen on
	lda $2002 ;reset the latch
	lda _Horiz_scroll
	sta $2005
	lda #$ff
	sta $2005

	pla
	tax
	pla
	tay
	pla
irq:
    rti

.segment "RODATA"
	.include "Music/famitone4.s"

music_data:
	.include "Music/SingleScreenPlatformer.s"

sounds_data:
	.include "Music/soundFx.s"
;none yet

.segment "VECTORS"

    .word nmi	;$fffa vblank nmi
    .word start	;$fffc reset
   	.word irq	;$fffe irq / brk


.segment "CHARS"

	.incbin "Graphics/tileset.chr"
