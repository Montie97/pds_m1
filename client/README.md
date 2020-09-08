<h1> Client </h1>

<h2> Comportamento del client </h2>

Il client, all'avvio, esegue una procedura che consta di quattro fasi:
-configurazione tramite il file conf.txt (in cui sono specificati username, password, nome della root, path della directory e ip:port del server)
-connessione al server
-autenticazione
-sincronizzazione del contenuto locale con il contenuto del server
Fatto ciò, il client monitora periodicamente la directory specificata nel file conf.txt e segnala al server ogni modifica rilevata.

<h2> Descrizione delle funzioni </h2>

-main: nel main, il client esegue le seguenti operazioni
	-configurazione: vengono acquisiti, tramite un file di configurazione, il nome dell'utente, la password, il percorso della directory da monitorare ed ip e 			porta del server a cui connettersi
	-connessione ed autenticazione: si instaura una connessione socket con il server e si invia a quest'ultimo il nome utente, la password (hashata) e il nome 			della root per effettuare la procedura di autenticazione
	-costruzione dell'immagine: viene creata un'immagine locale della directory da monitorare
	-sincronizzazione: l'immagine creata viene inviata al server
	-monitoraggio directory: si controlla periodicamente la directory e, ad ogni variazione rilevata, si inviano i conseguenti comandi al server in modo da 		aggiornare quest'ultimo

-connectAndAuthenticate: funzione che esegue le operazioni di connessione al server, autenticazione del client e rilevazione di un'eventuale discrepanza tra le 	versioni del protocollo di client e server

-sendAuthenticationData: funzione che invia al server i comandi necessari ad effettuare la procedura di autenticazione

-synchronizeWithServer: wrapper per la funzione ricorsiva synchronizeElWithServer che serve a comunicare al server l'inizio e la fine della procedura di 				sincronizzazione

-synchronizeElWithServer: funzione che percorre ricorsivamente l'albero della directory sincronizzandone il contenuto con il server

-compareOldNewDir: funzione che confronta ricorsivamente l'immagine che il client ha della directory da monitorare e il contenuto attuale della directory ed invia al 	server i comandi appropriati nel caso vengano riscontrate discrepanze (aggiunta/eliminazione/rinomina di elementi)

-checkRenamed: funzione che verifica se due oggetti Directory aventi nomi diversi sono in realtà la stessa Directory ma rinominata

-renamedElement: funzione chiamata nel caso in cui venga rilevata la rinomina di una Directory e che invia al server i comandi necessari a gestire tale rinominazione

-addedElement: funzione chiamata nel caso in cui venga rilevata l'aggiunta di un DirectoryElement e che invia al server i comandi necessari a gestire tale aggiunta 		(distinguendo tra l'aggiunta di un File e l'aggiunta di una Directory)

-removedElement: funzione chiamata nel caso in cui venga rilevata l'eliminazione di un DirectoryElement e che invia al server i comandi necessari a gestire tale 	eliminazione

-sendFile: funzione che gestisce l'invio dei comandi necessari all'invio di un oggetto File al server (il quale viene inviato a pezzi ("chunks"))

-sendDir: funzione che gestisce l'invio dei comandi necessari all'invio di un oggetto Directory, il quale viene inviato ricorsivamente insieme al suo contenuto

-sendModifiedFile: wrapper per la sendFile che distingue, a livello puramente logico, il caso di invio di un oggetto File in seguito ad una modifica di esso

-compareChecksum: funzione che confronta i checksum di due DirectoryElement

-build_dir_wrap e build_dir: funzioni che generano l'immagine (in modo ricorsivo) della cartella locale da sincronizzare

-receiveCodeFromServer: funzione che si mette in ascolto sul socket e attende un codice di risposta da parte del server, il quale viene poi ritornato al chiamante

-errorMessage: funzione che stampa a video dei messaggi di errore relativi alle eccezioni lanciate (e catturate) dal programma

