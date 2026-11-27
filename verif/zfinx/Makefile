# Golden-vector header for the Hazard3 zfinx_golden software test.
#
#   make             regenerate hazard3/test/sim/sw_testcases/include/zfinx_vectors.h
#   make N=5000      with a different vector count
#
# The header is committed in the Hazard3 fork, so this is only needed to
# change or extend the vectors.

ROOT   := $(abspath ../..)
SF     := $(ROOT)/third_party/berkeley-softfloat-3
SF_LIB := $(ROOT)/verif/fpu/build/softfloat/softfloat.a
OUT    := $(ROOT)/hazard3/test/sim/sw_testcases/include/zfinx_vectors.h

N      ?= 3000
SEED   ?= 1

.PHONY: all clean
all: $(OUT)

$(SF_LIB):
	$(MAKE) -C $(ROOT)/verif/fpu $(SF_LIB)

build/gen_zfinx_vectors: gen_zfinx_vectors.cpp $(SF_LIB)
	mkdir -p build
	g++ -O2 -I$(SF)/source/include $< $(SF_LIB) -o $@

$(OUT): build/gen_zfinx_vectors
	build/gen_zfinx_vectors $(N) $(SEED) > $@

clean:
	rm -rf build
