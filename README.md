## Debugger di un nucleo multiprogramamto in VSCode


### Contributi

#### Installare le dipendenze:

- Distribuzioni RPM/RHEL  

    ```
    # dnf install node yarn
    ```

- Distribuzioni   
    
    ```
    # apt install node yarn

    ```

#### clone the repo:
```
# git clone https://github.com/ilnerdchuck/VSCode-Nucleo-Debugger

```

#### build the extension:

```
cd VSCode-nucleo-debugger/
yarn install

```
Opzionale: se si vuole crare il package `.vsix` si può installare il comando `vsce` globalmente
```
yarn global add vsce

```

#### contribute to the extension:
Fare una fork della repository e seguire le istruzioni sovrastanti. Una volta sviluppata la propria feature creare una pull request.

### Risorse

#### GDB
- https://sourceware.org/gdb/current/onlinedocs/gdb.html/Python-API.html

#### GDB

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
- https://github.com/SolarTheory/Microsoft-VSCode-Codicons      
- https://code.visualstudio.com/api/working-with-extensions/publishing-extension      
- https://code.visualstudio.com/api/extension-guides/command      
- https://stackoverflow.com/questions/61705879/code-inside-activate-function-of-vs-code-extension-is-not-run   
- https://code.visualstudio.com/api/references/activation-events#onDebug      
#### Misc
- https://www.typescriptlang.org/docs/      
- https://handlebarsjs.com/      