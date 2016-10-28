function locationSuccess(pos) {
  var coordinates = pos.coords;
  Pebble.sendAppMessage({
    'LONGITUDE': '' + coordinates.longitude,
    'LATITUDE' : '' + coordinates.latitude
  });
}

function locationError(err) {
  console.warn('location error (' + err.code + '): ' + err.message);
  Pebble.sendAppMessage({
    'LONGITUDE': 0,
    'LATITUDE' : 0
  });
}

var locationOptions = {
  'enableHighAccuracy': false,
  'timeout': 150000,
  'maximumAge': 3600000
};

Pebble.addEventListener('ready', function (e) {
  console.log('connect!' + e.ready);
  window.navigator.geolocation.getCurrentPosition(locationSuccess, locationError,
    locationOptions);
  console.log(e.type);
});

Pebble.addEventListener('appmessage', function (e) {
  window.navigator.geolocation.getCurrentPosition(locationSuccess, locationError,
    locationOptions);
  console.log(e.type);
  console.log(e.payload.temperature);
  console.log('message!');
});

Pebble.addEventListener('webviewclosed', function (e) {
  console.log('webview closed');
  console.log(e.type);
  console.log(e.response);
});