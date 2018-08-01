# asiofi

C++ Boost.Asio language bindings for OFI libfabric.

EARLY DEVELOPMENT - API NOT STABLE

## Introduction

This project is developed as a dependency of another [project](https://github.com/FairRootGroup/FairMQ) which currently
limits the scope (`FI_MSG`, `FI_VERBS`). If you are interested in extending the scope and/or collaborating,
we are welcoming you to talk to us.

## Dependencies

Bindings (Core):

- [OFI libfabric](https://github.com/ofiwg/libfabric) (Hint location via `-DOFI_ROOT=/path/to/libfabric`)
- [Boost.Asio](http://www.boost.org/) (Hint location via `-DBOOST_ROOT=/path/to/boost`)
- [Boost.Container](http://www.boost.org/) (Hint location via `-DBOOST_ROOT=/path/to/boost`)

Fabtests (Optional, enable with `-DBUILD_FABTESTS=ON`):

- [Google benchmark](https://github.com/google/benchmark) (Hint location via `-DBENCHMARK_ROOT=/path/to/benchmark`)
- [Boost.Interprocess](http://www.boost.org/) (Hint location via `-DBOOST_ROOT=/path/to/boost`)
- [Boost.Program\_options](http://www.boost.org/) (Hint location via `-DBOOST_ROOT=/path/to/boost`)

## Quickstart

```bash
git clone https://github.com/FairRootGroup/asiofi
mkdir asiofi_build && cd asiofi_build
cmake -DCMAKE_INSTALL_PREFIX=./asiofi_install ../asiofi
cmake --build . --target install
```

## License

GNU Lesser General Public Licence (LGPL) version 3, see [LICENSE](LICENSE).

Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
