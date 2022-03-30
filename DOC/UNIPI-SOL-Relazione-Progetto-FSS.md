# UNIPI SOL: Relazione Progetto FSS
**studente:** Claudio Candelori
**matricola:** 549710
**corso:** A
**anno:** 2022
**repo GitHub:** https://github.com/OneClaudio/OneClaDrive
**report HackMD:** https://hackmd.io/@OneClaudio/B1fJu1-75

## 1. Struttura del codice:
1. **SHARED:**
    * **comm.h:** Codici di operazioni condivise fra server e API
    * **utils.h:** Codice di appoggio contenente funzioni di utilita' usate in tutte le componenti
    * **errcheck.h:** Libreria contenente macros di error checking
2. **FSS:** File Storage Server
    * **server.c:** Il core di tutto il progetto, contiene anche il codice dei thread worker
    * **filestorage.c:** Struttura dati (linked list) che ospita i file in RAM
    * **idlist.c:** Una struttura dati che contiene una coda/lista di identificatori di client, usata sia come coda di id pronti (da manager a workers) sia come lista di id che hanno aperto un file.
3. **API:**
    * **api.c:** Unica interfaccia che l'FSS ha bisogno di esporre per funzionare. Il client sfrutta esclusivamente le funzioni visibili al suo interno per comunicare col server
4. **CLIENT:**
    * **client.c:** Client che, come da specifica, puo' sfruttare i vari cmd line args per chiamare funzioni dell'API e quindi scambiare file con il server.
    * **optqueue.c:** Ospita una coda di opzioni con i relativi argomenti (ancora raw, non parsed), che il client puo' prendere ed eseguire uno ad uno.

## 2. Comunicazione SERVER/API:
```c
typedef struct {
	CmdCode code;
	int info;
	char filename[PATH_MAX];
	} Cmd;

typedef enum CmdCode{ IDLE, CREATE,	OPEN, CLOSE,
                      WRITE, APPEND,READ, READN,
                      LOCK, UNLOCK, REMOVE, QUIT } CmdCode;

#define	O_CREATE 0x1
#define O_LOCK	 0x2
```
Il modo con cui l'API (e quindi i client) comunica quale operazione deve essere eseguita al server e' tramite l'invio di un oggetto di tipo `Cmd`, per le operazioni piu' semplici contiene gia' al suo interno tutte le informazioni sufficienti per eseguirle:
*    `CmdCode code`: Codice (sfruttando il tipo enum) che identifica l'operazione richiesta.
*    `char filename[PATH_MAX]`: Pathname del file su cui si vuole operare. Richiesto da tutte le operazioni (tranne la `readNFiles()`).
*    `int info`: Parametro di informazioni ausiliari usate solo da poche operazioni. E' interpretato in maniera diversa a seconda dell'operazione. Per la `openFile()` conterra' i flag opzionali `O_CREATE, O_LOCK`. Per la `readNFiles()` invece rappresentera' il numero di file da leggere.

In tutto il progetto ho deciso di imporre come limite superiore alla lunghezza dei file `PATH_MAX` che e' definita in `<linux/limits.h>`. Anche se non e' un vero e proprio limite imposto dal sistema operativo, usarla conferisce la flessibilita' di poter scambiare messaggi con il server di dimensione fissata (come appunto `Cmd`).

```c
typedef enum Reply{ OK, ANOTHER, CACHE, NOTFOUND, EXISTS,
                    LOCKED, NOTOPEN, ALROPEN, NOTLOCKED,
                    ALRLOCKED, EMPTY, NOTEMPTY, TOOBIG, FATAL
                    } Reply;
```
Analogamente alla struttura precedente, `Reply` contiene invece tutti i codici che il server puo' inviare al client come risposta. Si tratta pero' solo di errori che riguardano la logica delle operazioni richieste. I possibili errori interni del server (es: errori delle malloc, errori di IO) sono tutti controllati ma sono principalmente errori gravi e distruttivi, ergo, rientrano tutti nel tag `FATAL` e non hanno dei codici dedicati.
*    `OK`: indica che l'operazione richiesta e' stata eseguita con successo, oppure che si puo' procedere al prossimo step (es: invio di file).
*    `ANOTHER`/`CACHE`: usati ogniqualvolta il server debba restituire dei file possibilmente in blocco. Ad esempio la `readNFiles()` comunica al server quanti file vuole leggere, ma non e' detto che siano tutti disponibili, oppure a seguito di una `writeFile()` l'algoritmo di cache potrebbe espellere piu' di un file per far posto al nuovo file da scrivere. In questi casi il server e il client entrano quindi in un loop e, fintanto che il client riceve come risposta uno di questi due codici, e' segno che il client debba continuare a ricevere file.
*    `ALROPEN`/`ALRLOCKED`: Gli unici codici che non sono veri e propri errori e sollevano semplicemente una notifica che indica la ridondanza dell'operazione richiesta. Non sono quindi distruttivi e se ricevuti non segnalano al client un fallimento dell'operazione (restituiscono 0), ma semplicemente stampano un warning.
*    `TUTTI GLI ALTRI`: Questi sono i veri e propri codici di errore, indicano che l'operazione richiesta non e' andata a buon fine e ne indicano il motivo, in genere una logica delle operazioni sbagliata. Per ognuno di questi una funzione di supporto nell'API (`errReply()`) stampa un messaggio di errore e **setta ERRNO** con un codice quanto piu' analogo possibile.

