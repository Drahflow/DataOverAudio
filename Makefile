all: oversound oversound2 oversoundfftw sendfreq freqanalyze sendsync recvsync sender2 doa

%: %.cpp
	g++ $< -ggdb -o $@

oversoundfftw: oversoundfftw.cpp
	g++ $< -ggdb -o $@ -lfftw3

freqanalyze: freqanalyze.cpp
	g++ $< -ggdb -o $@ -lfftw3

recvsync: recvsync.cpp
	g++ $< -ggdb -o $@ -lfftw3

sender2: sender2.c
	gcc $< -ggdb -o $@ -lm

doa: doa.o doa_algo.o
	g++ $^ -ggdb -o $@ -lfftw3

%.o: %.cpp
	g++ $< -ggdb -c -o $@ -W -Wall -Wextra -Werror

clean:
	rm -fv doa
	rm -fv *.o
