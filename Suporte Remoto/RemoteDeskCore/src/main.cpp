#include "RemoteDesktopSystem.h"
#include <iostream>
#include <string>
#include <vector>

/**
 * @brief Imprime as instruções de uso do programa.
 */
void PrintUsage() {
    std::cout << "Uso: remote_desktop_app.exe [modo] [opcoes]" << std::endl;
    std::cout << "--------------------------------------------------" << std::endl;
    std::cout << "Modos de Conexao:" << std::endl;
    std::cout << "  host                      - Inicia como host, gera um ID de sessao para acesso remoto." << std::endl;
    std::cout << "  join <id-da-sessao>       - Conecta a um host usando um ID de sessao." << std::endl;
    std::cout << "\nModos de Teste (Rede Local):" << std::endl;
    std::cout << "  server <porta>            - (LAN) Inicia no modo servidor, escutando na porta." << std::endl;
    std::cout << "  client <ip> <porta>       - (LAN) Inicia no modo cliente, conectando ao IP e porta." << std::endl;
    std::cout << "  loopback (ou sem args)    - Inicia no modo de teste loopback local." << std::endl;
    std::cout << "\nExemplos:" << std::endl;
    std::cout << "  remote_desktop_app.exe server 12345" << std::endl;
    std::cout << "  remote_desktop_app.exe client 192.168.1.100 12345" << std::endl;
}

/**
 * @brief Ponto de entrada principal da aplicação.
 *        Interpreta os argumentos da linha de comando para iniciar nos modos
 *        loopback, servidor ou cliente.
 */
int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv, argv + argc);
    RemoteDesktopSystem system;

    // Modo Loopback (padrão)
    if (args.size() == 1 || args[1] == "loopback") {
        std::cout << "Iniciando em modo Loopback..." << std::endl;
        if (system.InitializeLoopback(1920, 1080)) {
            system.Run();
        } else {
            std::cerr << "Falha ao inicializar o modo Loopback." << std::endl;
            return 1;
        }
    }
    // Modo Host (NAT Traversal)
    else if (args.size() == 2 && args[1] == "host") {
        std::cout << "Iniciando em modo Host..." << std::endl;
        system.SetUseMultiThreading(true);
        system.SetUseEncoding(true);
        system.SetUseNetworking(true);
        system.SetInputEnabled(true);

        // InitializeAsHost se conectaria ao servidor de sinalização e obteria um ID
        if (!system.InitializeAsHost("ws://your-signaling-server.com:8080")) {
            std::cerr << "Falha ao inicializar o modo Host." << std::endl;
            return 1;
        }
        system.Run();
    }
    // Modo Join/Guest (NAT Traversal)
    else if (args.size() == 3 && args[1] == "join") {
        std::string sessionId = args[2];
        std::cout << "Iniciando em modo Join para a sessao: " << sessionId << "..." << std::endl;
        system.SetUseMultiThreading(true);
        system.SetInputEnabled(true);

        if (!system.InitializeAsGuest("ws://your-signaling-server.com:8080", sessionId)) {
            std::cerr << "Falha ao conectar a sessao." << std::endl;
            return 1;
        }
        system.Run();
    }
    // Modo Servidor
    else if (args.size() == 3 && args[1] == "server") {
        try {
            int port = std::stoi(args[2]);
            std::cout << "Iniciando em modo Servidor na porta " << port << "..." << std::endl;

            system.SetUseMultiThreading(true);
            system.SetUseEncoding(true);
            system.SetUseNetworking(true);
            system.SetInputEnabled(true);

            if (!system.InitializeAsServer(port, 25)) { // Bitrate de 25 Mbps
                std::cerr << "Falha ao inicializar o modo Servidor." << std::endl;
                return 1;
            }
            system.Run();
        } catch (const std::exception& e) {
            std::cerr << "Porta invalida. Erro: " << e.what() << std::endl;
            PrintUsage();
            return 1;
        }
    }
    // Modo Cliente
    else if (args.size() == 4 && args[1] == "client") {
        try {
            std::string ip = args[2];
            int port = std::stoi(args[3]);
            std::cout << "Iniciando em modo Cliente para " << ip << ":" << port << "..." << std::endl;

            system.SetUseMultiThreading(true);
            system.SetInputEnabled(true);

            if (!system.InitializeAsClient(ip, port)) {
                std::cerr << "Falha ao inicializar o modo Cliente." << std::endl;
                return 1;
            }
            system.Run();
        } catch (const std::exception& e) {
            std::cerr << "Porta invalida. Erro: " << e.what() << std::endl;
            PrintUsage();
            return 1;
        }
    }
    // Argumentos inválidos
    else {
        PrintUsage();
        return 1;
    }

    return 0;
}