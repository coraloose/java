import java.io.*;
import java.net.*;
import java.text.SimpleDateFormat;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.locks.ReentrantLock;

/**
 * Server for the voting system.
 * 
 * Usage:
 *   java Server option1 option2 [option3 ...]
 * 
 * Requirements:
 * - Accept at least two voting options (each a single word).
 * - Use a fixed thread pool with 30 threads to manage client connections.
 * - Process two types of requests:
 *     - "list": return the current voting status.
 *     - "vote <option>": increment the vote count for <option> (if exists).
 * - Log every valid client request into log.txt in the format:
 *      date|time|client IP address|request
 */
public class Server {
    // Map to store voting options and their counts.
    private static Map<String, Integer> voteCounts = new HashMap<>();
    // Lock for synchronizing updates to voteCounts.
    private static ReentrantLock lock = new ReentrantLock();
    // Server listening port.
    private static final int PORT = 7777;
    // Fixed thread pool size.
    private static final int THREAD_POOL_SIZE = 30;
    // Log file to record valid client requests.
    private static File logFile = new File("log.txt");

    public static void main(String[] args) {
        // 检查命令行参数，确保至少有两个投票选项
        if (args.length < 2) {
            System.err.println("Error: At least two voting options are required.");
            System.err.println("Usage: java Server option1 option2 [option3 ...]");
            System.exit(1);
        }
        
        // 初始化投票选项，所有选项初始票数为 0
        for (String option : args) {
            voteCounts.put(option, 0);
        }
        
        // 如果 log.txt 文件已存在，则删除旧文件，确保每次启动都记录新日志
        if (logFile.exists() && !logFile.delete()) {
            System.err.println("Warning: Unable to delete existing log.txt file.");
        }
        
        // 创建固定大小的线程池
        ExecutorService executor = Executors.newFixedThreadPool(THREAD_POOL_SIZE);
        
        try (ServerSocket serverSocket = new ServerSocket(PORT)) {
            System.out.println("Server started. Listening on port " + PORT);
            // 持续监听客户端连接
            while (true) {
                try {
                    Socket clientSocket = serverSocket.accept();
                    executor.execute(new ClientHandler(clientSocket));
                } catch (IOException e) {
                    System.err.println("Error accepting client connection: " + e.getMessage());
                }
            }
        } catch (IOException e) {
            System.err.println("Server failed to start on port " + PORT + ": " + e.getMessage());
        } finally {
            executor.shutdown();
        }
    }
    
    /**
     * 记录客户端请求到 log.txt 文件中。
     * 格式为: yyyy-MM-dd|HH:mm:ss|client IP|request
     *
     * @param clientIP 客户端 IP 地址
     * @param request  请求类型 ("list" 或 "vote")
     */
    private static void logRequest(String clientIP, String request) {
        SimpleDateFormat dateFormat = new SimpleDateFormat("yyyy-MM-dd|HH:mm:ss");
        String logEntry = dateFormat.format(new Date()) + "|" + clientIP + "|" + request;
        try (FileWriter fw = new FileWriter(logFile, true);
             BufferedWriter bw = new BufferedWriter(fw);
             PrintWriter out = new PrintWriter(bw)) {
            out.println(logEntry);
        } catch (IOException e) {
            System.err.println("Error writing to log file: " + e.getMessage());
        }
    }
    
    /**
     * 内部类，用于处理每个客户端连接。
     */
    private static class ClientHandler implements Runnable {
        private Socket clientSocket;
        
        public ClientHandler(Socket socket) {
            this.clientSocket = socket;
        }
        
        @Override
        public void run() {
            String clientIP = clientSocket.getInetAddress().getHostAddress();
            try (BufferedReader in = new BufferedReader(
                     new InputStreamReader(clientSocket.getInputStream()));
                 PrintWriter out = new PrintWriter(clientSocket.getOutputStream(), true)) {
                 
                // 从客户端读取请求
                String request = in.readLine();
                if (request == null || request.trim().isEmpty()) {
                    out.println("Error: Received empty request.");
                    return;
                }
                
                String response;
                // 将请求按空格拆分，支持 "list" 或 "vote <option>" 格式
                String[] tokens = request.trim().split("\\s+");
                if (tokens[0].equalsIgnoreCase("list")) {
                    // 构造返回所有投票选项及票数的字符串
                    StringBuilder sb = new StringBuilder();
                    for (Map.Entry<String, Integer> entry : voteCounts.entrySet()) {
                        sb.append("'").append(entry.getKey()).append("' has ")
                          .append(entry.getValue()).append(" vote(s).\n");
                    }
                    response = sb.toString();
                    logRequest(clientIP, "list");
                } else if (tokens[0].equalsIgnoreCase("vote")) {
                    if (tokens.length < 2) {
                        response = "Error: Missing voting option. Usage: vote <option>";
                    } else {
                        String option = tokens[1];
                        if (voteCounts.containsKey(option)) {
                            // 使用锁确保票数更新的线程安全性
                            lock.lock();
                            try {
                                int current = voteCounts.get(option);
                                voteCounts.put(option, current + 1);
                            } finally {
                                lock.unlock();
                            }
                            response = "Incremented the number of votes for '" + option + "'.";
                            logRequest(clientIP, "vote");
                        } else {
                            response = "Error: Option '" + option + "' does not exist.";
                        }
                    }
                } else {
                    response = "Error: Invalid command. Please use 'list' or 'vote <option>'.";
                }
                // 发送响应到客户端
                out.println(response);
            } catch (IOException e) {
                System.err.println("Client handler encountered an error for client " 
                                   + clientIP + ": " + e.getMessage());
            } finally {
                try {
                    clientSocket.close();
                } catch(IOException ex) {
                    // 无法关闭客户端连接时输出警告
                    System.err.println("Warning: Failed to close client socket: " + ex.getMessage());
                }
            }
        }
    }
}
