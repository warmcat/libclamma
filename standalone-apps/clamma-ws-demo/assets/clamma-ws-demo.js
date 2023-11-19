(function() {

function handle(dest, curr)
{
	let s = curr.split('\n', 1);

	document.getElementById(dest).innerHTML = "<span class=\"query\">" + s[0] +
						  "</span><br>" +
						   curr.substring(s[0].length);
}

/* stuff that has to be delayed until all the page assets are loaded */

window.addEventListener("load", function() {

        let xhr0 = new XMLHttpRequest(), xhr1 = new XMLHttpRequest(),
            xhr2 = new XMLHttpRequest();
            
        xhr0.onreadystatechange = function() {
                document.getElementById("title").innerHTML =
                                        xhr0.response.replace('\n', "<br>");
          
                if (xhr1.readyState === 4) {
                        if (xhr0.status === 200) {
                        } else {
                                console.log('Error: ' + xhr0.status); // An error occurred during the request.
                        }
                }
        };
        
        xhr1.onreadystatechange = function() {
                handle("chat1", xhr1.response);
          
                if (xhr1.readyState === 4) {
                        if (xhr1.status === 200) {
                        } else {
                                console.log('Error: ' + xhr1.status); // An error occurred during the request.
                        }
                }
        };
        
        xhr2.onreadystatechange = function() {
                handle("chat2", xhr2.response);
          
                if (xhr2.readyState === 4) {
                        if (xhr2.status === 200) {
                        } else {
                                console.log('Error: ' + xhr2.status); // An error occurred during the request.
                        }
                }
        };
        
        xhr0.open('GET', '/clamma/desc');
        xhr0.timeout = 60000;
        xhr0.responseType = 'text';
        xhr0.send();
        
        xhr1.open('GET', '/clamma');
        xhr1.timeout = 60000;
        xhr1.responseType = 'text';
        xhr1.send();

        xhr2.open('GET', '/clamma');
        xhr2.timeout = 60000;
        xhr2.responseType = 'text';
        xhr2.send();

}, false);

}());
