#
# Makefile for a machine kernel image
#

CIRCLEHOME = third_party/circle-stdlib/libs/circle



-include $(CIRCLEHOME)/Config.mk

ifeq ($(strip $(AARCH)),64)
NEWLIBDIR = third_party/circle-stdlib/install/aarch64-none-circle
else
NEWLIBDIR = third_party/circle-stdlib/install/arm-none-circle
endif

APP_INCLUDES = -I"$(NEWLIBDIR)/include" -I$(STDDEF_INCPATH) \
	      -Ithird_party/circle-stdlib/include \
	      -I$(CIRCLEHOME)/include \
	      -I$(CIRCLEHOME)/addon \
	      -Ithird_party/vice-3.10/src \
	      -I$(CIRCLEHOME)/addon/fatfs

ifeq ($(MACHINE_CLASS),RASPI_PLUS4EMU)
	APP_INCLUDES += -I "third_party/plus4emu/src"
endif




APP_INCLUDES += -I.

EXTRAINCLUDE += $(APP_INCLUDES)

OBJS	= main.o kernel.o new_io.o usblog.o bmcmodem.o xum1541_glue.o \
		  viceoptions.o viceapp.o vice_network.o network_time_sync.o \
		  bmc_condivisione.o



bmc_condivisione.o: CPPFLAGS += -DBMC_SOCK_IOVEC -Ithird_party/smb/libsmb2/include \
	-Ithird_party/smb/libsmb2/include/smb2 -Ithird_party/smb/src






ifeq ($(RASPPI),5)
HDMI_SOUND = 1
endif
ifeq ($(HDMI_SOUND),1)
OBJS	+= vicesoundhdmi.o
else
OBJS	+= vicesound.o vicesoundbasedevice.o


ifneq ($(filter 4 5,$(RASPPI)),)
OBJS	+= vicesoundhdmi.o
endif
endif



ifneq ($(filter 4 5,$(RASPPI)),)
OBJS	+= vicesoundusb.o
endif







ifeq ($(strip $(AARCH)),64)
OBJS	+= fbl_native_kms.o
else
OBJS	+= fbl.o crt_pi_idx.o crt_pi_rgb.o
endif









ifeq ($(strip $(AARCH)),64)
OBJS	+= kms/kms_mode.o
ifeq ($(strip $(RASPPI)),5)
OBJS	+= pi5kms/pi5_kms.o pi5kms/pi5_kms_probe.o
endif
ifeq ($(strip $(RASPPI)),4)
OBJS	+= pi4kms/pi4_kms.o pi4kms/pi4_kms_dlist.o pi4kms/pi4_kms_mode.o \
	   pi4kms/pi4_kms_probe.o pi4kms/pi4_native_kms.o
endif
endif










ifeq ($(strip $(AARCH)),64)
OBJS	+= v3dcrt/v3d_crt.o v3dcrt/effect_params.o v3dcrt/semantic_values.o \
	   v3dcrt/rgba8_texture_layout.o v3dcrt/output_response.o \
	   v3dcrt/shaders/shader_package.o
ifeq ($(strip $(RASPPI)),5)
OBJS	+= pi5v3d/pi5_v3d.o pi5v3d/pi5_v3d71_texture_state.o \
	   pi5v3d/pi5_v3d_shader.o \
	   pi5v3d/shaders/pi5_shader_package_adapter.o \
	   pi5v3d/shaders/shader_artifact_materializer.o
endif
ifeq ($(strip $(RASPPI)),4)
OBJS	+= pi4v3d/pi4_v3d.o pi4v3d/pi4_v3d_identity.o pi4v3d/pi4_v3d_mmu.o \
	   pi4v3d/pi4_v3d_power.o \
	   pi4v3d/pi4_v3d42_shader_package_adapter.o \
	   pi4v3d/pi4_v3d42_texture_state.o pi4v3d/pi4_v3d_render.o
endif
endif

ifeq ($(MACHINE_CLASS),RASPI_PLUS4EMU)
OBJS	+= plus4emulatorcore.o
else
OBJS	+= viceemulatorcore.o sidworker.o








RESIDFP_DIR = $(VICE)/lib/libresidfp
RESIDFP_OBJS = residfp/Dac.o residfp/EnvelopeGenerator.o \
	residfp/ExternalFilter.o residfp/Filter.o residfp/Filter6581.o \
	residfp/Filter8580.o residfp/FilterModelConfig.o \
	residfp/FilterModelConfig6581.o residfp/FilterModelConfig8580.o \
	residfp/Integrator6581.o residfp/Integrator8580.o residfp/OpAmp.o \
	residfp/SID.o residfp/Spline.o residfp/State.o \
	residfp/WaveformCalculator.o residfp/WaveformGenerator.o \
	residfp/resample/SincResampler.o residfp/residfp_colla.o
OBJS	+= $(RESIDFP_OBJS)
endif

include $(CIRCLEHOME)/Rules.mk

CFLAGS += $(APP_INCLUDES) -D $(MACHINE_CLASS)
CPPFLAGS += $(APP_INCLUDES) -D $(MACHINE_CLASS) -fno-exceptions -fno-rtti





