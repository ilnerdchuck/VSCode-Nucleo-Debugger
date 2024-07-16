// create a webpage and return it 
export function getWebviewContent(boiler: any) {
    return `<!DOCTYPE html>
            <html lang="en">
                <head>
                    <meta charset="UTF-8">
                    <meta name="viewport" content="width=device-width, initial-scale=1.0">
                    <title>Cat Coding</title>
                </head>
                <body>
                        Processi in Esecuzione:</br>
                        <p id="lines-of-code-counter">0</p>
                    

                        //git rebase
                        <script>
                            const counter = document.getElementById('lines-of-code-counter');
    
                            // Handle the message inside the webview
                            window.addEventListener('message', event => {
                                counter.textContent = event.data.command;
                                const message = event.data; // The JSON data our extension sent
    
                            });
                        </script>
                </body>
            </html>`;
}

// execute custom command 
// export async function executeCustomCommand(session: typeof vscode.debug.activeDebugSession, command: string, arg?: any){
//     if(session === undefined){
//         console.log("session not valid or undefined");
//         return;
//     }
//     const sTrace = await session.customRequest('stackTrace', { threadId: 1 });
//     const frameId = sTrace.stackFrames[0].id;

//     // build and exec the command, in this case info registers 
//     const text = '-exec ' + command;
//     // const arg : DebugProtocol.EvaluateArguments = {expression: text, frameId: frameId, context:'hover'};
//     session.customRequest('evaluate', {expression: text, frameId: frameId, context:'hover'}).then((response) => {
//         console.log("------------------EVAL----------------");
//         console.log(response.result);
//         console.log("----------------END EVAL--------------");
//         return response.result;
//     });
// }
