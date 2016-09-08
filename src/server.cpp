//
// Created by vladislav on 07.09.16.
//

#include "server.hpp"

Server::Server(const std::string &address, const std::string & port, const std::string &directory, size_t workers)
        : requestHandler(directory), threadPool(workers), signalSet(ioService), tcpAcceptor(ioService), tcpSocket(ioService)  {
    bait::resolver resolver(ioService);
    bait::endpoint endpoint = *resolver.resolve({address, port});
    tcpAcceptor.open(endpoint.protocol());
    tcpAcceptor.set_option(bait::acceptor::reuse_address(true));
    tcpAcceptor.bind(endpoint);
    tcpAcceptor.listen();

    signalSet.add(SIGINT);
    signalSet.add(SIGTERM);
    signalSet.async_wait([this](boost::system::error_code, int) { stop(); });
}

Server::~Server() {
    stop();
}

void Server::listen() {
    for (;;) {
        boost::system::error_code ec;
        tcpAcceptor.accept(tcpSocket, ec);
        if (ec) {

            return;
        }

        ConnectionPtr connection = std::make_shared<Connection>(std::move(tcpSocket), requestHandler,
                                                                [this](ConnectionPtr connectionPtr) {
                                                                    std::unique_lock<std::mutex> lock(disconnect_mutex);
                                                                    connectedClients.erase(connectionPtr);
                                                                });
        {
            std::unique_lock<std::mutex> lock(disconnect_mutex);
            connectedClients.insert(connection);
        }

        threadPool.enqueue(std::bind(&Connection::read, &*connection));
    }
}

void Server::stop() {
    std::unique_lock<std::mutex> lock(disconnect_mutex);
    tcpAcceptor.close();
    tcpSocket.close();
    for (auto& cli: connectedClients)
        cli->close();
}
