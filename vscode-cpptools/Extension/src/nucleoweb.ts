import * as vscode from 'vscode';
const Handlebars = require('handlebars');

let interval: NodeJS.Timeout;

export class NucleoInfo {
    public static currentPanel: NucleoInfo | undefined;

	public static readonly viewType = 'nucleoInfo';

	public process_list: any | undefined;
	private readonly _extensionUri: vscode.Uri;
	// delare you new variable
	// public VAR: any | undefined;

    private readonly _panel: vscode.WebviewPanel;
	private _disposables: vscode.Disposable[] = [];
    private constructor(panel: vscode.WebviewPanel, extensionUri: vscode.Uri) {
		this._panel = panel;
		this._extensionUri = extensionUri;
		// Set the webview's initial html content
		this._update();

		// Listen for when the panel is disposed
		// This happens when the user closes the panel or when the panel is closed programmatically
		this._panel.onDidDispose(() => this.dispose(), null, this._disposables);

		// Update the content based on view changes (moved around the window)
		// this._panel.onDidChangeViewState(
		// 	e => {
		// 		if (this._panel.visible) {
		// 			this._update();
		// 		}
		// 	},
		// 	null,
		// 	this._disposables
		// );

        // This can be useful if you need to handle input from the webview
		// Handle messages from the webview
		// this._panel.webview.onDidReceiveMessage(
		// 	message => {
		// 		switch (message.command) {
		// 			case 'alert':
		// 				vscode.window.showErrorMessage(message.text);
		// 				return;
		// 		}
		// 	},
		// 	null,
		// 	this._disposables
		// );


        const session = vscode.debug.activeDebugSession;
		const updateInfo = async () => {
			this.process_list = await this.customCommand(session, "process list");
			// Insert you command
			// this.VAR = this.customCommand(session, "COMMAND");

			const infoPanel = this._panel.webview;
			

			infoPanel.html = this._getHtmlForWebview();
		};

        interval = setInterval(updateInfo, 500);
	}

    public dispose() {
		// Clean up our resources
		NucleoInfo.currentPanel = undefined;
        clearInterval(interval);
		this._panel.dispose();
	}

    // Update the webview
    private _update() {
		// const infoPanel = this._panel.webview;
        // infoPanel.html = this._getHtmlForWebview();
	}


    public static createInfoPanel(extensionUri: vscode.Uri) {
		// Otherwise, create a new panel.
		const panel = vscode.window.createWebviewPanel(
			NucleoInfo.viewType,
			'Info Nucleo',
			vscode.ViewColumn.Beside,
			getWebviewOptions(extensionUri),
		);
		NucleoInfo.currentPanel = new NucleoInfo(panel, extensionUri);		
	}

    // execute custom command 
    private async customCommand(session: typeof vscode.debug.activeDebugSession, command: string, arg?: any){
		if(session) {
			const sTrace = await session.customRequest('stackTrace', { threadId: 1 });
			if(sTrace.stackFrames[0] === undefined){
				return;
			}
			const frameId = sTrace.stackFrames[0].id;
		
			// Build and exec the command
			const text = '-exec ' + command;
			let result = session.customRequest('evaluate', {expression: text, frameId: frameId, context:'hover'}).then((response) => {
				return response.result;
			});
			return result
		}     
    }

	private formatProcessList(){
		let processListJson = JSON.parse(this.process_list);
		
		let source = `
			<div>
				<h3 id="palle">Processi in esecuzione:</h3>
				{{#each process}}
					{{#each this}}
						<p>{{@key}}: {{this}}</p>
					{{/each}}
				{{/each}}
			</div>
		`;
		
		let template = Handlebars.compile(source);
		return template(processListJson);
	}

    private _getHtmlForWebview() {
		const scriptPathOnDisk = vscode.Uri.joinPath(this._extensionUri, 'src/webview', 'main.js');

		// And the uri we use to load this script in the webview
		const scriptUri = this._panel.webview.asWebviewUri(scriptPathOnDisk);

		
		// Local path to css styles
		const styleResetPath = vscode.Uri.joinPath(this._extensionUri, 'src/webview', 'reset.css');
		const stylesPathMainPath = vscode.Uri.joinPath(this._extensionUri, 'src/webview', 'vscode.css');

		// Uri to load styles into webview
		const stylesResetUri = this._panel.webview.asWebviewUri(styleResetPath);
		const stylesMainUri = this._panel.webview.asWebviewUri(stylesPathMainPath);

		console.log("uwu");
		console.log(scriptUri);
		let sourceDocument = `
		<!DOCTYPE html>
			<html lang="en">
				<head>
					<meta charset="UTF-8">
					<meta name="viewport" content="width=device-width, initial-scale=1.0">
						<link href="${stylesResetUri}" rel="stylesheet">
						<link href="${stylesMainUri}" rel="stylesheet">
					<title>Info Nucleo</title>
				</head>
				<body>
					{{{processList}}}

				<script src="${scriptUri}"></script>
				</body>
			</html>
		`;

		let template = Handlebars.compile(sourceDocument);
		return template({ processList: this.formatProcessList() });
	}
}



function getWebviewOptions(extensionUri: vscode.Uri): vscode.WebviewOptions {
	return {
		// Enable javascript in the webview
		enableScripts: true,
	};
}
