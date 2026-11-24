NASM ?= nasm
BUILD := build
STAGE2_BYTES := 16384
IMAGE_BYTES := 16896

.PHONY: all clean check-tools
all: $(BUILD)/xbox-pc-boot.img

check-tools:
	@command -v $(NASM) >/dev/null || { echo "Missing NASM"; exit 1; }

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/bootsect.bin: bootsect.asm | check-tools $(BUILD)
	$(NASM) -f bin $< -o $@
	@test "$$(wc -c < $@)" -eq 512 || { echo "Boot sector must be 512 bytes"; exit 1; }

$(BUILD)/stage2.bin: stage2.asm | check-tools $(BUILD)
	$(NASM) -f bin $< -o $@
	@test "$$(wc -c < $@)" -le $(STAGE2_BYTES) || { echo "Stage 2 exceeds 32 sectors"; exit 1; }
	truncate -s $(STAGE2_BYTES) $@

$(BUILD)/xbox-pc-boot.img: $(BUILD)/bootsect.bin $(BUILD)/stage2.bin
	cat $(BUILD)/bootsect.bin $(BUILD)/stage2.bin > $@
	@test "$$(wc -c < $@)" -eq $(IMAGE_BYTES) || { echo "Unexpected image size"; exit 1; }

clean:
	rm -rf $(BUILD)
