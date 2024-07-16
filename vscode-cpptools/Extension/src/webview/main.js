// @ts-nocheck
// This script will be run within the webview itself
// It cannot access the main VS Code APIs directly.

(function () {
    document.addEventListener('DOMContentLoaded', (event) => {
        // Toggle Campi Aggiuntivi
        const pCaDumpLists = document.querySelectorAll('.toggle');
        pCaDumpLists.forEach((pCaDumpList) => {
            pCaDumpList.addEventListener('click', (event) => {
                event.target.classList.toggle('toggled');
            });
        });
    });
    
}());
