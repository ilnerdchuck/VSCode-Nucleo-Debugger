// @ts-nocheck
// This script will be run within the webview itself
// It cannot access the main VS Code APIs directly.

(function () {
    document.addEventListener('DOMContentLoaded', (event) => {
        // Toggle Campi Aggiuntivi
        const toggles = document.querySelectorAll('.toggle');
        var icon = document.createElement('i');
        icon.className = 'codicon codicon-chevron-down';
        toggles.forEach((button) => {
            button.classList.add("icon");
            let tmp = button.innerHTML;
            tmp ='<i class="codicon codicon-chevron-down rotate"></i>' + tmp;
            button.innerHTML = tmp;
            button.addEventListener('click', (event) => {
                button.firstChild.classList.toggle('rotate');
                button.parentNode.classList.toggle('toggled');
            });
        });
    });
    
    
}());
