CC ?= cc
SDL_CONFIG ?= sdl2-config
COMPONENT := ../../components/big_r_led
CFLAGS += -std=c11 -Wall -Wextra -O2 $(shell $(SDL_CONFIG) --cflags) -I$(COMPONENT)
LDLIBS += $(shell $(SDL_CONFIG) --libs) -lm

simulator: main.c $(COMPONENT)/big_r_animation.c $(COMPONENT)/color_format.c $(COMPONENT)/big_r_animation.h $(COMPONENT)/big_r_patterns.h
	$(CC) $(CFLAGS) main.c $(COMPONENT)/big_r_animation.c $(COMPONENT)/color_format.c $(LDFLAGS) $(LDLIBS) -o $@

.PHONY: clean
clean:
	rm -f simulator