CFLAGS += -DBMX_PI4_LEGACY_DISPLAY=$(if $(filter 4-32,$(RASPPI)-$(AARCH)),1,0)
CPPFLAGS += -DBMX_PI4_LEGACY_DISPLAY=$(if $(filter 4-32,$(RASPPI)-$(AARCH)),1,0)
CPPFLAGS += -fcheck-new


CPPFLAGS += -DBMX_SID_WORKER=1 -DBMX_SID_DIAGNOSTICS=0






fbl_native_kms.o: CPPFLAGS += -DNDEBUG

ifeq ($(HDMI_SOUND),1)
CFLAGS += -DBMC64_HDMI_SOUND
CPPFLAGS += -DBMC64_HDMI_SOUND
endif



ifeq ($(HDMI_SOUND),1)
VCHIQ_LIB =
else
VCHIQ_LIB = $(CIRCLEHOME)/addon/vc4/vchiq/libvchiq.a
endif

ifeq ($(strip $(AARCH)),64)
VC4_USERLAND_LIBS =
else
VC4_USERLAND_LIBS = \
	$(CIRCLEHOME)/addon/vc4/interface/bcm_host/libbcm_host.a \
	$(CIRCLEHOME)/addon/vc4/interface/khronos/libkhrn_client.a \
	$(CIRCLEHOME)/addon/vc4/interface/vcos/libvcos.a \
	$(CIRCLEHOME)/addon/vc4/interface/vmcs_host/libvmcs_host.a
endif

FILTERED_CIRCLE_NEWLIB = libcirclenewlib-bmc64.a

$(FILTERED_CIRCLE_NEWLIB): $(NEWLIBDIR)/lib/libcirclenewlib.a
	@cp $< $@
	@$(AR) d $@ io.o

EXTRACLEAN += $(FILTERED_CIRCLE_NEWLIB)

$(TARGET).img: $(FILTERED_CIRCLE_NEWLIB)

LIBS := $(VICELIBS) \
        third_party/common/libbmc64common.a \
        third_party/smb/libbmcsmb.a \
        $(NEWLIBDIR)/lib/libm.a \
	$(NEWLIBDIR)/lib/libc.a \
	$(FILTERED_CIRCLE_NEWLIB) \
 	$(CIRCLEHOME)/addon/SDCard/libsdcard.a \
  	$(CIRCLEHOME)/lib/usb/libusb.a \
 	$(CIRCLEHOME)/lib/input/libinput.a \
 	$(CIRCLEHOME)/lib/fs/libfs.a \
  	$(CIRCLEHOME)/lib/net/libnet.a \
  	$(VCHIQ_LIB) \
	$(VC4_USERLAND_LIBS) \
  	$(CIRCLEHOME)/addon/linux/liblinuxemu.a \
	$(CIRCLEHOME)/addon/fatfs/libfatfs.a \
	$(CIRCLEHOME)/addon/wlan/hostap/wpa_supplicant/libwpa_supplicant.a \
	$(CIRCLEHOME)/addon/wlan/libwlan.a \
	$(CIRCLEHOME)/lib/sound/libsound.a \
  	$(CIRCLEHOME)/lib/sched/libsched.a \
  	$(CIRCLEHOME)/lib/libcircle.a



kms/%.o: kms/%.cpp
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) -I. -c -o $@ $<

pi4kms/%.o: pi4kms/%.cpp
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) -I. -c -o $@ $<

pi5kms/%.o: pi5kms/%.cpp
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) -I. -c -o $@ $<

v3dcrt/%.o: v3dcrt/%.cpp
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) -I. -c -o $@ $<

v3dcrt/shaders/%.o: v3dcrt/shaders/%.cpp
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) -I. -c -o $@ $<

pi4v3d/%.o: pi4v3d/%.cpp
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) -I. -c -o $@ $<

pi5v3d/%.o: pi5v3d/%.cpp
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) -I. -c -o $@ $<

pi5v3d/shaders/%.o: pi5v3d/shaders/%.cpp
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) -I. -c -o $@ $<








RESIDFP_FLAGS = -std=gnu++17 -O3 -ffp-contract=off -fno-sized-deallocation \
	-DNDEBUG -DRESIDFP_SENZA_THREAD -DRESIDFP_SENZA_ECCEZIONI \
	-DRESIDFP_MASCHERA_VOCI -DRESIDFP_INVILUPPI_VISIBILI \
	-DRESIDFP_RUMORE_REGOLABILE

EXTRACLEAN += residfp/*.o residfp/*.d residfp/resample/*.o residfp/resample/*.d

residfp/residfp_colla.o: $(VICE)/sid/residfp.cc
	@mkdir -p $(@D)
	@echo "  CPP   $@"
	@$(CPP) -I"$(NEWLIBDIR)/include" $(CPPFLAGS) $(RESIDFP_FLAGS) -I$(VICE) -I$(VICE)/sid \
		-I$(VICE)/arch/shared -I$(VICE)/arch/raspi -I$(RESIDFP_DIR) -I$(RESIDFP_DIR)/src -c -o $@ $<

residfp/%.o: $(RESIDFP_DIR)/src/%.cpp
	@mkdir -p $(@D)
	@echo "  CPP   $@"
	@$(CPP) -I"$(NEWLIBDIR)/include" $(CPPFLAGS) $(RESIDFP_FLAGS) -I$(RESIDFP_DIR)/src -c -o $@ $<
