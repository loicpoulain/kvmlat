jmp start

start:
	movw %si, 0x02000000
	hlt

mmio_ping:
	exit: ret
