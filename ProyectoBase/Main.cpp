#include <SFML\Graphics.hpp>
#include <SFML\Network.hpp>
#include <string>
#include <thread>
#include <mutex>
#include <iostream>
#include <vector>

#define MAX_MENSAJES 30

std::mutex mu;
enum conexionType{blockThread, nonBlock, SockSelector};

void sendNormal(sf::TcpSocket* socket, std::string msj) {
	sf::Socket::Status status = socket->send(msj.c_str(), msj.length());
	if (status != sf::Socket::Done) {
		std::cout << "Error al enviar\n";
	}
}


void sendMessage(sf::TcpSocket* socket, std::string msj, char type) {
	if (type == 's')
		msj = "Server:	" + msj;
	else if(type == 'c')
		msj = "Client:	" + msj;
	sf::Socket::Status status = socket->send(msj.c_str(), msj.length());
	if (status != sf::Socket::Done) {
		std::cout << "Error al enviar\n";
	}
}

void sendMessageNoBlock(sf::TcpSocket* socket, std::string msj, char type) {
	if (type == 's')
		msj = "Server:	" + msj;
	else if (type == 'c')
		msj = "Client:	" + msj;
	size_t sent;
	//primer envio
	sf::Socket::Status status = socket->send(msj.c_str(), msj.length(), sent);
	//si el envio es partial reenviamos
	while (status == sf::Socket::Partial) {
		size_t newsent;
		status = socket->send(&msj.c_str()[sent], msj.length()-sent, newsent);
		sent += newsent;
	}
	//comprovamos si ha habido errores
	if (status != sf::Socket::Done)
		std::cout << "Error al enviar\n";

}
void sendExit(sf::TcpSocket* socket, char type, conexionType conType) {
	std::string msj= "se ha desconectado\n";
		

	if (conType == conexionType::nonBlock) {
		sendMessageNoBlock(socket, msj, type);
	}
	else {
		sendMessage(socket, msj, type);
	}
}

void receiveNonBlock(sf::TcpSocket* socket, std::vector<std::string>* aMensajes) {
	char data[100];
	std::size_t bytesReceived;

	sf::Socket::Status statusReceive = socket->receive(data, 100, bytesReceived);
	if (statusReceive == sf::Socket::NotReady) {
		return;
	}
	else if (statusReceive == sf::Socket::Done) {
		data[bytesReceived] = '\0';
		std::string str = data;
		aMensajes->push_back(str);
	}


}


void receive(sf::TcpSocket* socket, std::vector<std::string>* aMensajes) {
	bool open = true;
	char buffer[100];
	std::size_t bytesReceived;
	while (open) {
		sf::Socket::Status status = socket->receive(&buffer, 100, bytesReceived);
		if (status == sf::Socket::Status::Disconnected) {
			open = false;
		}
		else if(status==sf::Socket::Status::Done){
			buffer[bytesReceived] = '\0';
			mu.lock();
			aMensajes->push_back(std::string(buffer));
			mu.unlock();
		}
	}
}

void receiveBySelector(sf::SocketSelector* selector,sf::TcpSocket* socket, std::vector<std::string>* aMensajes) {
	bool open = true;
	char buffer[100];
	std::size_t bytesReceived;
	while (open) {
		//esperamos hasta que haya datos entrantes en alguno de los sockets dentro del selector
		if (selector->wait()) {
			//como sabemos que solo hay una conexion no necesitamos mirar el listener ni tener una lista de sockets
			if (selector->isReady(*socket)) {
				sf::Socket::Status status = socket->receive(&buffer, 100, bytesReceived);
				if (status == sf::Socket::Status::Disconnected) {
					open = false;
				}
				else if (status == sf::Socket::Status::Done) {
					buffer[bytesReceived] = '\0';
					mu.lock();
					aMensajes->push_back(std::string(buffer));
					mu.unlock();
				}
			}
		}
	}
}

