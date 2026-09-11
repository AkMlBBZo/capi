# capi

C++17 API based on socket and epoll.

## Build

```bash
mkdir -p cmake-build-release && cd cmake-build-release
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

## Run

```bash
./cmake-build-release/CXX_REST_API
```

Listens on `0.0.0.0:8091`.

## Test

```bash
cmake -B build-test -S .
cmake --build build-test
ctest --test-dir build-test --output-on-failure
```

## License

MIT.