;set the proper CPU
!cpu m65

VIC_BORDER_COLOR = $d020
VIC_BACKGROUND_COLOR = $d021
VIC_IRQ_MASK = $d01a
CIA1_IRQ_CONTROL = $dc0d
CIA2_NMI_CONTROL = $dd0d
VIC3_KEY = $d02f
VIC3_VICDIS = $d031

!macro EnableVIC4Registers {
    ; See manual 10-4
    ; Clear C65 memory map
    lda #$00
    tax
    tay
    taz
    map ; mapping memory using a/x/y/z
    eom ; end map sequence, allowing IRQ again
    ;Enable VIC IV
    lda #$47
    sta VIC3_KEY
    lda #$53
    sta VIC3_KEY
}

SCREEN_CHAR   = $0800

;for 40x25 the lower 1000 bytes of the color RAM are mapped here
SCREEN_COLOR  = $d800


* = $2001

    ;; Auto-detect C64/C65 mode and either way jump into the assembly
    ; 2020 IFPEEK(44)=8THENSYS2081:ELSEBANK0:SYS8225
    !byte 0x1f,0x20,0xe4,0x07,0x8b,0xc2,0x28,0x34,0x34,0x29,0xb2
    !byte 0x38,0xa7,0x9e
    !pet "2081"
    !byte 0x3a,0xd5,0xfe,0x02,0x30,0x3a,0x9e
    !pet "8225"
    !byte 0x00,0x00,0x00
;    sei
    ;this must follow after !cpu m65, because it adds a BANK to the BASIC
    ; upstart, and we need to restore the memory config to access the VIC4
    +EnableVIC4Registers
    ;Turn off CIA interrupts
;    lda #$7f
;    sta CIA1_IRQ_CONTROL
;    sta CIA2_NMI_CONTROL
    ;Turn off raster interrupts, used by C65 rom
;    lda #$00
;    sta VIC_IRQ_MASK
;    cli
    ;set 40x25 mode
    lda #$80
    trb VIC3_VICDIS
    lda #0
    sta VIC_BACKGROUND_COLOR
    ldx #0
-   lda TEXT_SCREEN_DATA,x
    sta SCREEN_CHAR,x
    lda TEXT_SCREEN_DATA + 1 * 250,x
    sta SCREEN_CHAR + 1 * 250,x
    lda TEXT_SCREEN_DATA + 2 * 250,x
    sta SCREEN_CHAR + 2 * 250,x
    lda TEXT_SCREEN_DATA + 3 * 250,x
    sta SCREEN_CHAR + 3 * 250,x
    lda TEXT_SCREEN_DATA + 1000,x
    sta SCREEN_COLOR,x
    lda TEXT_SCREEN_DATA + 1000 + 1 * 250,x
    sta SCREEN_COLOR + 1 * 250,x
    lda TEXT_SCREEN_DATA + 1000 + 2 * 250,x
    sta SCREEN_COLOR + 2 * 250,x
    lda TEXT_SCREEN_DATA + 1000 + 3 * 250,x
    sta SCREEN_COLOR + 3 * 250,x
    inx
    cpx #250
    bne -
    ;endless loop
    jmp *

TEXT_SCREEN_DATA
    ;contains 40x25 character bytes, followed by 40x25 color bytes
    ; !media "charscreen",CHARCOLOR
    !pet "1234567890123456789012345678901234567890"
    !pet "2                                      1"
    !pet "3                                      1"
    !pet "4                                      1"
    !pet "5                                      1"
    !pet "6                                      1"
    !pet "7                                      1"
    !pet "8                                      1"
    !pet "9                                      1"
    !pet "10                                     1"
    !pet "11                                     1"
    !pet "12                                     1"
    !pet "13                                     1"
    !pet "14                                     1"
    !pet "15                                     1"
    !pet "16                                     1"
    !pet "17                                     1"
    !pet "18                                     1"
    !pet "19                                     1"
    !pet "20                                     1"
    !pet "21                                     1"
    !pet "22                                     1"
    !pet "23                                     1"
    !pet "24                                     1"
    !pet "25----                                 1"
    !fill 1000,1