int main()
{
	//establecimiento de conexion
	//preguntar si es cliente o servidor (se ha de inicializar el server primero)
	char type;
	conexionType conType;
	std::thread t1;
	std::vector<std::string> aMensajes;

	sf::TcpSocket socket;
	sf::SocketSelector selector;
	std::cout << "Enter (s) for Server, Enter (c) for Client: ";
	std::cin >> type;
	if (type == 's') {
		//inicializamos server
		sf::TcpListener listener;
		//preguntamos por el tipo de conexion
		std::cout << "Select a type of conexion:\n	- 0: Blocking + Threading\n	- 1: NonBlocking\n	- 2: SocketSelector\n";
		int i;
		std::cin >> i;
		conType =(conexionType) i;

		//escuchamos si el cliente se quiere conectar
		sf::Socket::Status status=listener.listen(5000);

		//comprovamos que este bien
		if (status != sf::Socket::Done) {
			std::cout << "Listener no aceptado\n";
		}
		else {
			std::cout <<  "Listener Ready\n";
			//aceptamos y comprovamos que este bien
			if (listener.accept(socket) != sf::Socket::Done) {
				std::cout << "Accept fallido";
			}
			else {
				std::cout << "Accept done\n";
				//enviamos el tipo de conexion que establecemos
				switch (conType)
				{
				case blockThread:
					sendNormal(&socket, "0");
					//inicializamos el thread
					 t1= std::thread(&receive, &socket, &aMensajes);
					break;
				case nonBlock:
					sendNormal(&socket, "1");
					//seteamos el socket a nonblocking
					socket.setBlocking(false);
					break;
				case SockSelector:
					sendNormal(&socket, "2");
					//añadimos el socket al selector
					selector.add(socket);
					//inicializamos el thread
					t1 = std::thread(&receiveBySelector, &selector, &socket, &aMensajes);
					
					break;
				default:
					break;
				}
			}
		}
		listener.close();
	}
	else if (type == 'c') {
		sf::Socket::Status status= socket.connect("127.0.0.1", 5000, sf::seconds(5.f));

		if (status != sf::Socket::Status::Done) {
			std::cout << "Problema al establecer conexion\n";
		}
		else {
			std::cout << "Conectado\n";
			//esperamos a que el server nos diga el tipo de conexion
			char buffer;
			std::size_t bytesReceived;
			sf::Socket::Status status = socket.receive(&buffer, 100, bytesReceived);
			if (status == sf::Socket::Status::Disconnected) {
				std::cout << "Problema al establecer conexion\n";
			}
			else if (status == sf::Socket::Status::Done) {
				conType = (conexionType)(buffer - '0');
				switch (conType)
				{
				case blockThread:
					std::cout << "Modo Blocking\n";
					//inicializamos el thread
					t1 = std::thread(&receive, &socket, &aMensajes);
					break;
				case nonBlock:
					std::cout << "Modo NonBlocking\n";
					//seteamos el socket a non blocking
					socket.setBlocking(false);
					break;
				case SockSelector:
					std::cout << "Modo Socket Selector\n";
					//añadimos el socket al selector
					selector.add(socket);
					//inicializamos el thread
					t1 = std::thread(&receiveBySelector, &selector, &socket, &aMensajes);
				default:
					break;
				}
			}
		}
	}
	else {
		std::cout << "Not a valid Type\n";
	}

	sf::Vector2i screenDimensions(800, 600);

	sf::RenderWindow window;
	window.create(sf::VideoMode(screenDimensions.x, screenDimensions.y), "Chat");

	sf::Font font;
	if (!font.loadFromFile("calibril.ttf"))
	{
		std::cout << "Can't load the font file" << std::endl;
	}

	sf::String mensaje = " >";

	sf::Text chattingText(mensaje, font, 14);
	chattingText.setFillColor(sf::Color(0, 160, 0));
	chattingText.setStyle(sf::Text::Bold);


	sf::Text text(mensaje, font, 14);
	text.setFillColor(sf::Color(0, 160, 0));
	text.setStyle(sf::Text::Bold);
	text.setPosition(0, 560);

	sf::RectangleShape separator(sf::Vector2f(800, 5));
	separator.setFillColor(sf::Color(200, 200, 200, 255));
	separator.setPosition(0, 550);
	

	while (window.isOpen())
	{
		sf::Event evento;
		while (window.pollEvent(evento))
		{
			switch (evento.type)
			{
			case sf::Event::Closed:
				window.close();
				break;
			case sf::Event::KeyPressed:
				if (evento.key.code == sf::Keyboard::Escape)
					window.close();
				else if (evento.key.code == sf::Keyboard::Return)
				{
					if (mensaje == ">exit") {
						window.close();
						sendExit(&socket, type, conType);
					}
					else{
						//SEND
						switch (conType)
						{
						case blockThread:
							sendMessage(&socket, mensaje,type);
							break;
						case nonBlock:
							sendMessageNoBlock(&socket, mensaje, type);
							break;
						case SockSelector:
							sendMessage(&socket, mensaje, type);
							break;
						default:
							break;
						}
					
						mu.lock();
						aMensajes.push_back(mensaje);
						mu.unlock();

						if (aMensajes.size() > 25)
						{
							aMensajes.erase(aMensajes.begin(), aMensajes.begin() + 1);
						}

						mensaje = ">";
					}
				}
				break;
			case sf::Event::TextEntered:
				if (evento.text.unicode >= 32 && evento.text.unicode <= 126)
					mensaje += (char)evento.text.unicode;
				else if (evento.text.unicode == 8 && mensaje.getSize() > 0)
					mensaje.erase(mensaje.getSize() - 1, mensaje.getSize());
				break;
			}
		}
		//recibir datos
		if (conType == conexionType::nonBlock) {
			receiveNonBlock(&socket, &aMensajes);
		}

		window.draw(separator);
		for (size_t i = 0; i < aMensajes.size(); i++)
		{
			std::string chatting = aMensajes[i];
			chattingText.setPosition(sf::Vector2f(0, 20 * i));
			chattingText.setString(chatting);
			window.draw(chattingText);
		}
		std::string mensaje_ = mensaje + "_";
		text.setString(mensaje_);
		window.draw(text);


		window.display();
		window.clear();
	}
	socket.disconnect();
	if(conType!=conexionType::nonBlock)
		t1.join();
}