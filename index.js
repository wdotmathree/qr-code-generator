const http = require("http");
const fs = require("fs");
const { spawn } = require("child_process");

http.createServer((req, res) => {
	if (req.url == "/") {
		res.writeHead(200, { "Content-Type": "text/html" });
		res.write(fs.readFileSync("index.html"));
		res.end();
	} else if (req.url == "/script.js") {
		res.writeHead(200, { "Content-Type": "text/javascript" });
		res.write(fs.readFileSync("script.js"));
		res.end();
	} else if (req.url == "/generate") {
		if (req.method != "POST") {
			res.writeHead(405, { "Content-Type": "text/plain" });
			res.write("Method not allowed");
			res.end();
		}
		const upstream = spawn("python3", ["qr.py"]);
		req.on("data", (chunk) => {
			upstream.stdin.write(chunk);
		}).on("end", () => {
			upstream.stdin.end();
		});
		upstream.stdout
			.on("data", (chunk) => {
				res.write(chunk);
			})
			.on("end", () => {
				res.end();
			});
	} else {
		res.statusCode = 404;
		res.end();
	}
}).listen(3000);
