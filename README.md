# pds_m1

fatto: nome + size + time ultima modifica per hash file

bisogna inviare percorso + checksum e/o dati aggiuntivi

se directory mancante -> entrata in stato di trasmissione intera directory fino a fine trasmissione dir
se checksum errato si esegue una ricerca ricorsiva dell'elemento causa del checksum errato, una volta trovato
si ritrasmette il file o la dircetory (se mancante), se invece è una directory con checksum errato allora si deve
a sua volta esplorare l'interno di essa

fatto: modifica di file

fatto: rimozione di file e dir

meccanismo lato client da definire: rinominazione di file o dir


#######################################################################################
#######################################################################################
#######################################################################################


Descrizione Funzioni Protocollo

-startCommunication:
riceve messaggio START_COMMUNICATION, e nome della directory backup (si considera che non esista altro client che vuole
backuppare una cartella con lo stesso nome), dopodichè il server verifica l'esistenza della suddetta cartella. Se non esiste
il server crea la directory e invia il messaggio NOT_OK (successivi meccanismi di inizializzazione da gestire lato client),
altrimenti invia OK.

-verifyChecksum:
riceve percorso e checksum di un elemento nella direcotry, e verifica prima l'esistenza dell'elemento stesso
(manda MISSING_ELEMENT in caso di elemeno non trovato), dopodichè verifica l'uguaglianza del checksum, ritorna OK se
checksum coincide, NOT_OK altrimenti.

-mkDir:
riceve come messaggio un percorso a directory. La crea e aggiorna l'immagine del filesystem.

-rmvEl:
riceve percorso di elemento da eliminare, se esiste lo elimina e aggiorna l'immagine del filesystem.

-rnmEl:
riceve percorso di elemento da rinominare, se esiste lo rinomina.

-startSendingFile:
funzione che gestisce l'inizio di trasmissione e il trasferimento stesso del file.