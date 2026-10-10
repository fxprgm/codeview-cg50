/*
** fx-CG OS syscalls used by CodeView (numbers from Simon Lothar's fx-CG documentation; the
** handlers checked against the OS 3.60 table). Call them only inside gint_world_switch().
*/

.macro SYSCALL name, number
	.global	\name
	.align	2
\name:
	mov.l	1f, r2
	mov.l	2f, r0
	jmp	@r2
	nop
	.align	2
1:	.long	0x80020070
2:	.long	\number
.endm

	.text
	/* void *OpCodeToStr(int opcode, char *result): a token's text, in CASIO characters */
	SYSCALL	_os_OpCodeToStr, 0x128F
	/* int MCS_SearchDirectory(const char *dir, void **entry, char *dirno) */
	SYSCALL	_os_MCS_SearchDirectory, 0x1514
	/* int MCS_SearchDirectoryItem(const char *dir, const char *item, char *flags,
	       void **itemptr, void **data, int *length) */
	SYSCALL	_os_MCS_SearchDirectoryItem, 0x1519
