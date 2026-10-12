#---------------------------------------------------------------------------------
# qDNSm installer EBOOT - PSL1GHT build
#---------------------------------------------------------------------------------
# Modelled directly on vendor/PSL1GHT/samples/sys/threadtest/Makefile, which is
# PSL1GHT's own build system. That matters: this file used to be built with
# hand-rolled flags and it produced images that loaded and then died in
# sbrk_init, before main() was ever reached. PSL1GHT's own sample built the same
# way runs correctly, so the difference is what this file does.
#
# The two differences that mattered:
#
#   - the library set. This used to link only -lsysfs -llv2. sbrk_init is newlib
#     heap setup, and newlib's malloc needs sysutil's memory calls; without
#     -lsysutil those imports are never wired up, which is consistent with
#     RPCS3 reporting "lib_stub_ea=00, lib_stub_size=0x0" at module registration
#     and then the image jumping somewhere unmapped.
#   - letting ppu_rules own the flags. MACHDEP comes from there, and the same
#     values go on the compile and the link line.
#
# ExecParams.cpp is deliberately NOT built here. Under SNC the toolchain never
# emitted an SCE_PPU_PROC_PARAM block, so we had to supply one by hand. PSL1GHT
# supplies its own, and building ours as well put a second, conflicting block
# into the image - see ExecParams.cpp for the details.
#---------------------------------------------------------------------------------
.SUFFIXES:

ifeq ($(strip $(PSL1GHT)),)
  $(error "Please set PSL1GHT in your environment. export PSL1GHT=<path>")
endif

include $(PSL1GHT)/ppu_rules

# Pinned rather than $(notdir $(CURDIR)), so the artifact stays named
# installer.pkg wherever this tree lives.
TARGET		:=	installer
BUILD		:=	build
SOURCES		:=	.
INCLUDES	:=
TITLE		:=	qDNSm installer
APPID		:=	NPUB72381
CONTENTID	:=	UP0001-$(APPID)_00-0000000000000000

#---------------------------------------------------------------------------------
# Packaging - ppu_rules' own knobs for its %.pkg rule.
#
# Using that rule instead of signing by hand is the whole point. The rule runs
# strip + sprx_tool before make_self_npdrm, and generates EBOOT.BIN, PARAM.SFO and
# the pkg in a single pass so the three cannot drift apart.
#
# Signing by hand - make_self_npdrm installer.elf EBOOT.BIN $(CONTENTID) - fed
# the raw elf: no strip, no sprx_tool. The result was rejected by RPCS3 with
# "ppu_exec -> Failed to read ELF program data", while the toolchain's own
# .self built from the same sources loaded and ran. Same linker, same sources;
# the only difference was those two missing steps.
#---------------------------------------------------------------------------------

PKGFILES	:=	$(CURDIR)/../packaging/pkgfiles
ICON0		:=	$(CURDIR)/../packaging/assets/ICON0.PNG
PAYLOAD		:=	$(CURDIR)/../bin/Release-PS3/qDNSm.sprx

CFLAGS		=	-O2 -Wall -mcpu=cell $(MACHDEP) $(INCLUDE)
CXXFLAGS	=	$(CFLAGS) -DQDNSM_PSL1GHT

LDFLAGS		=	$(MACHDEP) -Wl,-Map,$(notdir $@).map

# Same set the PSL1GHT samples and pkgi use. -lsysutil is the one that was
# missing; the rest are here to match a known-good link line rather than because
# the installer calls them directly.
LIBS	:=	-lrsx -lgcm_sys -lio -lsysutil -lrt -llv2 -lm -lsysfs -lsysmodule

LIBDIRS	:=	

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR	:=	$(CURDIR)/$(BUILD)
export BUILDDIR	:=	$(CURDIR)/$(BUILD)

# Listed explicitly rather than globbed: CrtStubs.c lives in this directory and is
# an SNC-era leftover that must not be compiled here.
CPPFILES	:=	Installer.cpp InstallerFs.cpp BootPlugins.cpp Dialog.cpp

ifeq ($(strip $(CPPFILES)),)
  export LD	:=	$(CC)
else
  export LD	:=	$(CXX)
endif

export OFILES	:=	$(CPPFILES:.cpp=.o)
export INCLUDE	:=	$(foreach dir,$(INCLUDES), -I$(CURDIR)/$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					$(LIBPSL1GHT_INC) \
					-I$(CURDIR)/$(BUILD)
export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib) \
					$(LIBPSL1GHT_LIB)

export OUTPUT	:=	$(CURDIR)/$(TARGET)

.PHONY: $(BUILD) clean self pkg

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

# Build the PKG through ppu_rules' own %.pkg rule.
#
# Two things this replaces:
#   - hand-running make_self_npdrm, which skipped strip + sprx_tool
#   - a PARAM.SFO copied from Apollo's package and edited by hand, whose
#     structural offsets had never been validated by the sfo tool
#
# Staging the payload first: the installer reads qDNSm.sprx from its own USRDIR,
# so it has to ship beside the EBOOT or Install() has nothing to copy.
# Depends on $(BUILD), not $(OUTPUT).self: compilation happens in the build/
# sub-make, and asking the outer Makefile for $(OUTPUT).elf directly skips it
# and fails the link with "undefined reference to `.main`".
pkg: $(BUILD)
	@mkdir -p $(PKGFILES)/USRDIR
	@cp $(PAYLOAD) $(PKGFILES)/USRDIR/qDNSm.sprx
	@$(MAKE) --no-print-directory $(OUTPUT).pkg

# Wrap the ELF in a SELF. PSL1GHT's base_rules provide this; it is stated here so
# the dependency is visible rather than inherited.
$(OUTPUT).self: $(OUTPUT).elf

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).self $(OUTPUT).pkg

else

DEPENDS	:=	$(OFILES:.o=.d)

$(OUTPUT).self: $(OUTPUT).elf
$(OUTPUT).elf:	$(OFILES)


-include $(DEPENDS)

endif
