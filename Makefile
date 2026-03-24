
build:
	gcc -o file_server file_server.c

player:
ifeq ($(OS),Windows_NT)
	start player.html
else
	xdg-open player.html >/dev/null 2>&1 || open player.html
endif
