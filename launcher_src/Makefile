.SUFFIXES:

ifeq ($(strip $(PSL1GHT)),)
  $(error "Please set PSL1GHT in your environment. export PSL1GHT=<path>")
endif

include $(PSL1GHT)/ppu_rules

TARGET		:=	launcher
BUILD		:=	build
SOURCES		:=	. ../installer_src
INCLUDES	:=	. ../installer_src
TITLE		:=	qDNSm Launcher
APPID		:=	NPUB72382
CONTENTID	:=	UP0001-$(APPID)_00-0000000000000000

PKGFILES	:=	$(CURDIR)/../packaging/launcher-pkgfiles
# The launcher icon is distinct from the installer icon: it has no "Installer"
# caption. Prefer build/pkg/ICON0.PNG, and fall back to the source-tree copy so
# a clean build still packages the right artwork. Copy it aside for ppu_rules
# so its cp step does not try to copy a file onto itself.
LAUNCHER_ICON		:=	$(CURDIR)/build/pkg/ICON0.PNG
LAUNCHER_ICON_FALLBACK	:=	$(CURDIR)/ICON0.PNG
ICON0			:=	$(CURDIR)/build/launcher-ICON0.PNG
# Same rule for PARAM.SFO: source the edited copy before ppu_rules regenerates
# it, then let its PKGFILES copy overwrite the generated default.
LAUNCHER_PARAM		:=	$(CURDIR)/build/pkg/PARAM.SFO
LAUNCHER_PARAM_FALLBACK	:=	$(CURDIR)/PARAM.SFO
PAYLOAD		:=	$(CURDIR)/../bin/Release-PS3/qDNSm.sprx
GAMEDATA	:=	$(CURDIR)/../data/qDNSm-server-games.ini
EBOOT		:=	$(CURDIR)/../bin/Release-PS3/launcher/EBOOT.BIN

CFLAGS		=	-O2 -Wall -mcpu=cell $(MACHDEP) $(INCLUDE)
CXXFLAGS	=	$(CFLAGS) -DQDNSM_PSL1GHT

LDFLAGS		=	$(MACHDEP) -Wl,-Map,$(notdir $@).map

LIBS		:=	-lrsx -lgcm_sys -lio -lsysutil -lrt -llv2 -lsysfs

LIBDIRS		:=

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR	:=	$(CURDIR)/$(BUILD)
export BUILDDIR	:=	$(CURDIR)/$(BUILD)

CPPFILES	:=	Launcher.cpp LauncherLoader.cpp LauncherFiles.cpp LauncherSyscalls.cpp InstallerFs.cpp Dialog.cpp

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

pkg: $(BUILD)
	@test -f $(PAYLOAD) || { echo "ERROR: qDNSm.sprx missing at $(PAYLOAD)"; exit 1; }
	@test -f $(GAMEDATA) || { echo "ERROR: qDNSm-server-games.ini missing at $(GAMEDATA)"; exit 1; }
	@test -f $(LAUNCHER_ICON) -o -f $(LAUNCHER_ICON_FALLBACK) || { echo "ERROR: launcher icon missing at $(LAUNCHER_ICON)"; exit 1; }
	@if [ -f $(LAUNCHER_ICON) ]; then cp $(LAUNCHER_ICON) $(ICON0); else cp $(LAUNCHER_ICON_FALLBACK) $(ICON0); fi
	@test -f $(LAUNCHER_PARAM) -o -f $(LAUNCHER_PARAM_FALLBACK) || { echo "ERROR: launcher PARAM.SFO missing at $(LAUNCHER_PARAM)"; exit 1; }
	@mkdir -p $(PKGFILES)/USRDIR
	@if [ -f $(LAUNCHER_PARAM) ]; then cp $(LAUNCHER_PARAM) $(PKGFILES)/PARAM.SFO; else cp $(LAUNCHER_PARAM_FALLBACK) $(PKGFILES)/PARAM.SFO; fi
	@cp $(PAYLOAD) $(PKGFILES)/USRDIR/qDNSm.sprx
	@cp $(GAMEDATA) $(PKGFILES)/USRDIR/qDNSm-server-games.ini
	@rm -f $(OUTPUT).pkg
	@$(MAKE) --no-print-directory ICON0=$(ICON0) $(OUTPUT).pkg
	@mkdir -p $(dir $(EBOOT))
	@cp $(BUILDDIR)/pkg/USRDIR/EBOOT.BIN $(EBOOT)
	@grep -q 'NPD' $(EBOOT) || { echo "ERROR: the EBOOT carries no NPD block."; exit 1; }
	@grep -q '$(CONTENTID)' $(EBOOT) || { echo "ERROR: the EBOOT does not carry the content ID $(CONTENTID)."; exit 1; }
	@echo "Launcher package: $(OUTPUT).pkg"

$(OUTPUT).self: $(OUTPUT).elf

clean:
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).self $(OUTPUT).pkg $(PKGFILES)/USRDIR

else

DEPENDS	:=	$(OFILES:.o=.d)

$(OUTPUT).self: $(OUTPUT).elf
$(OUTPUT).elf:	$(OFILES)

-include $(DEPENDS)

endif
