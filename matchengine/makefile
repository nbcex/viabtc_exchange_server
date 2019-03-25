TARGET  := matchengine.exe
INCS = -I ../network -I ../utils -I ../depends/librdkafka-0.11.6/src
LIBS = -L ../utils -lutils -L ../network -lnetwork -Wl,-Bstatic -lev -ljansson -lmpdec -lz -lssl -lcrypto -lhiredis -Wl,-Bdynamic -lm -lpthread -ldl -lmysqlclient -lrdkafka -L ../build/lib -lrdkafka
include ../makefile.inc
