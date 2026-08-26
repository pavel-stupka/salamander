# Viewer F3 pro zdrojové soubory a různé textové konfigurační soubory

Cílem tohoto rozšíření je implementace nového pluginu - prohlížeče různých typů zdrojových a konfiguračních textových souborů.

Tento plugin bude sloužit jako primární prohlížeč (tj. read only) po stisknutí F3 namísto současného built-in textového prohlížeče.

Cílem je, aby uživatel mohl dostat rychlý náhled se zvýrazněním syntaxe daného textového zdrojového nebo konfiguračního souboru.

Současný stav je takový, že když prohlížím .js, .ts, .cpp, .c, .yml, .yaml, .toml, .ini, .php, .java a stovky dalších textových
a zdrojových souborů, tak se mi zobrazí, pokud nemám nastaveno jinak, ve výchozím built-in view prohlížeči bez jakékoliv zvýraznění
syntaxe.

## Technické řešení

Proveď detailní analýzu, jak by bylo nejlepší a nejefektinější celé řešení realizovat - především s ohledem na podporu co
největšího počtu různých zdrojových/konfiguračních souborů a možnosti nastavení různých barevných schémet od světlých po tmavé.

Ideální by nejspíš bylo použit webový render (webview) a syntaxi zvýrazňovat pomocí HTML. K tomu ostatně máme připravený
sdílení webview - předehřívaný v rácmi úpravy Markdown view - viz: 065-mdview-instant-render. Ideální by tedy bylo
vytvoření tohoto pluginu analogicky jako MDView plugin s tím, že oba budou sdílet předehřívaný (tj. do paměti připravený)
webview.

WebView, resp. renderování pomocí HTML by zároveň mohlo být vhodné, protože určitě budou existovat přímo webové knihovny
právě pro mnoho stovek různých textových formátů ke zvýraznění.

## Barevné motivy zvýrazňování syntaxe

Plugin musí určitě obshaovat alespoň několik světlých a tmavých témat pro zvýraznění.

