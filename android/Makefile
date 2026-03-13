.DEFAULT_GOAL := all

.PHONY: all
all: build install logs


.PHONY: build
build:
	@mkdir -p output
	DOCKER_BUILDKIT=1 docker build --network=host -t kprox-builder .
	docker run --rm -v "$(PWD)/output:/host" kprox-builder

.PHONY: clean
clean:
	@rm -rf output/
	@docker rmi kprox-builder 2>/dev/null || true
	rm -rf output

.PHONY: install
install:
	adb uninstall com.kprox.discovery || true
	adb install output/kprox-discovery.apk

.PHONY: logs
logs:
	adb logcat | grep KProx
