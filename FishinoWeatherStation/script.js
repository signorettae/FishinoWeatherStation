$(document).ready(function() {

	var exampleData = {
        "umidita": "44%",
        "temperatura": "24°C",
        "pressione": "1008.54hPa",
        "vento": "2km/h",
        "uv": "...",
        "pioggia": "2ml",
        "ulmini": "Si",
        "dataora": "24/01/2017 12:53:22"
	};

	window.setTimeout(function() {
		getData();
	}, 1000);

	function getData() {

	$.getJSON("/data.js", function(data) {
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

        $("#umidita").text(data.umidita);
        $("#temperatura").text(data.temperatura);
        $("#pressione").text(data.pressione);
        $("#vento").text(data.vento);
        $("#uv").text(data.uv);
        $("#pioggia").text(data.pioggia);
        $("#fulmini").text(data.fulmini);
        $("#dataora").text(data.dataora);
	}

    getData();
});
