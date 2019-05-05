$(document).ready(function() {

	var exampleData = {
        "umidita": "44%",
        "temperatura": "24°C",
        "pressione": "1008.54hPa",
        "vento": "2km/h",
        "co": "...",
        "uv": "...",
        "pioggia": "2ml",
        "ulmini": "Si",
        "dataora": "24/01/2017 12:53:22"
	};

	window.setTimeout(function() {
		getData();
	}, 1000);

	function getData() {

	$.getJSON("/data_en.js", function(data) {
		// Stampo i dati ricevuti nella console del browser
		console.log(data);

		// Sistemo i dati negli elementi html
		placeData(data);

		window.setTimeout(function() {
			getData();
		}, 400);

	});
}

	function placeData(data) {
		// Qui sistemo i dati

        $("#humidity").text(data.humidity);
        $("#temperature").text(data.temperature);
        $("#pressure").text(data.pressure);
        $("#wind").text(data.wind);
        $("#uv").text(data.uv);
        $("#rain").text(data.rain);
        $("#strokes").text(data.strokes);
        $("#datetime").text(data.datetime);
	}

    getData();
});
