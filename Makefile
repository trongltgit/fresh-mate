.PHONY: build run clean docker

build:
	mkdir -p build
	cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && make -j$$(nproc)

run: build
	cd build && ./freshmate_server

clean:
	rm -rf build

docker:
	docker build -t freshmate-web .
	docker run --rm -p 8080:8080 -e ANTHROPIC_API_KEY=$$ANTHROPIC_API_KEY freshmate-web