# Nuculeo-VSExtension

### Cose interessanti

- referencesViews.ts: `void vscode.commands.executeCommand('setContext', 'cpptools.hasReferencesResults', hasResults);`
-


### Importante

- gli eseguibili in bin/ e le cartelle LLVM/ e debugAdapters/ non vengono fornite dalla microsoft e ogni cambio di release andranno copiate e incollate a mano nella cartella Extensions/ prima di lanciare il comando di build `vsce package`



### Strutturra della tesi

- Introduzione: Cosa vogliamo fare
    - Introduzione a vscode
    - obbiettivo? realizzazione di un estensione per debug del nucleo multiprogrammato

- Approccio:
    - VsCode debug interface
    - DAP e DP: spiegazione di cosa  sono e come funzionano
- Configurazione DAP:
    - launch.json e task.json
- Conclusione
################################################################################



sdasdsa