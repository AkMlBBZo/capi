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

## License

MIT.