# Benchmark czasu klatek

Opcja jest domyślnie wyłączona. Kod pomiaru jest osłonięty przez
`#ifdef USE_FRAME_BENCHMARK`. Włączanie dla CMake:

```sh
cmake -S . -B build/benchmark -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=23 -DUSE_FRAME_BENCHMARK=ON
cmake --build build/benchmark --parallel "$(( $(nproc) / 2 > 0 ? $(nproc) / 2 : 1 ))"
```

Dla Make użyj osobnego katalogu, żeby nie wykorzystać obiektów zwykłej wersji:

```sh
make BUILD_DIR=build/benchmark-make USE_FRAME_BENCHMARK=1 -j"$(( $(nproc) / 2 > 0 ? $(nproc) / 2 : 1 ))"
```

Uruchom zbudowaną grę i zagraj lub odtwórz demo. Przy normalnym wyjściu
(np. `quit`) powstają trzy CSV w `benchmarks/`, w katalogu zapisu aktualnego
moda (`fs_homepath` / katalog gry). Nazwa zawiera datę i znacznik czasu:

- `*-summary.csv`: wynik całej sesji.
- `*-timeline.csv`: wyniki kolejnych, niepokrywających się przedziałów około
  jednej sekundy; ostatni przedział może być krótszy.
- `*-frames.csv`: czas każdej zmierzonej klatki w oryginalnej kolejności.

Pomiar korzysta z rzeczywistego zegara, pomiędzy początkami kolejnych wywołań
`CL_Frame`, gdy klient jest w `CA_ACTIVE`. Obejmuje pracę klienta i serwera,
oczekiwanie na limit FPS oraz VSync. To czas klatki całego silnika, a nie sam
czas GPU. Menu przed połączeniem i ładowanie map nie są mierzone; menu lub
konsola otwarte podczas aktywnej gry są częścią pomiaru. Pierwsze wywołanie
po przerwie ustawia początek pomiaru. Nie ma odrzucania rozgrzewki ani
najwolniejszych próbek.

Oś `start_s` / `end_s` / `elapsed_s` to suma zmierzonych czasów klatek;
nie obejmuje przerw na rozłączenie lub ładowanie. Przedział kończy się na
pierwszej klatce osiągającej jedną sekundę. Zawieszenie jednej klatki może
więc wydłużyć przedział.

Average frametime, median, p95, p99 i p99.9 są w milisekundach. Percentyle
korzystają z interpolacji liniowej na posortowanych próbkach. `1% low`
i `0.1% low` są w FPS: 1000 podzielone przez średnią czasu najwolniejszych
odpowiednio 1% i 0,1% klatek. Liczba wybranych klatek jest zaokrąglana w górę.
Przy niewielkiej liczbie próbek skrajne statystyki mają małą miarodajność;
kolumna `frames` pozwala sprawdzić liczebność każdego przedziału.

Próbki są przechowywane w RAM (8 bajtów na klatkę plus zapas tablicy),
a sortowanie i zapis następują dopiero podczas zamykania klienta. Nie ma
zapisu na dysk w każdej klatce. Nagłe zabicie procesu nie zapisze raportu.
Brak pamięci zatrzymuje zbieranie i zgłasza częściowy raport w konsoli.
Przy ponownym uruchomieniu powstają nowe pliki.

Test obliczeń i zbierania próbek (bez uruchamiania gry):

```sh
cc -Wall -Wextra tests/frame_benchmark_test.c -lm -o /tmp/frame_benchmark_test
/tmp/frame_benchmark_test
```
