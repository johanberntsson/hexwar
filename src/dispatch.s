; The MAP window at $6000-$7FFF and the calls into it; src/region.h has the
; scheme. All of this is resident, in the ordinary code section, since none
; of it may be under the window it changes.

            .rtmodel version, "1"
            .rtmodel core, "*"

            .section code, text
            .public region_init, set_region, dispatch, cur_region

; The window's map for each slot, lower half only: A is the offset's bits
; 8-15, X its bits 16-19 and in its top nibble the 8 KB blocks mapped --
; $80 is block 3, $6000-$7FFF. Slot 0 is region 0's, the KERNAL's own map,
; taken at start-up; slot s in bank 1 is at SLOT_BASE(s), offset
; SLOT_BASE(s) - $6000: $0D000 for slot 1 and $0F000 for slot 2.
map_a:      .byte   0, 0xd0, 0xf0
map_x:      .byte   0, 0x80, 0x80
cur_region: .byte   0

; Region r's slot (REGION_SLOT() in src/region.h, which has to agree), and
; the region each slot holds, 0 for none yet: region_load() leaves them all
; empty, so a region is first copied in when it is first called.
slot_of:    .byte   0, 1, 2, 2
slot_held:  .byte   0, 0, 0

; Where each region is in attic RAM, REGION_ATTIC(r): bank bits 16-19 and
; bits 8-15, for r from 0 (unused) up. And where each slot is in bank 1,
; bits 8-15.
attic_bank: .byte   0, 1, 1, 1
attic_hi:   .byte   0, 0x00, 0x20, 0x40
slot_hi:    .byte   0, 0x30, 0x50

; Each region's length, set by region_load() from its file.
            .public region_length_lo, region_length_hi
region_length_lo:
            .byte   0, 0, 0, 0
region_length_hi:
            .byte   0, 0, 0, 0

; The copy from attic RAM into a slot: an enhanced job with the source in
; megabyte $80, the destination in $00, then F018B as in src/mega65.c.
fetch_list: .byte   0x0b, 0x80, 0x80, 0x81, 0x00, 0x00
            .byte   0x00            ; copy
fetch_count:
            .word   0
            .byte   0x00            ; source bits 0-7: a region is page-aligned
fetch_src_hi:
            .byte   0
fetch_src_bank:
            .byte   0
            .byte   0x00            ; destination bits 0-7
fetch_dst_hi:
            .byte   0
            .byte   1               ; bank 1
            .byte   0               ; sub-command
            .word   0               ; modulo

; Region 0, the KERNAL's map; also what region_load() calls to forget what
; the slots held.
region_init:
            lda     0x011c
            sta     map_a
            lda     0x011d
            sta     map_x
            lda     #0
            sta     cur_region
            sta     slot_held+1
            sta     slot_held+2
            rts

; Map region A into the window, first copying it from attic RAM into its
; slot if the slot holds another region (or none). Clobbers A, X and Y; Z
; comes back 0, as compiled code expects. **The KERNAL's copy of the map at
; $011C-$011F is written first**, so that whatever it maps back to after an
; interrupt or a disk call is this; the upper half (the KERNAL at $E000) is
; left as the copy says. Interrupts are held off from the first store to
; the MAP.
set_region:
            sta     cur_region
            tax
            ldy     slot_of,x
            beq     map_slot        ; region 0 is always where it is
            txa
            cmp     slot_held,y
            beq     map_slot
            sta     slot_held,y
            lda     region_length_lo,x
            sta     fetch_count
            lda     region_length_hi,x
            sta     fetch_count+1
            lda     attic_hi,x
            sta     fetch_src_hi
            lda     attic_bank,x
            sta     fetch_src_bank
            lda     slot_hi,y
            sta     fetch_dst_hi
            lda     #0x47           ; the I/O knock, as do_dma() does
            sta     0xd02f
            lda     #0x53
            sta     0xd02f
            lda     #0
            sta     0xd702
            sta     0xd704
            lda     #.byte1 fetch_list
            sta     0xd701
            lda     #.byte0 fetch_list
            sta     0xd705          ; the CPU waits until the copy is done
map_slot:   tya
            tax
            php
            sei
            lda     map_a,x
            sta     0x011c
            lda     map_x,x
            sta     0x011d
            tax
            lda     0x011c
            ldy     0x011e
            ldz     0x011f
            map
            eom
            ldz     #0
            plp
            rts

; A call through a stub: A is the region, d_a the caller's A, d_tgt the
; function, and X and Y are still the caller's. The caller's region goes on
; the stack, so a region may call another and be mapped back on the way
; out. Arguments in the pseudo registers and on the C stack are not
; touched; a result in A and X comes back through d_a and d_x.
dispatch:
            stx     d_x
            sty     d_y
            tax
            lda     cur_region
            pha
            txa
            jsr     set_region
            lda     d_a
            ldx     d_x
            ldy     d_y
            jsr     (d_tgt)
            sta     d_a
            stx     d_x
            sty     d_y
            pla
            jsr     set_region
            lda     d_a
            ldx     d_x
            ldy     d_y
            rts

d_a:        .byte   0
d_x:        .byte   0
d_y:        .byte   0
d_tgt:      .word   0

; A stub: the plain name of a function in a region, which is called
; `name_rN` inside its own file.
stub:       .macro  name, target, region
            .public \name
            .extern \target
\name:      sta     d_a
            lda     #.byte0 \target
            sta     d_tgt
            lda     #.byte1 \target
            sta     d_tgt+1
            lda     #\region
            jmp     dispatch
            .endm

; Region 0: the screens, the menus, the save file and the tile sheet's
; and the map's loaders.
            stub    sheet_load, sheet_load_r0, 0
            stub    map_load, map_load_r0, 0
            stub    help_screen, help_screen_r0, 0
            stub    menu_orders, menu_orders_r0, 0
            stub    menu_draw, menu_draw_r0, 0
            stub    menu_click, menu_click_r0, 0
            stub    menu_key, menu_key_r0, 0
            stub    title_show, title_show_r0, 0
            stub    title_run, title_run_r0, 0
            stub    save_write, save_write_r0, 0
            stub    save_read, save_read_r0, 0

; Region 1, slot 1: the computer players and the cities' yields and
; growth.
            stub    ai_place, ai_place_r1, 1
            stub    ai_turn, ai_turn_r1, 1
            stub    ai_barbarians, ai_barbarians_r1, 1
            stub    city_work, city_work_r1, 1
            stub    city_grow, city_grow_r1, 1
            stub    city_toggle, city_toggle_r1, 1
            stub    hex_yield, hex_yield_r1, 1
            stub    player_has, player_has_r1, 1

; Region 2, slot 2: the city screen, Top Five Cities and the builders'
; improvements.
            stub    city_screen, city_screen_r2, 2
            stub    top_cities_screen, top_cities_screen_r2, 2
            stub    improve_can, improve_can_r2, 2
            stub    improve_best, improve_best_r2, 2
            stub    improve_start, improve_start_r2, 2
            stub    improve_turn, improve_turn_r2, 2

; Region 3, slot 2: diplomacy.
            stub    diplomacy_reset, diplomacy_reset_r3, 3
            stub    diplomacy_turn, diplomacy_turn_r3, 3
            stub    declare_war, declare_war_r3, 3
            stub    diplomacy_contact, diplomacy_contact_r3, 3
            stub    diplomacy_greet, diplomacy_greet_r3, 3
            stub    diplomacy_screen, diplomacy_screen_r3, 3
