#ifndef __DMA_H
#define __DMA_H

#define POKE(X, Y) (*(unsigned char *)(X)) = Y
#define PEEK(X) (*(unsigned char *)(X))

void dma_init(void);
void mega65_io_enable(void);

unsigned char dma_peek(dword address);
void dma_poke(dword address, byte value);
void dma_copy(dword source_address, dword destination_address, word count);
void dma_fill(dword destination_address, byte value, word count);
void dma_fill_skip(dword destination_address, byte value, word count,byte skip);

#endif
