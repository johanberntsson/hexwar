;;; The memory map of Seven Ships of the Enemy.
;;;
;;; This is the tool chain's stock mega65-plain.scm
;;; (/usr/local/lib/calypsi-6502-5.18/linker-rules/mega65-plain.scm) with the
;;; highram window added, and it is worth knowing what is *not* here.
;;;
;;; **The C65 ROM is left mapped and this program never banks it out.**
;;; ~/commodore/mega65/hexgame flattens the memory map to get at
;;; $20000-$3FFFF, and pays for it: a Hypervisor call to lift the ROM's
;;; write protection, a second one to put it back (it is a toggle that
;;; outlives the program), a copy of the whole 128 KB ROM to attic RAM so the
;;; machine can still be reset, and a rule that all disk I/O must finish
;;; before any of it happens.
;;;
;;; The game needs none of that because of one number: the map view is 60x23
;;; characters, so its tile store is 60*23*64 = 88320 bytes, and that fits in
;;; banks 4 and 5 ($40000-$559FF) with 40 KB to spare. The ROM area is never
;;; wanted. So the KERNAL stays live, printf works, the interrupt vectors are
;;; still the ROM's, and quitting is a return from main().
;;;
;;; The old cc65 build had this as the #else branch of a #if on the view size
;;; (setup_view in old-cc65/src/graphics.c). It is now the only branch.
;;;
;;; **$2001-$9FFF is 32 KB, and $6000-$7FFF of it is a code window**
;;; (src/region.h). resLo and resHi are the resident program: code, data,
;;; BSS, C stack and heap, everything that has to be visible whatever the
;;; window shows -- which includes the redraw path. win0 is region 0, the
;;; PRG's own $6000-$7FFF; win1 and up are regions 1 and up, linked at the
;;; same addresses but read from files of their own into attic RAM and
;;; copied into a slot in bank 1 when called (src/region.h). **Only sections
;;; named rNtext and rNrodata go into a window**, through #pragma clang
;;; section in the banked files; a window memory has no (type ...), so the
;;; linker never puts anything else there. The link is raw, one file a
;;; memory, and tools/packprg.py makes the PRG and the region files from
;;; them. build/sevenships.lst is the only place the real figures are
;;; written down and is worth reading after anything that might have grown
;;; them.
;;;
;;; **$A000-$BFFF is the 8 KB under the BASIC ROM**, which src/bank.s banks
;;; out before main() and back in before it returns. HIGH_BSS (src/mega65.h)
;;; puts a variable there; map_tile is the first. **No `(type ...)` on it, and
;;; that is the whole trick**, found by Search And Rescue: declared `any` the
;;; linker may put initialised data or the C stack up here, which makes a
;;; second content area -- "multiple program areas not allowed in prg
;;; output" -- and `(type ram)` does not work either. Declared like
;;; freeSpace, a name and a section and nothing else, it takes the named
;;; section and never becomes one.
;;;
;;; **$1600-$1EFF is 2304 bytes of ordinary chip RAM.** Its first 56 bytes
;;; are `zpsave`, where the startup saves BASIC's zero page $02-$39 before
;;; the C pseudo registers take it over, and from where _Stub_exit puts it
;;; back after main() returns (the SDK's preserve_zero_page.s, pulled in by
;;; src/bank.s). **zpsave must start at $1600**, since the copy loops
;;; address it by .sectionStart. The rest is `lowfree`: near-addressable,
;;; so a table there costs nothing per access. Mark a variable with
;;; LOW_FREE (src/mega65.h) to move it. Until 19 Oct 2026 LOW_FREE *was*
;;; zpsave and the save was not linked at all, so BASIC got the pseudo
;;; registers' last values back for its zero page: READY came up, but DIM
;;; then hung, on a real MEGA65 and in xemu. Note that
;;; naming `zdata` in this memory instead -- which is what hexgame does --
;;; would move the *whole* BSS down here, and map.tile alone does not fit.
(define memories
  '((memory resLo
            (address (#x2001 . #x5fff)) (type any)
            (section (programStart #x2001) (startup #x200e)))
    (memory win0 (address (#x6000 . #x7fff)) (section r0text r0rodata))
    (memory resHi (address (#x8000 . #x9fff)) (type any))
    (memory win1 (address (#x6000 . #x7fff)) (section r1text r1rodata))
    (memory win2 (address (#x6000 . #x7fff)) (section r2text r2rodata))
    (memory win3 (address (#x6000 . #x7fff)) (section r3text r3rodata))
    (memory win4 (address (#x6000 . #x7fff)) (section r4text r4rodata))
    (memory win5 (address (#x6000 . #x7fff)) (section r5text r5rodata))
    (memory win6 (address (#x6000 . #x7fff)) (section r6text r6rodata))
    (memory zeroPage (address (#x2 . #x7f)) (type ram) (qualifier zpage)
            (section (registers #x2)))
    (memory highram
            (address (#xa000 . #xbfff))
            (section highbss))
    (memory stackPage (address (#x100 . #x1ff)) (type ram))
    (memory freeSpace (address (#x1600 . #x1eff))
            (section (zpsave #x1600) lowfree))
    ))
