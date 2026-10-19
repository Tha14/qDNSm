/*
 * One installer, two toolchains.
 *
 * The installer is built by two quite different compilers, because only one of
 * them can produce an image the PS3 loader will actually accept.
 *
 * SNC (the Sony SDK, used for qDNSm.sprx and the original VS project) cannot. An
 * SNC Application is not a dynamically linked module, so neither the compiler nor
 * ps3ppuld emits an SCE_PPU_PROC_PARAM block. RPCS3 requires one and refuses the
 * image without it:
 *
 *   if (proc_prx_param.magic != 0x1b434cecu)
 *       ppu_loader.error("ppu_load_exec(): Bad magic! (0x%x)", ...);
 *
 * Supplying the block by hand does not rescue it, because ps3ppuld does not honour
 * the :phdr assignment that would place it at the head of segment 0x60000002 - the
 * data lands in the image but 40 bytes behind the start of the segment. Sony's own
 * SDK example has the identical defect, which is what settled the question.
 *
 * PSL1GHT (ps3dev toolchain, GCC 7.2.0 + GNU ld) emits that block itself, from its
 * own crt1.c, and gets e_entry, the segment layout and the param block right with
 * no help. That is the toolchain the working homebrew EBOOTs were built with.
 *
 * So the installer logic is shared and only these few spellings differ:
 *
 *   - the filesystem API is cellFs* under SNC and sysFs* under PSL1GHT. The
 *     signatures are identical, so this is a rename rather than a rewrite.
 *   - the module macros exist only under SNC. An EXEC is entered through main();
 *     module_start/module_stop are for PRX modules and are not wanted here.
 *
 * The installer used to ask the loader where its own image lived, via
 * sys_prx_get_module_info, and that needed a workaround here: sys/prx.h and
 * lv2/prx.h have circular include guards in this toolchain, so whichever order is
 * chosen one side loses its declarations. That is all gone - the EBOOT's location
 * is a fixed path (see InstFs::SelfDirectory), so nothing needs the module API.
 *
 * Define QDNSM_PSL1GHT on the command line to select the PSL1GHT path.
 */

#ifndef QDNSM_TARGET_COMPAT_HPP
#define QDNSM_TARGET_COMPAT_HPP

#include <string.h>

#if defined(QDNSM_PSL1GHT)

#include <lv2/sysfs.h>
#include <ppu-types.h>
#include <sys/stat.h>

typedef s32        CellFsErrno;
typedef sysFSStat  CellFsStat;

#define CELL_FS_SUCCEEDED  0

/*
 * These are NOT SNC's values, and guessing them was a real bug rather than a
 * harmless shortcut. The two SDKs disagree on every open flag, and the mismatch
 * was silent because both are plausible-looking bit patterns:
 *
 *                      SNC (what was here)   PSL1GHT (correct)
 *   O_RDONLY                 0x0001                000000 = 0
 *   O_WRONLY                 0x0002                000001 = 1
 *   O_RDWR                      --                 000002 = 2
 *   O_CREAT                  0x0200                000100 = 64
 *   O_EXCL                      --                 000200 = 128
 *   O_TRUNC                  0x0400                001000 = 512
 *   O_APPEND                    --                 002000 = 1024
 *
 * Two failures came out of it, and neither announced itself as a flag problem:
 *
 *   - kFlagsRead was 0x0001, which under PSL1GHT is O_WRONLY. The installer
 *     opened its own payload for writing and then tried to read it, so every
 *     load died with CELL_EBADF.
 *   - kFlagsWrite was 0x0602, which under PSL1GHT decodes as
 *     O_RDWR|O_APPEND|O_TRUNC - with no O_CREAT at all. Creating the install log
 *     therefore failed with CELL_ENOENT every time.
 *
 * Taken straight from lv2/sysfs.h rather than retyped, so a toolchain update
 * cannot quietly reintroduce this.
 */
#define CELL_FS_O_RDONLY   SYS_O_RDONLY
#define CELL_FS_O_WRONLY   SYS_O_WRONLY
#define CELL_FS_O_CREAT    SYS_O_CREAT
#define CELL_FS_O_TRUNC    SYS_O_TRUNC

/*
 * sysFSStat::st_mode carries the same S_IF* bits as any other stat(), so the
 * tests are done with the macros rather than hardcoded masks.
 */
#define CELL_FS_S_ISREG(m)  S_ISREG(m)
#define CELL_FS_S_ISDIR(m)  S_ISDIR(m)

/* mkdir's mode argument: a directory with rwxrwxrwx. */
#define CELL_FS_S_IFDIR     S_IFDIR

#define cellFsOpen        sysFsOpen
#define cellFsClose       sysFsClose
#define cellFsRead        sysFsRead
#define cellFsWrite       sysFsWrite
#define cellFsStat        sysFsStat
#define cellFsChmod       sysFsChmod
#define cellFsUnlink      sysFsUnlink
#define cellFsMkdir       sysFsMkdir

#else

#include <cellstatus.h>
#include <sys/prx.h>
#include <cell/cell_fs.h>

#endif

#endif
