!zone DMA {

.dma_list
                            ; extended options
                            !byte $0b ; Use 12-byte F011B DMA list format
                            !byte $80 ; Source address bits 20 – 27
dma_source_high_address     !byte 0    
                            !byte $81 ; Destination address bits 20 – 27
dma_dest_high_address       !byte 0
                            !byte $85 ; Destination skip rate (whole bytes)
dma_dest_skip_bytes         !byte 1    
                            !byte $0  ; End of options
                            ; F011B message
dma_command                 !byte 0   ; 0 = Copy, 3 = fill
dma_count                   !word 0 
dma_source_address          !word 0
dma_source_bank_and_flags   !byte 0
dma_dest_address            !word 0
dma_dest_bank_and_flags     !byte 0
dma_command_msb             !byte 0   ; 0 = linear addressing for src and dest
dma_modulo                  !word 0   ; 0 = not using the MODULO flag


!macro dma_set_source_address @address {
    lda #<@address
    sta dma_source_address
    lda #>@address
    sta dma_source_address + 1
    lda #^@address
    and #$0f
    sta dma_source_bank_and_flags
    lda #@address >>> 20
    sta dma_source_high_address
}

!macro dma_set_dest_address @address {
    lda #<@address
    sta dma_dest_address
    lda #>@address
    sta dma_dest_address + 1
    lda #^@address
    and #$0f
    sta dma_dest_bank_and_flags
    lda #@address >>> 20
    sta dma_dest_high_address
}

!macro dma_set_count @count {
    lda #<@count
    sta dma_count 
    lda #>@count
    sta dma_count  + 1
}

!macro dma_set_value @value {
    lda #@value
    sta dma_source_address
}

!macro dma_set_skip @skip {
    lda #@skip
    sta dma_dest_skip_bytes
}

!macro lfill_skip @address, @value, @count, @skip {
    lda #3 ; fill
    sta dma_command
    +dma_set_dest_address @address
    +dma_set_count @count
    +dma_set_value @value
    +dma_set_skip @skip
    jsr trigger_dma
}

!macro lfill @address, @value, @count {
    lda #3 ; fill
    sta dma_command
    +dma_set_dest_address @address
    +dma_set_count @count
    +dma_set_value @value
    +dma_set_skip 1
    jsr trigger_dma
}

!macro lpoke @address, @value {
    lda #3
    sta dma_command
    +dma_set_dest_address @address
    +dma_set_count 1
    +dma_set_value @value
    +dma_set_skip 1
    jsr trigger_dma
}

trigger_dma
    +enableIO
    lda #0 ; dma is in $0000xxxx
    sta $d702
    sta $d704
    lda #>.dma_list
    sta $d701
    lda #<.dma_list
    sta $d705 ; $d705 instead of $d700 to signal extended list format
    rts
}

