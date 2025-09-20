DMA sends 64bytes to ring buf. Ringbuf is sent to USB. It works.
But if we need to send to buf less than 64 - it will be a problem to DMA... thats why it stops.
so. This milestone is ok. Next - remove ringbuf - make 2 bufs or 3 buffs ping pong..