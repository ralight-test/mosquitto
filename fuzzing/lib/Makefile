R=../..
.PHONY: all clean

FUZZERS:= \
	lib_fuzz_pub_topic_check2 \
	lib_fuzz_sub_topic_check2 \
	lib_fuzz_utf8

LOCAL_CPPFLAGS+=-I${R}/include/
LOCAL_CXXFLAGS+=-g -Wall -Werror -pthread
LOCAL_LDFLAGS+=
LOCAL_LIBADD+=$(LIB_FUZZING_ENGINE) ${LIBMOSQ_STATIC} -lssl -lcrypto

all: $(FUZZERS)

lib_fuzz_pub_topic_check2 : lib_fuzz_pub_topic_check2.cpp
	$(CXX) $(LOCAL_CXXFLAGS) $(LOCAL_CPPFLAGS) $(LOCAL_LDFLAGS) -o $@ $^ $(LOCAL_LIBADD)
	install $@ ${OUT}/$@

lib_fuzz_sub_topic_check2 : lib_fuzz_sub_topic_check2.cpp
	$(CXX) $(LOCAL_CXXFLAGS) $(LOCAL_CPPFLAGS) $(LOCAL_LDFLAGS) -o $@ $^ $(LOCAL_LIBADD)
	install $@ ${OUT}/$@

lib_fuzz_utf8 : lib_fuzz_utf8.cpp
	$(CXX) $(LOCAL_CXXFLAGS) $(LOCAL_CPPFLAGS) $(LOCAL_LDFLAGS) -o $@ $^ $(LOCAL_LIBADD)
	install $@ ${OUT}/$@

clean:
	rm -f *.o $(FUZZERS)
