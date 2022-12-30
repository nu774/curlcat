CXXFLAGS=-O0 -g3 -Wall
LDFLAGS=-lcurl

all: curlcat

curlcat: main.o
	$(CXX) -o $@ $< $(LDFLAGS)

main.o: main.cpp curl_wrapper.hpp util.hpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) -o $@ $<

clean:
	$(RM) *.o curlcat