I vari codici delle operazioni sono riutilizzati anche all'interno del file di log, grazie a due funzioni che restituiscono la stringa corrispondente al codice enum.

## 3. ERROR CHECKING:
Trovando molto poco leggibile il paradigma di esecuzione di certe funzioni dentro agli `if` per controllarne il valore di ritorno con conseguente gestione dell'errore, ho scritto una mia personale libreria che sfrutta pesantemente le macro, fortemente ispirata e semplificata da quella presente nel manuale Advanced Unix Programming (Rochkind). A mio parere questo metodo migliora di molto la leggibilita' del codice e gli `if` possono cosi' tornare a fare solamente il proprio "lavoro".

Principalmente una macro (le piu' usate sono `ErrNULL()` e `ErrNEG1( )`) esegue e confronta il valore di ritorno con il codice di fallimento. Se questo e' il caso genera un messaggio di errore, usando sempre lo stesso formato, con tutte le informazioni necessarie per risalirvi, dopodiche' salta ad una sezione di **codice di cleanup**. Il codice di cleanup e' delimitato dalle macro `ErrCLEANUP` ... `ErrCLEAN`, la prima contiene il label a cui saltano tutte le varie macro, inoltre stampa un warning se la zona di cleanup viene raggiunta (per dimenticanza) dal flusso di controllo.
Inoltre usando lo standard `gnu99` e' possibile dichiarare piu' volte la stessa label in blocchi annidati. Senza di questo una label puo' essere usata solo una volta in ogni funzione separata.
Alcune altre macro usate sporadicamente sono `ErrFAIL` e `ErrSHHH` che vanno direttamente al codice di cleanup senza controlli, la prima stampa il messaggio di errore mentre la seconda fallisce silenziosamente.

`-D_GNU_SOURCE`: inoltre permette di avere disponibile la funzione `strdupa()` che e' una `strdup()` che viene deallocata automaticamente all'uscita dello scope.

Alcune funzioni di libreria molto usate o funzioni d logging frequenti/stampa errori hanno una macro che snellisce ulteriormente la lettura e che ingloba al suo interno tutto l'error checking necessario, in genere ha proprio il nome della funzione originale in all caps come ad esempio:
```c
#define LOCK(l) ErrERRNO(  pthread_mutex_lock(l)   );

#define FWRITE( addr, size, n, id) ErrNEG1(  fwrite( addr, size, n, id)  );
```

## 4. SERVER:

### 4.1. Componenti usate:
```c
struct IdList{
    IdNode* first;		//enqId()--> x-[ ]<-[ ]<-[ ]<-[ ]<-[ ] -->deqId()		
    IdNode* last;		//	        ^                   ^	
    pthread_mutex_t mutex;	//	     first		      last
	}
```
**`IdList`:** utilizza un pattern molto usato all'interno del progetto,una singly linked list permette inserimento e estrazione in O(1), ma ricerca solo in O(n). Viene utilizzata all'interno del server come una **coda** usando le funzioni `enqId()` e `deqId()`. Viene utilizzata nella seguente struttura `File` per contenere la lista di clienti che hanno il file aperto, tramite le operazioni `enqId()` e `findRmvId()` per aggiungere/rimuovere un id, `findId()` per sapere se un certo id e' nella lista.
Dovendo distinguere quindi fra varie liste, ognuna di queste funzioni prende come parametro anche la lista su cui dovra' andare ad operare.

```c
struct File{
    char  name[PATH_MAX];
    void*  cont;
    size_t size;
    
    IdList*	openIds;
    int 	lockId;
    
    struct File* prev;
    }

struct Storage{
    File* first;
    File* last;
    
	pthread_rwlock_t lock;
	
	size_t numfiles;
	size_t capacity;
	}
```
**`File`/`Storage`:** Essendomi riuscito a concentrare solo sulla politica FIFO la scelta iniziale e' ricaduta inizialmente sempre su una lista, ed e' quindi piu' adatta all'inserimento di nuovi file e all'espulsione dettata dal cache algorithm (`addNewFile()` e `rmvLastFile()` ) che avvengono in tempo costante, mentre le opzioni che richiedono di operare su un file specifico richiedono tempo lineare: la `getFile()` restituisce un riferimento al file cercato sul quale il server puo' eseguire tutti i controlli e le operazioni necessarie, in piu' e' presente un'operazione (`removeThisFile()`) per la rimozione di un file specifico (ottenuto tramite la getFile). Queste sono le uniche funzioni necessarie al server per operare sullo storage ed, essendo poche e semplici, in futuro possono essere sostituite con strutture dati piu' performanti a seconda dei bisogni.

Essendo lo storage un'unica entita' in tutto il lato server, e' dichiarato globalmente nell'header `filestorage.h` con il nome molto creativo di `Storage storage` e tutte le funzioni e i worker thread operano implicitamente su di esso senza bisogno di doverlo ricevere o passare come parametro ogni volta.
Contiene al suo interno una read/write lock, oltre ai parametri di dimensione e n. file.
L'aggiornamento di questi parametri avviene implicitamente ogni volta che i file vengono aggiunti o rimossi dalle funzioni per operare sullo storage `addNewFile()`, `removeLastFile()`, `removeThisFile()`. Ad eccezione di operazioni che incrementano la dimensione dei file (`WRITE`/`APPEND`) che invece se hanno successo aggiornano sia la dimensione dei file che dello storage.

### 4.2. Manager thread:
Il pattern utilizzato per la realizzazione del server e' quello **Manager-Worker** con **Thread-pool**.
Il thread manager corrisponde al main del server.c.
Ha una fase di avvio che utilizza per installare correttamente i gestori dei segnali, leggere ed inizializzare le opzioni e i vari limiti del server dal file di config, aprire il file di log, aprire la socket, avviare i thread pool e inizializzare le strutture con cui comunichera' con essi.

Dopo la fase di startup il manager entra in un suo ciclo che non verra' mai bloccato se non dall'arrivo dei segnali indicati dalla specifica. Il main si blocca sulla `pselect()` che puo' stare in ascolto dei vari fd E dei segnali e bloccarli automaticamente durante l'esecuzione di una qualsiasi operazione.

Se la `pselect()` viene interrotta da un segnale restituisce 0, non entra quindi nel loop di gestioni delle connessioni e viene semplicemente eseguita la funzione handler corrispondente che cambia lo stato del server e annota sul log l'evento, il server ha una variabile globale `Status` che puo' assumere i valori: `ON`,`SOFT`, `OFF`. Il secondo viene settato da SIGHUP, l'ultimo da SIGINT e SIGQUIT.

Il ciclo principale e' ovviamente dotato di condizioni di guardia che escono dal loop quando richiesto:
Esce se `Status==OFF`, `Status==SOFT && activeCid==0`, inoltre non accetta piu' nuove connessioni se lo stato passa a `Status==SOFT`.

Il manager accetta nuove connessioni e le inserisce nel set di fd controllati (`all`).

Per la comunicazione da manager a workers e' stata utilizzata come gia' accennato la `IdList pending` che e' dichiarata globalmente e viene usata come coda concorrente tramite le funzioni `enqId()` e `deqId()`, che al loro interno acquisiscono una lock (specifica di ogni `IdList`) prima di poter estrarre o inserire un nuovo id. Il manager inserisce gli id pronti con un'operazione da seguire su questa coda, mentre i thread vi si bloccano in automatico ogni volta che e' vuota in attesa di un client per cui soddisfare una richiesta. Allo spegnimento del server vengono inseriti sulla coda dei codici `TERMINATE`, uno per ogni worker creato, che indicano al worker di uscire e terminare cosi' in maniera pulita.

Gli fd vengono rimossi dal set di fd controllati durante l'esecuzione, da parte di un worker, delle richieste del client corrispondente. Il worker esegue esclusivamente **una singola API call** dopodiche' restituisce l'id del client su una named pipe `done`, cosiche' il manager possa reinserirlo nella lista di id ascoltati. Un client che vuole terminare le richieste invia una notifica di disconnessione (un `Cmd` con codice `QUIT`) ed ogni worker dopo aver ricevuto questa intenzione inserisce un codice speciale `DISCONN` nella pipe `done`,  serve al manager che decrementa di conseguenza il numero di connessioni attive `activeCid`.

### 4.3. Worker threads:
Ogni thread, prende un Client ID (`cid`), dopodiche legge da esso il comando che vuole eseguire leggendo `Cmd cmd`, e ottiene dallo storage il file corrspondente sul quale operare (il nome univoco e' in `cmd.filename` e se presente nello storage il file corrispondente viene assegnato ad una variabile `File* f`).
Dopo di che entra in un lungo switch case sul tipo di richiesta ricevuta (`cmd.code`), controlla che il cliente abbia tutte le carte in regola e, se e' il caso, esegue e completa la richiesta, in genere chiudendo con `REPLY(OK)` o con una reply contenente i motivi per cui l'operazione non puo' essere completata.
Se si presenta une errore distruttivo (es: esaurimento memoria malloc) il worker risponde al client con `REPLY(FATAL)`, e subito dopo manda un segnale `SIGINT` al processo stesso per cercare di far spegnere tutto il server nel modo piu' pulito possibile.

### 4.4. Logfile:
Il logging e' effettuato da una macro `LOG()` nel manager, e da una macro `LOGOPT( outcome, file, size )` che prende in automatico informazioni sul ClientID `cid`, sull'operazione in corso `cmd.code`, e stampa l'esito dell'operazione, e se una lettura/scrittura la dimensione in Byte inviati/ricevuti, e ovviamente su quale file.
Di seguito un modello di un possibile logfile:
```
MAIN: CREATED WORKER THREAD #3
MAIN: SERVER READY
MAIN: ACCEPTED NEW CLIENT 8
MAIN: ACCEPTED NEW CLIENT 9
WORK: CID=8   OPEN   OK         		./FILES/clearly.png
WORK: CID=8   LOCK   OK         		./FILES/clearly.png
WORK: CID=9   WRITE  CACHE     127863 B	./FILES/riccio-barbq.jpg
WORK: CID=9   WRITE  CACHE     4192 B	        ./FILES/clinteast.jpg
MAIN: SOFT QUIT SIGNAL RECEIVED -> NO NEW CONNECTIONS ALLOWED
WORK: CID=9   WRITE  CACHE     58806 B	./FILES/absolutelyp.jpeg
WORK: CID=9   WRITE  CACHE     212741 B	./FILES/river-rms.jpg
WORK: CID=9   WRITE  OK        995122 B	./FILES/sand.gif
WORK: CID=6   OPEN   EXISTS     		./FILES/clearly.png
WORK: CID=7   LOCK   NOTFOUND   		./FILES/river-rms.jpg
WORK: CID=8   WRITE  TOOBIG     		./FILES/clearly.png
```

## 5. API:
Le varie funzioni richieste dalla specifica sono state tutte implementate e fanno largo uso di alcune funzioni interne:
* `int firstCMDREPLY()`: Invia al server un elemento di tipo `Cmd` contenente un bundle di informazioni che come gia' spiegato indicano operazione da eseguire, su quale file e un int di informazioni speciali. Subito dopo legge una `Reply` dal server cosi' che puo' capire se puo' continuare o no. Questa funzione e' in genere il primo comando di ogni funzione dell'API.
* `int RECVfile( char* savedir)`: Riceve dal server un file (non si sa quale) quindi ne legge in ordine dimensione, contenuto e pathname. Se la cartella `savedir` e' specificata invoca la funzione successiva sul file appena ricevuto. Usata sia dalla funzionalita' `readNFiles()` sia dalle funzioni che potrebbero ricevere file espulsi dal cache alg. (`openFile()`,`writeFile()`,`appendToFile()`) tramite la funzione `CACHEretrieve()`.
* `int SAVEfile()`: Salva il file specificato nei parametri `cont`,`size`,`pathname` nella cartella specificata, se la cartella non esiste viene creata dalla `mkpath()` che crea tutto il path necessario. E' stato deciso di trattare queste cartelle di "salvataggio" come se fossero dei "cestini". I file vengono stoccati dentro la cartella specificata come parametro. L'univocita' dei file e' mantenuta perche' l'informazione sul loro path univoco salvando viene scritta sul nome dello stesso file salvato.

## 6. CLIENT:
Il client e' stato costruito cercando di aderire quanto piu' possibile alla specifica di progetto, che afferma che un'opzione debba potuta essere ripetuta piu' volte da un client. La scelta e' stata quindi quella di eseguire un parsing iniziale con la funzione `getOpt()`, e trovare tutte le opzioni iniziali (-h, -f, -p). Tutti gli altri comandi vengono inseriti in una coda `optQueue`, e eseguiti successivamente nell'ordine in cui appaiono sulla linea di comando.
Prima di iniziare ad eseguire le azioni elencate, ogni client invoca la `openConnection()` sulla socket specificata dall'opzione -f o su una di dafult, ed installa un handler per il segnale `SIGPIPE`.

I comandi della specifica sono stati interpretati nel seguente modo:
    * **-h, -f, -p:** sono per configurare informazioni iniziali di tutta l'esecuzione del client.
    * **-d, -D, -t:** sono per aggiornare informazioni che hanno la possibilita' di cambiare durante l'esecuzione del client. Ad esempio `./client -t 200 -w ./READ -t 500 -R` scrivera' tutti i file della cartella `./READ` a ritmo di uno ogni 200ms, il secondo -t cambia l'intervallo a 500ms e quindi il client continuera' a leggere i file dal server uno ogni 500ms.
    * **-w, -W:** Non essendo presente in specifica un singolo comando per aprire/chiudere i file, ho deciso di adattarmi a questa indicazione. Le opzioni di scrittura invocano implicitamente una `openFile(..., O_CREATE | O_LOCK, ...)` dopodiche' scrivono il file con la `writeFile()` e successivamente chiudono il file con `closeFile()`, ognuna di queste API call avviene con il ritardo specificato da -t.
    * **-r:** Allo stesso modo -r esegue una `OPEN`>`READ`>`CLOSE` ma stavolta la open avviene senza flag di creazione/lock.
    * **-l, -u:** La -l esegue una `openFile()` e una `lockFile()` che puo' eventualmente bloccarsi in caso di file bloccato da altri client. La -u esegue una `unlockFile()` seguita da una `closeFile()`. Le lock sono indispensabili per eseguire le seguenti operazioni di append -a e rimozione -c, a differenza della -w che assume la lock implicitamente.
    * **-a:** Opzione aggiunta per testare la funzionalita' di append. Necessita di una chiamata -l antecedente sullo stesso file per avere successo. Accetta 2 file, di cui il primo e' l'informazione da appendere e il secondo la destinazione.
    * **-R:** Ho scelto di interpretare l'opzione -R come un grande dump di tutto lo storage, per ottenere i contenuti di tutti i file al suo interno (meno quelli locked da altri client). Quindi sia lato server che lato client la `readNFiles()` non richiede che i file vengano aperti precedentemente, ne invoca una `openFIle()`.
    
Tutte le opzioni vengono eseguite in uno switch case sulla lettera corrispondente all'opzione. Tutte le opzioni che accettano una lista di file separati, inoltre, hanno un ciclo interno allo switch case in cui viene fatto il parsing di ogni file con la funzione di libreria `strtok_r` e per ogni file eseguito il comando corrispondente.

## 7. MAKEFILE/TESTING:
Il progetto ha una struttura:
    **SRC**: Contiene tutti i vari file sorgente divisi per ogni componente, e i file di oggetto creati in una sottocartella di ogni componente in modo che non ingombrino.
    **BIN**: Contiene la cartella degli ASSET sui quali e' stato testato il progetto, e gli eseguibili e i file generati dall'esecuzione come socket e logfile.
    **TEST**: Contiene gli script in bash e le diverse config richiesti dalla specifica.
    **DOC**: Licenze e questa relazione.
    
Le varie cartelle sono dichiarate all'interno del makefile come variabili e quindi facilmente modificabili se si vuole cambiare la struttura delle folders.

Gli script dei test in bash vedono automaticamente il contenuto della cartella degli asset e lo assegnano ad un array che poi viene mischiato con il comando `shuf` in modo che ogni esecuzione risulti almeno parzialmente randomizzata. Questo approccio mi ha permesso di correggere varie dimenticanze che potrei non aver riconosciuto con dei test statici.
Ulteriori file di test possono essere aggiunti direttamente nella cartella FILES e verranno utilizzati dai client.
Sia server che client vengono lanciati in background con i vari timer indicati dalla specifica.

## 8. COMMENTI FINALI:
Avendo avuto problemi con le tempistiche, sono assenti alcune funzionalita' richieste dalla specifica. In particolare: sono assenti le informazioni di recap stampate dal server alla fine dell'esecuzione e il parser del logfile, anche se il logfile e' stato pensato per poter estrapolare semplicemente tutte le informazioni che si vuole.
Inoltre non ho avuto modo di testare il progetto sul sistema operativo fornito dal corso. Ad ogni modo lo sviluppo e' stato portato avanti su una macchina con Linux Mint XFCE che dovrebbe essere molto simile alla VM di XUbuntu che era stata fornita.
Il codice e' ampliamente commentato completamente in inglese, ma manca una specifica dettagliata di tutte le funzioni.
