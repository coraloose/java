import java.io.*;
import java.net.*;

/**
 * Client for the voting system.
 * 
 * Usage:
 *   java Client list
 *   java Client vote <option>
 * 
 * This client connects to the server on localhost:7777,
 * sends a request, and prints the server's response.
 */
public class Client {
    // 服务器地址和端口号
    private static final String HOST = "localhost";
    private static final int PORT = 7777;
    
    public static void main(String[] args) {
        // 检查命令行参数，必须提供至少一个参数
        if (args.length < 1) {
            System.err.println("Usage: java Client <command> [option]");
            System.err.println("Commands: list, vote <option>");
            System.exit(1);
        }
        
        String command = args[0];
        String request;
        // 根据命令构造请求字符串
        if (command.equalsIgnoreCase("list")) {
            request = "list";
        } else if (command.equalsIgnoreCase("vote")) {
            if (args.length < 2) {
                System.err.println("Error: 'vote' command requires an option. Usage: vote <option>");
                System.exit(1);
                return; // 虽然 System.exit 已终止，但加 return 保证编译通过
            }
            request = "vote " + args[1];
        } else {
            System.err.println("Error: Invalid command. Please use 'list' or 'vote <option>'.");
            System.exit(1);
            return;
        }
        
        // 尝试与服务器建立连接，并发送请求
        try (Socket socket = new Socket(HOST, PORT);
             PrintWriter out = new PrintWriter(socket.getOutputStream(), true);
             BufferedReader in = new BufferedReader(
                     new InputStreamReader(socket.getInputStream()))) {
            
            out.println(request);
            // 读取并输出服务器响应
            String response;
            while ((response = in.readLine()) != null) {
                System.out.println(response);
            }
        } catch (UnknownHostException e) {
            System.err.println("Error: Unknown host " + HOST + ". " + e.getMessage());
            System.exit(1);
        } catch (IOException e) {
            System.err.println("Error: Unable to connect to " + HOST + " on port " + PORT + ". " + e.getMessage());
            System.exit(1);
        }
    }
}
