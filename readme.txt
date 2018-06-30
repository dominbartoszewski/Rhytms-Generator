W paczce znajdują się wszystkie pliki, których używałem do realizacji drugiego
projektu.

* katalog nationalepic w którym znajdują się poszczególne księgi
* plik noticeboard pełniący rolę tablicy ogłoszeniowej
	- gdy plik nie istnieje to serwer sam go tworzy

* pliki źródłowe client.c i server.c 
* binarki client.out i server.out

Ważne uwagi:

* Pliki z księgami nazywają się 1 2 3 4 5 6 7 8 9 10 11 12 . Bez txt - aby sobie ułatwić sprawę z alokacją poszczególnych wierszy dla tablicy dwuwymiarowej.

* Interwał zawiera tylko 8 bitów - wg specyfikacji zadania. Nie byłem w stanie przez 
to przeskoczyć problemu, aby przerwy między wysłaniem kolejnych fragmentów były dłuższe niż (1/64)*255 sekundy.

====

Serwer po włączeniu wczytuje księgi do dynamicznej tablicy.
Sposób przechowywania ksiąg:
	* każdy rozdział trzymany jest w jednym wierszu (indeks wiersza + 1 -> numer księgi)
	* dodatkowo używam dynamicznej tablicy przechowującej długość każdego rozdziału, aby przypadkiem nie wysłać czegoś spoza zakresu tablicy z tekstem księgi

Uzasadnienie:
	*podczas wysyłania łatwo jest dzielić tekst na linie, wiersze i litery 
	w zależności od życzenia klienta

* Używam funkcji generującej 32 znakowe randomowe adresy socketów. Domena abstrakcyjna oczywiście( 1 znak jest nullem);

Maksymalna długość tekstu wysyłanego do socketów to 256 znaków.

Przyznaję - w celu zapewnienia wysyłania dla wielu klientów użyłem fork();

====

Jak ominąłem problem z polskimi znakami? Skryptem zamieniającym w księdze np ą na a.

====

Przykładowy sposób włączenia serwera i klienta:

./server.out -k nationalepic -p noticeboard
./client.out -r 5 -f s -p  noticeboard -o 30 -x 2 -s 3575

Kompilowałem używając gcc:

gcc -Wall client.c -o client.out
gcc -Wall server.c -o server.out -lrt

====
