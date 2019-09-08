.PHONY: clean All

All:
	@echo "----------Building project:[ notekit - Debug ]----------"
	@"$(MAKE)" -f  "notekit.mk"
clean:
	@echo "----------Cleaning project:[ notekit - Debug ]----------"
	@"$(MAKE)" -f  "notekit.mk" clean
