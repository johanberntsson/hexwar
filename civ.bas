include "xc-megalib/mega65.bas"
include "xc-megalib/memory.bas"
include "xc-megalib/fullcolor.bas"

function x_hex2array as byte (q as int, r as int) static
    return q mod 2
end function

function y_hex2array as byte (q as int, r as int) static
    return r
end function


sub draw_tile(q as byte, r as int, status as word) static
end sub

main:
    dim key as byte
    dim q as int
    dim r as int
    dim name as String*80

    call enable_40mhz()
    ' 640*200 (expand x)
    call fc_init(true, false, 0, 0)
    call fc_setMergeTileMode(10,1, 60, 23, true)

    tiles = fc_loadFCI("tiles.fci")
    call fc_loadFCIPalette(tiles)

    q=0:r=0:print q;",";r, x_hex2array(q, r);",";y_hex2array(q, r)
    q=0:r=1:print q;",";r, x_hex2array(q, r);",";y_hex2array(q, r)
    q=1:r=0:print q;",";r, x_hex2array(q, r);",";y_hex2array(q, r)
    q=1:r=4:print q;",";r, x_hex2array(q, r);",";y_hex2array(q, r)
    q=2:r=6:print q;",";r, x_hex2array(q, r);",";y_hex2array(q, r)
    q=-3:r=6:print q;",";r, x_hex2array(q, r);",";y_hex2array(q, r)

    call fc_fatal()
    'call draw_map()

    call draw_tile(0, 0, 1)

loop:
    key = fc_getkey(false)
    if key = 97 or key = 157 then call fc_scrollMergeLeft()
    if key = 100 or key = 29 then call fc_scrollMergeRight()
    if key = 119 or key = 145 then call fc_scrollMergeUp()
    if key = 115 or key = 17 then call fc_scrollMergeDown()
    if key = 13 then call fc_fatal()
    goto loop
