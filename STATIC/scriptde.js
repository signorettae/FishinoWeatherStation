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

	$.getJSON("/data_de.js", function(data) {
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

        $("#feuchtigkeit").text(data.feuchtigkeit);
        $("#temperatur").text(data.temperatur);
        $("#luftdruck").text(data.luftdruck);
        $("#wind").text(data.wind);
        $("#co").text(data.co);
        $("#uv").text(data.uv);
        $("#regen").text(data.regen);
        $("#blitz").text(data.blitz);
        $("#datumstunde").text(data.datumstunde);
	}

    getData();
});
