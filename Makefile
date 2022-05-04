all: cliente servidor

cliente: cliente.cpp
	g++ -o cliente cliente.cpp -pthread

servidor: servidor.cpp
	g++ -o servidor servidor.cpp usuario.cpp -lpthread