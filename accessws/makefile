TARGET  := accessws.exe
INCS = -I ../network -I ../utils -I ../depends/librdkafka/src -I ../depends/curl/include
LIBS = -L ../utils -lutils -L ../network -lnetwork -Wl,-Bstatic -lev -ljansson -lmpdec -lz -lssl -lcrypto -lhiredis -Wl,-Bdynamic -lm -lpthread -ldl -L ../build/lib -lcurl -lrdkafka 
include ../makefile.inc
