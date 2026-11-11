# atari800online

[English](#english) | [Polski](#polski)

---

## English

**atari800online** is a fork of the official [Atari800](https://github.com/atari800/atari800)
emulator (Atari 8-bit computers and the 5200 console), enriched with an **online game
browser that downloads games directly from [atarionline.pl](https://atarionline.pl)**
and starts them automatically. Everything else in the emulator works as in upstream Atari800.

### Why atarionline.pl?

[atarionline.pl](https://atarionline.pl) is a Polish community catalogue of Atari 8-bit
software, organised alphabetically (letter → title → files). It is a convenient, curated
source of disk images, executables, cartridges and tapes, so instead of hunting for files,
unpacking them and pointing the emulator at them, you can browse the catalogue from inside
the emulator and play with a couple of key presses.

Please be kind to the service: the browser keeps request volume minimal (one request at a
time, at least ~1.1 s between requests, lists cached on disk). Games remain the property of
their respective authors.

### What is new compared to upstream

* **Online Games browser** (`Alt+N`, also in the main menu as *Online Games*):
  browse the atarionline.pl catalogue by letter → title → file, directly in the emulator UI.
* **Direct download and automatic mounting**: pick a file and it is downloaded with libcurl,
  saved locally, and started right away (the emulator reboots into it). Disk images
  (`.atr .xfd .atx .pro .dcm`), executables (`.xex .com .exe .obx`), BASIC (`.bas`), tapes
  (`.cas`), cartridges (`.car .rom .bin`) and compressed files (`.gz .atz .xfz`) are
  recognised. For cartridge ROMs without a header you are asked for the cartridge type.
* **ZIP support**: if the download is a ZIP archive, it is extracted automatically and the
  first runnable file inside is started.
* **Local catalogue / offline play**: everything you download is stored under
  `~/.atari800/atarionline/` (one folder per letter). Downloaded games are never fetched
  twice and can be started again without network access.
  * **Previously downloaded games** – a history list (newest first, up to 200 entries)
    available from every level of the browser. It works offline: if the catalogue cannot be
    reached, the emulator opens this list directly.
  * **Delete downloaded games** – in the history list press `D` to delete a game from disk
    (with a Yes/No confirmation). Keys: `Enter` run, `D` delete, `Esc` back.
  * **Run local files** – the standard *Run Atari Program* (`Alt+R`) file browser lets you
    start any file from your own local directories.
* **Disk cache of the catalogue** lists (30 days) so browsing is instant and costs no
  requests; *[Refresh this list]* re-downloads the current list.
* **Remembers your last position** in the browser (letter / title / file).
* **Esc in the main menu**: a short tap resumes emulation, holding it for ~2 s quits
  (a hint is shown).
* **Fixed START / SELECT / OPTION keys** with SDL2 (F2–F4 were not detected on some setups).

### Building

Requirements: the usual Atari800 build tools, SDL2 and **libcurl** development headers
(the download feature is enabled automatically when libcurl is found; use
`--disable-download` to turn it off).

```sh
./autogen.sh
./configure --with-video=sdl2 --with-sound=sdl2   # adjust to your needs
make -j$(nproc)
cd src && ./atari800
```

You need Atari system ROMs (e.g. `ATARIXL.ROM`, `ATARIBAS.ROM`) in the emulator's ROM
directory or set through the emulator's configuration. They are **not** included and are
not part of this repository. Alternatively use the free replacement OS shipped with Atari800.

### Usage

1. Run `./atari800` (optionally with a file: `./atari800 game.atr`).
2. Press `Alt+N` (or `F1` → *Online Games*).
3. Choose a letter, then a title, then a file. The game downloads and starts.
4. Next time use *[Previously downloaded games]* to start it again, also offline.

For the full list of emulator options and keys see `README.TXT`, `DOC/` and the upstream
project.

### License

GPL v2 or later, same as Atari800 (see `COPYING`). This is an unofficial fork, not
affiliated with atarionline.pl.

---

## Polski

**atari800online** to fork oficjalnego emulatora [Atari800](https://github.com/atari800/atari800)
(komputery Atari 8-bit i konsola 5200) wzbogacony o **przeglądarkę gier online, która pobiera
gry wprost z serwisu [atarionline.pl](https://atarionline.pl)** i automatycznie je uruchamia.
Pozostałe funkcje emulatora działają tak jak w oryginalnym Atari800.

### Dlaczego atarionline.pl?

[atarionline.pl](https://atarionline.pl) to polski, społecznościowy katalog oprogramowania na
Atari 8-bit, uporządkowany alfabetycznie (litera → tytuł → pliki). To wygodne źródło obrazów
dyskietek, programów, kartridży i kaset. Zamiast szukać plików, rozpakowywać je i wskazywać
emulatorowi, możesz przeglądać katalog w samym emulatorze i zagrać po kilku naciśnięciach klawiszy.

Prosimy o rozsądne korzystanie z serwisu: przeglądarka ogranicza ruch do minimum (jedno
zapytanie naraz, odstęp co najmniej ok. 1,1 s, listy zapisywane w pamięci podręcznej na dysku).
Gry pozostają własnością ich autorów.

### Co nowego względem oryginału

* **Przeglądarka Online Games** (`Alt+N`, także w menu głównym jako *Online Games*):
  przeglądanie katalogu atarionline.pl: litera → tytuł → plik, bezpośrednio w interfejsie emulatora.
* **Bezpośrednie pobieranie i automatyczne montowanie**: wybrany plik jest pobierany przez
  libcurl, zapisywany lokalnie i od razu uruchamiany (emulator restartuje się z grą). Obsługiwane
  są obrazy dyskietek (`.atr .xfd .atx .pro .dcm`), programy (`.xex .com .exe .obx`), BASIC
  (`.bas`), kasety (`.cas`), kartridże (`.car .rom .bin`) i pliki spakowane (`.gz .atz .xfz`).
  Dla ROM-ów kartridży bez nagłówka emulator zapyta o typ kartridża.
* **Obsługa ZIP**: jeśli pobrany plik jest archiwum ZIP, zostaje automatycznie rozpakowany,
  a pierwszy uruchamialny plik z archiwum jest startowany.
* **Katalog lokalny / gra offline**: wszystko, co pobierzesz, trafia do
  `~/.atari800/atarionline/` (osobny folder dla każdej litery). Pobrane gry nie są ściągane
  ponownie i można je uruchamiać bez dostępu do sieci.
  * **Wcześniej pobrane gry**: lista historii (od najnowszej, do 200 pozycji) dostępna na każdym
    poziomie przeglądarki. Działa offline: gdy katalog jest nieosiągalny, emulator otwiera ją
    od razu.
  * **Usuwanie pobranych gier**: na liście historii naciśnij `D`, aby usunąć grę z dysku
    (z potwierdzeniem Tak/Nie). Klawisze: `Enter` uruchom, `D` usuń, `Esc` wstecz.
  * **Uruchamianie plików lokalnych**: standardowa przeglądarka *Run Atari Program* (`Alt+R`)
    pozwala uruchomić dowolny plik z własnych katalogów.
* **Pamięć podręczna list katalogu** na dysku (30 dni): przeglądanie jest natychmiastowe i nie
  generuje zapytań; *[Refresh this list]* pobiera bieżącą listę ponownie.
* **Zapamiętywanie ostatniej pozycji** w przeglądarce (litera / tytuł / plik).
* **Esc w menu głównym**: krótkie naciśnięcie wznawia emulację, przytrzymanie ok. 2 s kończy
  program (wyświetlana jest podpowiedź).
* **Naprawione klawisze START / SELECT / OPTION** w SDL2 (F2–F4 nie były wykrywane w niektórych
  konfiguracjach).

### Kompilacja

Wymagania: standardowe narzędzia do budowy Atari800, SDL2 oraz nagłówki **libcurl** (funkcja
pobierania włącza się automatycznie, gdy libcurl jest dostępna; `--disable-download` ją wyłącza).

```sh
./autogen.sh
./configure --with-video=sdl2 --with-sound=sdl2   # dostosuj do swoich potrzeb
make -j$(nproc)
cd src && ./atari800
```

Potrzebujesz ROM-ów systemowych Atari (np. `ATARIXL.ROM`, `ATARIBAS.ROM`) w katalogu ROM
emulatora lub wskazanych w jego konfiguracji. **Nie są** one dołączone do tego repozytorium.
Można też użyć darmowego zamiennika systemu dołączonego do Atari800.

### Użycie

1. Uruchom `./atari800` (opcjonalnie z plikiem: `./atari800 gra.atr`).
2. Naciśnij `Alt+N` (lub `F1` → *Online Games*).
3. Wybierz literę, potem tytuł, potem plik. Gra zostanie pobrana i uruchomiona.
4. Następnym razem użyj *[Previously downloaded games]*, aby uruchomić ją ponownie, także offline.

Pełną listę opcji i klawiszy emulatora znajdziesz w `README.TXT`, `DOC/` oraz w oryginalnym projekcie.

### Licencja

GPL v2 lub nowsza, tak jak Atari800 (zob. `COPYING`). To nieoficjalny fork, niezwiązany z
serwisem atarionline.pl.
