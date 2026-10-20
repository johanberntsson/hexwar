; The MAP window at $6000-$7FFF and the calls into it; src/region.h has the
; scheme. All of this is resident, in the ordinary code section, since none
; of it may be under the window it changes.

            .rtmodel version, "1"
            .rtmodel core, "*"

            .section code, text
            .public region_init, set_region, dispatch, cur_region

; Region r's map, lower half only: A is the offset's bits 8-15, X its bits
; 16-19 and in its top nibble the 8 KB blocks mapped -- $80 is block 3,
; $6000-$7FFF. Region 0's is the KERNAL's, taken at start-up; region r in
; bank 1 is offset REGION_BASE(r) - $6000, $0D000 for region 1 and $0F000
; for region 2.
map_a:      .byte   0, 0xd0, 0xf0
map_x:      .byte   0, 0x80, 0x80
cur_region: .byte   0

region_init:
            lda     0x011c
            sta     map_a
            lda     0x011d
            sta     map_x
            lda     #0
            sta     cur_region
            rts

; Map region A into the window. Clobbers A, X and Y; Z comes back 0, as
; compiled code expects. **The KERNAL's copy of the map at $011C-$011F is
; written first**, so that whatever it maps back to after an interrupt or a
; disk call is this; the upper half (the KERNAL at $E000) is left as the
; copy says. Interrupts are held off from the first store to the MAP.
set_region:
            sta     cur_region
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
; loader.
            stub    sheet_load, sheet_load_r0, 0
            stub    help_screen, help_screen_r0, 0
            stub    menu_orders, menu_orders_r0, 0
            stub    menu_draw, menu_draw_r0, 0
            stub    menu_click, menu_click_r0, 0
            stub    menu_key, menu_key_r0, 0
            stub    title_show, title_show_r0, 0
            stub    title_run, title_run_r0, 0
            stub    save_write, save_write_r0, 0
            stub    save_read, save_read_r0, 0

; Region 1: the computer players, the builders' improvements and the
; cities' yields and growth.
            stub    ai_place, ai_place_r1, 1
            stub    ai_turn, ai_turn_r1, 1
            stub    ai_barbarians, ai_barbarians_r1, 1
            stub    improve_can, improve_can_r1, 1
            stub    improve_best, improve_best_r1, 1
            stub    improve_start, improve_start_r1, 1
            stub    improve_turn, improve_turn_r1, 1
            stub    city_work, city_work_r1, 1
            stub    city_grow, city_grow_r1, 1

; Region 2: the city screen and Top Five Cities.
            stub    city_screen, city_screen_r2, 2
            stub    top_cities_screen, top_cities_screen_r2, 2
