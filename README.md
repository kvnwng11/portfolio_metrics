To build:

```
cmake -S . -B build
cmake --build build
```

To run:

```
.\build\Debug\portfolio_metrics.exe
```


To test:

```
ctest --test-dir build -C Debug --output-on-failure
```