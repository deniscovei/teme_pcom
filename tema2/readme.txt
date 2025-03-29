Autor: Covei Denis

In server sunt deschisi 3 socketi:
- unul de tcp, pe care primesc requesturi de conectare si deconectare a subscriberilor;
- unul de udp, pe care se primesc mesaje tip udp;
- unul pe care se primesc comenzi de la stdin.

Pentru multiplexare, am folosit poll.

De fiecare data cand se conecteaza un nou subscriber, acesta este adaugat in array-ul de file descriptori ai serverului. Subscriberul va trimite un mesaj catre server prin care isi anunta id-ul cu care se conecteaza. Acesta este adaugat intr-un hash_set de utilizatori online.
Se vor primi trei tipuri de mesaje din partea subscriberilor catre server:
- subscribe: topicul la care s-a abonat clientul este adaugat intr-un hash_map care asociaza un subscriber cu un hash_set de topicuri;
- unsubscribe: topicul este scos din hash_map;
- exit: clientul se deconecteaza, iar serverul il scoate din hash_set-ul de utilizatori online.

Mesajele udp primite de server se concateneaza intr-un header ce contine ip-ul si port-ul pe care au fost trimise. Se analizeaza topicul primit si se face match cu topicele la care subscriberii sunt abonati. Pentru wildcards, am impartit topic-ul primit pe udp in tokeni separati de caracterul '/', am procedat asemenea pentru pattern-urile primite de la clienti, iar match-ul l-am realizat folosind programare dinamica.

In momentul in care se primeste comanda "exit" de catre server, clientii sunt anuntati sa inchida conexiunea, iar serverul inchide, la randul sau, toti socketii deschisi (tcp, udp si clienti).

Incadrarea mesajelor a fost facuta in felul urmator: pentru fiecare mesaj trimis, se da send cu dimensiunea sa, apoi cu mesajul efectiv. Pentru situatia in care nu s-a realizat trimiterea completa a pachetelor, se da send pana se atinge dimensiunea dorita. La primire, se realizeaza procedeul invers, se primeste dimensiunea pachetului, apoi pachetul efectiv, progresiv, pana la atingerea dimensiunii date.
