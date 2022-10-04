b start

msg_hello_world: .asciz "Hello World, from guest!\r\n"

/* print_string - x0 is the string address, x1 is the console fifo address */
.align 4
print_string:
	ldrb	w8, [x0], #1
	cbz	w8, p1
	str     w8, [x1]
	dsb	nsh
	b	print_string
p1:	ret

/* Let's go ! */
.align 4
start:
	mov	x0, msg_hello_world
	mov	x1, #0x01000000
	bl print_string
	mov	x0, #1000
	bl mmio_ping
s1:	wfi
	b	s1

.align 4
mmio_ping:
	mov	x8, #0x02000000
	mov	x9, #0x02100000
	mrs	x11, CNTFRQ_EL0
	mrs	x10, CNTVCT_EL0
	str	x11, [x8]
	dsb	st
	mrs	x12, CNTVCT_EL0
	sub	x12, x12, x10
	str	x12, [x9]
	dsb	st
	sub	x0, x0, #1
	cbnz	x0, mmio_ping
	ret
