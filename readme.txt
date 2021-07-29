Basaraba Razvan - 324CC

    Am deschis socketii de TCP si UDP, apoi i-am adaugat in multimea de
citire. Apoi am salvat intr-o multime auxiliara, socketii de unde se primeste
informatie la momentul actual si am avut mai multe cazuri:
1. Daca s-a primit o comnda de la tastatura, verificam daca acesta este "exit"
caz in care trimiteam la toti clientii activi (connected == 1) un mesaj pentru
a stii sa se inchida, golim multimile de citire si inchideam si socketi de UDP si
TCP.
2. Daca am primit o cerere de conexiune de la socketul TCP. Aici receptionam idul 
clientului care tocmai s-a conectat si parcurgeam vectorul de clienti pentru a vedea
daca clientul este conectat caz in care trimitem un mesaj subscriberului pentru a-l
inchide sau a fost conectat.(caz in care restam flagul de connected). Daca clientul
nu exista il adaugam in vectoreul de clienti.
3. Daca am primit un mesaj de la socketul UDP il convertim la o strctura interna udp
si in funtie de type parsam contentul. Apoi, pentru fiecare client din vectorul
de clienti, parcurgem topicuri la care este abonat, iar daca acesta are acelasi
nume avem din nou 2 cazuri: clientul este activ, caz in care ii trimitem mesajul,
sau clientul este inactiv, caz in care adaugam mesajul in vectorul recover al
clientului.
4. Primesc o comanda de la unul din clienti. Daca aceasta este "Disconnect" setam ca
variabila connceted a clientului sa ia valoarea 2, inchidem socketul respectiv si il 
stergem din multimea de citire. Daca primul cuvant este subscribe parsam bufferul
primit si il adaugam la vectorul de topicuri a clientului, iar daca este unsubscribe il
stergem din vectorul de topicuri. 