# Projeto: Implementação Cliente-Servidor com Raspberry Pi Pico W

Este repositório contém três variações de uma aplicação cliente-servidor utilizando o Raspberry Pi Pico W. Cada variação implementa uma arquitetura de rede diferente para hospedar ou consumir uma interface de monitoramento (painel).

## 📁 joy_server

Neste modo, o **Raspberry Pi Pico W** atua como **servidor** em uma **rede pré-existente** (como uma rede Wi-Fi doméstica).

- **Função**: Hospeda localmente um painel de monitoramento acessível via navegador.
- **Rede**: Conectado a um roteador existente (modo estação).
- **Uso**: Ideal quando há uma infraestrutura Wi-Fi disponível.

---

## 📁 joy_server_ap

Neste modo, o **Pico W** atua como um **servidor e ponto de acesso (Access Point)**.

- **Função**: Cria sua própria rede Wi-Fi e hospeda o painel.
- **Rede**: O próprio Pico cria uma rede e entrega o IP (via DHCP).
- **Uso**: Ideal para ambientes isolados ou onde não há Wi-Fi disponível.

---

## 📁 remote_server

Neste modo, o **Pico W** atua como **cliente**, consumindo dados ou enviando informações para um servidor remoto.

- **Função**: Conecta-se a um servidor central para envio/recebimento de dados.
- **Rede**: Utiliza uma rede Wi-Fi para acessar um servidor externo.
- **Uso**: Indicado para arquiteturas centralizadas com múltiplos dispositivos conectados.

---

## 📌 Observações

- Todos os modos compartilham o objetivo de permitir a interação com um painel de monitoramento.
- A escolha da arquitetura depende do ambiente e da necessidade de acesso à rede local ou externa.

