# Virtual-Memory-Allocator

Functii skel

Functia alloc_arena

Asiguram existenta unui buffer de memorie virtual prin crearea listei de
blocuri, actualizarea marimii arenei si alocarea dinamica "fictiva" a acesteia,
deoarece nu se utilizeaza size-ul dat ca parametru. Arena reprezinta zona
totala de memorie pe care o putem utiliza in program.

Functia dealloc_arena

Se efectueaza parcurgerea listei de blocuri. Dezalocam ficare nod din lista de
miniblocuri, iar avand grija sa dezalocam corect nodul conform structurii
predefinite pentru a nu avea memory leaks. In acest sens, nu utilizez functia
dll_free, deoarece ar efectua o dezalocare incompleta. Am definit functiile
purge_miniblock si purge_block pentru a elimina reziduurile ramase in urma
functiei ce elimina al n-lea nod din lista. (pentru usurinta eliminam mereu
headul listei). La final putem da free listei de blocuri si arenei

Functia alloc_block

In cazul in care adresa si size-ul date respecta dimensiunile arenei si nu se
intersecteaza cu zone deja alocate, putem aloca un nou miniblock. In cazul in
care acesta nu apartine niciunui bloc din lista, este necesar sa creeam unul la
adresa respectiva, de marime size, in care sa fie inclus noul miniblock. In
cazul in care am adaugat un bloc are o granita comuna cu unul dintre
blocurile deja existente, zona de memorie se realoca, adaugand in lista un nou
block ce contine in lista de miniblocuri cele 2 blockuri cu granita comuna.
Dimensiunea blockului este data de suma dimensiunilor miniblocurilor.

Functia free_block

In cazul in care lista de blockuri este nevida si adresa data este valida (luand
in considerare si size), vom elimina miniblocul identificat. Ulterior, este
necesara verificarea continuitatii listei de miniblocuri, pentru ca in cazul in
care miniblocul cerut nu este head sau tail al listei, fragmenteaza lista de
miniblocuri in 2 blocuri distincte. Blocurile contin mkniblocurile ramase in
aceeasi ordine, de aceea este necesar sa efectuam corect parcurgerile atunci
cand eliminam fiecare minibloc si il adaugam in lista noului bloc de care
apartine. Ne asiguram ca stergerea elementelor din miniblock_list este completa
si ca noile blocuri au granitele corecte: pentru primul bloc, de la head-ul
listei anterioare de miniblockuri pana la adresa de start a miniblocul
eliminat, iar pentru al doilea bloc, de la finalul miniblocului targetat
(start_address + size), pana la tail-ul listei de miniblockuri.

Functia read

Pentru o lista nevida,ne asiguram ca informatia este citita din acelasi bloc
si in caz afirmativ parsam miniblockurile pana cand size-ul dat este citit.
Luam in calcul situatia in care adresa data se afla in interiorul unui
miniblock, pentru a putea calcula dimensiunea citita din acesta, iar ulterior
putem citi miniblockuri complete pana cand size este 0 sau a devenit mai mic
decat miniblock->size, caz in care citim doar ce ramane necesar, nu intregul
miniblock. Pentru size ce depaseste dimensiunea miniblockului, dar adresa de
start este alocata, trunchiem size pentru a citi doar pana la finalul blocului.
Utilizam fwrite pentru a afisa la ecran numarul dorit de elemente dupa spatiul
alocat fiecarui miniblock.

Functia write

Conditiile de validare ale operatiei coincid cu cele enumerate pentru functia
read, iar parsarea miniblocurilor se face in acelasi mod. Trunchierea
dimensiunii este de asemenea valibila. Utilizam memcpy pentru a copia in
buffer-ul miniblocului data in mod corect. Contorul sum tine locul unui cursor
ce parseaza data pentru a nu scrie repetitiv / a omite secvente din data.

Functia pmap

Efectuam parsarea listelor imbricate, pentru fiecare bloc afisand granitele
acestuia si lista de miniblocuri corespunzatoare. Calculam memoria libera prin
insumarea dimensiunilor blocurilor din lista si efectuarea diferentei dintre
dimensiunea arenei (memoria totala) si rezultatul obtinut anterior. Caracterul
scocat in campul perm al unui miniblock reprezinta codificarea binara a
fiecarei permisiuni

Functia mprotect (bonus)

Odata ce am identificat permisiunile asignate zonei de memorie targetate,
verificam daca aceasta reprezinta startul unui miniblock pentru a putea
modifica permisiunile in maniera ceruta. Aceasta operatie poate limita
functionalitatea functilor read si write, de aceea este necesar sa
verificam inainte de a parsa miniblockurile ca intreaga zona de
memorie ceruta sa poata fi citita / scrisa, respectiv ambele.
