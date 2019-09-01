.PHONY: clean All

All:
	@echo "----------Building project:[ notekit - Release ]----------"
	@"$(MAKE)" -f  "notekit.mk"
clean:
	@echo "----------Cleaning project:[ notekit - Release ]----------"
	@"$(MAKE)" -f  "notekit.mk" clean
