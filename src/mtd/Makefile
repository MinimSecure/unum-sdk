# The original Makefile from openwrt, 
# adapted for the unum build

CFLAGS += -Wall -Werror -I.

obj = mtd.o jffs2.o crc32.o md5.o 
obj.seama = seama.o md5alt.o
obj.ar71xx = trx.o $(obj.seama)
obj.brcm = trx.o
obj.brcm47xx = $(obj.brcm)
obj.bcm53xx = $(obj.brcm) $(obj.seama)
obj.brcm63xx = imagetag.o
obj.ramips = $(obj.seama)
obj.mvebu = linksys_bootcount.o
obj.kirkwood = linksys_bootcount.o
obj.ipq806x = linksys_bootcount.o

ifdef FIS_SUPPORT
  obj += fis.o
endif

all: mtd

$(obj) $(obj.$(TARGET_PLATFORM)): %.o: %.c
	$(GCC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

mtd: $(obj) $(obj.$(TARGET_PLATFORM))
	$(GCC) $(LDFLAGS) -o $@ $^

clean:
	rm -f *.o mtd
