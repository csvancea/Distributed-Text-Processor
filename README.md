# Distributed Text Processor


Structura si implementare:
--------------------------

- Codul este impartit in 3 parti majore:
  1. Master
  2. Worker
  3. SimpleThreadPool


- La pornirea procesului, se interogheaza rank-ul acestuia:
  - Procesul cu rank 0 devine Master si instantiaza un obiect de tip Master
  - Procesele cu rank 1..4 devin Workeri si fiecare instantiaza un obiect 
  de tip *Worker (aici depinde de tipul de worker)

- Master-ul are urmatoarele roluri:
  - La instantiere acesta isi creeaza 4 thread-uri, cate unul pentru fiecare
  nod worker.
  - Thread-urile nou create au fiecare acelasi rol:
    1. Citesc fisierul de intrare cautand paragrafele asociate lor
    2. Cand gasesc un paragraf destinat lor, il trimit in intregime catre nodul
    worker asociat
    3. La finalul procesarii fisierului de intrare, thread-urile vor astepta
    datele procesate de la workeri
  - Dupa ce fiecare thread isi incheie executia, Master-ul genereaza fisierul
  de iesire si isi incheie activitatea

- Worker-ul are urmatoarele roluri:
  - La instantiere isi creeaza 1 thread aditional(?) care executa urmatoarele:
    - Instantiaza un SimpleThreadPool cu P-1 thread-uri
    - Asteapta paragrafe de la nodul Master
    - Fiecare paragraf primit se imparte in job-uri de cate 20 de linii care se
    trimit spre executie la SimpleThreadPool
      - Optimizare: daca exista thread-uri libere in ThreadPool, acestea pot
      procesa alte paragrafe sosite de la Master
    - Cand nu mai exista paragrafe de primit, worker-ul asteapta ca ThreadPool-ul
    sa isi incheie toate job-urile si ii da ShutDown
    - Se trimit paragrafele procesate inapoi la Master si se inchide procesul.

- SimpleThreadPool este o implementare naiva a unui Thread Pool:
  - Un job este reprezentat de o functie.
  - La Start se spawneaza N thread-uri in pool (N = argument)
  - Exista metoda AddJob care primeste ca parametru o functie care trebuie
  executata pe un thread din pool.
    - La momentul apelarii se da lock pe un mutex global, se adauga job-ul
    in coada, se notifica workerii si se da unlock.
    - Mutex-ul are rolul de a limita accesul la coada la maximum 1 thread
    concomitent.
  - Fiecare thread din pool asteapta folosind un conditional variable pana
  cand este notificat ca exista un nou job (doar un thread este "trezit")
  - Acest thread nou trezit da lock pe mutex-ul global, extrage din coada
  job-ul, da unlock si executa job-ul.
  - Trimiterea inapoi a rezultatului se face prin efect lateral (codul
  executat de job este responsabil sa isi puna rezultatul la o locatie
  stabilita in prealabil)
  - Am implementat metoda WaitForJobsToComplete care blocheaza pana cand
  coada de job-uri se goleste.
    - Asteptarea se face cu ajutorul altei conditional variable.
    - Cand un thread din pool termina de executat un job, aceasta notifica
    thread-ul care asteapta. Daca coada este goala, metoda returneaza,
    altfel intra din nou in wait.
  - Metoda ShutDown se apeleaza la final. Aceasta are rolul de a da join
  la thread-uri.

  - Implementarea considera ca metodele de administrare ale thread pool-ului
  sunt apelate mereu din acelasi thread (pot aparea probleme daca 2 thread-uri
  diferite apeleaza ShutDown spre exemplu).


Protocol de comunicatie intre noduri:
-------------------------------------

- Nodurile trimit intre ele date astfel:
  1. ID-ul paragrafului (fiecare paragraf are asociat un ID global
  in functie de pozitia sa in fisierul de intrare; aceste ID-uri
  sunt la fel la nivelul fiecarui worker)
  2. Lungimea intregului paragraf
  3. Paragraful efectiv.
  - Cand nu mai exista paragrafe, se semnaleaza trimitandu-se un paragraf
  cu ID-ul negativ (invalid)

- Nodul Master isi incepe executia ca Sender, iar workerii ca Receivers.
- Dupa ce Master termina toate paragrafele de trimis, se inverseaza rolurile.


Mecanisme de sincronizare intre thread-uri:
-------------------------------------------

- Aplicatia a fost dezvoltata astfel incat sa nu necesite (prea multa) sincronizare
intre thread-uri.

- Thread-urile nodului Master:
  - La citirea si trimiterea datelor nu este nevoie de sincronizare
  - La finalul parsarii fisierului de intrare, trebuie alocat un buffer in care
  se vor receptiona toate paragrafele.
  - Acest buffer este comun intre toate thread-urile, deci trebuie sa existe
  o sincronizare. Aceasta este facuta printr-o variabila atomic, astfel incat
  primul thread care termina de citit toate paragrafele (si implicit stie
  cate paragrafe sunt in tot fisierul) este cel care aloca buffer-ul.
  - La receptionarea datelor nu mai este nevoie de sincronizare deoarece
  fiecare thread receptioneaza paragrafe cu ID-uri diferite si vor fi scrise
  in buffer la locatii diferite intre ele.

- Thread-urile nodului Worker:
  - Sincronizarea se face aici doar intre thread-ul de Receive/Send si
  thread-urile din job pool.
  - Mecanismele folosite pentru sincronizare au fost explicate anterior la
  detalierea thread pool-ului.


Scalabilitate:
--------------

- Aplicatia a fost conceputa sa scaleze pe sisteme cu mai multe core-uri.
- Cu cat sunt mai multe core-uri, cu atat workerii pot procesa in paralel
mai multe linii sau chiar paragrafe (implementarea folosind un thread pool
imi permite sa procesez in paralel linii din paragrafe diferite).
- Cum am precizat mai sus, existenta a putine puncte de sincronizare in
program creste eficacitatea intrucat exista foarte putini timpi "liberi"
in care un thread trebuie sa astepte alt thread.
