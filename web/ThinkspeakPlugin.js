<script type='text/javascript' src='https://ajax.googleapis.com/ajax/libs/jquery/1.9.1/jquery.min.js'></script>
<script type='text/javascript' src='https://www.google.com/jsapi'></script>
<script type='text/javascript'>

  // set your channel id here
  var channel_id = 8203;
  // set your channel's read api key here if necessary
  var api_key = '';

  var tankFullDistance = 30;
  var tankEmptyDistance = 283;

  // global variables
  var waterLevelData;
  var waterLevelChart;

  var batteryVoltageData;
  var batteryVoltageChart;

  var tempData;
  var tempChart;

  var csqData;
  var csqChart;

  // load the google gauge visualization
  google.load('visualization', '1', {packages:['gauge','corechart','bar']});
  google.setOnLoadCallback(initChart);

  // display the data
  function displayData(waterLevel, waterLevelTime, batteryVoltage, temp, csq) {
  //var view = new google.visualization.DataView(waterLevelData);
  //view.setColumns([0, 1,
  //{ calc: "stringify",
  //sourceColumn: 1,
  //type: "string",
  //role: "annotation" },
  //2]);

  waterLevelData.setValue(0, 1, waterLevel);
  waterLevelData.setValue(0, 3, waterLevel.toString() + '%');
  waterLevelChart.draw(waterLevelData, google.charts.Bar.convertOptions(waterLevelOptions));

  batteryVoltageData.setValue(0, 0, 'Battery (V)');
  batteryVoltageData.setValue(0, 1, batteryVoltage);
  batteryVoltageChart.draw(batteryVoltageData, batteryVoltageOptions);

  tempData.setValue(0, 0, 'Temp (C)');
  tempData.setValue(0, 1, temp);
  tempChart.draw(tempData, tempOptions);

  csqData.setValue(0, 0, 'Signal');
  csqData.setValue(0, 1, csq);
  csqChart.draw(csqData, csqOptions);
  }

  // load the data
  function loadData() {
    // get the data from thingspeak
    $.getJSON('https://api.thingspeak.com/channels/' + channel_id +
            '/feeds.json?days=2', function(latestData) {

    // search backwards for the most recent non-null water level field
    var feedsLastIndex = latestData.feeds.length - 1;
    var searchIndex = feedsLastIndex;
    while ((searchIndex >= 0) &&
           (latestData.feeds[searchIndex].field6 == null)) {
        --searchIndex;
    }
		  
    if (searchIndex >= 0) {
        // found non-null water level field
        var waterLevelData = latestData.feeds[searchIndex];

        // set latest water level
        waterLevel = parseInt(waterLevelData.field6);
        if (waterLevel < 0) {
            waterLevel = 0;
        } else if (waterLevel > 100){
            waterLevel = 100;
        }
        waterLevelTime = waterLevelData.created_at;
    } else {
      waterLevel = 0;
    }

    // compute the average battery voltage over the latest feed data
    var numBatteryVoltageSamples = 0;
    var batteryVoltageSum = 0.0;
    for (i = 0; i < latestData.feeds.length; ++i) {
        var batteryVoltageSample = latestData.feeds[i].field3;
        if (batteryVoltageSample !== null) {
            ++numBatteryVoltageSamples;
            batteryVoltageSum += parseFloat(batteryVoltageSample);
        }
    }
    var batteryVoltageAverage = 0;
    if (numBatteryVoltageSamples > 0) {
        batteryVoltageAverage = batteryVoltageSum / numBatteryVoltageSamples;
    }
    
  channelUpdateTime = latestData.feeds[feedsLastIndex].created_at;
  batteryVoltage = batteryVoltageAverage.toString();
  temp = latestData.feeds[feedsLastIndex].field2;
  csq = latestData.feeds[feedsLastIndex].field4;

  // if there is a data point display it
  if (batteryVoltage && temp) {
  displayData(waterLevel, waterLevelTime, batteryVoltage, temp, csq);
  }

  var waterLevelUpdateTime = new Date(waterLevelTime);
  waterLevelAsOfDiv.innerHTML = '(Water level as of<br> ' +
    waterLevelUpdateTime.toLocaleString() + ')';
  var lastUpdateTime = new Date(channelUpdateTime);
  asOfDiv.innerHTML = '(Data last updated at ' +
    lastUpdateTime.toLocaleString() + ')';
});

}

  // initialize the chart
  function initChart() {

  waterLevelData = google.visualization.arrayToDataTable([
  ['Tank', 'Level', { role: 'style' },{ role: 'annotation' }],
  ['Water', 0, 'blue', '0%']            // RGB value
  ]);
  waterLevelChart = new google.visualization.ColumnChart(document.getElementById('waterLevel_div'));

  batteryVoltageData = new google.visualization.DataTable();
  batteryVoltageData.addColumn('string', 'Label');
  batteryVoltageData.addColumn('number', 'Value');
  batteryVoltageData.addRows(1);
  batteryVoltageChart = new google.visualization.Gauge(document.getElementById('batteryVoltage_div'));

  tempData = new google.visualization.DataTable();
  tempData.addColumn('string', 'Label');
  tempData.addColumn('number', 'Value');
  tempData.addRows(1);
  tempChart = new google.visualization.Gauge(document.getElementById('temp_div'));

  csqData = new google.visualization.DataTable();
  csqData.addColumn('string', 'Label');
  csqData.addColumn('number', 'Value');
  csqData.addRows(1);
  csqChart = new google.visualization.Gauge(document.getElementById('csq_div'));

  waterLevelAsOfDiv = document.getElementById('waterLevelAsOf_div');
  asOfDiv = document.getElementById('asOf_div');

  waterLevelOptions = {
  title: "water level in tank (%)",
  width: 240,
  height: 240,
  vAxis: {viewWindow: {min: 0, max: 100}},
  bars: 'vertical',
  legend: { position: "none" }
  };

  batteryVoltageOptions = {
  width: 120,
  height: 120,
  max: 8,
  redFrom: 0, redTo: 3.5,
  yellowFrom:3.5, yellowTo:4.5,
  greenFrom:4.5, greenTo:8,
  majorTicks:['0','1','2','3','4','5','6','7','8'],
  minorTicks: 4};

  tempOptions = {
  width: 120,
  height: 120,
  max: 50,
  redFrom: 35, redTo: 50,
  majorTicks:['0','10','20','30','40','50'],
  minorTicks: 10};

  csqOptions = {
  width: 120,
  height: 120,
  max: 25,
  redFrom: 0, redTo: 3,
  yellowFrom:3, yellowTo:5,
  greenFrom:5, greenTo:25,
  majorTicks:['0','5','10','15','20','25'],
  minorTicks: 5};

  loadData();

  // load new data every 60 seconds
  setInterval('loadData()', 60000);
  }

</script>
