SHELL := /bin/bash
.PHONY: all bootstrap test clean package-tools test-package preview check check-imports candidate-tools tide-data suite
tide-data:
	python3 scripts/configure-tides.py
suite: all
	./scripts/prepare-suite.sh
all:
	./scripts/build.sh
preview:
	./scripts/generate-preview.sh
check: test all test-package preview
check-imports:
	python3 scripts/check-imports.py
candidate-tools:
	./scripts/bootstrap-candidate.sh
bootstrap:
	./scripts/bootstrap.sh
package-tools:
	./scripts/bootstrap-package.sh
test-package:
	node tests/test_package.mjs
test: tide-data
	mkdir -p build
	$(CC) -std=c99 -Wall -Wextra -Werror -pedantic -Isrc src/nav.c tests/test_nav.c -lm -o build/test_nav
	./build/test_nav
	$(CC) -std=c99 -Wall -Wextra -Werror -pedantic -Isrc src/nav.c src/waypoints.c tests/test_waypoints.c -lm -o build/test_waypoints
	./build/test_waypoints
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -pedantic -Isrc tests/test_nav_trig.c -lm -o build/test_nav_trig
	./build/test_nav_trig
	$(CC) -std=c99 -Wall -Wextra -Werror -pedantic -DNAV_SMALL_MATH=1 -Isrc src/nav.c tests/test_nav.c -lm -o build/test_nav_small
	./build/test_nav_small
	python3 tests/test_small_math_accuracy.py
	./scripts/test-watch.sh
	$(CC) -std=c99 -Wall -Wextra -Werror -pedantic -Isrc src/outdoor_data.c tests/test_outdoor_data.c -lm -o build/test_outdoor_data
	./build/test_outdoor_data
	python3 tests/test_import_tides.py
	python3 tests/test_fetch_chs.py
	$(CC) -std=c99 -Wall -Wextra -Werror -pedantic -Wno-pedantic -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Itests/stubs_barometer tests/test_barometer.c -lm -o build/test_barometer
	./build/test_barometer
	./scripts/test-tides-sun.sh
	$(CC) -std=c99 -Wall -Wextra -Werror -pedantic -Wno-pedantic -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Itests/stubs_compass tests/test_compass.c -o build/test_compass
	./build/test_compass
clean:
	rm -rf build
