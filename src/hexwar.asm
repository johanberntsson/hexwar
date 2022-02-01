;set the proper CPU
!cpu m65
!src <m65/std.a>

* = $0801
;code for a BASIC line containing SYS2061
!byte $0B,$08,$e3,$07,$9e,$32,$30,$36,$31,$00,$00,$00
    jmp prg_start

VIC2CTRL = $d016
VIC3CTRL = $d031
VIC4CTRL = $d054
VIC3KEY  = $d02f
HOTREG   = $d05d
COLPTR   = $d064

SCREEN_ADDRESS = $12000
COLOUR_ADDRESS = $ff81000

!macro enable40Mhz {
    lda #$41
    sta $0
}
!macro enableIO {
    lda #$47     
    sta VIC3KEY
    lda #$53
    sta VIC3KEY
    lda #$00
}

!macro enableVIC4Registers {
    +enableIO
    tax
    tay
    taz
    map
    eom
}

!macro VIC4_SetCharLocation @addr {
    lda #<@addr
    sta $d068
    lda #>@addr
    sta $d069
    lda #^@addr
    sta $d06a
}

!macro VIC4_SetScreenLocation @addr {
    lda #<@addr
    sta $d060
    lda #>@addr
    sta $d061
    lda #^@addr
    sta $d062
    lda #$f0      ; can't set the screen to attic ram
    sta $d063
}

!src "asm/dma.a"

gScreenRows !byte 0
gScreenColumns !byte 0

    
fc_640_400
    +enableIO
    lda #50
    sta gScreenRows
    lda HOTREG
    ora #$80 ; enable HOTREG if previously disabled
    sta HOTREG
    lda VIC4CTRL
    ora #$04 ; enable full colour for characters with high ubyte set
    ora #$01 ; enable 16 bit characters
    sta VIC4CTRL
    lda VIC3CTRL
    ora #$80 ; enable H640
    ora #$08 ; enable V400
    sta VIC3CTRL
    lda VIC2CTRL
    ora #$01 ; shift one pixel to the right (VIC-III bug)
    sta VIC2CTRL
    lda #80
    sta gScreenColumns

    +lfill_skip SCREEN_ADDRESS, 32, 4000, 2
    +lfill COLOUR_ADDRESS, 0, 8000

    lda #0
    sta $d020
    sta $d021

    ; move colour RAM because of stupid CBDOS himem usage
    lda #$00
    sta COLPTR
    lda #$10
    sta COLPTR + 1

    +VIC4_SetScreenLocation SCREEN_ADDRESS
    rts

fc_plotpetchar
    ; input:
    ; a = char
    ; x = x
    ; y = y
    ; z = color|attributes
    +lpoke $12000, $30
    +lpoke $12001, $00
    +lpoke $0ff81000, $02
    +lpoke $0ff81001, $02
    rts

prg_start
    +enable40Mhz
    +enableVIC4Registers
    jsr fc_640_400


    +movq $02000030 ; red "0" at 0,0
    jsr fc_plotpetchar
    rts



