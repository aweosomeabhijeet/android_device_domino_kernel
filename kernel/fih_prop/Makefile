#
# FIH proprietory
#

ifeq ($(KBUILD_SRC),)
    FIH_PROP_SRC_PATH = $(objtree)/kernel/fih_prop/src
    FIH_PROP_OBJ_PATH = $(objtree)/kernel/fih_prop/obj
else
    FIH_PROP_SRC_PATH = $(KBUILD_SRC)/kernel/fih_prop/src
    FIH_PROP_OBJ_PATH = $(KBUILD_SRC)/kernel/fih_prop/obj
endif

$(warning FIH_PROP_SRC_PATH=$(FIH_PROP_SRC_PATH) src_dir=$(KBUILD_SRC) objtree=$(objtree))
#$(shell export > /root/workspace/paul_log)

ifeq ($(shell test -d "$(FIH_PROP_SRC_PATH)" && echo y),y)
    obj-y     += src/
    $(warning $(FIH_PROP_SRC_PATH) exist!)
else
    obj-y     :=
    $(warning $(FIH_PROP_SRC_PATH) does not exist!)
    ifeq ($(shell test -d "$(FIH_PROP_OBJ_PATH)" && echo y),y)
    $(warning "$(FIH_PROP_OBJ_PATH)" exist!)
    # copy obj files back to /kernel/kernel/fih_prop/obj
    $(shell cp -R $(FIH_PROP_OBJ_PATH)/* $(objtree)/kernel/fih_prop)
    endif
endif

$(warning obj-y=$(obj-y))
