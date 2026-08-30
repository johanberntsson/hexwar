; this routine expands a full color char (64 bytes) to double width,
; storing the two next chars immediately after the source char

.segment    "ZEROPAGE": zeropage
.export     _expand_fullcolor_char_x
.export     _disable_rom_protection
; .exportzp 

.setcpu "4510"
.code

; =================================

_disable_rom_protection:
    lda #$00
    tax
    tay
    taz
    map
    lda #$36 ; (no C64 basic)
    sta $01
    lda #$47
    sta $d02f
    lda #$53
    sta $d02f
    eom
    lda #$70
    sta $d640
    nop
    rts

; =================================

_expand_fullcolor_char_x:
    ldx #0   ; 0 - 63 (source data at $0334)
    ldy #0   ; pointer into $0374,y and $03b4, y
@loop:
    txa
    and #$03
    bne @stillcopying
    txa
    and #$f8
    tay
@stillcopying: 
    txa
    and #$04
    bne @bank2
    lda $0334,x
    sta $0374,y
    iny
    sta $0374,y
    jmp @done
@bank2:
    lda $0334,x
    sta $03b4,y
    iny
    sta $03b4,y
@done:
    iny
    inx
    cpx #64
    bne @loop
    rts

