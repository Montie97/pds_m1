# pds_m1

## Comportamento server

Il server si metterà in ascolto sulla porta specificata nel file config.txt, dopodichè gestirà la registrazione
dei nuovi utenti nel file database.txt e/o il loro login, seguito dalle operazioni di sincronizzazione della
cartella specificata dal client.
Il server lavora con una thread pool, con N thread, dove N è specificato nel file config.txt.
Ogni thread si occupa di un solo client.
Nella cartella in cui è presente il file m1_server.exe, devono essere presenti anche il file config.txt e database.txt;
nella quale, inoltre, verranno create le cartelle personali di ogni utente registrato.
Dentro le cartelle personali di ogni utente, sono presenti la/le cartelle root che l'utente ha desiderato backuppare.
Ogni utente può sincronizzare una cartella root per volta.

## Contenuto file config.txt

Prima riga: numero threads nella thread pool
Seconda riga: porta sulla quale il server è in ascolto

## Contenuto file database.txt

Ogni riga contiene un nome utente e la password hashata separati da uno spazio

## Descrizione funzioni

-main:
nel main viene posto il server in ascolto (sulla tcp_port) delle connessioni in entrata, e viene gestita una thread pool.
Il numero di threads nella thread pool e la tcp_port vengono letti nel file config.txt,
come prima riga e seconda riga riga rispettivamente.

-clientHandler:
funzione che controlla la comunicazione socket tra client e server, gestita dal protocollo application-level.

-startCommunication:
[code, versione, username, password_hashed, percorso]
riceve versione, username, password hashata e nome root. Si controlla prima di tutto la versione e, in caso di mismatch, 
viene notificato il client, che a sua volta interromperà la connessione. 
Conseguentemente si controllerà la presenza dell'utente già loggato, e in caso lo si trovasse, si notificherà all'utente 
NOT_OK, che verrà interpretato come errore di autenticazione (login duplicato o wrong password). Infine si controlla
la presenza dell'utente nel database (database.txt), in caso lo si trovi, si controlla la password e, se coincidono,
si logga l'utente e si invia OK al client; altrimenti se la password è sbagliata si invia NOT_OK.

-setNotRemovedFlagsRecursive:
funzione che setta lo stato booleano < check_not_removed_flag > in modo ricorsivo a partire dall'elemento passato
per parametro

-verifyChecksum:
[code, percorso, checksum]
riceve percorso e checksum di un elemento nella direcotry, e verifica prima l'esistenza dell'elemento stesso
(manda MISSING_ELEMENT in caso di elemeno non trovato), dopodichè verifica l'uguaglianza del checksum, ritorna OK se
checksum coincide, NOT_OK altrimenti.

-removeNotFlaggedElements:
funzione ricorsiva che controlla lo stato di < check_not_removed_flag > di ogni elemento e, se risulta false, elimina
l'elemento e eventualmente i sotto elementi.

-mkDir:
[code, percorso]
riceve come messaggio un percorso a directory. La crea e aggiorna l'immagine del filesystem.

-rmvEl:
[code, percorso]
riceve percorso di elemento da eliminare, se esiste lo elimina e aggiorna l'immagine del filesystem.

-rnmEl:
[code, percorso_old, percorso_new]
riceve percorso di elemento da rinominare, se esiste lo rinomina.

-startSendingFile:
[code, percorso, file_size, last_edit]
funzione che gestisce l'inizio di trasmissione e il trasferimento stesso del file.

-ACK:
manda OK come ACK in vari punti a scopo di sincronizzazione col client.

-sendNotOK:
manda NOT_OK come messaggio di errore a seguito di comportamenti scorretti da parte del server che devono
essere comunicati al client.

-build_dir_wrap e build_dir:
funzioni che generano l'immagine (in modo ricorsivo) della cartella locale da sincronizzare.
