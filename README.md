# pds_m1

## by MONTALTO Lorenzo & MORINA Alessandro


### Logica protocollo

Il funzionamento generale della procedura di sincronizzazione è suddivisa in due fasi:

### Sincronizzazione iniziale 

Si occupa della situazione in cui il server e il client non sanno cosa ha memorizzatu l'uno rispetto all'altro
nell'immagine della cartella da sincronizzare. Il client invia una serie di messaggi attraverso synchronizeElWithServer
in modo tale da capire la struttura che il server ha memorizzato e contemporaneamente inviare i file che, dal lato client,
sono stati aggiornati o aggiunti. Alla fine della "scansione", i file che dal lato server sono stati ignorati dai messagi
di verifica, vengono considerati eliminati (perchè appunto non erano presenti lato client).

### Sincronizzazione a regime

Ogni tot secondi il client esegue una scansione della cartella in locale e, attraverso la compareOldNewDir, riesce a
dedurre le aggiunte, rimozioni, modifice e rinominazioni delle cartelle effettuare, così da mandare dei specifici
messaggi al server in modo da aggiornare la controparte in remoto.

### Risorse esterne

- Libreria boost versione 1.73
- Implementazione SHA1
