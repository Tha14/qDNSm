/*
 * The PS3 module C runtime defines _exit and __exit_user_prx_modules, but only inside
 * target/ppu/lib/crt1.o - which also carries a .sys_proc_prx_param section. This project
 * links with -oformat=prx and no -T linker script, so ps3ppuld uses its built-in PRX layout,
 * which does not place that section. prx-fixup then fails with:
 *
 *   section '.sys_proc_prx_param' needs allocation and has relocation data,
 *   but it isn't in program segment
 *
 * __fini is taken from ecrti.o instead, which is clean of that section.
 *
 * libc.a's exit.o references all three as undefined, and it is only pulled in once Dialog.c
 * makes libc's exit path reachable - which is why these never showed up before.
 *
 * qDNSm never calls exit(). module_stop performs its own teardown and then calls
 * _sys_ppu_thread_exit, so the loader - not libc - ends this module. Empty definitions are
 * therefore safe here and keep crt1.o (and its unplaceable section) out of the link.
 */

void __exit_user_prx_modules(void)
{
}

void _exit(int status)
{
	(void)status;
}