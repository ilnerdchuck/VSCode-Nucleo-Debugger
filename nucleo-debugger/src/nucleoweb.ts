import * as vscode from 'vscode';
const Handlebars = require('handlebars');

let interval: NodeJS.Timeout;

export class NucleoInfo {
    public static currentPanel: NucleoInfo | undefined;

	public static readonly viewType = 'nucleoInfo';

	private readonly _extensionUri: vscode.Uri;
	
	public process_list: any | undefined;
	// delare your new GDB response variable
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
			
			// Insert your command
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
			if(sTrace === undefined){
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

	// Handles the HTML formatting for the process_list command
	private formatProcessList(){
		let processListJson = JSON.parse(this.process_list);
		let proc_count = processListJson.process.length;
		let proc_sys: any = [];
		let proc_utn: any = [];
		processListJson.process.forEach(element => {
			if(element.livello == "sistema"){
				proc_sys.push(element);
			}else{
				proc_utn.push(element);
			}
		});
		if(proc_count ==0){
			return `<h3>PROCESSI IN ESECUZIONE<span class="info">: ${proc_count}</span></h3>`
		}
		let source = `
		<div class="">
			<h3 class="toggle">PROCESSI IN ESECUZIONE<span class="info">: ${proc_count}</span></h3>
			<div class="toggable">
				<div class="">
					<h3 class="p-title toggle"><span class="key">sistema</span><span class="info">: ${proc_sys.length}</span></h3>
					<div class="toggable">
						{{#each proc_sys}}
							<div class="">
								<h3 class="p-title toggle"><span class="key">[{{pid}}]</span><span class="info">: object</span></h3>
								<ul class="p-dump toggable">
									<li class="p-item"><span class="key"> pid = </span> <span class="value">{{pid}}</span></li>			
									<li class="p-item"><span class="key"> livello = </span> <span class="value">{{livello}}</span></li>			
									<li class="p-item"><span class="key"> corpo = </span> <span class="value">{{corpo}}</span></li>			
									<li class="p-item"><span class="key"> rip = </span> <span class="value">{{rip}}</span></li>
									<li class="p-ca-dump-list" >
										<div class="toggle"><span class="key">campi aggiuntivi</span><span class="info">: array[]</span></div> 
										<ul class="toggable">
											{{#each campi_aggiuntivi}}
												<li class="p-dmp-item"> <span class="key">{{@key}} =</span> <span class="value">{{this}}</span></li>
											{{/each}}
										</ul>
									</li>
									<li class="p-dump-list "> 
										<div class="toggle"><span class="key">dump Pila</span><span class="info">: array[]</span></div> 
										<ul class="toggable">
											{{#each pila_dmp}}
												<li class="p-dmp-item"> <span class="key">{{@key}} =</span> <span class="value">{{this}}</span></li>
											{{/each}}
										</ul>
									</li>
									<li class="p-dump-list"> 
										<div class="toggle"><span class="key">dump registri</span><span class="info">: array[]</span></div> 
										<ul class="toggable">
											{{#each reg_dmp}}
												<li class="p-dmp-item"> <span class="key">{{@key}} =</span> <span class="value">{{this}}</span></li>
											{{/each}}
										</ul>
									</li>
								</ul>
							</div>
						{{/each}}
					</div>
				</div>
				<div class="">
					<h3 class="p-title toggle"><span class="key">utente</span><span class="info">: ${proc_utn.length}</span></h3>
					<div class="toggable">
						{{#each proc_utn}}
							<div class="">
								<h3 class="p-title toggle"><span class="key">[{{pid}}]</span><span class="info">: object</span></h3>
								<ul class="p-dump toggable">
									<li class="p-item"><span class="key"> pid = </span> <span class="value">{{pid}}</span></li>			
									<li class="p-item"><span class="key"> livello = </span> <span class="value">{{livello}}</span></li>			
									<li class="p-item"><span class="key"> corpo = </span> <span class="value">{{corpo}}</span></li>			
									<li class="p-item"><span class="key"> rip = </span> <span class="value">{{rip}}</span></li>
									<li class="p-ca-dump-list" >
										<div class="toggle"><span class="key">campi aggiuntivi</span><span class="info">: array[]</span></div> 
										<ul class="toggable">
											{{#each campi_aggiuntivi}}
												<li class="p-dmp-item"> <span class="key">{{@key}} =</span> <span class="value">{{this}}</span></li>
											{{/each}}
										</ul>
									</li>
									<li class="p-dump-list "> 
										<div class="toggle"><span class="key">dump pila</span><span class="info">: array[]</span></div> 
										<ul class="toggable">
											{{#each pila_dmp}}
												<li class="p-dmp-item"> <span class="key">{{@key}} =</span> <span class="value">{{this}}</span></li>
											{{/each}}
										</ul>
									</li>
									<li class="p-dump-list"> 
										<div class="toggle"><span class="key">dump registri</span><span class="info">: array[]</span></div> 
										<ul class="toggable">
											{{#each reg_dmp}}
												<li class="p-dmp-item"> <span class="key">{{@key}} =</span> <span class="value">{{this}}</span></li>
											{{/each}}
										</ul>
									</li>
								</ul>
							</div>
						{{/each}}
					</div>
				</div>
			</div>
		</div>
		`;

		let template = Handlebars.compile(source);
		return template({proc_sys: proc_sys, proc_utn: proc_utn,});
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
		const codiconsUri = this._panel.webview.asWebviewUri(vscode.Uri.joinPath(this._extensionUri, 'node_modules', '@vscode/codicons', 'dist', 'codicon.css'));
		let sourceDocument = `
		<!DOCTYPE html>
			<html lang="en">
				<head>
					<meta charset="UTF-8">
					<meta name="viewport" content="width=device-width, initial-scale=1.0">
						<link href="${stylesResetUri}" rel="stylesheet">
						<link href="${stylesMainUri}" rel="stylesheet">
						<link href="${codiconsUri}" rel="stylesheet" />
					<title>Info Nucleo</title>
				</head>
				<body>
					{{{processList}}}
					
					//{{varNAME}}

				<script src="${scriptUri}"></script>
				</body>
			</html>
		`;

		let template = Handlebars.compile(sourceDocument);
		
		return template({ processList: this.formatProcessList(), varNAME: this.YOUR_FORMATTER() });
	}
}



function getWebviewOptions(extensionUri: vscode.Uri): vscode.WebviewOptions {
	return {
		// Enable javascript in the webview
		enableScripts: true,
		localResourceRoots: [vscode.Uri.joinPath(extensionUri, '/')]
	};
}
