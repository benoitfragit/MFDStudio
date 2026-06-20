// Renders ```mermaid code blocks client-side, loading mermaid.js lazily so
// pages without diagrams never pay for it.
(function ()
{
    function convertCodeBlocksToMermaidDivs()
    {
        var blocks = document.querySelectorAll("code.language-mermaid");
        if (blocks.length === 0)
        {
            return false;
        }

        blocks.forEach(function (block)
        {
            var pre = block.parentElement;
            var container = document.createElement("div");
            container.className = "mermaid";
            container.textContent = block.textContent;
            pre.replaceWith(container);
        });

        return true;
    }

    function loadMermaidScript(onLoaded)
    {
        var script = document.createElement("script");
        script.src = "https://cdn.jsdelivr.net/npm/mermaid@10/dist/mermaid.min.js";
        script.onload = onLoaded;
        document.head.appendChild(script);
    }

    function initializeMermaid()
    {
        var isDark = document.documentElement.className.indexOf("navy") !== -1 ||
            document.documentElement.className.indexOf("coal") !== -1 ||
            document.documentElement.className.indexOf("ayu") !== -1;

        window.mermaid.initialize({
            startOnLoad: false,
            theme: isDark ? "dark" : "default",
        });
        window.mermaid.run({ querySelector: ".mermaid" });
    }

    document.addEventListener("DOMContentLoaded", function ()
    {
        if (!convertCodeBlocksToMermaidDivs())
        {
            return;
        }

        loadMermaidScript(initializeMermaid);
    });
})();
