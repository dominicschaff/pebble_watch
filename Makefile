all: buildapp run

buildapp:
	pebble build

run:
	pebble install --phone 192.168.43.1

logs:
	pebble logs --phone=192.168.43.1

shot:
	pebble screenshot --phone=192.168.43.1

clean:
	pebble clean