## Debugger di un nucleo multiprogramamto in VSCode

### Dipendenze:
#### Installare il Nucleo:
Integro le istruzioni presenti sul sito del Professor Lettieri per installare l'ambiente per lo sviluppo del Nucleo:

#### Installazione della `libce`:
Installare il supporto per la compilazione a 32 bit:
- RPM/RHEL:
    ```
    # dnf update
    # dnf group install "C Development Tools and Libraries" "Development Tools"
    ```
- Ubuntu/Debian: 
    ```
	# apt update
	# apt install build-essential gcc-multilib g++-multilib
    ```
Aprire un emulatore di terminale e scaricare i sorgenti della libreria:
```
wget https://calcolatori.iet.unipi.it/resources/libce-3.0.5.tar.gz
```
Scompattarli:
```
tar xvzf libce-3.0.5.tar.gz
```

Compilare e installare la libreria:
```
cd libce-3.0.5
make
make install
```

Questo comando installa anche gli script compile, boot e debug. Per usarli comodamente, però, è necessario aggiungere la directory $HOME/CE/bin alla propria variable PATH. Per esempio, in un sistema Debian/Ubuntu con shell di default (bash), si può eseguire (una sola volta) questo comando:
```
echo 'PATH=$HOME/CE/bin:$PATH' >> ~/.bashrc
```
oppure in caso si usi zsh:
```
echo 'PATH=$HOME/CE/bin:$PATH' >> ~/.zshrc
```

In questo modo il comando che modifica PATH viene aggiunto in fondo al file .bashrc nella propria home, e verrà eseguito dalla shell ogni volta che aprite un nuovo terminale.


#### QEMU

Prima di tutto si devono installare gli strumenti per la compilazione, insieme ad alcune librerie usate da QEMU. Apprire un emulatore di terminale e scrivete i seguenti comandi:
- RPM/RHEL:
```
# dnf update
# dnf group install "C Development Tools and Libraries" "Development Tools"
# dnf install python3 zlib-devel SDL2-devel pixman-devel perl-JSON-XS ninja-build ncurses-devel python3-pip
```

- Ubuntu/Debian:
```
# apt update
# apt install build-essential python3 zlib1g-dev libsdl2-dev libpixman-1-dev libjson-xs-perl ninja-build ncurses-dev python3-pip
```

(Nota: solo i comandi precedenti sono specifici di Debian, Ubuntu, etc. Chi vuole usare un'altra distribuzione deve installare le librerie zlib, SDL2, pixman e ncurses, oltre a make, gcc, python3 e tutto il necessario per compilare. Fatto questo, i comandi successivi si applicano a qualunque distribuzione).

I comandi seguenti devono essere eseguiti come utente semplice, non come amministratore.

Scaricate i sorgenti:

	wget https://calcolatori.iet.unipi.it/resources/qemu-ce-8.2.1-1.tar.gz

Scompattateli:

	tar xvf qemu-ce-8.2.1-1.tar.gz

Compilate e installate QEMU:

	cd qemu-ce-8.2.1-1
	./install

Il comando ./install provvederà a scaricare la versione standard di QEMU, quindi applicherà le modifiche necessarie per il corso, infine compilerà e installerà la versione modificata.

Attenzione: Se si usa la distribuzione standard di QEMU (fornita per esempio dalla vostra distribuzione linux) non sarà possibile far funzionare gli esercizi d'esame sulle periferiche di I/O, l'esempio sul bus mastering funzionerà in modo diverso, e il comando apic nel debugger non sarà disponibile; tutto il resto dovrebbe funzionare senza problemi. 


### Installazione e sviluppo dell'estensione
#### Installare le dipendenze:

- Distribuzioni RPM/RHEL  

    ```
    # dnf install node yarn
    ```

- Distribuzioni   
    
    ```
    # apt install node yarn
    ```

#### Copiare la repository:
```
# git clone https://github.com/ilnerdchuck/VSCode-Nucleo-Debugger
```

#### Installare le dipendenze dell'estensione:

```
cd VSCode-nucleo-debugger/
yarn install
```
Opzionale: se si vuole crare il package `.vsix` si può installare il comando `vsce` globalmente:
```
yarn global add vsce
```
#### Avviare l'estensione:
! Disistallare l'estensione da VSCode prima di avviarla.

Aprire l'ambiente di sviluppo su VSCode
```
cd VSCode-nucleo-debugger/nucleo-debugger
code .
```
Avviare il debug premendo il tasto `F5` o dal menu dedicato di VSCode. Verrá aperta una nuova finestra di VSCode, a questo punto aprire una cartella contenete una versione del nucleo e avviare la sessione di debug.

### Contribuire all'estensione:
Fare una fork della repository e seguire le istruzioni sovrastanti. Una volta sviluppata la propria feature creare una pull request.

### Risorse

#### GDB
- https://sourceware.org/gdb/current/onlinedocs/gdb.html/Python-API.html

#### DAP e DP

- https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Evaluate      
- https://microsoft.github.io/debug-adapter-protocol/overview      
- https://github.com/microsoft/VSDebugAdapterHost/tree/main      

#### VSCode API

- https://code.visualstudio.com/api/get-started/your-first-extension      
- https://github.com/microsoft/vscode-extension-samples?tab=readme-ov-file      
- https://vscode-api.js.org      
- https://code.visualstudio.com/api/      
- https://code.visualstudio.com/Docs/editor/tasks
- https://code.visualstudio.com/api/extension-guides/webview
- https://code.visualstudio.com/Docs/editor/debugging      
- https://code.visualstudio.com/api/extension-guides/debugger-extension      
- https://code.visualstudio.com/api/working-with-extensions/publishing-extension      
- https://code.visualstudio.com/api/extension-guides/command      
- https://code.visualstudio.com/api/references/activation-events#onDebug      
- https://stackoverflow.com/questions/61705879/code-inside-activate-function-of-vs-code-extension-is-not-run   
#### Misc
- https://github.com/SolarTheory/Microsoft-VSCode-Codicons      
- https://www.typescriptlang.org/docs/      
- https://handlebarsjs.com/      