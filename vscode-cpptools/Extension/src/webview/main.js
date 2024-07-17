// @ts-nocheck
// This script will be run within the webview itself
// It cannot access the main VS Code APIs directly.

(function () {
    document.addEventListener('DOMContentLoaded', (event) => {
        // Toggle Campi Aggiuntivi
        const toggles = document.querySelectorAll('.toggle');
        toggles.forEach((button) => {
            button.addEventListener('click', (event) => {
                button.parentNode.classList.toggle('toggled');
            });
        });
    });
    
    
}());
