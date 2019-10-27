target = build/program
lib = -lm -lstdc++fs
cc = g++
ext = .cpp
hdr_ext = .hpp

c_flags = \
 -funsigned-char -Wall -Wextra -Wno-char-subscripts -Wno-unused-variable \
 -Werror -std=c++17 -fmax-errors=1 -O3

obj := $(patsubst src/%$(ext), build/%.o, $(wildcard src/*$(ext)))
hdr = $(wildcard src/*$(hdr_ext))

all: $(target)

$(obj) : build/%.o: src/%$(ext) $(hdr)
	mkdir -p build/
	$(cc) -c $(c_flags) $< -o $@

.PRECIOUS : $(target) $(obj)

$(target) : $(obj)
	$(cc) -o $@ $(obj) -Wall $(lib)

clean :
	rm -rf build/

test :
	python3.7 test.py tests/

.PHONY : all clean test
