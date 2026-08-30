;;; Hexwar's memory map.
;;;
;;; This is the tool chain's stock mega65-plain.scm
;;; (/usr/local/lib/calypsi-6502-5.18/linker-rules/mega65-plain.scm) with one
;;; line added, and it is worth knowing what is *not* here.
;;;
;;; **The C65 ROM is left mapped and this program never banks it out.** The
;;; other two MEGA65 projects in this tree (~/commodore/mega65/hexgame and
;;; ~/commodore/SearchAndRescue) both flatten the memory map to get at
;;; $20000-$3FFFF, and both pay for it: a Hypervisor call to lift the ROM's
;;; write protection, a second one to put it back (it is a toggle that
;;; outlives the program), a copy of the whole 128 KB ROM to attic RAM so the
;;; machine can still be reset, and a rule that all disk I/O must finish
;;; before any of it happens.
;;;
;;; Hexwar needs none of that because of one number: the map view is 60x23
;;; characters, so its tile store is 60*23*64 = 88320 bytes, and that fits in
;;; banks 4 and 5 ($40000-$559FF) with 40 KB to spare. The ROM area is never
;;; wanted. So the KERNAL stays live, printf works, the interrupt vectors are
;;; still the ROM's, and quitting is a return from main().
;;;
;;; The old cc65 build had this as the #else branch of a #if on the view size
;;; (setup_view in old-cc65/src/graphics.c). It is now the only branch.
;;;
;;; **$2001-$9FFF is 32 KB for everything**: code, data, BSS, C stack and heap.
;;; The tile sheet is linked in rather than loaded (build/tiles.s, 6573 bytes)
;;; and map.tile is another 4000, so about 10.5 KB is spoken for before any
;;; game code. build/hexwar.lst is the only place the real figure is written
;;; down and is worth reading after anything that might have grown it.
;;;
;;; **$1600-$1EFF is 2304 bytes of ordinary chip RAM the rules hand to a
;;; section called `zpsave`, which nothing in this program uses.** It is
;;; near-addressable, so putting a table there costs nothing per access.
;;; Mark a variable with LOW_FREE (src/mega65.h) to move it. Note that
;;; naming `zdata` in this memory instead -- which is what hexgame does --
;;; would move the *whole* BSS down here, and map.tile alone does not fit.
(define memories
  '((memory program
            (address (#x2001 . #x9fff)) (type any)
            (section (programStart #x2001) (startup #x200e)))
    (memory zeroPage (address (#x2 . #x7f)) (type ram) (qualifier zpage)
            (section (registers #x2)))
    (memory stackPage (address (#x100 . #x1ff)) (type ram))
    (memory freeSpace (address (#x1600 . #x1eff)) (section zpsave))
    ))
