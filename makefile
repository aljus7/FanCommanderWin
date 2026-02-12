PROG = fanCommander
MAIN = src/main
FANCONTROL = src/fanControl
READJSON = src/readJson

CC = g++
STD = c++17
CFLAGS = `pkg-config --cflags gtk+-3.0`
LIBS = `pkg-config --libs gtk+-3.0`

${PROG}: ${MAIN}.cpp
	${CC} -std=${STD} -o ${PROG} ${MAIN}.cpp ${FANCONTROL}.cpp ${READJSON}.cpp

clean:
	rm ${PROG}