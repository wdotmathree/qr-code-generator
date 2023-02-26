function main() {
	document.querySelector("form").addEventListener("submit", (e) => {
		e.preventDefault();
		let body = String.fromCharCode(
			document.getElementById("version").value,
			document.getElementById("ec").selectedIndex ^ 1,
			document.getElementById("content").value.length >> 8,
			document.getElementById("content").value.length & 0xff
		);
		body += document.getElementById("content").value;
		fetch(
			new Request("/generate", {
				method: "POST",
				body: body,
			})
		).then((res) => {
			res.text().then((text) => {
				var multiplier = Math.floor(790 / Math.sqrt(text.length));
				multiplier ||= 1;
				document.querySelector("canvas").width = Math.sqrt(text.length) * multiplier;
				document.querySelector("canvas").height = Math.sqrt(text.length) * multiplier;
				for (let i = 0; i < Math.sqrt(text.length); i++) {
					for (let j = 0; j < Math.sqrt(text.length); j++) {
						let ctx = document.querySelector("canvas").getContext("2d");
						if (text[i * Math.sqrt(text.length) + j] == "0") {
							ctx.fillStyle = "white";
						} else if (text[i * Math.sqrt(text.length) + j] == "1") {
							ctx.fillStyle = "black";
						} else if (text[i * Math.sqrt(text.length) + j] == "2") {
							ctx.fillStyle = "red";
						} else if (text[i * Math.sqrt(text.length) + j] == "3") {
							ctx.fillStyle = "blue";
						} else if (text[i * Math.sqrt(text.length) + j] == "4") {
							ctx.fillStyle = "cyan";
						} else {
							ctx.fillStyle = "yellow";
						}
						ctx.fillRect(i * multiplier, j * multiplier, multiplier, multiplier);
					}
				}
			});
		});
	});
}

window.onload = main;
