; BASIC out of $A000-$BFFF for the run, and back in for the return.
;
; **The 8 KB under the BASIC ROM is the HIGH_BSS window** (src/mega65.h and
; sevenships.scm). $D030 bit 4 maps the ROM there; with it clear the CPU sees the
; RAM underneath. The arrangement is Search And Rescue's (its src/bank.s),
; confirmed on a real MEGA65 on 2 Oct 2026 with the KERNAL reading the whole
; disk banked like this -- the KERNAL, its interrupt vectors and $C000-$CFFF
; are left alone, so printf, file I/O and quitting by returning from main()
; all work as before.
;
; **Here and not in main()**, because the startup clears BSS after this and
; before main: a write to the window falls through to the RAM under the ROM
; either way, but a read would come from the ROM, so nothing may use the
; window until the bit is clear. The startup has set the C stack pointer by
; now; nothing here uses it.

            .rtmodel version, "1"
            .rtmodel core, "*"

ROMMAP:     .equ 0xd030
ROM_BASIC:  .equ 0x10

            .section code, text
            .public __low_level_init, bank_basic_in

; **BASIC's zero page has to be saved and put back.** The C pseudo
; registers are $02-$39, which BASIC 65 uses for its own pointers. The
; SDK's preserve_zero_page.s has the startup copy them to zpsave first and
; _Stub_exit copy them back after main() returns, but only if something
; asks for it, and nothing did until 19 Oct 2026: BASIC came back to
; READY on whatever the program had left there, and the next DIM hung.
            .require __preserve_zp

__low_level_init:
            lda     ROMMAP
            sta     rom_d030
            and     #~ROM_BASIC & 0xff
            sta     ROMMAP
            rts

; $D030 as the ROM had it, so that BASIC is there to return to. Call it last
; thing before main() returns: nothing in HIGH_BSS may be used after it.
bank_basic_in:
            lda     rom_d030
            sta     ROMMAP
            rts

rom_d030:   .byte   0               ; in code, which nothing initialises over
