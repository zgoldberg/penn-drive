TARGETS = kv_cache

all: $(TARGETS)

%.o: %.cpp %.hh
	g++ -g -std=c++17 $< -c -o $@

kv_cache: kv_cache.o
	g++ -g -std=c++17 $^ -o $@

clean::
	rm -fv $(TARGETS) *~
