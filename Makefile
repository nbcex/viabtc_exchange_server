CIMAGES = matchengine accessws alertcenter marketprice matchengine readhistory accesshttp

cpkgmap.matchengine	:= matchengine 
cpkgmap.accessws	:= accessws
cpkgmap.alertcenter	:= alertcenter
cpkgmap.marketprice	:= marketprice
cpkgmap.matchengine	:= matchengine
cpkgmap.readhistory	:= readhistory
cpkgmap.accesshttp	:= accesshttp

all: $(patsubst %, build/bin/engine/%, $(CIMAGES))

build/bin/engine/%:
	@mkdir -p $(@D)
	@echo "$@"
	make -C $(@F)
	@echo "Binary available as $@.exe"
	@cp $(cpkgmap.$(@F))/$(@F).exe $@.exe
	@touch $@.exe

build/bin/engine/%-clean:
	@echo "$@"
	$(eval TARGET = $(patsubst %-clean, %, $(@F)))
	@echo $(TARGET).exe
	make -C $(cpkgmap.$(TARGET)) clean 
	@rm -f $(@D)/$(TARGET).exe
	@echo "Clean binary $@.exe"

.PHONY: matchengine
matchengine: build/bin/engine/matchengine

.PHONY: accessws
accessws: build/bin/engine/accessws

.PHONY: alertcenter
alertcenter: build/bin/engine/alertcenter

.PHONY: marketprice
marketprice: build/bin/engine/marketprice

.PHONY: matchengine
matchengine: build/bin/engine/matchengine

.PHONY: readhistory
readhistory: build/bin/engine/readhistory

.PHONY: accesshttp
accesshttp: build/bin/engine/accesshttp

.PHONY: clean
clean: $(patsubst %, build/bin/engine/%-clean, $(CIMAGES))

.PHONY: depends
depends:
	#make -C depends
	make -C utils 
	make -C network
	@mkdir -p build/lib
	@cp utils/libutils.a build/lib
	@cp network/libnetwork.a build/lib
	@cp depends/librdkafka/src/librdkafka.so build/lib
	@cp depends/curl/lib/.libs/libcurl.a build/lib
