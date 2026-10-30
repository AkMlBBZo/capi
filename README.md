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

## Хранилище маршрутов
Указывается явно, в данный момент реализовано только LinearStorage, планируется реализация TrieStorage

```c++
#include "core/CAPI.h"
#include "router/storage/linear_storage.h"

auto server = capi::CAPI<capi::LinearStorage>::create(8091);
```

CAPI<Storage> владеет Router<Storage>.
Тип Storage должен иметь конструктор по умолчанию, предоставлять методы
```cpp
bool add_route(const Route& route);
RequestParams get_route(const Request& req) const;
```
Также он должен иметь move и copy конструкторы, это используется в фабрике сервера.
Наследование от базовых классов не требуется.

## Routing
- Маршрут выбирается по совпадению пути и HTTP метода 
обработка HEAD через GET на данном этапе не предусмотрена
- Регистр в пути учитывается, повторные слеши игнорируются
- Пустой путь "" не существует, add_route для пустого пути вернет false
- Допустимы параметры типа {param}, но недопустимы фигурные скобки внутри
- При нескольких полностью совпавших путях, 
приоритет определяется сравнением сегментов слева направо.
Например, при пути /x/y/z, среди /x/{y}/{z} и /{x}/y/z, будет выбран /x/{y}/{z}
- При повторной регистрации метода и шаблона, add_route вернет false.
Для разных методов использование одинаковых шаблонов разрешено
- Все пути необходимо регестрировать до начала работы сервера
- Аргументы обработчика действительны только в течение синхронного вызова, 
они хранятся во временном буфере

## TODO
- [ ] переписать README на английский
- [ ] переписать ADR на английский
- [ ] добавить TrieStorage
- [ ] добавить автоматическую обработку HEAD через GET
- [ ] добавить обработку разбиения ответа на несколько сообщений
- [ ] полноценная обработка медленных сообщений (EAGAIN + EPOLLIN)
- [ ] создать отдельный файл с инстанцированием sotrage для CAPI

## License

MIT.