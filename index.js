const http = require("http");
const fs = require("fs");

http.createServer((req, res) => {
	if (req.url == "/") {
		res.writeHead(200, { "Content-Type": "text/html" });
		res.write(fs.readFileSync("index.html"));
		res.end();
	} else if (req.url == "/script.js") {
		res.writeHead(200, { "Content-Type": "text/javascript" });
		res.write(fs.readFileSync("script.js"));
		res.end();
	} else {
		res.statusCode = 404;
		res.end();
	}
}).listen(3000);
